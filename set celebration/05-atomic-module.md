# 05 · 独立原子模块 · 契约与嵌入

> 定位：庆祝界面是一个**独立、自包含的原子交互模块**。它只负责"把这一次庆祝演完"（UI + 撒花 + 音效 + 播放预生成语音），**不拥有导航**——演完之后去哪儿，由**当时的业务逻辑**决定，模块自己不决定往下走。

## 1. 原则

- **自包含**：给定一份配置，模块独立完成一次庆祝的渲染与播放，不依赖外部状态机。
- **不导航**：模块**不知道**也**不关心**"休息 / 结课数据 / 退出"等去向；不做页面跳转。
- **可嵌入**：同一个模块可挂到任意业务流程里（组间、结课、成就弹窗、甚至调试页）。
- **只发信号**：演完（或被点击跳过）时发一个 `onDone` 事件，宿主收到后**自行决定下一步**。

## 2. 模块契约（接口）

**输入 config（宿主把已决定好的解耦维度传进来）**
```
{
  scope:  set | action | course,     // D1 范围
  state:  logged | done,             // D2 状态
  perf:   none | great | pb,         // D3 表现
  lang:   "zh" | "en" | ...,         // 语言
  form:   round | phone,             // 形态（设备圆 / 手机）
  dwellMs?: number,                  // 可选：本次停留时长；不传用档位默认
  skippable?: bool                   // 可选：是否允许点击跳过（默认 true）
}
```

**行为**
- 渲染三行文字 + 撒花 + 圆边光环；播放完成音效；t+300ms 播放**预生成**语音（按情境挑，不上屏）。
- 运行 `dwellMs`（默认按档位：本组 2000 / PB 2400 / 结课 ≥3000），到时或被点击后：停止媒体 → 触发 `onDone`。

**输出**
```
onDone()   // 唯一出口信号：本次庆祝已结束。宿主在这里决定下一步。
```

> 停留时长可以由宿主通过 `dwellMs` 控制，也可以让宿主完全接管生命周期（模块只在收到 `dismiss()` 时结束）。二选一，见 §3。

## 3. 两种生命周期模式（择一，推荐 A）

**A · 模块自计时（推荐）**：宿主 `show(config)` → 模块演完 `dwellMs` → 自动 `onDone` → 宿主导航。
```
celebration.show({scope:'set', state:'done', perf:'pb', form:'round'});
celebration.onDone = () => router.go(nextByBusiness());  // 宿主决定去哪
```

**B · 宿主接管**：模块只渲染、不自动结束；宿主自己决定何时 `dismiss()`。
```
celebration.show(cfg);            // 一直展示
// ...宿主自己的时机...
celebration.dismiss();            // 宿主收起 → 再导航
```

## 4. 嵌入示例（模块中立，去向由业务定）

```text
// 组间：完成一组后
celebration.show(cfgForSet);    celebration.onDone = () => goRest();

// 结课：整节课完成后
celebration.show(cfgForCourse); celebration.onDone = () => goSummary();

// 成就弹窗：任意位置
celebration.show(cfgForPB);     celebration.onDone = () => closeOverlay();
```
三处用的是**同一个模块**，只是宿主在 `onDone` 里各自决定下一步——模块本身对去向一无所知。

## 5. 边界（属于模块内部，与导航无关）
- 反向取消（status=0）：宿主不 `show` 即可；已在展示可 `dismiss()`。
- 快速连续：宿主对同一 totalOrder 去抖，只 `show` 一次。
- 可跳过：点击立即停止媒体并 `onDone`（有最短停留 ~1.2s 兜底，避免秒切突兀）。
- 设备端 / 手机端：`form` 参数切换视觉；模块逻辑一致。

## 6. 好处
- **解耦导航**：业务流程怎么变，庆祝模块不用改。
- **高复用**：一处实现，多处嵌入。
- **易测试**：给不同 config 就能独立预览每种组合（见交互原型）。
