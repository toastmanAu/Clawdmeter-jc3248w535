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
static uint8_t           touch_addr = 0x3B;

#ifdef JC3248W535
static void touch_read() {
    static uint32_t last_poll = 0;
    if (millis() - last_poll < 20) return;
    last_poll = millis();

    // AXS15231B Magic Command to request data
    static const uint8_t magic[] = {0xb5, 0xab, 0xa5, 0x5a, 0x00, 0x00, 0x00, 0x08};
    
    Wire.beginTransmission(touch_addr);
    Wire.write(magic, 8);
    Wire.endTransmission();
    delayMicroseconds(500); // Small pause for chip processing

    if (Wire.requestFrom(touch_addr, (uint8_t)8) == 8) {
        uint8_t data[8];
        for (int i = 0; i < 8; i++) data[i] = Wire.read();

        // Parse AXS15231B based on raw HEX dump
        if (data[0] == 0x00 && data[1] > 0) {
            uint8_t event = data[2] >> 4;
            if (event == 4) { 
                // 0x40 = Release Event
                touch_pressed = false;
            } else {
                // 0x00 = Down, 0x80 = Contact
                touch_pressed = true;
                
                // Raw values are ALREADY in Native Portrait Pixels (0-319, 0-479)
                uint16_t tx = ((uint16_t)(data[2] & 0x0F) << 8) | data[3];
                uint16_t ty = ((uint16_t)(data[4] & 0x0F) << 8) | data[5];
                
                // Map Native Portrait to Manual Rotation 3 Landscape
                touch_x = (ty < 480) ? (479 - ty) : 0;
                touch_y = tx;
            }
        } else {
            touch_pressed = false;
        }
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
    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            int32_t dx = y;
            int32_t dy = 480 - x - 1;
            rot_buf[dy * 320 + dx] = src[y * w + x];
        }
    }
    
    gfx->draw16bitRGBBitmap(0, 0, rot_buf, 320, 480);
    gfx->flush();
    lv_display_flush_ready(disp);
}

static void rounder_cb(lv_event_t* e) {
    lv_area_t *area = (lv_area_t*)lv_event_get_param(e);
    area->x1 &= ~7; area->y1 &= ~7;
    area->x2 = (area->x2 + 8) & ~7;
    area->y2 = (area->y2 + 8) & ~7;
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n--- Clawdmeter: Raw Touch Packet Sniffer ---");

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
    ui_show_screen(SCREEN_USAGE);
    Serial.println("System Ready.");
}

void loop() {
    touch_read();
    lv_timer_handler();
    ui_tick_anim();
    ble_tick();

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
