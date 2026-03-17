/**
 * @file    oled_driver.h
 * @brief   OLED 显示屏驱动头文件（U8g2 I2C 封装）
 *
 * 适配平台：STM32F103C8T6 + HAL 库
 * 显示屏：  SSD1306 / SH1106，128x64 或 128x32，I2C 接口
 *
 * 移植自 WouoUI-Lite_General Arduino 版本
 */

#ifndef __OLED_DRIVER_H
#define __OLED_DRIVER_H

#include "main.h"
#include "i2c.h"
#include "u8g2.h"
#include "u8x8.h"

/* ============================== 用户配置 ============================== */

/* OLED I2C 7位地址：SSD1306 通常为 0x3C（SA0=0）或 0x3D（SA0=1）
 * 注意：HAL_I2C_Master_Transmit 需要左移一位（即 8位地址），
 *       这里存储 8位格式：0x3C<<1 = 0x78，0x3D<<1 = 0x7A */
#define OLED_I2C_ADDRESS    0x78U

/* 屏幕分辨率选择：取消注释对应型号 */
#define OLED_USE_SSD1306_128X64     /* SSD1306 128x64（最常见） */
/* #define OLED_USE_SSD1306_128X32 */   /* SSD1306 128x32 */
/* #define OLED_USE_SH1106_128X64  */   /* SH1106  128x64 */

/* I2C 句柄：与 CubeMX 生成的 i2c.c 保持一致 */
#define OLED_I2C_HANDLE     hi2c1

/* ====================================================================== */

/* 全局 U8g2 实例（在 oled_driver.c 中定义，对外提供） */
extern u8g2_t u8g2;

/**
 * @brief 初始化 OLED 显示屏（U8g2 + I2C）
 *        需在 main() 中外设初始化完成后调用
 * @param contrast 初始亮度（0~255）
 */
void oled_init(uint8_t contrast);

/**
 * @brief U8g2 I2C 字节传输回调（硬件 I2C，HAL 实现）
 *        由 U8g2 内部调用，不需要用户直接调用
 */
uint8_t u8x8_byte_stm32_hw_i2c(u8x8_t *u8x8, uint8_t msg,
                                 uint8_t arg_int, void *arg_ptr);

/**
 * @brief U8g2 GPIO 和延时回调
 *        由 U8g2 内部调用，不需要用户直接调用
 */
uint8_t u8x8_gpio_and_delay_stm32(u8x8_t *u8x8, uint8_t msg,
                                    uint8_t arg_int, void *arg_ptr);

#endif /* __OLED_DRIVER_H */
