#include "ui.h"
#include "splash.h"
#include <lvgl.h>
#include "logo_tiny.h"
#include "header_final.h"
#include "icons.h"
#include "display_cfg.h"
#include "ble.h"

LV_FONT_DECLARE(font_styrene_28);
LV_FONT_DECLARE(font_styrene_24);
LV_FONT_DECLARE(font_styrene_20);
LV_FONT_DECLARE(font_styrene_16);
LV_FONT_DECLARE(font_mono_18);

#include "theme.h"
#define COL_BG        THEME_BG
#define COL_PANEL     THEME_PANEL
#define COL_TEXT      THEME_TEXT
#define COL_DIM       THEME_DIM
#define COL_ACCENT    THEME_ACCENT
#define COL_BAR_BG    THEME_BAR_BG

#define SCR_W         480
#define SCR_H         320
#define MARGIN        20
#define TITLE_Y       65
#define CONTENT_Y     125
#define CONTENT_W     (SCR_W - 2 * MARGIN)

static lv_obj_t* usage_container;
static lv_obj_t* bar_session; static lv_obj_t* lbl_session_pct; static lv_obj_t* lbl_session_reset;
static lv_obj_t* bar_weekly; static lv_obj_t* lbl_weekly_pct; static lv_obj_t* lbl_weekly_reset;
static lv_obj_t* lbl_anim;
static lv_obj_t* logo_img;
static lv_obj_t* battery_img;
static lv_image_dsc_t battery_dscs[5];
static lv_image_dsc_t tiny_logo_dsc;
static lv_image_dsc_t main_header_dsc;
static screen_t current_screen = SCREEN_USAGE;

static uint32_t anim_last_ms = 0;
static uint8_t anim_spinner_idx = 0;
static uint8_t anim_phase = 0;
static uint8_t anim_msg_idx = 0;
static uint32_t anim_msg_start = 0;

static const char* const spinner_frames[] = {"\xC2\xB7", "\xE2\x9C\xBB", "\xE2\x9C\xBD", "\xE2\x9C\xB6", "\xE2\x9C\xB3", "\xE2\x9C\xA2"};
#define SPINNER_COUNT 6
static const uint16_t spinner_ms[6] = {260, 130, 130, 130, 130, 260};
static const char* const anim_messages[] = {"Actualizing", "Booping", "Processing", "Thinking", "Working"};
#define ANIM_MSG_COUNT 5

static lv_color_t pct_color(float pct) { return (pct >= 80.0f) ? THEME_RED : (pct >= 50.0f ? THEME_AMBER : THEME_GREEN); }
static void format_reset_time(int mins, char* buf, size_t len) {
    if (mins < 0) snprintf(buf, len, "---");
    else if (mins < 60) snprintf(buf, len, "Resets in %dm", mins);
    else if (mins < 1440) snprintf(buf, len, "Resets in %dh %dm", mins / 60, mins % 60);
    else snprintf(buf, len, "Resets in %dd %dh", mins / 1440, (mins % 1440) / 60);
}

static void init_icon_dsc_rgb565a8(lv_image_dsc_t* dsc, int w, int h, const uint8_t* data) {
    dsc->header.w = w; dsc->header.h = h; dsc->header.cf = LV_COLOR_FORMAT_RGB565A8;
    dsc->header.stride = w * 2; dsc->data = data; dsc->data_size = w * h * 3;
}

static void make_u_panel(lv_obj_t* par, int y, const char* p_txt, lv_obj_t** o_pct, lv_obj_t** o_bar, lv_obj_t** o_res) {
    lv_obj_t* p = lv_obj_create(par); lv_obj_set_pos(p, MARGIN, y); lv_obj_set_size(p, CONTENT_W, 85);
    lv_obj_set_style_bg_color(p, COL_PANEL, 0); lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(p, 8, 0); lv_obj_set_style_border_width(p, 0, 0); lv_obj_set_style_pad_all(p, 8, 0);
    *o_pct = lv_label_create(p); lv_label_set_text(*o_pct, "---%"); lv_obj_set_style_text_font(*o_pct, &font_styrene_24, 0); lv_obj_set_style_text_color(*o_pct, COL_TEXT, 0);
    lv_obj_t* pill = lv_label_create(p); lv_label_set_text(pill, p_txt); lv_obj_set_style_text_font(pill, &font_styrene_16, 0);
    lv_obj_set_style_bg_color(pill, COL_BAR_BG, 0); lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0); lv_obj_set_style_radius(pill, 10, 0); lv_obj_set_style_pad_all(pill, 4, 0); lv_obj_align(pill, LV_ALIGN_TOP_RIGHT, 0, 0);
    *o_bar = lv_bar_create(p); lv_obj_set_pos(*o_bar, 0, 32); lv_obj_set_size(*o_bar, CONTENT_W - 20, 10);
    lv_obj_set_style_bg_color(*o_bar, COL_BAR_BG, LV_PART_MAIN); lv_obj_set_style_bg_color(*o_bar, THEME_GREEN, LV_PART_INDICATOR);
    *o_res = lv_label_create(p); lv_label_set_text(*o_res, "---"); lv_obj_set_style_text_font(*o_res, &font_styrene_16, 0); lv_obj_set_style_text_color(*o_res, COL_TEXT, 0); lv_obj_set_pos(*o_res, 0, 48);
}

