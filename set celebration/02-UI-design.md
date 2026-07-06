# 02 · UI 设计

> 设计图见 [`02-ui-mockup.svg`](./02-ui-mockup.svg)；可交互原型见 [`prototype-celebration.html`](./prototype-celebration.html)（浏览器打开，可切语言/范围/状态/表现/形态）。

## 1. 视觉语言（恒定层 L0，两端一致）
- **配色**：荧光绿 `#a3e635` × 纯黑 `#000`；金色 `#f5c451`（新纪录）；白/暗绿点缀。
- **字标**：无衬线、粗体、紧字距；状态词发光（neon glow）。
- **撒花**：小纸屑（矩形片），绿/白/金；纯代码绘制、无素材（见 [`03-confetti.c`](./03-confetti.c)）。

## 2. 布局：三行堆叠

```
        ┌───────────────┐
        │     本组       │  D1 范围   小(上)   #a9c47e
        │   已完成       │  D2 状态   最大(中·主视觉·发光)  #a3e635
        │  〔新纪录〕    │  D3 表现   徽章(下)  金/绿底
        └───────────────┘
```

- **D2 状态**是绝对主视觉，字号最大（圆屏 ~60px / 手机 ~74px）。
- **D1 范围**在上、小字、压暗，作上下文。
- **D3 表现**是独立徽章（第三行），无表现时不显示——**解耦**，不进主文字。
- 入场错峰：范围淡入 → 状态弹出 → 徽章跳入。

## 3. 两种形态

| 端 | 形状 | 要点 |
|---|---|---|
| **设备端（主）** | **纯圆 466×466** | 内容收进圆内**安全区**（内接正方 ≈ 直径/√2 ≈ 329px）；撒花圆形裁剪；**圆边 neon 光环**替代全屏爆发 |
| **手机 App** | 竖向方屏 | 空间更足，字标更大，可承载更多 |

## 4. 多语言（词元拼接，非整句写死）

主文字 = `模板(范围, 状态)`；每词元独立翻译：

| Key | zh | en |
|---|---|---|
| scope.set/action/course | 本组 / 本动作 / 全部 | Set / Exercise / Course |
| state.logged/done | 已记录 / 已完成 | Logged / Complete |
| badge.great/pb | 很棒 / 新纪录 | GREAT / NEW PB |
| 拼接 | `{范围}{状态}` | `{范围} {状态}`（含空格） |

## 5. 动效清单
- 状态词 scale-in（弹性曲线）。
- 撒花：中上部向上抛 + 重力下落 + 淡出（粒子系统）。
- 圆边光环：完成时点亮，结束时熄灭；PB/结课更强。
- 徽章：跳入（overshoot）。
- 遵循 `prefers-reduced-motion`：减粒子、去弹跳。

## 5bis. 典型状态界面图（[`ui-states/`](./ui-states)）

| 文件 | 状态 | 说明 |
|---|---|---|
| [01-set-done.svg](./ui-states/01-set-done.svg) | 本组 · 已完成 | 最常用；音效 当当 |
| [02-set-great.svg](./ui-states/02-set-great.svg) | 本组 · 已完成 · 很棒 | GREAT 徽章(neon)，视觉为主不叠音 |
| [03-set-pb.svg](./ui-states/03-set-pb.svg) | 本组 · 已完成 · 新纪录 | PB：金徽章+金光环+叠层啪啪 |
| [04-set-logged.svg](./ui-states/04-set-logged.svg) | 本组 · 已记录 | 未达标，仍撒花仍庆祝；咚咚(暖) |
| [05-course-done.svg](./ui-states/05-course-done.svg) | 全部 · 已完成 | 结课；当当·和弦 |
| [06-course-grand.svg](./ui-states/06-course-grand.svg) | 全部 · 已完成 · 新纪录 | 满贯：金光环+金花+啪啪 |
| [07-en-set-pb.svg](./ui-states/07-en-set-pb.svg) | EN · Set/Complete/NEW PB | 多语言词元拼接 |
| [08-phone-course-pb.svg](./ui-states/08-phone-course-pb.svg) | 手机 · 全部/已完成/新纪录 | 方屏形态 |

> 均为纯代码 SVG，GitHub 可直接预览；配色/布局与 [`03-confetti.c`](./03-confetti.c) 及原型一致。

## 6. 通道时间线（示例）
```
t0     撒花喷发 + 状态词入场 + 完成音效
t+90   状态词弹出到位
t+300  徽章跳入 + 预生成语音播放（不上屏）
t+2000 溶解 → 下一页（见 05 停留跳转）
```
