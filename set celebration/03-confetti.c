/*
 * confetti.c — 纯代码撒花/庆祝粒子动画（嵌入式圆屏 466x466）· 升级版
 * ----------------------------------------------------------------------------
 * 升级点（分级）：
 *   - 飘摆翻转 flutter：粒子横向正弦摆动 + 旋转 + 空气阻力，像真纸屑，不再直线掉。
 *   - 多形状：矩形 / 圆 / 细丝带 streamer / 星点 spark。
 *   - 彩纸炮 cannon：从底部两角向内上方高速喷发（party popper）——大场合才放。
 *   - 分级调色：普通=品牌色；很棒=更亮；PB=加金；结课/满贯=加低饱和多彩。
 *   - 节奏：宿主先做 ~180–260ms「蓄力」(环收拢/闪光/上扬音)，到爆发瞬间再调
 *           celebration_emit()——蓄力→爆发的节奏由 UI 层控制，见文末。
 *
 * 仍然：C、无素材、固定粒子池、无 malloc、圆形裁剪、可选 alpha 混合。
 * ----------------------------------------------------------------------------
 */
#include <stdint.h>
#include <string.h>
#include <math.h>

#define SCR_W        466
#define SCR_H        466
#define SCR_ROUND    1
#define MAX_PARTICLES 520

typedef uint16_t px_t;
#define RGB565(r,g,b)  ((px_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b) >> 3)))

/* 品牌色 + 大场合多彩点缀（写死常量，无素材） */
static const px_t COL_NEON  = RGB565(0xA3, 0xE6, 0x35);
static const px_t COL_WHITE = RGB565(0xF2, 0xF7, 0xEA);
static const px_t COL_GOLD  = RGB565(0xF5, 0xC4, 0x51);
static const px_t COL_DIM   = RGB565(0x6F, 0x9B, 0x26);
static const px_t COL_CORAL = RGB565(0xF2, 0x85, 0x6B);
static const px_t COL_CYAN  = RGB565(0x6B, 0xD6, 0xE6);
static const px_t COL_LILAC = RGB565(0xC9, 0x8B, 0xDB);

enum { SHAPE_RECT, SHAPE_CIRCLE, SHAPE_STREAMER, SHAPE_SPARK };

typedef struct {
    float x, y, vx, vy;   /* 位置/速度（像素、像素/帧） */
    float sway_ph, sway_a;/* 横向摆动相位/幅度（flutter） */
    float rot, vr;        /* 旋转 */
    uint8_t size, shape;
    uint8_t life, decay;  /* 255→0，兼作 alpha */
    px_t   color;
    uint8_t active;
} particle_t;

static particle_t g_pool[MAX_PARTICLES];
static int g_count = 0;

/* xorshift RNG（轻、可复现） */
static uint32_t g_rng = 0x1234abcdu;
static inline uint32_t rnd(void){ g_rng^=g_rng<<13; g_rng^=g_rng>>17; g_rng^=g_rng<<5; return g_rng; }
static inline float frand(void){ return (float)(rnd() & 0xffffff) / (float)0xffffff; }      /* 0..1 */
static inline float frand2(void){ return frand()*2.f - 1.f; }                                /* -1..1 */
void confetti_seed(uint32_t s){ if(s) g_rng = s; }

/* ---------- 分级调色板 ---------- */
typedef enum { SCOPE_SET, SCOPE_ACTION, SCOPE_COURSE } scope_t;
typedef enum { STATE_LOGGED, STATE_DONE }              state_t;
typedef enum { PERF_NONE, PERF_GREAT, PERF_PB }        perf_t;

static px_t pick_color(scope_t scope, perf_t perf){
    /* 普通：品牌色；PB：偏金；结课/满贯：掺多彩 */
    uint32_t r = rnd() % 100;
    if (perf == PERF_PB) {                       /* 金为主 */
        if (r < 45) return COL_GOLD;
        if (r < 70) return COL_NEON;
        return COL_WHITE;
    }
    if (scope == SCOPE_COURSE) {                  /* 结课/满贯：多彩点缀 */
        if (r < 40) return COL_NEON;
        if (r < 55) return COL_WHITE;
        if (r < 66) return COL_GOLD;
        if (r < 78) return COL_CORAL;
        if (r < 90) return COL_CYAN;
        return COL_LILAC;
    }
    if (perf == PERF_GREAT) {                      /* 很棒：更亮 + 一点金 */
        if (r < 50) return COL_NEON;
        if (r < 80) return COL_WHITE;
        return COL_GOLD;
    }
    if (r < 50) return COL_NEON;                   /* 普通：品牌色 */
    if (r < 78) return COL_WHITE;
    return COL_DIM;
}

