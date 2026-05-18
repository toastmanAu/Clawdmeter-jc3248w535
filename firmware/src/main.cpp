#include <Arduino.h>
#include <lvgl.h>
#include <ArduinoJson.h>
#include "display_cfg.h"
#include "data.h"
#include "ui.h"
#include "ble.h"
#include "power.h"
#include "imu.h"
#include "splash.h"
#include "usage_rate.h"

#ifdef JC3248W535
// Verified stable 20MHz Portrait Driver
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3, 20000000L);

// Native Portrait (320x480). IPS=false (Verified correct color).
Arduino_GFX *gfx = new Arduino_AXS15231B(
    bus, GFX_NOT_DEFINED, 0 /* rotation 0 */, false /* IPS */,
    320, 480, 0, 0, 0, 0);
#else
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_CO5300 *gfx_co5300 = new Arduino_CO5300(bus, LCD_RESET, 0, LCD_WIDTH, LCD_HEIGHT, 0, 0, 0, 0);
Arduino_GFX *gfx = gfx_co5300;
TouchDrvCST92xx touch;
XPowersPMU pmu;
SensorQMI8658 imu;
#endif

static UsageData usage = {};
static volatile bool     touch_pressed = false;
static volatile uint16_t touch_x = 0;
static volatile uint16_t touch_y = 0;
static volatile bool     touch_data_ready = false;

static void IRAM_ATTR touch_isr(void) {
    touch_data_ready = true;
}

#ifdef JC3248W535
static void touch_read() {
    static uint32_t last_poll = 0;
    if (millis() - last_poll < 20) return;
    last_poll = millis();

    static const uint8_t magic[] = {0xb5, 0xab, 0xa5, 0x5a, 0x00, 0x00, 0x00, 0x08};
    Wire.beginTransmission(0x3B);
    Wire.write(magic, 8);
    Wire.endTransmission();
    delayMicroseconds(500);

    if (Wire.requestFrom(0x3B, (uint8_t)8) == 8) {
        uint8_t data[8];
        for (int i = 0; i < 8; i++) data[i] = Wire.read();
        if (data[0] == 0x00 && data[1] > 0) {
            uint8_t event = data[2] >> 4;
            if (event == 4) { touch_pressed = false; }
            else {
                touch_pressed = true;
                uint16_t tx = ((uint16_t)(data[2] & 0x0F) << 8) | data[3];
                uint16_t ty = ((uint16_t)(data[4] & 0x0F) << 8) | data[5];
                touch_x = (ty < 480) ? (479 - ty) : 0;
                touch_y = tx;
            }
        } else { touch_pressed = false; }
    }
}
#endif

static uint16_t *buf1 = nullptr;
static uint16_t *rot_buf = nullptr;

static void my_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    int32_t w = area->x2 - area->x1 + 1;
    int32_t h = area->y2 - area->y1 + 1;
    uint16_t *src = (uint16_t*)px_map;

    // Manual Pixel Rotation: 270 deg clockwise (Rotation 3)
    // When in FULL mode, area is always 0,0 to 479,319
    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            // Map 480x320 landscape buffer to 320x480 portrait hardware
            // dx = y
            // dy = 479 - x
            rot_buf[(479 - x) * 320 + y] = src[y * 480 + x];
        }
    }
    
    gfx->draw16bitRGBBitmap(0, 0, rot_buf, 320, 480);
    gfx->flush();
    lv_display_flush_ready(disp);
}

static void rounder_cb(lv_event_t* e) {
    lv_area_t *area = (lv_area_t*)lv_event_get_param(e);
    area->x1 &= ~7; 
    area->y1 &= ~7;
    area->x2 = (area->x2 | 7);
    if (area->x2 >= 480) area->x2 = 479;
    area->y2 = (area->y2 | 7);
    if (area->y2 >= 320) area->y2 = 319;
}

static char cmd_buf[64];
static int cmd_pos = 0;

