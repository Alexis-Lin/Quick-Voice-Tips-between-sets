# 需求一 · 每组完成庆祝语音

> 用户每完成一组（打点 logset）时，**finish 瞬间**播一句简洁、有温度、且不重复的庆祝短句。
> 与"休息/报告"无关，是对"完成一组"这个动作的即时情绪反馈。

---

## 1. 定位

- **它是什么**：完成一组的"小小庆祝"，每组都有，BOOST 鼓励类。
- **它不是什么**：不是休息阶段的个性化小结，也不是技术分析报告（那是[需求二](./req2-analysis-and-report.md)）。
- **与现有代码的关系**：和 `EncourageVoiceEngine`（鼓励引擎）同一语义族；
  代码里"整组表现好/不好"`EncourageVoiceEngine:103-112`、独立的 `RecordBreakingVoiceEngine`
  本就是这个意图，目前未启用——本需求相当于把它做实、做全。

---

## 2. 触发点：打点完成一组（logset）

`CourseLivingServiceImpl.opCompleted()` 的顺序：

```
opCompleted()
  ├─ sendSetCompletedMessage(courseInstanceId, totalOrder, status=1)   // ★完成一组事件
  └─ doNext() → ... → 阶段切 REST                                       // 需求二在这之后
```

**接入方式**：在 `CourseInstanceMessageHandler.sendSetCompletedMessage(...)` 内，
当 `status == 1`（真实完成，非反向取消）时调用庆祝服务：

```java
public OpCompletedMessage sendSetCompletedMessage(Long courseInstanceId, Integer totalOrder, int status) {
    ...
    messageSender.sendMessage(buildMessageDto(opCompletedMessage, user.getUuid(), Constant.Role.STUDENT));

    if (status == 1) {
        // ★完成一组 → 秒出庆祝短句（独立、与 REST/LLM 解耦）
        setCelebrationVoiceService.sendImmediate(courseInstance.getUserId(), courseInstanceId, totalOrder);
    }
    return opCompletedMessage;
}
```

要点：
- **只在 `status==1` 触发**，`status==0`（反向取消）不播。
- **挂在 logset 而非 REST**：每组（含最后一组、无 REST 的情况）都会播。
- **与 LLM 彻底解耦**：庆祝是固定短句，不等任何 LLM/分析。

---

## 3. 设计（已与产品确认）

| 维度 | 决策 |
|---|---|
| 话术内容 | **纯通用短句**，不带本组具体数字 → TTS 几乎永远命中 OSS 缓存、最快 |
| 话术组织 | **按表现分 3 池**：普通完成 / 破纪录(PB) / 表现优秀 |
| 防重复 | **洗牌袋（shuffle-bag）**，每课程 × 每池，一池用完才可能再现，绝不连续撞 |
| 语义 | `VoiceCategoryEnum.BOOST` + `VoiceTypeEnum.ENCOURAGE` |

### 3.1 话术分类：优先复用现有 ENCOURAGE 分类

`TextCategoryEnum` 已有三项，正好对应三池，**可直接复用、无需新增枚举**：

| 池 | 复用现有分类 | 备注 |
|---|---|---|
| 破纪录 PB | `ENCOURAGE_RECORD_BREAKING`（"创记录"，已存在） | 直接用 |
| 表现优秀 | `ENCOURAGE_PERFORMED_WELL`（"整组表现好"，已存在） | 直接用 |
| 普通完成 | `ENCOURAGE_PERFORMED_NOT_WELL`（"整组表现不好"，已存在） | 话术按"中性鼓励"而非"批评"口径填，避免负面 |

话术文案存 `TextI18n` 表（`lang + category` 多语言），加 / 改话术零改代码。

### 3.2 表现分池逻辑（数据来自 Redis 缓存，快；缺数据降级到普通）