static particle_t *alloc_p(void){
    for (int i = 0; i < MAX_PARTICLES; i++) if (!g_pool[i].active) return &g_pool[i];
    return 0;
}
static void init_common(particle_t *p, scope_t scope, perf_t perf){
    p->sway_ph = frand()*6.28f;
    p->sway_a  = 0.6f + frand()*1.6f;             /* flutter 幅度 */
    p->rot = frand()*6.28f; p->vr = frand2()*0.5f;
    p->size = 4 + (rnd()%5);
    p->shape = (uint8_t)(rnd()%5); if (p->shape>SHAPE_SPARK) p->shape = SHAPE_RECT;
    p->life = 255; p->decay = 2 + (rnd()%3);
    p->color = pick_color(scope, perf);
    p->active = 1; g_count++;
}

/* 顶部飘落（overhead rain）：所有档都有，柔和 */
static void emit_rain(int n, scope_t scope, perf_t perf){
    for (int i=0;i<n;i++){ particle_t*p=alloc_p(); if(!p)return;
        p->x = SCR_W*(0.5f + frand2()*0.31f);
        p->y = -8.f - frand()*SCR_H*0.15f;
        p->vx = frand2()*1.6f; p->vy = 1.0f + frand()*2.2f;
        init_common(p, scope, perf);
    }
}
/* 彩纸炮：从底角向内上方高速喷（fx=0.12 左 / 0.88 右） */
static void emit_cannon(float fx, int n, scope_t scope, perf_t perf){
    float ox = SCR_W*fx, oy = SCR_H*1.02f, dir = (fx<0.5f)?1.f:-1.f;
    for (int i=0;i<n;i++){ particle_t*p=alloc_p(); if(!p)return;
        float ang = -1.5708f + dir*(0.12f + frand()*0.5f);
        float spd = 11.f + frand()*9.f;
        p->x=ox; p->y=oy; p->vx=cosf(ang)*spd; p->vy=sinf(ang)*spd;
        init_common(p, scope, perf);
    }
}
/* 中心爆裂（配合状态词撞击） */
static void emit_burst(int n, scope_t scope, perf_t perf){
    float cx=SCR_W*0.5f, cy=SCR_H*0.46f;
    for (int i=0;i<n;i++){ particle_t*p=alloc_p(); if(!p)return;
        float ang=frand()*6.28f, spd=3.f+frand()*9.f;
        p->x=cx; p->y=cy; p->vx=cosf(ang)*spd; p->vy=sinf(ang)*spd - 4.f;
        init_common(p, scope, perf);
    }
}

/* ---------- 每帧更新：重力 + 阻力 + 横向摆动(flutter) + 旋转 ---------- */
void confetti_update(void){
    for (int i=0;i<MAX_PARTICLES;i++){ particle_t*p=&g_pool[i]; if(!p->active) continue;
        p->vy += 0.30f;            /* 重力 */
        p->vx *= 0.992f;           /* 空气阻力 */
        p->sway_ph += 0.15f;
        p->x += p->vx + sinf(p->sway_ph)*p->sway_a;   /* flutter */
        p->y += p->vy;
        p->rot += p->vr;
        if (p->life <= p->decay){ p->active=0; g_count--; continue; }
        p->life -= p->decay;
        if (p->y > SCR_H + 12){ p->active=0; g_count--; }
    }
}
int  confetti_alive(void){ return g_count; }
void confetti_clear(void){ memset(g_pool,0,sizeof(g_pool)); g_count=0; }

