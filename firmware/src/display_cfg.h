// ---- Guition JC3248W535 (3.5" 480x320 IPS) ----
#pragma once
#include <Arduino_GFX_Library.h>
#include <Wire.h>

#ifdef JC3248W535
#define LCD_WIDTH   480
#define LCD_HEIGHT  320
#define LCD_CS      45
#define LCD_SCLK    47
#define LCD_SDIO0   21
#define LCD_SDIO1   48
#define LCD_SDIO2   40
#define LCD_SDIO3   39
#define LCD_RESET   12
#define LCD_BL      1
#define IIC_SDA     4
#define IIC_SCL     8
#define TP_INT      11
#define TP_RST      12
#else
// ---- Waveshare ESP32-S3-Touch-AMOLED-2.16 (480x480) ----
#include <TouchDrvCSTXXX.hpp>
#include <XPowersLib.h>
#include <SensorQMI8658.hpp>
#define LCD_WIDTH   480
#define LCD_HEIGHT  480
#define LCD_CS      12
#define LCD_SCLK    38
#define LCD_SDIO0   4
#define LCD_SDIO1   5
#define LCD_SDIO2   6
#define LCD_SDIO3   7
#define LCD_RESET   2
#define IIC_SDA     15
#define IIC_SCL     14
#define TP_INT      11
#define TP_RST      2
#define CST9220_ADDR 0x5A
#define AXP2101_ADDR 0x34
#endif

extern Arduino_DataBus *bus;
extern Arduino_GFX *gfx;
#ifndef JC3248W535
extern TouchDrvCST92xx touch;
extern XPowersPMU pmu;
extern SensorQMI8658 imu;
#endif
