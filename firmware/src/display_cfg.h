#pragma once

#include <Arduino_GFX_Library.h>
#include <Wire.h>

#ifdef JC3248W535
// ---- Guition JC3248W535 (3.5" 480x320 IPS) ----
#define LCD_WIDTH   480
#define LCD_HEIGHT  320

// QSPI display pins
#define LCD_CS      45
#define LCD_SCLK    47
#define LCD_SDIO0   21
#define LCD_SDIO1   48
#define LCD_SDIO2   40
#define LCD_SDIO3   39
#define LCD_RESET   -1 // Internal
#define LCD_BL      1

// Touch pins (AXS15231 via I2C)
#define IIC_SDA     4
#define IIC_SCL     8
#define TP_INT      3
#define TP_RST      12

// No dedicated PMU or IMU on this board
#else
// ---- Waveshare ESP32-S3-Touch-AMOLED-2.16 (480x480) ----
#include <TouchDrvCSTXXX.hpp>
#include <XPowersLib.h>
#include <SensorQMI8658.hpp>

#define LCD_WIDTH   480
#define LCD_HEIGHT  480

// QSPI display pins (CO5300)
#define LCD_CS      12
#define LCD_SCLK    38
#define LCD_SDIO0   4
#define LCD_SDIO1   5
#define LCD_SDIO2   6
#define LCD_SDIO3   7
#define LCD_RESET   2

// Touch pins (CST9220 via I2C)
#define IIC_SDA     15
#define IIC_SCL     14
#define TP_INT      11
#define TP_RST      2    // shared with LCD_RESET
#define CST9220_ADDR 0x5A

// PMU (AXP2101 via same I2C)
#define AXP2101_ADDR 0x34
#endif

// ---- Global hardware objects (defined in main.cpp) ----
extern Arduino_DataBus *bus;
extern Arduino_GFX *gfx;

#ifndef JC3248W535
extern TouchDrvCST92xx touch;
extern XPowersPMU pmu;
extern SensorQMI8658 imu;
#endif