static void send_screenshot() {
    const uint32_t w = 480, h = 320;
    const uint32_t buf_size = w * h * 2;
    uint8_t* sbuf = (uint8_t*)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    if (!sbuf) return;
    lv_draw_buf_t draw_buf;
    lv_draw_buf_init(&draw_buf, w, h, LV_COLOR_FORMAT_RGB565, w * 2, sbuf, buf_size);
    if (lv_snapshot_take_to_draw_buf(lv_screen_active(), LV_COLOR_FORMAT_RGB565, &draw_buf) == LV_RESULT_OK) {
        Serial.printf("SCREENSHOT_START %lu %lu %lu\n", (unsigned long)w, (unsigned long)h, (unsigned long)buf_size);
        Serial.flush();
        Serial.write(sbuf, buf_size);
        Serial.flush();
        Serial.println("\nSCREENSHOT_END");
    }
    heap_caps_free(sbuf);
}

static void check_serial_cmd() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            cmd_buf[cmd_pos] = '\0';
            if (strcmp(cmd_buf, "screenshot") == 0) send_screenshot();
            cmd_pos = 0;
        } else if (cmd_pos < 63) cmd_buf[cmd_pos++] = c;
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n--- Clawdmeter: Final UI Fix Build ---");

    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, HIGH);

    pinMode(12, OUTPUT); digitalWrite(12, LOW);
    delay(150);
    digitalWrite(12, HIGH);
    delay(200);

    Wire.begin(IIC_SDA, IIC_SCL);

    if (!gfx->begin()) Serial.println("GFX Begin Failed");
    gfx->fillScreen(0x0000);
    gfx->flush();

    lv_init();
    lv_tick_set_cb([](){ return (uint32_t)millis(); });

    buf1 = (uint16_t*)heap_caps_malloc(480 * 320 * 2, MALLOC_CAP_SPIRAM);
    rot_buf = (uint16_t*)heap_caps_malloc(480 * 320 * 2, MALLOC_CAP_SPIRAM);
    
    lv_display_t* disp = lv_display_create(480, 320);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, my_flush_cb);
    lv_display_set_buffers(disp, buf1, nullptr, 480 * 320 * 2, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_add_event_cb(disp, rounder_cb, LV_EVENT_INVALIDATE_AREA, NULL);

    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, [](lv_indev_t* indev, lv_indev_data_t* data) {
        if (touch_pressed) {
            data->point.x = touch_x; data->point.y = touch_y;
            data->state = LV_INDEV_STATE_PRESSED;
        } else data->state = LV_INDEV_STATE_RELEASED;
    });

    ble_init();
    ui_init();
    
    ui_update_ble_status(ble_get_state(), ble_get_device_name(), ble_get_mac_address());
    ui_update_battery(power_battery_pct(), power_is_charging());

    ui_show_screen(SCREEN_USAGE);
    Serial.println("System Ready.");
}

void loop() {
    check_serial_cmd();
    touch_read();
    lv_timer_handler();
    ui_tick_anim();
    ble_tick();
    power_tick();
    imu_tick();
    splash_tick();

    ble_state_t bs = ble_get_state();
    static ble_state_t last_bs = BLE_STATE_INIT;
    if (bs != last_bs) {
        last_bs = bs;
        ui_update_ble_status(bs, ble_get_device_name(), ble_get_mac_address());
    }

    int pct = power_battery_pct();
    static int last_pct = -2;
    if (pct != last_pct) {
        last_pct = pct;
        ui_update_battery(pct, power_is_charging());
    }

    if (ble_has_data()) {
        JsonDocument doc;
        if (deserializeJson(doc, ble_get_data()) == DeserializationError::Ok) {
            usage.session_pct = doc["s"] | 0.0f;
            usage.session_reset_mins = doc["sr"] | -1;
            usage.weekly_pct = doc["w"] | 0.0f;
            usage.weekly_reset_mins = doc["wr"] | -1;
            strlcpy(usage.status, doc["st"] | "unknown", sizeof(usage.status));
            usage.ok = doc["ok"] | false;
            usage.valid = true;
            ui_update(&usage);
            ble_send_ack();
        } else ble_send_nack();
    }
    delay(5);
}
