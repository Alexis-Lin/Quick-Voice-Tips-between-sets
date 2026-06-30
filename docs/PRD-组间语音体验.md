# PRD · 组间语音体验（每组完成庆祝 + 技术分析与报告）

| 项 | 内容 |
|---|---|
| 文档状态 | Draft v0.1 |
| 负责模块 | adam（语音/课程）、video-processor（异步量化）、前端 |
| 关联设计文档 | [README](./README.md) · [需求一](./req1-set-completion-celebration.md) · [需求二](./req2-analysis-and-report.md) |
| 一句话 | 把"做完一组后的语音"拆成**即时庆祝**和**个性化分析报告**两件事，前者秒出、后者渐进 |

---

## 1. 背景与问题

Body Park 阿童木在用户做完一组动作后会给语音 + 文字小结，但当前：

- **延迟高**：往往休息 5~10s 才出声，用户"凉掉"、感知不到。
- **根因**：完整链路把"庆祝、个性化小结、数据报告"混在 REST 休息阶段串行处理，
  且语音走"整段 LLM 生成 → 整段 TTS 合成"两次阻塞外部调用，用户要等全部跑完才听到第一个字。

## 2. 目标 / 非目标

**目标**
- G1：用户**完成一组的瞬间**就有语音反馈（首音 < 1s），消除空窗。
- G2：庆祝话术**简洁有温度、且不重复**，多组练下来不显套路。
- G3：个性化技术小结与数据报告**渐进呈现**，不阻塞即时反馈。
- G4：建立**可扩展的话术/意图分类体系**，后续新增触发情境无需大改。

**非目标**
- 不在本期改造力线/视频的**异步计算管线本身**的算法耗时（只改"何时返回/如何渐进"）。
- 不替换 TTS / LLM 供应商。

## 3. 分类体系（话术分类 + 意图分类）

> 这是本 PRD 的基础设施层，也是后续多处接口要对齐的"协议"。现有代码已有四层枚举，本期**优先复用**，
> 并定义"意图"层使其可扩展。

### 3.1 现有四层（代码现状）

| 层 | 枚举 | 作用 | 现有取值（节选） |
|---|---|---|---|
| **场景 Scene**（何时触发） | `VoiceSceneEnum` | 绑定触发时机 → 引擎 | `AFTER_ACTION_COUNT`、**`AFTER_OP_COMPLETE`（已预留、未接引擎）**、`REST`、`OPENING` |
| **类别 Category**（优先级仲裁） | `VoiceCategoryEnum` | 多条语音并发时按优先级仲裁 | `SAFE(1000)` > `FORM(100)` > `PACE(10)` > `BOOST(1)` |
| **类型 Type**（内容种类） | `VoiceTypeEnum` | 内容大类 / 前端展示区分 | `ENCOURAGE`、`REST`、`OPENING`、`POSITION`、`EXHAUSTION`、`POSTURE`、`ACTION_INSTRUCTION` |
| **话术分类 TextCategory**（文案池） | `TextCategoryEnum` | 叶子级话术库分类（`TextI18n`） | `ENCOURAGE_RECORD_BREAKING`、`ENCOURAGE_PERFORMED_WELL`、`ENCOURAGE_PERFORMED_NOT_WELL`、`POSTURE_ERROR_*`、`EXHAUSTION` … |

### 3.2 新增的"意图 Intent"层（本期定义，面向未来扩展）

**问题**：把"什么情境 → 播哪池话术"的判断硬编码在引擎里，未来加情境要改代码。
**方案**：定义**意图（Intent）**作为"情境 → 话术池"的可配置映射。

```
意图 Intent = f(场景 Scene, 信号 Signals)
信号 Signals = { 是否破纪录, perfect占比, 是否首组, 是否末组, 疲劳信号, 连续表现趋势 ... }
意图 → 绑定一个 TextCategory（话术池） + Category（优先级） + Type
```

**本期落地的意图（完成一组场景）：**

| 意图 Intent | 触发信号 | 话术池 TextCategory | Category | Type |
|---|---|---|---|---|
| `SET_DONE_PB` | trainNumberBest 或 weightBest | `ENCOURAGE_RECORD_BREAKING` | BOOST | ENCOURAGE |
| `SET_DONE_EXCELLENT` | perfect占比 ≥ 0.6 | `ENCOURAGE_PERFORMED_WELL` | BOOST | ENCOURAGE |
| `SET_DONE_NORMAL` | 其余 / 数据缺失兜底 | `ENCOURAGE_PERFORMED_NOT_WELL`（中性鼓励口径） | BOOST | ENCOURAGE |

**未来可扩展的意图（占位，不在本期开发）：**
`SET_DONE_COMEBACK`（状态回升）、`SET_DONE_FATIGUE`（疲劳明显）、`FIRST_SET`（课程首组）、
`LAST_SET`（收尾组）、`STREAK_PB`（连续破纪录）……
扩展方式 = 新增一行"意图→话术池"映射 + 在 `TextI18n` 配该池话术，**判定信号若已采集则零引擎改动**。

