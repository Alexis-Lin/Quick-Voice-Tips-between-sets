# 04 · 庆祝语音：预生成方案 + 文案（按组合分组）

> 需求：语音**提前生成好**（不实时 TTS）；**解耦实时纠错/统计**（保速度、便于预生成）；文案按 **第一(范围) × 第二(状态) × 第三(表现)** 的排列组合分组，每个 case 给 **8–10 条**，**中英分列**。

## 1. 组合维度（"第一、第二、第三"）

| 维 | 取值 |
|---|---|
| **① 范围 Scope** | 本组 SET / 本动作 ACTION / 全部课程 COURSE |
| **② 状态 State** | 已完成 DONE（达标） / 已记录 LOGGED（未达标） |
| **③ 表现 Perf** | 无 / 很棒 GREAT / 新纪录 PB |

- 文案**只依赖这三个粗标记**（完成瞬间即有的布尔/枚举），**不含具体次数/纠错**——可离线预生成，运行时洗牌袋挑一条即播，零延时。
- 长度：中文 ≤~20 字、英文 ≤~12 词，**2–3 秒内说完**，正好卡界面停留。
- 下列为常用组合 case；其余组合（如 本动作×很棒）可按同法扩展。

---

## Case 1 ｜ ① 本组 × ② 已完成 × ③ 无

**中文**
1. 这组稳稳拿下，节奏保持得很好。
2. 完成！动作很到位，喘口气继续。
3. 干得漂亮，这组质量在线，保持住。
4. 好样的，这组很扎实，放松一下。
5. 完成一组，状态不错，接着来。
6. 稳了，这组节奏很舒服，休息片刻。
7. 搞定这组，深呼吸，准备下一组。
8. 完成得很利落，继续保持这股劲。
9. 这组收得漂亮，喝口水缓一缓。
10. 又拿下一组，稳扎稳打，很好。

**English**
1. Nice — that set was smooth and steady.
2. Done! Great form, take a breath.
3. Solid work, quality's on point — keep it up.
4. Well done, a strong set. Relax a sec.
5. Set complete, good rhythm — let's keep going.
6. Clean and steady — grab some water.
7. That's a wrap, breathe and reset.
8. Crisp finish, hold onto that momentum.
9. Nicely done, ease up for a moment.
10. Another set down, steady as you go.

---

## Case 2 ｜ ① 本组 × ② 已完成 × ③ 很棒

**中文**
1. 这组几乎组组到位，控制力很棒。
2. 又稳又准，动作标准得像教科书。
3. 质量很高，这组打得非常漂亮。
4. 太棒了，这组的完成度拉满了。
5. 动作干净利落，专业范儿十足。
6. 控制得很到位，这组质量真高。
7. 稳定又精准，这一组相当出色。
8. 细节都做到位了，非常漂亮。
9. 这组状态在线，几乎无可挑剔。
10. 很出色，这组的水准很能打。

**English**
1. Almost every rep on point — great control.
2. Steady and precise — textbook form.
3. High quality — that set was beautiful.
4. Excellent, fully dialed in this set.
5. Crisp and clean — real pro form.
6. Great control, top-quality set.
7. Steady and sharp — outstanding work.
8. Every detail nailed, beautifully done.
9. Dialed in, nearly flawless.
10. Superb — that set was strong.

---

## Case 3 ｜ ① 本组 × ② 已完成 × ③ 新纪录

**中文**
1. 破纪录啦，你刚刚超越了自己！
2. 新高度达成，这一组值得记住！
3. 全力以赴的样子真帅，为你鼓掌！
4. 又刷新纪录了，今天状态在线！
5. 突破了，这就是进步的样子！
6. 新纪录到手，你比昨天更强了！
7. 历史最佳，就在此刻，太棒了！
8. 刷新自己的上限，燃爆了！
9. 这一组创纪录，实至名归！
10. 超越自我，这份突破值得骄傲！

**English**
1. New record — you just beat yourself!
2. New high — worth remembering!
3. All in and it shows — take a bow!
4. Another record broken, you're dialed in!
5. Breakthrough — this is what progress looks like!
6. New PB — stronger than yesterday!
7. All-time best, right now. Amazing!
8. You raised your own ceiling — on fire!
9. A record-setting set, well earned!
10. Beyond your limit — be proud of that!

---

## Case 4 ｜ ① 本组 × ② 已记录（未达标） × ③ 无

