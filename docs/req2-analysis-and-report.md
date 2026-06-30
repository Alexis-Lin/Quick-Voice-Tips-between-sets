# 需求二 · 技术分析与报告

> 用户进入组间休息后，给出**个性化技术小结（语音）**与**训练数据报告（图文）**。
> 与[需求一](./req1-set-completion-celebration.md)（每组完成庆祝）解耦——这里是分析，不是即时庆祝。

---

## 1. 现状与延迟根因

### 语音个性化小结（push）

阶段切 REST → `VoiceHub.trigger(REST)` → `RestVoiceEngine.execute()`：

1. 取数据（用户 / 当前+下一组 / 完成情况 / 评分 / 纠错）
2. `buildPrompt()`
3. **`aiClientProxy.getLlmResult()` —— 同步阻塞，等整段文案（~2–5s）**
4. 写库 `saveSetAiSummary`
5. **`ttsService.getAudioUrl(整段)` —— 同步阻塞，整段 TTS + 上传 OSS（~1–3s）**
6. MQ → connection → websocket → 设备

> 根因：②整段 LLM + ⑤整段 TTS **串行、且都针对整段文本**，用户要等两者都跑完才听到第一个字。
> TTS 按文本 md5 缓存，LLM 文案每次不同 → 基本不命中。

### 数据报告（pull）

前端拉 `StudentTrainDataController#getTrainData(courseInstanceId, totalOrder)`，纯 DB 组装。
按"产出时机"分两类：

| 数据 | 来源 | 时机 |
|---|---|---|
| 次数 / PB / 负重 / 评分 / 纠错 / 心率 / 动作名 | `OpSumBo` + Redis 评分缓存 + 编排 | **finish 即有（快）** |
| 力线量化（velocity/power/travels）+ 轨迹视频 | `StudentTrainIndexQuantify` | **异步后到（euler→video-processor，慢）** |
| 反畸变视频 | `AiFileInfo` | **异步后到** |

`getTrainData` 本身不阻塞（量化为空时构造空 DTO 继续返回 `:108`），所以"报告凉"多半是
**前端等齐重数据才整体渲染**，或在轮询等视频。

---

## 2. 优化方向（可独立排期）

### 2.1 语音小结流式化（放量已有 `RestVoiceStreamEngine`）

代码已有流式引擎：`aiClientProxy.stream()` 边收 token 边按标点凑句，**每句 TTS + 下发**，
首音从"整段 LLM+整段 TTS"降到"第一句"。当前仅对灰度名单生效，与阻塞版互斥
（`RestVoiceStreamEngine:100` / `RestVoiceEngine:77` 的 `getKeyForCodeTest` 开关）。

**放量前需补的坑：**
1. 流式版**缺 `saveSetAiSummary`** → 文字小结会丢，需补落库。
2. 逐句下发顺序保证。
3. TTS 仍是逐句阻塞 → 可做句间并行 / 接 TTS 流式。

### 2.2 报告两段式渐进返回

把 `getTrainData` 拆成显式两段，而非前端盲轮询：

- **Phase-1（finish 秒出）**：次数 / 评分 / 纠错 / 心率等"快数据"，立即返回并渲染。
- **Phase-2（异步就绪后）**：力线 + 轨迹视频 + 反畸变视频，
  由后端**主动 websocket 推**"report.part2 ready"（复用 `MessageSender`），或前端仅对重数据轻量轮询。

收益：finish 瞬间就看到"数字版报告"，视频/力线渐进补齐，消除整体空窗。

### 2.3 80% 预生成（进阶）

训练中每计一次数走 `handleActionCountMessage`，已知 `count` 与目标 `trainNumber`。
当 `count ≥ 0.8 × trainNumber` 时：
- 预拼 prompt 前缀（用户 / 本组 / 下一组 / 已累积纠错）
- 预热 LLM/TTS 连接

finish 后只把最后 20% 的次数与终评分**增量**塞入 → 直接流式吐第一句，
把"取数据 + 冷启动"从关键路径上挪走。

---

## 3. 建议优先级

1. **2.1 流式放量**（已有实现，补 3 个坑即可）—— 性价比最高，直接砍语音首音延迟。
2. **2.2 报告两段式**（前后端协议改动）—— 解决图文报告的"凉场"。
3. **2.3 80% 预生成**（工程量稍大）—— 锦上添花。
