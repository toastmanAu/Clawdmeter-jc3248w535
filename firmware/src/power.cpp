#include "power.h"
#include "display_cfg.h"
#include <Arduino.h>

#ifndef JC3248W535
// Poll intervals
#define BATTERY_POLL_MS   2000
#define CHARGING_POLL_MS  500

static int      cached_pct      = -1;
static bool     cached_charging = false;
static bool     pwr_pressed_flag = false;
static uint32_t last_battery_ms  = 0;
static uint32_t last_charging_ms = 0;
static uint32_t last_pwr_ms      = 0;
#define PWR_POLL_MS 50
#endif

void power_init(void) {
#ifdef JC3248W535
    // No PMU on this board
    Serial.println("Power: JC3248W535 stubs");
#else
    if (!pmu.begin(Wire, AXP2101_ADDR, IIC_SDA, IIC_SCL)) {
        Serial.println("AXP2101 init failed");
        return;
    }
    Serial.println("AXP2101 init OK");

    pmu.enableBattDetection();
    pmu.enableBattVoltageMeasure();

    // Enable PWR button short-press IRQ (mid button for cycling screens)
    pmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    pmu.clearIrqStatus();
    pmu.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ);

    cached_charging = pmu.isCharging();
    cached_pct = pmu.getBatteryPercent();
#endif
}

void power_tick(void) {
#ifndef JC3248W535
    uint32_t now = millis();

    if (now - last_charging_ms >= CHARGING_POLL_MS) {
        last_charging_ms = now;
        cached_charging = pmu.isCharging();
    }

    if (now - last_battery_ms >= BATTERY_POLL_MS) {
        last_battery_ms = now;
        cached_pct = pmu.getBatteryPercent();
    }

    // Poll PWR button (AXP2101 short-press IRQ)
    if (now - last_pwr_ms >= PWR_POLL_MS) {
        last_pwr_ms = now;
        pmu.getIrqStatus();
        if (pmu.isPekeyShortPressIrq()) {
            pwr_pressed_flag = true;
        }
        pmu.clearIrqStatus();
    }
#endif
}

int power_battery_pct(void) {
#ifdef JC3248W535
    return -1;
#else
    return cached_pct;
#endif
}

bool power_is_charging(void) {
#ifdef JC3248W535
    return false;
#else
    return cached_charging;
#endif
}

bool power_pwr_pressed(void) {
#ifdef JC3248W535
    return false;
#else
    if (pwr_pressed_flag) {
        pwr_pressed_flag = false;
        return true;
    }
    return false;
#endif
}
