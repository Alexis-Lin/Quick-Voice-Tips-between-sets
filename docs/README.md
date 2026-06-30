# 组间语音体验 · 需求拆分

> 📄 产品需求见 **[PRD-组间语音体验.md](./PRD-组间语音体验.md)**（含话术/意图分类体系、受影响接口清单、大面伪代码）。


用户做完一组（set）后的语音体验，原本混在"休息/报告"链路里、延迟高（5~10s 才出声）。
经梳理，把它拆成**两个相互独立的需求**，触发点、语义、延迟策略都不同：

| 需求 | 文档 | 触发 | 语义 | 目标 |
|---|---|---|---|---|
| **一 · 每组完成庆祝** | [req1-set-completion-celebration.md](./req1-set-completion-celebration.md) | 打点完成一组（logset / `opCompleted`） | BOOST 鼓励，即时情绪反馈 | finish 瞬间秒出一句简洁有温度、不重复的庆祝 |
| **二 · 技术分析与报告** | [req2-analysis-and-report.md](./req2-analysis-and-report.md) | REST 休息阶段 + 前端拉 `getTrainData` | PACE 个性化分析 + 数据报告 | 个性化小结流式化、报告两段式渐进返回 |

## 为什么这么拆

"完成一组（logset）"在代码里本就是一个**独立、且早于 REST 休息阶段**的事件：

```
opCompleted()                                   // 打点 / 计数达标自动完成
  ├─ sendSetCompletedMessage(status=1)          // ★完成一组事件 → 需求一（庆祝）挂这里
  └─ doNext() → ... → 阶段切 REST               // → 需求二（分析+报告）在这之后
```

- **语义不同**：庆祝是 `VoiceCategoryEnum.BOOST`（鼓励），和 `EncourageVoiceEngine` 同类；
  分析报告是 `PACE`（节奏/报告）。
- **代码本有此意图**：`EncourageVoiceEngine` 里"整组表现好/不好"逻辑、独立的
  `RecordBreakingVoiceEngine`，就是"完成一组的庆祝"，目前是注释/未启用状态。
- **顺带修一个隐性问题**：挂在 REST 的话，**最后一组没有 REST 阶段**就不会播；
  挂在 logset 则每组（含最后一组）都正确触发。

两件事可以独立排期、独立上线，互不阻塞。
