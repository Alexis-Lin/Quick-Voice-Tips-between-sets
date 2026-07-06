# 组间语音音效 · AI 生成器 Prompt 清单

> 用于 **ElevenLabs Sound Effects** / Stable Audio / Meta AudioGen 等 text-to-SFX 工具。
> 直接复制英文 prompt 到生成框，按下方"建议参数"设置时长即可。每类 4 个变体，生成后挑最合适的。

## 通用使用建议

- **Duration（时长）**：确认音 0.4–0.7s；完成音 1.0–1.8s；庆祝音 2.0–3.5s。ElevenLabs 可设固定时长，别让它自动拉长。
- **Prompt influence（提示词贴合度）**：调偏高（如 0.6–0.8），让它更忠实于描述、少自由发挥。
- **每条多生成几版**（3–5 次）再挑，AI SFX 有随机性。
- **导出后处理**：裁掉头尾静音、统一响度（确认音要明显更轻、更短，避免每次动作都吵）、按 App 需要转 mp3/ogg。
- **关键词技巧**：写清 **乐器/材质 + 情绪 + 动作 + "clean, no music, short tail, mono"**，避免它给你配乐或加长混响。

---

## 类别 1 · 课中动作确认「咚咚」（愉悦、短促）

> 目标：每次完成动作即时反馈。要**干净、克制、悦耳**，不能吵。

**1a · 马林巴双击（推荐主选）**
```
A short, pleasant two-note marimba tap, warm wooden mallet, quickly rising in pitch (like C then G), crisp attack, soft natural decay, clean and dry, no reverb tail, no music, mono UI confirmation sound.
```
`时长 0.5s · 影响 0.7`

**1b · 钢片琴/铃 双击（明亮）**
```
Two quick bright bell dings, glockenspiel, octave apart, sparkling and clean, gentle metallic shimmer, very short, positive UI confirm, no music.
```
`时长 0.5s`

**1c · 软 UI 轻快双击（最克制）**
```
A soft, clean, modern UI confirmation blip, two quick rounded digital taps, subtle and pleasant, minimal, not harsh, short and dry, mobile app feedback, no music.
```
`时长 0.35s`

**1d · 卡林巴温暖双击（有机）**
```
Two warm kalimba (thumb piano) plucks in quick succession, mellow and organic, soft rounded tone, cozy and satisfying, short natural decay, clean, no music.
```
`时长 0.5s`

---

## 类别 2 · 完成本组目标（达成音，有仪式感）

> 目标：一组练完的成就感，积极但**不夸张、不俗气**。

**2a · 上行三音琶音 + 尾和弦（推荐主选）**
```
A short, uplifting three-note ascending arpeggio on marimba and celesta, resolving into a soft warm major chord that gently rings out, cheerful and rewarding, clean, light shimmer, game level-up feel but tasteful, no drums, no music bed.
```
`时长 1.4s · 影响 0.6`

**2b · 钟鸣大三和弦长尾（大气）**
```
A warm glockenspiel major chord struck once, bright and full, with a long gentle shimmering tail, uplifting and accomplished, spacious, clean, no music bed.
```
`时长 1.6s`

**2c · 小号 fanfare（胜利感）**
```
A short triumphant brass fanfare, a few bright ascending trumpet notes, celebratory and heroic but clean and not cheesy, dry stage sound, no backing music, no drums.
```
`时长 1.3s`

**2d · 上行 + 高音点亮 sparkle（轻盈现代）**
```
An ascending mallet arpeggio finishing with a bright magical sparkle of high bells and twinkles, light, modern, positive achievement sound for a fitness app, clean, no music bed.
```
`时长 1.4s`

---

## 类别 3 · 鼓掌 & 庆祝（大庆祝，破纪录/收尾）

> 目标：真实感的掌声/欢呼。**这类务必带真实录音质感**——AI 生成或音效库都行。

**3a · 温暖掌声 + 铃点缀（推荐主选）**
```
Warm, genuine applause from a small enthusiastic group of people clapping, natural room ambience, with a light celebratory bell chime on top, encouraging and heartfelt, not a huge stadium, clean start.
```
`时长 2.5s · 影响 0.6`

**3b · 号角 + 闪光琶音（典礼感）**
```
A short triumphant fanfare followed by a cascade of magical sparkling chimes and twinkles, celebratory and festive, bright and rewarding, clean, no vocals.
```
`时长 2.2s`

**3c · 欢呼掌声（热烈，渐强）**
```
Enthusiastic crowd applause and cheering, people clapping and celebrating, building up energy, warm and lively, realistic recorded ambience, natural, uplifting.
```
`时长 2.8s`

**3d · 组合：号角 → 掌声 → 闪光（信息量最大）**
```
A celebration sequence: a quick triumphant brass fanfare, then warm genuine applause and cheering, finishing with bright sparkling chimes, joyful and rewarding, realistic, clean.
```
`时长 3.2s`

---

## 生成后：接入 App 的小提示

- **响度分层**：确认音（类1）应明显比完成/庆祝**更轻更短**，否则每组动作都很吵。建议做响度归一后，确认音再压低几 dB。
- **格式**：App 端一般用 `mp3` 或 `ogg`；确认音这种高频短音，`ogg`/`aac` 体积更优。
- **缓存/预载**：和语音话术一样，音效文件建议随 App 预置或起服预热，避免首次播放拉取延迟。
- **一致性**：三类最好来自**同一音色家族/同一生成风格**，听起来才像"一套"。挑选时优先保证调性统一，而不是每个单独最惊艳。
