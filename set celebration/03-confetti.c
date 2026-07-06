/*
 * confetti.c — 纯代码撒花/庆祝粒子动画（嵌入式圆屏 466x466）
 * ----------------------------------------------------------------------------
 * 设计目标（对应需求问题 2）：
 *   (a) 语言：C。直接写 RGB565 framebuffer；也可挂到 LVGL 的定时器里绘制。
 *   (b) 嵌入式可播：可以。粒子系统是最省的动画之一——几百个小方块，
 *       RGB565、定点/浮点均可，30fps 在中端 MCU 上轻松跑。
 *   (c) 高性能、无素材：粒子是"画"出来的（小矩形），没有任何图片资源；
 *       固定粒子池、无动态内存、可选圆形裁剪与 alpha 混合。
 *
 * 集成：
 *   - 每帧调用 confetti_update(dt) + confetti_render(fb)。
 *   - 完成时调用 confetti_emit(...) 按档位喷发。
 *   - 文字三行（范围/状态/徽章）由你的字体库（如 LVGL label）另绘，
 *     本文件只负责粒子层与圆形合成，文字绘制留了 hook（见文末注释）。
 * ----------------------------------------------------------------------------
 */
#include <stdint.h>
#include <string.h>

/* ---------- 屏幕配置 ---------- */
#define SCR_W        466
#define SCR_H        466
#define SCR_ROUND    1           /* 圆屏：开启圆形裁剪 */
#define MAX_PARTICLES 480        /* 粒子池上限（满贯档最多）；普通档只用一小部分 */

typedef uint16_t px_t;           /* RGB565 */
#define RGB565(r,g,b)  ((px_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b) >> 3)))

/* 品牌配色（荧光绿×黑 + 金/白）——写死为常量，无素材 */
static const px_t COL_NEON  = RGB565(0xA3, 0xE6, 0x35);
static const px_t COL_WHITE = RGB565(0xF2, 0xF7, 0xEA);
static const px_t COL_GOLD  = RGB565(0xF5, 0xC4, 0x51);
static const px_t COL_DIM   = RGB565(0x6F, 0x9B, 0x26);

/* ---------- 粒子 ---------- */
typedef struct {
    int16_t  x, y;      /* 位置（1/16 像素定点，避免浮点：真实像素 = x>>4） */
    int16_t  vx, vy;    /* 速度（1/16 像素/帧） */
    int16_t  gravity;   /* 重力增量 */
    uint8_t  size;      /* 边长（像素） */
    uint8_t  life;      /* 剩余生命 255→0，兼作 alpha */
    uint8_t  decay;     /* 每帧生命衰减 */
    px_t     color;
    uint8_t  active;
} particle_t;

static particle_t g_pool[MAX_PARTICLES];
static int        g_count = 0;

/* ---------- 快速 RNG（xorshift，不依赖 libc rand，可复现、超轻） ---------- */
static uint32_t g_rng = 0x1234abcdu;
static inline uint32_t rnd(void) {
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5;
    return g_rng;
}
static inline int rnd_range(int lo, int hi) { return lo + (int)(rnd() % (uint32_t)(hi - lo + 1)); }

void confetti_seed(uint32_t s) { if (s) g_rng = s; }

/* ---------- 喷发 ----------
 * count : 粒子数（按档位：本组~70 / 动作~150 / 课程~260 / PB 再×1.7）
 * gold  : 是否掺金色（表现=新纪录时）
 * 从屏幕中上部向上抛出，靠重力落下——经典礼花/撒花。
 */
void confetti_emit(int count, int gold) {
    if (count > MAX_PARTICLES) count = MAX_PARTICLES;
    for (int i = 0; i < MAX_PARTICLES && count > 0; i++) {
        if (g_pool[i].active) continue;
        particle_t *p = &g_pool[i];
        int cx = (SCR_W / 2) << 4;
        p->x = (int16_t)(cx + (rnd_range(-SCR_W/4, SCR_W/4) << 4) / 2);
        p->y = (int16_t)((SCR_H * 42 / 100) << 4);
        p->vx = (int16_t)(rnd_range(-80, 80));         /* 横向散开 */
        p->vy = (int16_t)(rnd_range(-200, -70));       /* 先向上冲 */
        p->gravity = 12;                               /* 重力 */
        p->size = (uint8_t)rnd_range(4, 8);
        p->life = 255;
        p->decay = (uint8_t)rnd_range(2, 4);
        /* 配色：金色档 45% 概率金，其余绿/白/暗绿 */
        uint32_t r = rnd() % 100;
        if (gold && r < 45)      p->color = COL_GOLD;
        else if (r < 55)         p->color = COL_NEON;
        else if (r < 78)         p->color = COL_WHITE;
        else                     p->color = COL_DIM;
        p->active = 1;
        g_count++;
        count--;
    }
}

/* ---------- 每帧更新 ---------- */
void confetti_update(void) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        particle_t *p = &g_pool[i];
        if (!p->active) continue;
        p->vy = (int16_t)(p->vy + p->gravity);
        p->x  = (int16_t)(p->x + p->vx);
        p->y  = (int16_t)(p->y + p->vy);
        if (p->life <= p->decay) { p->active = 0; g_count--; continue; }
        p->life = (uint8_t)(p->life - p->decay);
        if ((p->y >> 4) > SCR_H + 8) { p->active = 0; g_count--; }
    }
}

