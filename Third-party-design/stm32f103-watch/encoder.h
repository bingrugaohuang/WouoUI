/**
 * @file    encoder.h
 * @brief   EC11 旋转编码器驱动头文件
 *
 * 适配平台：STM32F103C8T6 + HAL 库
 * 功能：
 *   - 通过 EXTI 中断检测旋转方向（A 相触发中断，B 相判断方向）
 *   - 通过 TIM4 定时中断（5ms）轮询按键状态，带软件消抖
 *   - 提供与 WouoUI ui_main.c 兼容的按键信号接口
 *
 * 引脚配置（需与 CubeMX 生成代码中的标签一致）：
 *   - ENC_A_Pin / ENC_A_GPIO_Port：编码器 A 相（外部中断，PA0）
 *   - ENC_B_Pin / ENC_B_GPIO_Port：编码器 B 相（普通输入，PA1）
 *   - ENC_SW_Pin / ENC_SW_GPIO_Port：编码器按键（普通输入，PA2）
 */

#ifndef __ENCODER_H
#define __ENCODER_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

/* ============================== 用户配置 ============================== */

/* 编码器引脚：需与 CubeMX 生成的 main.h 中的宏定义名称一致
 * 如果 CubeMX 中设置了 User Label，则宏名为 "标签_Pin" 和 "标签_GPIO_Port" */
#define ENC_A_Pin               GPIO_PIN_0
#define ENC_A_GPIO_Port         GPIOA

#define ENC_B_Pin               GPIO_PIN_1
#define ENC_B_GPIO_Port         GPIOA

#define ENC_SW_Pin              GPIO_PIN_2
#define ENC_SW_GPIO_Port        GPIOA

/* 按键参数（单位：TIM4 中断周期，即 5ms）
 * 短按检测：松开前按下时长 < BTN_SHORT_PRESS_TICKS
 * 长按检测：按下时长 >= BTN_LONG_PRESS_TICKS */
#define BTN_SHORT_PRESS_TICKS   6     /* 6 × 5ms = 30ms，消抖 */
#define BTN_LONG_PRESS_TICKS    60    /* 60 × 5ms = 300ms，长按阈值 */

/* ====================================================================== */

/* 按键 ID，与 WouoUI ui_main.h 中的 BTN_ID_* 定义保持一致 */
#define BTN_ID_CC    0    /* 逆时针旋转（Counter-Clockwise） */
#define BTN_ID_CW    1    /* 顺时针旋转（Clockwise） */
#define BTN_ID_SP    2    /* 短按（Short Press） */
#define BTN_ID_LP    3    /* 长按（Long Press） */

/**
 * @brief 编码器状态结构体
 *        由驱动内部维护，通过 encoder_get_event() 读取事件
 */
typedef struct
{
    volatile bool    pressed;   /* 有新事件待处理 */
    volatile uint8_t id;        /* 事件类型：BTN_ID_CC / CW / SP / LP */
} Encoder_Event_t;

/* 全局编码器事件（extern，供 ui_main.c 访问） */
extern Encoder_Event_t encoder_event;

/**
 * @brief 初始化编码器
 *        必须在 HAL_Init() 和 SystemClock_Config() 之后、
 *        MX_GPIO_Init() 和 MX_TIM4_Init() 之后调用
 */
void encoder_init(void);

/**
 * @brief 在 EXTI0 中断处理函数中调用（检测旋转方向）
 *        在 stm32f1xx_it.c 的 EXTI0_IRQHandler() 中调用
 */
void encoder_exti_handler(void);

/**
 * @brief 在 TIM4 中断处理函数中调用（按键扫描，5ms 周期）
 *        在 stm32f1xx_it.c 的 TIM4_IRQHandler() 中调用
 */
void encoder_tim_handler(void);

/**
 * @brief 检查是否有新的编码器事件
 * @return true  有新事件（pressed 标志为真）
 *         false 无新事件
 */
bool encoder_has_event(void);

/**
 * @brief 清除已处理的事件标志
 *        在 UI 逻辑处理完事件后调用
 */
void encoder_clear_event(void);

#endif /* __ENCODER_H */
