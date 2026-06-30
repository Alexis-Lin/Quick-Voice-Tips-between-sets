# 组间兜底语音话术 · 设计方案

> 目标：用户每做完一组（set）进入休息时，**finish 瞬间**就有一句简洁、有温度、且不重复的语音播报，
> 立刻止住"凉场"；随后 LLM 个性化小结再渐进补上。

---

## 1. 背景与问题

### 现状链路

**语音推送（push）**：阶段切到 `REST` 时触发
`CourseInstanceMessageHandler.recordAndSendStateChangeMessage()` → `VoiceHub.trigger(REST)`
→ `RestVoiceEngine.execute()`：

1. 取数据（用户 / 当前+下一组动作 / 完成情况 / 评分 / 纠错）
2. `buildPrompt()`
3. **`aiClientProxy.getLlmResult()` —— 同步阻塞，等整段文案生成完（~2–5s）**
4. 写库
5. `VoiceHub.sendVoiceInteractionMessage()` →
   **`ttsService.getAudioUrl(整段)` —— 同步阻塞，整段 TTS + 上传 OSS（~1–3s）**
   → MQ → connection → websocket → 设备

**文字组间报告（pull）**：前端拉 `StudentTrainDataController#getTrainData`
（纯 DB 组装，本身不慢；力线量化 / 轨迹视频 / 反畸变视频是 euler→video-processor 异步产出，会后到）。

### 延迟根因

语音侧 5~10s 的体感，来自 **②整段 LLM + ④整段 TTS 串行、且都针对整段文本**，
用户必须等两者都跑完才听到第一个字。TTS 的 OSS 缓存按文本 md5，LLM 文案每次不同 → 基本不命中。

---

## 2. 方案总览

把"等整段"拆成两层，**finish 那一刻不再空白**：

```
finish 瞬间 ── ① 兜底短句（固定话术，秒出，TTS 命中 OSS 缓存）
           └─ ② LLM 个性化小结（详细，随后流式逐句补 / 替换）
几秒后     ── ③ 报告 Phase-2：力线 + 视频（异步管线就绪后补齐，本文不展开）
```

本方案聚焦 **① 兜底话术**，它是改动最小、风险最低、最快止血的一环。

### 设计要点（已与产品确认）

| 维度 | 决策 |
|---|---|
| 话术内容 | **纯通用短句**，不带本组具体数字 → TTS 几乎永远命中 OSS 缓存、最快 |
| 话术组织 | **按表现分 3 池**：普通完成 / 破纪录(PB) / 表现优秀 |
| 防重复 | **洗牌袋（shuffle-bag）**，每课程 × 每池，一池用完才可能再现，绝不连续撞 |
| 触发 | 与 LLM **解耦**：REST 触发处先独立秒发兜底，再走 `voiceHub.trigger` 跑 LLM |

---

## 3. 实现设计

### 3.1 新增话术分类 `TextCategoryEnum`

```java
REST_FALLBACK_NORMAL("restFallback:normal", "组间兜底-普通完成"),
REST_FALLBACK_PB("restFallback:pb", "组间兜底-破纪录"),
REST_FALLBACK_EXCELLENT("restFallback:excellent", "组间兜底-表现优秀"),
```

话术文案存 `TextI18n` 表（按 `lang + category` 多语言），**加 / 改话术零改代码**。

### 3.2 触发落点：先兜底，后 LLM

`CourseInstanceMessageHandler.recordAndSendStateChangeMessage()`（REST 分支），
在现有 `voiceHub.trigger` **之前**插一行：

```java
if (RoomStateConstant.REST.equals(phase)) {
    courseInstanceAutoService.restTimeCountdown(...);

    // ★新增：finish 瞬间秒出兜底话术（与 LLM 解耦，不被慢 LLM 阻塞）
    restFallbackVoiceService.sendImmediate(courseInstanceCache.getUserId(), courseInstanceId, totalOrder);

    VoiceContext voiceContext = new VoiceContext(VoiceSceneEnum.REST, ...);
    voiceContext.setCourseStatus(courseStatus);
    voiceHub.trigger(voiceContext);   // 随后：LLM 个性化小结
}
```

> **为什么不做成普通 REST 引擎**：`VoiceEngineManager` 会把 REST 场景下所有引擎跑完，
> 再由 `VoiceHub` 统一 TTS + 发送，会被慢 LLM 引擎拖住。独立 `sendImmediate` 才能真正秒出。

### 3.3 表现分池逻辑（数据全来自 Redis 缓存，快）

复用 `RestVoiceEngine` 同款数据源，缺数据时安全降级到 NORMAL：

```java
OpSumBo sum = courseInstanceOpCompletedService.getOpSumBo(ciId, totalOrder);
CourseInstanceStudentAction perf = courseInstanceStudentActionService.getPerformanceFromCache(ciId, totalOrder);

TextCategoryEnum bucket;
if (sum != null && (Boolean.TRUE.equals(sum.getTrainNumberBest())
                 || Boolean.TRUE.equals(sum.getWeightBest()))) {
    bucket = REST_FALLBACK_PB;                          // 破纪录优先
} else if (perf != null && perfectRatio(perf) >= 0.6) {
    bucket = REST_FALLBACK_EXCELLENT;                   // perfect 占比高
} else {
    bucket = REST_FALLBACK_NORMAL;                      // 普通完成（含数据缺失兜底）
}
```

`perfectRatio` = `perfectNum / (perfectNum + goodNum + wrongNum)`，阈值 0.6 可配。

### 3.4 防重复：洗牌袋（每课程 × 每池）

