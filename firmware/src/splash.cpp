#include "splash.h"
#include "splash_animations.h"
#include "theme.h"
#include "usage_rate.h"
#include "display_cfg.h"
#include <Arduino.h>
#include <string.h>
#include <esp_heap_caps.h>

// 20x20 grid scaled (Original 24x for 480x480, 14x for 320x480)
#define GRID         20
#ifdef JC3248W535
#define CELL         14
#else
#define CELL         24
#endif
#define CANVAS_W     (GRID * CELL)
#define CANVAS_H     (GRID * CELL)

#define COL_BG       THEME_BG
#define COL_EMPTY    0x0000

static lv_obj_t *splash_container = NULL;
static lv_obj_t *canvas = NULL;
static uint16_t *canvas_buf = NULL;
static bool active = false;

static uint32_t last_pick_ms = 0;
static uint8_t  cur_anim = 0;
static uint8_t  cur_frame = 0;
static uint32_t frame_started_ms = 0;

static void render_frame(const uint8_t *cells, const uint16_t *palette) {
    if (!canvas_buf) return;
    for (int gy = 0; gy < GRID; gy++) {
        uint16_t row[CANVAS_W];
        for (int gx = 0; gx < GRID; gx++) {
            uint8_t code = cells[gy * GRID + gx];
            uint16_t color = (palette && code < SPLASH_PALETTE_SIZE) ? palette[code] : COL_EMPTY;
            for (int i = 0; i < CELL; i++) row[gx * CELL + i] = color;
        }
        for (int dy = 0; dy < CELL; dy++) {
            memcpy(&canvas_buf[(gy * CELL + dy) * CANVAS_W], row, CANVAS_W * 2);
        }
    }
    if (splash_container) lv_obj_invalidate(splash_container);
}

void splash_pick_for_current_rate() {
    int group_idx = usage_rate_group();
    const char *target_cat = "Idle";
    
    if (group_idx >= 3) target_cat = "Work";
    else if (group_idx == 2) target_cat = "Dance";
    else if (group_idx == 1) target_cat = "Expressions";

    int candidates[SPLASH_ANIM_COUNT];
    int count = 0;
    for (int i = 0; i < SPLASH_ANIM_COUNT; i++) {
        if (strcmp(splash_anims[i].category, target_cat) == 0) {
            candidates[count++] = i;
        }
    }

    if (count > 0) {
        cur_anim = candidates[random(count)];
        cur_frame = 0;
        frame_started_ms = millis();
        render_frame(splash_anims[cur_anim].frames[0], splash_anims[cur_anim].palette);
    }
    last_pick_ms = millis();
}

void splash_init(lv_obj_t *parent) {
    canvas_buf = (uint16_t*)heap_caps_malloc(CANVAS_W * CANVAS_H * 2, MALLOC_CAP_SPIRAM);
    splash_container = lv_obj_create(parent);
    lv_obj_set_size(splash_container, 480, 320); // Fixed size for HMI
    lv_obj_set_pos(splash_container, 0, 0);
    lv_obj_set_style_bg_color(splash_container, COL_BG, 0);
    lv_obj_set_style_bg_opa(splash_container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(splash_container, 0, 0);
    lv_obj_set_style_pad_all(splash_container, 0, 0);
    lv_obj_clear_flag(splash_container, LV_OBJ_FLAG_SCROLLABLE);

    canvas = lv_canvas_create(splash_container);
    lv_canvas_set_buffer(canvas, canvas_buf, CANVAS_W, CANVAS_H, LV_COLOR_FORMAT_RGB565);
    lv_obj_center(canvas);

    active = false;
    randomSeed(analogRead(0));
}

void splash_tick(void) {
    if (!active || SPLASH_ANIM_COUNT == 0) return;

    if (millis() - last_pick_ms >= 15000) {
        splash_pick_for_current_rate();
    }

    const splash_anim_def_t *a = &splash_anims[cur_anim];
    uint32_t hold = (a->holds) ? a->holds[cur_frame] : 100;
    
    if (millis() - frame_started_ms >= hold) {
        cur_frame = (cur_frame + 1) % a->frame_count;
        frame_started_ms = millis();
        render_frame(a->frames[cur_frame], a->palette);
    }
}

void splash_show(void) {
    if (splash_container) lv_obj_clear_flag(splash_container, LV_OBJ_FLAG_HIDDEN);
    splash_pick_for_current_rate();
    active = true;
}

void splash_hide(void) {
    if (splash_container) lv_obj_add_flag(splash_container, LV_OBJ_FLAG_HIDDEN);
    active = false;
}

bool splash_is_active(void) { return active; }
lv_obj_t* splash_get_root(void) { return splash_container; }