int confetti_alive(void) { return g_count; }
void confetti_clear(void) { memset(g_pool, 0, sizeof(g_pool)); g_count = 0; }

/* ---------- alpha 混合（把粒子色按 life 淡入背景；无 FPU 也 OK） ---------- */
static inline px_t blend565(px_t bg, px_t fg, uint8_t a) {
    /* a: 0..255。拆 5/6/5 通道线性插值 */
    uint32_t br = (bg >> 11) & 0x1F, bgc = (bg >> 5) & 0x3F, bb = bg & 0x1F;
    uint32_t fr = (fg >> 11) & 0x1F, fgc = (fg >> 5) & 0x3F, fb = fg & 0x1F;
    uint32_t r = (fr * a + br * (255 - a)) / 255;
    uint32_t g = (fgc * a + bgc * (255 - a)) / 255;
    uint32_t b = (fb * a + bb * (255 - a)) / 255;
    return (px_t)((r << 11) | (g << 5) | b);
}

/* ---------- 圆形裁剪：像素是否在 466 圆内 ---------- */
static inline int in_circle(int x, int y) {
#if SCR_ROUND
    int dx = x - SCR_W / 2, dy = y - SCR_H / 2;
    int rr = (SCR_W / 2) * (SCR_H / 2);
    return (dx * dx + dy * dy) <= rr;
#else
    (void)x; (void)y; return 1;
#endif
}

/* ---------- 渲染粒子层到 framebuffer（RGB565，行主序） ----------
 * 高性能要点：
 *   - 只遍历活跃粒子，画各自的小矩形，不做全屏扫描。
 *   - 圆屏只对粒子覆盖的像素做圆判定，成本极低。
 *   - 想更快：关掉 blend 直接写 color（去掉淡出），或降到 30fps。
 */
void confetti_render(px_t *fb) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        particle_t *p = &g_pool[i];
        if (!p->active) continue;
        int px = p->x >> 4, py = p->y >> 4;
        int s = p->size, sh = s >> 1;           /* 画成扁一点的小片，更像纸屑 */
        for (int yy = py; yy < py + sh; yy++) {
            if (yy < 0 || yy >= SCR_H) continue;
            for (int xx = px; xx < px + s; xx++) {
                if (xx < 0 || xx >= SCR_W) continue;
                if (!in_circle(xx, yy)) continue;
                px_t *dst = &fb[yy * SCR_W + xx];
                *dst = blend565(*dst, p->color, p->life);
            }
        }
    }
}

/* ---------- 圆边光环（替代方屏"全屏爆发"，做 T3/T4 强化） ----------
 * intensity: 0..255。沿圆边画一圈发光描边。
 */
void ring_render(px_t *fb, px_t color, uint8_t intensity, int thickness) {
#if SCR_ROUND
    int cx = SCR_W / 2, cy = SCR_H / 2, R = SCR_W / 2 - 3;
    int r2o = R * R, r2i = (R - thickness) * (R - thickness);
    for (int y = 0; y < SCR_H; y++) {
        for (int x = 0; x < SCR_W; x++) {
            int dx = x - cx, dy = y - cy, d2 = dx*dx + dy*dy;
            if (d2 <= r2o && d2 >= r2i) {
                px_t *dst = &fb[y * SCR_W + x];
                *dst = blend565(*dst, color, intensity);
            }
        }
    }
#else
    (void)fb;(void)color;(void)intensity;(void)thickness;
#endif
}

/* ============================================================================
 * 档位 → 喷发参数（解耦组合：范围 × 状态 × 表现，不穷举）
 * ==========================================================================*/
typedef enum { SCOPE_SET, SCOPE_ACTION, SCOPE_COURSE } scope_t;
typedef enum { STATE_LOGGED, STATE_DONE } state_t;
typedef enum { PERF_NONE, PERF_GREAT, PERF_PB } perf_t;

void celebration_emit(scope_t scope, state_t state, perf_t perf) {
    int base = (scope == SCOPE_SET) ? 70 : (scope == SCOPE_ACTION) ? 150 : 260;
    if (state == STATE_LOGGED) base = base * 7 / 10;   /* 已记录：仍撒花，稍收 */
    if (perf == PERF_GREAT)    base = base * 13 / 10;
    if (perf == PERF_PB)       base = base * 17 / 10;
    confetti_emit(base, perf == PERF_PB /*gold*/);
}

/* ============================================================================
 * 主循环集成示例（伪，展示怎么用）：
 *
 *   celebration_emit(SCOPE_SET, STATE_DONE, PERF_PB);   // 完成瞬间喷发
 *   while (celebrating) {                                // 每帧（~30fps）
 *       confetti_update();
 *       fb_fill(fb, RGB565(0,0,0));                      // 黑底
 *       confetti_render(fb);                             // 粒子层
 *       if (course_tier) ring_render(fb, COL_GOLD, ring_alpha, 4);
 *       draw_text_three_lines(fb, scope_str, state_str, badge_str); // 你的字体库
 *       fb_flush(fb);                                    // 推屏
 *   }
 *
 * 文字三行由字体库绘制（LVGL: 三个 lv_label；裸 framebuffer: 位图字体 blit）。
 * 本文件保证：无素材、无 malloc、纯整数可选、圆屏裁剪就绪。
 * ==========================================================================*/