/* ---------- alpha 混合 ---------- */
static inline px_t blend565(px_t bg, px_t fg, uint8_t a){
    uint32_t br=(bg>>11)&0x1F, bgc=(bg>>5)&0x3F, bb=bg&0x1F;
    uint32_t fr=(fg>>11)&0x1F, fgc=(fg>>5)&0x3F, fb=fg&0x1F;
    uint32_t r=(fr*a+br*(255-a))/255, g=(fgc*a+bgc*(255-a))/255, b=(fb*a+bb*(255-a))/255;
    return (px_t)((r<<11)|(g<<5)|b);
}
static inline int in_circle(int x,int y){
#if SCR_ROUND
    int dx=x-SCR_W/2, dy=y-SCR_H/2; return dx*dx+dy*dy <= (SCR_W/2)*(SCR_H/2);
#else
    (void)x;(void)y; return 1;
#endif
}
/* 画一个轴对齐小块（旋转用近似：这里按 size 直接铺；需要更精细可加旋转采样） */
static void plot_rect(px_t *fb, int cx, int cy, int w, int h, px_t col, uint8_t a){
    for (int y=cy-h/2; y<cy-h/2+h; y++){ if(y<0||y>=SCR_H) continue;
        for (int x=cx-w/2; x<cx-w/2+w; x++){ if(x<0||x>=SCR_W) continue; if(!in_circle(x,y)) continue;
            px_t *d=&fb[y*SCR_W+x]; *d = blend565(*d, col, a); } }
}
/* ---------- 渲染：按形状画（rect/circle/streamer/spark 的近似） ---------- */
void confetti_render(px_t *fb){
    for (int i=0;i<MAX_PARTICLES;i++){ particle_t*p=&g_pool[i]; if(!p->active) continue;
        int x=(int)p->x, y=(int)p->y, s=p->size; uint8_t a=p->life;
        switch (p->shape){
            case SHAPE_CIRCLE:   plot_rect(fb,x,y,s,s,p->color,a); break;                 /* 近似 */
            case SHAPE_STREAMER: plot_rect(fb,x,y,s/2>1?s/2:1,s*2,p->color,a); break;      /* 细长 */
            case SHAPE_SPARK:    plot_rect(fb,x,y,s,2,p->color,a);
                                 plot_rect(fb,x,y,2,s,p->color,a); break;                 /* 十字 */
            default:             plot_rect(fb,x,y,s,s/2>1?s/2:1,p->color,a); break;        /* 纸屑 */
        }
    }
}

/* 圆边光环（结课/满贯强化，替代方屏全屏爆发） */
void ring_render(px_t *fb, px_t color, uint8_t intensity, int thickness){
#if SCR_ROUND
    int cx=SCR_W/2, cy=SCR_H/2, R=SCR_W/2-3, r2o=R*R, r2i=(R-thickness)*(R-thickness);
    for (int y=0;y<SCR_H;y++) for (int x=0;x<SCR_W;x++){
        int dx=x-cx,dy=y-cy,d2=dx*dx+dy*dy;
        if (d2<=r2o && d2>=r2i){ px_t*d=&fb[y*SCR_W+x]; *d=blend565(*d,color,intensity); } }
#else
    (void)fb;(void)color;(void)intensity;(void)thickness;
#endif
}

/* ============================================================================
 * 档位 → 喷发（分级组合：普通只柔和飘落；很棒/PB/结课 才放彩纸炮）
 * ==========================================================================*/
void celebration_emit(scope_t scope, state_t state, perf_t perf){
    int base = (scope==SCOPE_SET)?70 : (scope==SCOPE_ACTION)?150 : 260;
    if (state==STATE_LOGGED) base = base*7/10;
    if (perf==PERF_GREAT)    base = base*13/10;
    if (perf==PERF_PB)       base = base*17/10;

    emit_rain(base/2, scope, perf);
    int bursty = (perf!=PERF_NONE) || (scope!=SCOPE_SET);   /* 分级：普通不放炮 */
    if (bursty){
        emit_cannon(0.12f, base/2, scope, perf);
        emit_cannon(0.88f, base/2, scope, perf);
        emit_burst(base*2/5, scope, perf);
    } else {
        emit_burst(base*7/20, scope, perf);
    }
}

/* ============================================================================
 * 蓄力 → 爆发（节奏由 UI 层控制）示例：
 *
 *   // 1) 蓄力 ~180–260ms：环收拢/屏幕微光/上扬音（大场合更久）
 *   ui_charge(is_big ? 260 : 180);
 *   // 2) 爆发瞬间：
 *   celebration_emit(scope, state, perf);   // 撒花（含彩纸炮）
 *   ui_flash(); ui_word_pop();              // 屏幕闪光 + 状态词 overshoot 弹入
 *   // 3) 每帧：
 *   while (celebrating){ confetti_update();
 *       fb_fill(fb, RGB565(0,0,0)); confetti_render(fb);
 *       if (scope==SCOPE_COURSE||perf==PERF_PB) ring_render(fb, COL_GOLD, ring_a, 4);
 *       draw_text_three_lines(fb, ...); fb_flush(fb); }
 *
 * 性能：仍是固定池 + 无 malloc；sinf/cosf 若无 FPU 可换定点 LUT。
 * ==========================================================================*/