> 落地形态建议：意图映射先用代码内的 `enum + 映射表` 落地；
> 待意图数量增长后，迁到 `SysConfigs` 配置（JSON），实现运营可配。

## 4. 需求一 · 每组完成庆祝

### 4.1 用户故事
> 作为训练用户，当我**完成一组**时，希望立刻听到一句简短、有温度、不重复的肯定，
> 让我有"被看见、被鼓励"的即时反馈。

### 4.2 功能需求

| 编号 | 需求 |
|---|---|
| FR1-1 | 每次"完成一组（logset，status=1）"触发一条庆祝语音；反向取消（status=0）不触发。 |
| FR1-2 | 触发点为**完成一组事件**（早于 REST），保证**最后一组（无 REST 阶段）也播**。 |
| FR1-3 | 话术按表现分 3 池（普通/PB/优秀），按 §3.2 意图映射选池。 |
| FR1-4 | 话术**纯通用短句、不含本组具体数字**，保证 TTS 命中 OSS 缓存、秒出。 |
| FR1-5 | **防重复**：每课程 × 每池"洗牌袋"，一池用完才可能再现，绝不连续撞。 |
| FR1-6 | 语音与 LLM/报告**解耦**，不被任何分析逻辑阻塞。首音目标 < 1s。 |
| FR1-7 | 话术文案存 `TextI18n`，支持多语言、运营可增删改、零代码上线。 |
| FR1-8 | 起服对全部庆祝话术**预热 TTS**（批量灌 OSS 缓存），消除首播冷启动。 |

### 4.3 触发时序

```
opCompleted()                              // 用户打点 / 计数达标自动完成
  ├─ sendSetCompletedMessage(status=1)
  │     ├─ addLog(...)                      // 写 logset 记录
  │     ├─ 发 OpCompletedMessage 给学员
  │     └─ [本期新增] 庆祝服务 sendImmediate(...)   ← 秒出庆祝
  └─ doNext() → ... → 阶段切 REST → 需求二（分析+报告）
```

## 5. 需求二 · 技术分析与报告

### 5.1 用户故事
> 作为训练用户，进入组间休息后，希望看到这组的**个性化点评**与**力线/视频/数据报告**，
> 即便重数据要算几秒，也希望"数字版"先到、视频随后补齐，而不是一片空白。

### 5.2 功能需求

| 编号 | 需求 |
|---|---|
| FR2-1 | 语音个性化小结**流式化**：边生成边逐句 TTS 下发，首音降到"第一句"。 |
| FR2-2 | 流式小结**文字落库**（补齐现流式版缺失的 `saveSetAiSummary`）。 |
| FR2-3 | 报告 `getTrainData` **两段式**：Phase-1 快数据（次数/评分/纠错/心率）秒返；Phase-2 重数据（力线/视频）就绪后**主动推送**或轻量轮询补齐。 |
| FR2-4 | （进阶）训练进行到 80% 时**预拼 prompt / 预热 LLM**，finish 后只补增量。 |

详见 [需求二设计文档](./req2-analysis-and-report.md)。

## 6. 受影响接口与改动点

> 本节是给研发的"波及面"清单——除新增服务外，多处现有接口/配置需对齐。

| 区域 | 对象 | 改动类型 | 说明 |
|---|---|---|---|
| 分类 | `TextCategoryEnum` | 复用（本期不新增） | 庆祝 3 池复用现有 `ENCOURAGE_*`；未来新意图在此加叶子分类 |
| 分类 | `VoiceSceneEnum.AFTER_OP_COMPLETE` | 复用/确认 | 已预留，本期把庆祝引擎/服务绑定到该场景语义 |
| 配置 | `TextI18n` 表 | 新增数据 | 写入 3 池 × 多语言话术；建议提供后台/脚本批量导入 |
| 触发 | `CourseInstanceMessageHandler#sendSetCompletedMessage` | 改 | `status==1` 处接入庆祝服务 |
| 新增 | `SetCelebrationVoiceService` | 新建 | 意图判定 + 洗牌袋 + TTS + 下发 |
| 去重 | `EncourageVoiceEngine`（注释的"整组表现好/不好"） | 迁移/下线 | 避免与庆祝服务**重复触发**；统一到完成一组事件 |
| 去重 | `RecordBreakingVoiceEngine` | 评估合并 | 与 PB 意图重叠，确认归一 |
| 需求二 | `RestVoiceEngine` / `RestVoiceStreamEngine` | 改 | 流式放量 + 灰度开关收敛 + 补落库 |
| 需求二 | `StudentTrainDataController#getTrainData` | 改 | 两段式返回 / 新增 part2 就绪推送 |
| 需求二 | 前端 | 改 | 消费两段式报告 + websocket part2 消息 |
| 仲裁 | `VoiceDecisionCenter`（当前空壳） | 预留 | 未来多意图并发时按 `VoiceCategoryEnum` 优先级仲裁 |
| 运维 | TTS 预热任务 | 新增 | 起服批量 `getAudioUrl` 灌缓存 |

## 7. 大面上的伪代码