```java
// key = restFallbackBag:{courseInstanceId}:{bucketCode}
List<TextI18n> pool = textI18nRepository.getListByLangAndCategory(lang, bucket);
if (CollectionUtils.isEmpty(pool)) return;             // 没配话术则不发，绝不报错

String idx = redis.opsForList().leftPop(bagKey);
if (idx == null) {                                     // 袋空 → 重新洗一轮
    List<Integer> order = shuffledRange(pool.size());  // [0..n-1] 打乱
    redis.opsForList().rightPushAll(bagKey, order.stream().map(String::valueOf).toArray(String[]::new));
    redis.expire(bagKey, 6, TimeUnit.HOURS);
    idx = redis.opsForList().leftPop(bagKey);
}
String text = pool.get(Integer.parseInt(idx)).getText();
```

效果：**一节课内整池用完才可能再现，绝不连续撞**——比"只避开上一句"更稳。
（注：依赖 `getListByLangAndCategory` 在一节课内返回顺序稳定；按主键 id 排序可保证。）

### 3.5 秒出发送（TTS 命中 OSS 缓存）

```java
VoiceEvent ev = new VoiceEvent();
ev.setEventId(IDUtils.createUUID());
ev.setCategory(VoiceCategoryEnum.PACE);
ev.setVoiceType(VoiceTypeEnum.REST);
ev.setContent(text);
ev.setInterruptible(true);

String audioUrl = ttsService.getAudioUrl(text, "mp3");   // 固定短句 → 首次后 md5 命中 OSS，≈0 延迟

// 复用 VoiceHub 同款 VoiceInteractionMessage 组装 + MessageSender 下发
```

> 建议起服后对全部话术做一次**预热**（批量 `getAudioUrl` 生成 OSS 缓存），避免每条话术首次播放的冷启动。

---

## 4. 话术文案草稿

风格：**简洁干脆，带一点温度**。每池中英各 10 条起步，运营可继续扩充。

### 4.1 普通完成 `REST_FALLBACK_NORMAL`

| zh | en |
|---|---|
| 这组稳了，喘口气~ | Set done — take a breath. |
| 完成！放松一下肩膀。 | Nice, ease those shoulders. |
| 好，拿下这组，歇会儿。 | Done. Grab some water. |
| 节奏不错，继续保持。 | Solid pace, keep it up. |
| 搞定，深呼吸放松下。 | That's a wrap — relax a sec. |
| 这组结束，喝口水吧。 | Good work, recover up. |
| 稳稳的，休息一下。 | Steady. Rest a moment. |
| 不错，养精蓄锐下一组。 | Nailed it, breathe easy. |
| 完成啦，松松肩膀。 | Set complete, loosen up. |
| 好样的，缓一缓~ | Well done — catch your breath. |

### 4.2 破纪录 `REST_FALLBACK_PB`

| zh | en |
|---|---|
| 破纪录啦！为你鼓掌。 | New record — bravo! |
| 新高度，超越了自己！ | New high, you beat yourself! |
| 刷新记录，太燃了！ | Record smashed — on fire! |
| 这组封神，新纪录到手！ | Personal best, right now! |
| 突破！你又进步了。 | Breakthrough — you leveled up. |
| 历史最佳，就是现在！ | All-time best, that's you! |
| 纪录改写，状态在线！ | Record rewritten, dialed in! |
| 哇，这组创纪录了！ | Whoa, a new PR! |
| 新高峰，给你点个大赞。 | New peak — huge respect. |
| 超越昨天的自己，漂亮！ | Better than yesterday. Beautiful. |

### 4.3 表现优秀 `REST_FALLBACK_EXCELLENT`

| zh | en |
|---|---|
| 这组质量很高，动作标准！ | High quality — clean form! |
| 漂亮，几乎组组到位。 | Beautiful, rep after rep. |
| 又稳又准，这组能打。 | Steady and precise, strong set. |
| 标准得像教科书，赞！ | Textbook form, love it! |
| 控制力满分，很棒！ | Full control, excellent! |
| 动作干净利落，保持住。 | Crisp and sharp — keep it. |
| 这组完成度拉满了！ | Maxed-out execution! |
| 细节到位，专业范儿。 | Dialed in to the detail. |
| 稳定输出，状态真好。 | Consistent and sharp — nice. |
| 几乎零瑕疵，继续。 | Nearly flawless. Onward. |

---

## 5. 落地步骤（建议顺序）

1. `TextI18n` 表写入 3 池 × 中英文案（上表草稿）。
2. `TextCategoryEnum` 增 3 个枚举项。
3. 新增 `RestFallbackVoiceService.sendImmediate(userId, courseInstanceId, totalOrder)`
   （分池 + 洗牌袋 + TTS + MessageSender 下发）。
4. `CourseInstanceMessageHandler` REST 分支接入 `sendImmediate`（在 `voiceHub.trigger` 之前）。
5. 起服预热：批量 `getAudioUrl` 把全部话术 TTS 灌入 OSS 缓存。
6. 灰度验证：观察 finish→首音延迟、话术重复率、与随后 LLM 小结的衔接体验。

---

## 6. 后续（不在本期范围）

- **报告两段式**：`getTrainData` 显式拆 Phase-1（数字，秒出）/ Phase-2（力线+视频，websocket 推送补齐）。
- **LLM 语音流式放量**：`RestVoiceStreamEngine` 边生成边逐句 TTS 下发；放量前需补
  ① 文字小结落库（流式版缺 `saveSetAiSummary`）② 逐句下发顺序 ③ 句间 TTS 并行。
- **80% 预生成**：`handleActionCountMessage` 里当 `count ≥ 0.8 × trainNumber` 时预拼 prompt / 预热 LLM，
  finish 后只补最后增量再流式输出。