**中文**
1. 这组先记下，调整呼吸下组再战。
2. 没关系，稳住节奏，坚持就很棒。
3. 尽力就好，喘口气我们继续。
4. 记上了，缓一缓，下一组找回状态。
5. 先记一笔，慢慢来，别着急。
6. 这组算数，休息一下再出发。
7. 稳住，能坚持下来已经很了不起。
8. 记录好了，放松肩膀，继续加油。
9. 不急，调整好我们接着来。
10. 先歇口气，下一组稳稳发挥。

**English**
1. Logged — breathe and reset for the next one.
2. It's okay, hold your rhythm — keep at it.
3. Give what you can, breathe, let's continue.
4. Noted. Ease up, find your groove next set.
5. Marked down — no rush, take your time.
6. This one counts. Rest and go again.
7. Hang in there — showing up already matters.
8. Logged. Loosen those shoulders, keep going.
9. No hurry — reset and we'll continue.
10. Catch your breath, nail the next one.

---

## Case 5 ｜ ① 本动作 × ② 已完成（该动作全部组完成）

**中文**
1. 这个动作全部完成，做得很扎实！
2. 这一项拿下了，稳稳当当，很好。
3. 动作收官，质量保持得不错。
4. 完成这个动作，节奏一直很稳。
5. 这一动作搞定，继续下一个。
6. 全部组数完成，状态在线！
7. 这个动作漂亮收尾，休息一下。
8. 稳稳完成，进入下一个动作。

**English**
1. That whole exercise is done — solid work!
2. This one's complete, steady all the way.
3. Exercise wrapped, quality held up nicely.
4. Done with this move — rhythm stayed strong.
5. That exercise is finished — on to the next.
6. All sets complete, dialed in!
7. Clean finish on this one — take a breather.
8. Steadily done, moving to the next exercise.

---

## Case 6 ｜ ① 全部课程 × ② 已完成 × ③ 无（结课）

**中文**
1. 全部完成，今天你坚持到底了！
2. 练完啦，这份努力都算数，真棒。
3. 收工！从头到尾都很稳，鼓个掌。
4. 整节课拿下，为你的坚持骄傲！
5. 今天的训练圆满结束，好样的！
6. 全部搞定，你完成了今天的挑战！
7. 课程完成，坚持到最后就是胜利。
8. 练到底了，给自己一个大大的赞！
9. 收官啦，今天的你很了不起。
10. 全部完成，愿这份坚持继续下去！

**English**
1. All done — you saw it through today!
2. Workout complete — every effort counts. Great job.
3. That's a wrap — steady throughout. Take a bow.
4. Whole session done — proud of your grit!
5. Today's training is complete. Well done!
6. All finished — you beat today's challenge!
7. Course complete — finishing is winning.
8. You went all the way — give yourself a hand!
9. Wrapped up — you were remarkable today.
10. All done — keep that consistency going!

---

## Case 7 ｜ ① 全部课程 × ② 已完成 × ③ 新纪录（满贯）

**中文**
1. 满贯表现，今天几乎无可挑剔！
2. 完美收官，这就是最好的你！
3. 全程高质量，这份坚持太强了！
4. 今天封神，从头燃到尾！
5. 满分收尾，你把自己刷新了！
6. 高光整场，实至名归的满贯！
7. 完美一课，这份突破值得骄傲！
8. 全程在线，今天你无懈可击！
9. 巅峰表现，为今天的你喝彩！
10. 满贯达成，这一课载入你的记录！

**English**
1. A perfect run — nearly flawless today!
2. Perfect finish — this is the best you!
3. High quality all the way — incredible grit!
4. On fire start to finish — legendary!
5. A flawless close — you reset your best!
6. Highlight session — a well-earned sweep!
7. A perfect class — be proud of this breakthrough!
8. Locked in the whole time — untouchable today!
9. Peak performance — cheers to you today!
10. A grand slam — this one's in your records!

---

## 2. 预生成流水线

```
文案表(csv: case=①②③, 语言, 变体id, 文本)
   │ 离线批量 TTS(ElevenLabs / 火山 等)
   ▼
音频(opus/mp3) → 索引 {case}/{lang}/{variantId}.opus → OSS/CDN 或 设备 flash
   │ 运行时
   ▼
完成事件 → 定 case(①×②×③) → 洗牌袋挑变体 → 播本地/缓存音频（无实时合成）
```

- **不上屏**：语音只播、UI 不写（三行文字已表达"什么·状态·表现"）。
- **与音效错峰**：完成音效 t0 起，语音 t+300ms 起。
- **多语言扩展**：新增语言只补一列文本，重跑流水线，代码不动。
- **要短版时**：可从每条截出前半句作短版（如"完成！""破纪录！"），或单列短版列表。