### 7.1 庆祝服务（需求一）

```text
service SetCelebrationVoiceService:

  function sendImmediate(userId, courseInstanceId, totalOrder):
      lang   = resolveLang(courseInstanceId)
      intent = classifyIntent(courseInstanceId, totalOrder)      // §7.2
      pool   = textRepo.getListByLangAndCategory(lang, intent.textCategory)
      if pool is empty: return                                   // 没配话术则静默

      text = drawFromShuffleBag(courseInstanceId, intent.code, pool)   // §7.3
      send(userId, courseInstanceId, text, intent)               // §7.4

  // §7.2 意图判定（信号 → 意图 → 话术池），未来加意图在此扩表
  function classifyIntent(ciId, totalOrder):
      sum  = opCompletedService.getOpSumBo(ciId, totalOrder)
      perf = studentActionService.getPerformanceFromCache(ciId, totalOrder)
      if sum != null and (sum.trainNumberBest or sum.weightBest):
          return INTENT.SET_DONE_PB          // → ENCOURAGE_RECORD_BREAKING
      if perf != null and perfectRatio(perf) >= 0.6:
          return INTENT.SET_DONE_EXCELLENT   // → ENCOURAGE_PERFORMED_WELL
      return INTENT.SET_DONE_NORMAL          // → ENCOURAGE_PERFORMED_NOT_WELL（兜底）

  // §7.3 洗牌袋防重复（每课程 × 每意图）
  function drawFromShuffleBag(ciId, intentCode, pool):
      bagKey = "setCelebrationBag:" + ciId + ":" + intentCode
      idx = redis.lpop(bagKey)
      if idx == null:                                   // 袋空 → 重新洗一轮
          order = shuffle([0 .. pool.size-1])
          redis.rpushAll(bagKey, order); redis.expire(bagKey, 6h)
          idx = redis.lpop(bagKey)
      return pool[idx].text

  // §7.4 秒出下发（TTS 命中 OSS 缓存）
  function send(userId, ciId, text, intent):
      event = VoiceEvent{ category: BOOST, type: ENCOURAGE, content: text, interruptible: true }
      audioUrl = ttsService.getAudioUrl(text, "mp3")    // 固定短句 → md5 命中缓存 ≈0 延迟
      msg = buildVoiceInteractionMessage(event, audioUrl, ciId)
      messageSender.send(msg → STUDENT)
```

### 7.2 接入点（需求一）

```text
function sendSetCompletedMessage(ciId, totalOrder, status):
      opCompleted = opCompletedService.addLog(ciId, totalOrder, status)
      messageSender.send(OpCompletedMessage ...)
      if status == 1:                                   // 仅真实完成
          setCelebrationVoiceService.sendImmediate(userId, ciId, totalOrder)
      return opCompletedMessage
```

### 7.3 报告两段式（需求二，概要）

```text
// Phase-1：finish 秒返快数据
function getTrainData(ciId, totalOrder):
      dto = assembleFastData(ciId, totalOrder)          // 次数/评分/纠错/心率/动作名
      dto.heavyDataReady = quantifyExists(ciId, totalOrder) && videoExists(...)
      return dto                                        // 不阻塞等重数据

// Phase-2：异步管线就绪 → 主动推送
on quantifyAndVideoReady(ciId, totalOrder):
      pushToStudent(ReportPart2ReadyMessage{ ciId, totalOrder, forceLine, videos })
```

## 8. 埋点与指标

- **首音延迟**：finish → 庆祝语音到达设备（目标 P90 < 1s）。
- **话术重复率**：同一课程内同句出现间隔分布（目标：一池内不连续重复）。
- **TTS 缓存命中率**：庆祝话术 `getAudioUrl` 命中比例（目标 > 95%，预热后）。
- **报告首屏延迟**：finish → Phase-1 渲染 / → Phase-2 补齐。
- **体验指标**：用户对组间反馈的满意度 / 完课率（A/B）。

## 9. 灰度与上线

1. 需求一可独立先上：话术入库 → 预热 → 灰度名单 → 全量。
2. 需求二按子项分批：流式放量（补坑后灰度）→ 报告两段式（前后端联调）→ 80% 预生成。
3. 回滚：庆祝服务以开关（`SysConfigs`）控制，可一键关闭回退到当前行为。

## 10. 风险与开放问题

- **Q1 话术与随后 LLM 小结是否会"话太多"**？建议庆祝极简（一短句），LLM 小结隔拍补；需产品确认节奏。
- **Q2 `EncourageVoiceEngine` 注释逻辑与庆祝服务的归一**：是否彻底迁移，避免双触发。
- **Q3 意图映射落地形态**：本期代码内置 vs 直接上 `SysConfigs` 可配——取决于近期意图扩展速度。
- **Q4 `getListByLangAndCategory` 顺序稳定性**：洗牌袋按序号取，需保证一节课内顺序稳定（按主键排序）。
- **Q5 多语言话术覆盖**：非中英语言池为空时的降级策略。
- **Q6 报告 Phase-2 推送通道**：复用现有 websocket 消息类型还是新增。