```java
OpSumBo sum = courseInstanceOpCompletedService.getOpSumBo(ciId, totalOrder);
CourseInstanceStudentAction perf = courseInstanceStudentActionService.getPerformanceFromCache(ciId, totalOrder);

TextCategoryEnum bucket;
if (sum != null && (Boolean.TRUE.equals(sum.getTrainNumberBest())
                 || Boolean.TRUE.equals(sum.getWeightBest()))) {
    bucket = ENCOURAGE_RECORD_BREAKING;                 // 破纪录优先
} else if (perf != null && perfectRatio(perf) >= 0.6) {
    bucket = ENCOURAGE_PERFORMED_WELL;                  // perfect 占比高
} else {
    bucket = ENCOURAGE_PERFORMED_NOT_WELL;             // 普通完成（含数据缺失兜底）
}
// perfectRatio = perfectNum / (perfectNum + goodNum + wrongNum)，阈值 0.6 可配
```

### 3.3 防重复：洗牌袋（每课程 × 每池）

```java
// key = setCelebrationBag:{courseInstanceId}:{bucketCode}
List<TextI18n> pool = textI18nRepository.getListByLangAndCategory(lang, bucket);
if (CollectionUtils.isEmpty(pool)) return;             // 没配话术则不发，绝不报错

String idx = redis.opsForList().leftPop(bagKey);
if (idx == null) {                                     // 袋空 → 重新洗一轮
    List<Integer> order = shuffledRange(pool.size());  // [0..n-1] 打乱
    redis.opsForList().rightPushAll(bagKey,
        order.stream().map(String::valueOf).toArray(String[]::new));
    redis.expire(bagKey, 6, TimeUnit.HOURS);
    idx = redis.opsForList().leftPop(bagKey);
}
String text = pool.get(Integer.parseInt(idx)).getText();
```

效果：**一节课内整池用完才可能再现，绝不连续撞**——比"只避开上一句"更稳。
（依赖 `getListByLangAndCategory` 一节课内返回顺序稳定，按主键 id 排序即可保证。）

### 3.4 秒出发送（TTS 命中 OSS 缓存）

```java
VoiceEvent ev = new VoiceEvent();
ev.setEventId(IDUtils.createUUID());
ev.setCategory(VoiceCategoryEnum.BOOST);
ev.setVoiceType(VoiceTypeEnum.ENCOURAGE);
ev.setContent(text);
ev.setInterruptible(true);

String audioUrl = ttsService.getAudioUrl(text, "mp3");  // 固定短句 → 首次后 md5 命中 OSS，≈0 延迟
// 复用 VoiceInteractionMessage 组装 + MessageSender 下发
```

> 建议起服后对全部话术做一次**预热**（批量 `getAudioUrl` 灌入 OSS 缓存），消除每条话术首播冷启动。

---

## 4. 话术文案草稿

风格：**简洁干脆，带一点温度**。每池中英各 10 条起步，运营可继续扩充。

### 4.1 普通完成（`ENCOURAGE_PERFORMED_NOT_WELL`，中性鼓励口径）

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

### 4.2 破纪录（`ENCOURAGE_RECORD_BREAKING`）

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

### 4.3 表现优秀（`ENCOURAGE_PERFORMED_WELL`）

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

## 5. 落地步骤

1. `TextI18n` 表写入 3 池 × 中英文案（上表草稿）。
2. 新增 `SetCelebrationVoiceService.sendImmediate(userId, courseInstanceId, totalOrder)`
   （分池 + 洗牌袋 + TTS + MessageSender 下发）。
3. `CourseInstanceMessageHandler.sendSetCompletedMessage` 在 `status==1` 处接入 `sendImmediate`。
4. 起服预热：批量 `getAudioUrl` 把全部话术 TTS 灌入 OSS 缓存。
5. 灰度验证：finish→首音延迟、话术重复率、与后续（需求二）小结的衔接体验。

> 可选：直接复活 `EncourageVoiceEngine` 中被注释的"整组表现好/不好"逻辑，把它从 `AFTER_ACTION_COUNT`
> 场景迁到"完成一组"事件触发，统一进本服务，避免两套实现。