void ui_init(void) {
    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COL_BG, 0); lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    init_icon_dsc_rgb565a8(&tiny_logo_dsc, 48, 48, tiny_logo);
    init_icon_dsc_rgb565a8(&main_header_dsc, MAIN_HEADER_W, MAIN_HEADER_H, main_header);
    init_icon_dsc_rgb565a8(&battery_dscs[0], ICON_BATTERY_W, ICON_BATTERY_H, icon_battery_data);
    init_icon_dsc_rgb565a8(&battery_dscs[1], ICON_BATTERY_LOW_W, ICON_BATTERY_LOW_H, icon_battery_low_data);
    init_icon_dsc_rgb565a8(&battery_dscs[2], ICON_BATTERY_MEDIUM_W, ICON_BATTERY_MEDIUM_H, icon_battery_medium_data);
    init_icon_dsc_rgb565a8(&battery_dscs[3], ICON_BATTERY_FULL_W, ICON_BATTERY_FULL_H, icon_battery_full_data);
    init_icon_dsc_rgb565a8(&battery_dscs[4], ICON_BATTERY_CHARGING_W, ICON_BATTERY_CHARGING_H, icon_battery_charging_data);

    usage_container = lv_obj_create(scr); lv_obj_set_size(usage_container, SCR_W, SCR_H);
    lv_obj_set_style_bg_color(usage_container, COL_BG, 0); lv_obj_set_style_bg_opa(usage_container, LV_OPA_COVER, 0); lv_obj_set_style_border_width(usage_container, 0, 0);
    lv_obj_add_event_cb(usage_container, [](lv_event_t* e) { if (ui_get_current_screen() != SCREEN_BLUETOOTH) ui_toggle_splash(); }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* h_img = lv_image_create(usage_container);
    lv_image_set_src(h_img, &main_header_dsc);
    lv_obj_align(h_img, LV_ALIGN_TOP_MID, 5, TITLE_Y - 7);

    make_u_panel(usage_container, CONTENT_Y, "Current", &lbl_session_pct, &bar_session, &lbl_session_reset);
    make_u_panel(usage_container, CONTENT_Y + 95, "Weekly", &lbl_weekly_pct, &bar_weekly, &lbl_weekly_reset);

    lbl_anim = lv_label_create(usage_container); lv_obj_set_style_text_font(lbl_anim, &font_mono_18, 0); lv_obj_set_style_text_color(lbl_anim, COL_ACCENT, 0);
    lv_obj_set_style_bg_color(lbl_anim, COL_BG, 0); lv_obj_set_style_bg_opa(lbl_anim, LV_OPA_COVER, 0); lv_obj_align(lbl_anim, LV_ALIGN_BOTTOM_MID, 0, -5);

    auto make_btn = [&](const char* t, lv_align_t al, int ox) {
        lv_obj_t* b = lv_button_create(usage_container); lv_obj_set_size(b, 100, 32); lv_obj_align(b, al, ox, 0);
        lv_obj_set_style_bg_color(b, COL_PANEL, 0); lv_obj_set_style_pad_all(b, 0, 0);
        lv_obj_t* l = lv_label_create(b); lv_label_set_text(l, t); lv_obj_center(l); return b;
    };
    lv_obj_t* bv = make_btn("Voice", LV_ALIGN_BOTTOM_LEFT, MARGIN);
    lv_obj_add_event_cb(bv, [](lv_event_t* e) { if (lv_event_get_code(e) == LV_EVENT_PRESSED) ble_keyboard_press(0x2C, 0); else if (lv_event_get_code(e) == LV_EVENT_RELEASED) ble_keyboard_release(); }, LV_EVENT_ALL, NULL);
    lv_obj_t* bt = make_btn("Toggle", LV_ALIGN_BOTTOM_RIGHT, -MARGIN);
    lv_obj_add_event_cb(bt, [](lv_event_t* e) { if (lv_event_get_code(e) == LV_EVENT_CLICKED) { ble_keyboard_press(0x2B, 0x02); delay(50); ble_keyboard_release(); } }, LV_EVENT_CLICKED, NULL);

    logo_img = lv_image_create(scr); lv_image_set_src(logo_img, &tiny_logo_dsc); lv_obj_set_pos(logo_img, MARGIN, TITLE_Y - 10);
    lv_obj_add_flag(logo_img, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(logo_img, [](lv_event_t* e) { ui_cycle_screen(); }, LV_EVENT_CLICKED, NULL);

    battery_img = lv_image_create(scr); lv_image_set_src(battery_img, &battery_dscs[0]); lv_obj_set_pos(battery_img, SCR_W - 48 - MARGIN, TITLE_Y - 10);
    splash_init(scr);
    ui_show_screen(SCREEN_USAGE);
}

void ui_show_screen(screen_t s) {
    lv_obj_add_flag(usage_container, LV_OBJ_FLAG_HIDDEN);
    splash_hide();
    if (s == SCREEN_USAGE) lv_obj_clear_flag(usage_container, LV_OBJ_FLAG_HIDDEN);
    else if (s == SCREEN_SPLASH) splash_show();
    if (logo_img) { if (s == SCREEN_SPLASH) lv_obj_add_flag(logo_img, LV_OBJ_FLAG_HIDDEN); else lv_obj_clear_flag(logo_img, LV_OBJ_FLAG_HIDDEN); }
    if (battery_img) { if (s == SCREEN_SPLASH) lv_obj_add_flag(battery_img, LV_OBJ_FLAG_HIDDEN); else lv_obj_clear_flag(battery_img, LV_OBJ_FLAG_HIDDEN); }
    current_screen = s;
}

void ui_cycle_screen(void) { ui_show_screen((current_screen == SCREEN_USAGE) ? SCREEN_USAGE : SCREEN_USAGE); } // Placeholder
void ui_toggle_splash(void) { if (current_screen == SCREEN_SPLASH) ui_show_screen(SCREEN_USAGE); else ui_show_screen(SCREEN_SPLASH); }

void ui_update(const UsageData* d) {
    if (!d || !d->valid) return;
    lv_label_set_text_fmt(lbl_session_pct, "%d%%", (int)(d->session_pct+0.5f)); lv_bar_set_value(bar_session, (int)d->session_pct, LV_ANIM_ON);
    lv_obj_set_style_bg_color(bar_session, pct_color(d->session_pct), LV_PART_INDICATOR);
    char b[48]; format_reset_time(d->session_reset_mins, b, 48); lv_label_set_text(lbl_session_reset, b);
    lv_label_set_text_fmt(lbl_weekly_pct, "%d%%", (int)(d->weekly_pct+0.5f)); lv_bar_set_value(bar_weekly, (int)d->weekly_pct, LV_ANIM_ON);
    lv_obj_set_style_bg_color(bar_weekly, pct_color(d->weekly_pct), LV_PART_INDICATOR);
    format_reset_time(d->weekly_reset_mins, b, 48); lv_label_set_text(lbl_weekly_reset, b);
}

void ui_tick_anim(void) {
    if (current_screen != SCREEN_USAGE) return;
    uint32_t n = lv_tick_get();
    if (n - anim_msg_start >= 4000) { anim_msg_idx = (anim_msg_idx + 1) % 5; anim_msg_start = n; }
    if (n - anim_last_ms >= 200) {
        anim_last_ms = n; anim_phase = (anim_phase + 1) % 10;
        anim_spinner_idx = (anim_phase < 6) ? anim_phase : (10 - anim_phase);
        static char b[80]; snprintf(b, 80, "%s %s\xE2\x80\xA6", spinner_frames[anim_spinner_idx], anim_messages[anim_msg_idx]);
        lv_label_set_text(lbl_anim, b);
    }
}

void ui_update_ble_status(ble_state_t s, const char* n, const char* m) {}
void ui_update_battery(int p, bool c) {
    int idx = (c) ? 4 : ((p < 0 || p <= 10) ? 0 : (p <= 35 ? 1 : (p <= 75 ? 2 : 3)));
    lv_image_set_src(battery_img, &battery_dscs[idx]);
}
screen_t ui_get_current_screen(void) { return current_screen; }
