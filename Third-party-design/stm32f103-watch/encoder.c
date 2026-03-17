/**
 * @file    encoder.c
 * @brief   EC11 旋转编码器驱动实现
 *
 * 适配平台：STM32F103C8T6 + HAL 库
 *
 * 旋转检测原理：
 *   EC11 旋转时，A 相产生脉冲，B 相的电平决定旋转方向：
 *   - A 相下降沿时，B 相为高 → 顺时针（CW）
 *   - A 相下降沿时，B 相为低 → 逆时针（CC）
 *   （实际实现中，记录 A 相下降沿时的 B 状态，在 A 相上升沿时确认）
 *
 * 按键检测原理：
 *   通过 TIM4 每 5ms 轮询一次按键状态，计数连续按下时间，
 *   按键释放后根据计数时长判断短按或长按。
 */

#include "encoder.h"
#include "tim.h"

/* 全局编码器事件 */
Encoder_Event_t encoder_event = { .pressed = false, .id = 0 };

/* 内部状态变量 */
static struct
{
    /* 旋转检测 */
    bool a_prev;        /* A 相上次状态（用于边沿检测） */
    bool b_at_fall;     /* A 相下降沿时 B 相的电平 */
    bool rot_pending;   /* 是否有旋转事件待确认 */

    /* 按键检测 */
    bool  sw_prev;      /* 按键上次状态 */
    bool  sw_pressing;  /* 按键正在被按下 */
    int32_t sw_count;   /* 按键按下持续时间计数（单位：TIM4 中断周期） */
} enc_state;

/**
 * @brief 初始化编码器
 */
void encoder_init(void)
{
    enc_state.a_prev      = (HAL_GPIO_ReadPin(ENC_A_GPIO_Port, ENC_A_Pin) == GPIO_PIN_SET);
    enc_state.b_at_fall   = false;
    enc_state.rot_pending = false;
    enc_state.sw_prev     = (HAL_GPIO_ReadPin(ENC_SW_GPIO_Port, ENC_SW_Pin) == GPIO_PIN_SET);
    enc_state.sw_pressing = false;
    enc_state.sw_count    = 0;

    encoder_event.pressed = false;
    encoder_event.id      = 0;

    /* 启动 TIM4，使能更新中断 */
    HAL_TIM_Base_Start_IT(&htim4);
}

/**
 * @brief EXTI0 中断回调（A 相边沿触发）
 *
 * 在 A 相任意边沿触发：
 *   - 下降沿：记录 B 相当前电平
 *   - 上升沿：根据之前记录的 B 相电平判断方向，发出事件
 */
void encoder_exti_handler(void)
{
    bool a_now = (HAL_GPIO_ReadPin(ENC_A_GPIO_Port, ENC_A_Pin) == GPIO_PIN_SET);
    bool b_now = (HAL_GPIO_ReadPin(ENC_B_GPIO_Port, ENC_B_Pin) == GPIO_PIN_SET);

    if (enc_state.a_prev && !a_now)
    {
        /* A 相下降沿：记录 B 相状态 */
        enc_state.b_at_fall   = b_now;
        enc_state.rot_pending = true;
    }
    else if (!enc_state.a_prev && a_now && enc_state.rot_pending)
    {
        /* A 相上升沿且有待处理旋转事件：确认方向 */
        enc_state.rot_pending = false;

        /* 仅在无其他未处理事件时更新（避免覆盖未处理的按键事件） */
        if (!encoder_event.pressed)
        {
            if (enc_state.b_at_fall)
            {
                /* B 为高 → 顺时针 */
                encoder_event.id      = BTN_ID_CW;
                encoder_event.pressed = true;
            }
            else
            {
                /* B 为低 → 逆时针 */
                encoder_event.id      = BTN_ID_CC;
                encoder_event.pressed = true;
            }
        }
    }

    enc_state.a_prev = a_now;
}

/**
 * @brief TIM4 中断回调（5ms 周期，按键扫描）
 *
 * 采用状态机方式：
 *   - 检测到按键按下后开始计数
 *   - 按键释放时，根据计数时长判断短按 / 长按
 *   - 计数超过长按阈值后，立即发出长按事件（无需等待释放）
 */
void encoder_tim_handler(void)
{
    bool sw_now = (HAL_GPIO_ReadPin(ENC_SW_GPIO_Port, ENC_SW_Pin) == GPIO_PIN_RESET);
    /* 低电平有效（内部上拉，按下接 GND） */

    if (!enc_state.sw_pressing && sw_now)
    {
        /* 按键刚按下，开始计数 */
        enc_state.sw_pressing = true;
        enc_state.sw_count    = 0;
    }
    else if (enc_state.sw_pressing)
    {
        if (sw_now)
        {
            enc_state.sw_count++;

            /* 达到长按阈值，立即发出长按事件 */
            if (enc_state.sw_count >= BTN_LONG_PRESS_TICKS && !encoder_event.pressed)
            {
                encoder_event.id      = BTN_ID_LP;
                encoder_event.pressed = true;
                /* 继续等待释放，避免重复触发长按 */
                enc_state.sw_count    = BTN_LONG_PRESS_TICKS; /* 钳位，不再增加 */
            }
        }
        else
        {
            /* 按键释放 */
            if (enc_state.sw_count >= BTN_SHORT_PRESS_TICKS
                && enc_state.sw_count < BTN_LONG_PRESS_TICKS
                && !encoder_event.pressed)
            {
                /* 有效短按（消抖后、长按前释放） */
                encoder_event.id      = BTN_ID_SP;
                encoder_event.pressed = true;
            }
            enc_state.sw_pressing = false;
            enc_state.sw_count    = 0;
        }
    }

    enc_state.sw_prev = sw_now;
}

/**
 * @brief 检查是否有新的编码器事件
 */
bool encoder_has_event(void)
{
    return encoder_event.pressed;
}

/**
 * @brief 清除事件标志
 */
void encoder_clear_event(void)
{
    encoder_event.pressed = false;
}
