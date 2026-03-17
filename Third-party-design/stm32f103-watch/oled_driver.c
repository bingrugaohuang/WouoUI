/**
 * @file    oled_driver.c
 * @brief   OLED 显示屏驱动实现（U8g2 I2C 封装）
 *
 * 适配平台：STM32F103C8T6 + HAL 库
 * 显示屏：  SSD1306 / SH1106，128x64 或 128x32，I2C 接口
 *
 * 移植自 WouoUI-Lite_General Arduino 版本
 */

#include "oled_driver.h"

/* U8g2 全局实例 */
u8g2_t u8g2;

/**
 * @brief U8g2 I2C 字节传输回调（硬件 I2C，HAL 实现）
 *
 * U8g2 通过此回调将一帧数据通过 I2C 发送给 OLED。
 * 内部使用静态缓冲区积累数据，在 END_TRANSFER 时一次性发送，
 * 比逐字节发送效率更高。
 */
uint8_t u8x8_byte_stm32_hw_i2c(u8x8_t *u8x8, uint8_t msg,
                                 uint8_t arg_int, void *arg_ptr)
{
    /* U8g2 单次传输不超过 32 字节，但全帧缓冲可以更大 */
    static uint8_t buffer[256];
    static uint8_t buf_idx;
    uint8_t *data;

    switch (msg)
    {
        case U8X8_MSG_BYTE_INIT:
            /* I2C 外设已由 CubeMX 初始化，此处无需重复初始化 */
            break;

        case U8X8_MSG_BYTE_START_TRANSFER:
            buf_idx = 0;
            break;

        case U8X8_MSG_BYTE_SEND:
            data = (uint8_t *)arg_ptr;
            while (arg_int > 0)
            {
                buffer[buf_idx++] = *data;
                data++;
                arg_int--;
            }
            break;

        case U8X8_MSG_BYTE_END_TRANSFER:
            /* 一次性通过 HAL I2C 发送整个缓冲区 */
            if (HAL_I2C_Master_Transmit(&OLED_I2C_HANDLE,
                                         OLED_I2C_ADDRESS,
                                         buffer, buf_idx,
                                         1000) != HAL_OK)
            {
                return 0;
            }
            break;

        case U8X8_MSG_BYTE_SET_DC:
            /* I2C 接口无需 DC 信号 */
            break;

        default:
            return 0;
    }
    return 1;
}

/**
 * @brief U8g2 GPIO 和延时回调
 *
 * 提供 U8g2 所需的延时函数，I2C 接口无需 GPIO 操作。
 */
uint8_t u8x8_gpio_and_delay_stm32(u8x8_t *u8x8, uint8_t msg,
                                    uint8_t arg_int, void *arg_ptr)
{
    (void)u8x8;
    (void)arg_ptr;

    switch (msg)
    {
        case U8X8_MSG_DELAY_100NANO:
            __NOP();
            break;

        case U8X8_MSG_DELAY_10MICRO:
            /* 约 10us 延时（72MHz 下约 720 个 NOP） */
            for (uint16_t n = 0; n < 720; n++)
            {
                __NOP();
            }
            break;

        case U8X8_MSG_DELAY_MILLI:
            HAL_Delay(arg_int);
            break;

        case U8X8_MSG_DELAY_I2C:
            /* I2C 接口专用：约 5us 延时 */
            for (uint16_t n = 0; n < 360; n++)
            {
                __NOP();
            }
            break;

        case U8X8_MSG_GPIO_I2C_CLOCK:
        case U8X8_MSG_GPIO_I2C_DATA:
            /* 硬件 I2C 无需手动控制 GPIO */
            break;

        default:
            u8x8_SetGPIOResult(u8x8, 1);
            break;
    }
    return 1;
}

/**
 * @brief 初始化 OLED 显示屏
 * @param contrast 初始对比度（亮度），范围 0~255
 */
void oled_init(uint8_t contrast)
{
    /* 根据宏选择对应的 U8g2 Setup 函数 */
#if defined(OLED_USE_SSD1306_128X64)
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0,
        u8x8_byte_stm32_hw_i2c,
        u8x8_gpio_and_delay_stm32);

#elif defined(OLED_USE_SSD1306_128X32)
    u8g2_Setup_ssd1306_i2c_128x32_univision_f(&u8g2, U8G2_R0,
        u8x8_byte_stm32_hw_i2c,
        u8x8_gpio_and_delay_stm32);

#elif defined(OLED_USE_SH1106_128X64)
    u8g2_Setup_sh1106_i2c_128x64_noname_f(&u8g2, U8G2_R0,
        u8x8_byte_stm32_hw_i2c,
        u8x8_gpio_and_delay_stm32);
#endif

    u8g2_InitDisplay(&u8g2);               /* 发送初始化序列 */
    u8g2_SetPowerSave(&u8g2, 0);           /* 退出省电模式（开启显示） */
    u8g2_SetContrast(&u8g2, contrast);     /* 设置初始亮度 */
    u8g2_ClearDisplay(&u8g2);             /* 清屏 */
}
