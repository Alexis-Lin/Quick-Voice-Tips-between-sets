/*
 * celebration.h — 动作完成庆祝：独立原子模块 · 接口定义
 * ----------------------------------------------------------------------------
 * 定位（见 05-atomic-module.md）：
 *   庆祝界面是一个自包含的原子交互模块。宿主给一份 config，模块独立演完
 *   一次庆祝（UI 三行 + 撒花 + 音效 + 预生成语音），演完发 on_done 回调。
 *   模块【不拥有导航】——去哪儿由宿主业务当时决定。
 *
 * 解耦维度（见 01-PRD.md / 04-voice-copy.md）：
 *   ① 范围 scope × ② 状态 state × ③ 表现 perf  →  组合出 文字/徽章/音效/语音，
 *   不穷举。文字=范围+状态(三行,状态最大)，表现走独立徽章，音效=base+PB叠层。
 * ----------------------------------------------------------------------------
 */
#ifndef CELEBRATION_H
#define CELEBRATION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- ① 范围 / ② 状态 / ③ 表现 ---------- */
typedef enum { CELEB_SCOPE_SET, CELEB_SCOPE_ACTION, CELEB_SCOPE_COURSE } celeb_scope_t;
typedef enum { CELEB_STATE_LOGGED, CELEB_STATE_DONE }                    celeb_state_t;
typedef enum { CELEB_PERF_NONE,  CELEB_PERF_GREAT, CELEB_PERF_PB }       celeb_perf_t;

/* ---------- 语言 / 形态 ---------- */
typedef enum { CELEB_LANG_ZH, CELEB_LANG_EN }        celeb_lang_t;
typedef enum { CELEB_FORM_ROUND, CELEB_FORM_PHONE }  celeb_form_t;   /* 圆屏466 / 手机方屏 */

/* ---------- 配置：宿主把"已决定好的组合"传进来 ---------- */
typedef struct {
    celeb_scope_t scope;
    celeb_state_t state;
    celeb_perf_t  perf;
    celeb_lang_t  lang;
    celeb_form_t  form;
    uint16_t      dwell_ms;    /* 0 = 用档位默认(本组~2000 / PB~2400 / 结课>=3000) */
    uint8_t       skippable;   /* 非0 = 允许点击跳过(最短停留~1200ms 兜底) */
} celeb_config_t;

/* 演完信号：唯一出口。宿主在这里决定下一步（不由模块导航）。 */
typedef void (*celeb_done_cb)(void *user);

/* ============================================================================
 * 原子模块 API（生命周期）
 * ==========================================================================*/

/* 开始一次庆祝：渲染三行 + 喷发撒花 + 播完成音效 + t+300ms 播预生成语音。 */
void celeb_show(const celeb_config_t *cfg, celeb_done_cb on_done, void *user);

/* 每帧推进（~30fps）。到 dwell 或被点击后自动停止并触发 on_done。
 * 返回 1 = 仍在庆祝，0 = 已结束。裸 framebuffer 集成时由主循环调用。 */
int  celeb_tick(uint32_t dt_ms, uint16_t *fb /* RGB565, 可为 NULL 仅推进逻辑 */);

/* 宿主主动收起（生命周期模式 B）：立即停止媒体并触发 on_done。 */
void celeb_dismiss(void);

/* 用户点击（可跳过时立即结束）。 */
void celeb_on_tap(void);

/* ============================================================================
 * 组合 → 资源映射（供实现层使用；不穷举，按维度取值）
 * ==========================================================================*/

/* 语音情境 key："set_done" / "set_pb" / "course_grand" ... 用于挑预生成音频。
 * 语音【预生成、不实时合成】：{case}/{lang}/{variantId}.opus，运行时洗牌袋挑一条。 */
const char *celeb_voice_case(const celeb_config_t *cfg);

/* 撒花喷发（实现见 03-confetti.c）：数量 = f(范围)×f(表现)，PB 掺金色。 */
void celebration_emit(celeb_scope_t scope, celeb_state_t state, celeb_perf_t perf);

/* 音效：基础音 base(范围,状态) + 表现叠层(PB=啪啪)。由平台音频层实现。
 * 已记录=咚咚(暖) / 已完成=当当(范围越大越隆重) / PB 叠「啪啪」。 */
void celeb_play_sfx(const celeb_config_t *cfg);

#ifdef __cplusplus
}
#endif
#endif /* CELEBRATION_H */
