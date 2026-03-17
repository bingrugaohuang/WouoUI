/**
 * @file    ui_main.h
 * @brief   WouoUI Lite_General STM32 移植版头文件
 *
 * 适配平台：STM32F103C8T6 + HAL 库
 * 显示屏：  SSD1306 / SH1106，128x64，I2C 接口
 * 输入：    EC11 旋转编码器（旋转 + 按键）
 *
 * 移植自 WouoUI-Lite_General v2.3（Arduino .ino 版本）
 * 原作者：RQNG（B 站 UID：9182439）
 *
 * 移植说明：
 *   - 将 C++ .ino 文件转为标准 C
 *   - U8g2 C++ API 替换为 C API（u8g2_DrawBox 等）
 *   - Arduino delay()/EEPROM/USB HID 替换为 STM32 HAL 等效实现
 *   - 删除 USB HID 功能（手表项目不需要）
 */

#ifndef __UI_MAIN_H
#define __UI_MAIN_H

#include "main.h"
#include "oled_driver.h"
#include "encoder.h"
#include "flash_storage.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ============================== 用户配置 ============================== */

/* 屏幕分辨率 */
#define DISP_H              64
#define DISP_W              128

/* 列表参数（超窄行高度，适合手表小屏幕）
 * 如需更大行高，取消下面默认参数的注释，注释掉窄行参数 */

/* 默认参数（行高 16px，字体高度 8px）
#define LIST_FONT           u8g2_font_HelvetiPixel_tr
#define LIST_TEXT_H         8
#define LIST_LINE_H         16
#define LIST_TEXT_S         4
#define LIST_BAR_W          5
#define LIST_BOX_R_X10      5    // 圆角半径 × 10，实际为 0.5
*/

/* 超窄行高度（行高 7px，字体高度 5px，在 64px 屏幕上可显示 9 行） */
#define LIST_FONT           u8g2_font_4x6_tr
#define LIST_TEXT_H         5
#define LIST_LINE_H         7
#define LIST_TEXT_S         1
#define LIST_BAR_W          7
#define LIST_BOX_R_X10      5   /* 列表选择框圆角半径，实际值 = 此宏 / 10 = 0.5px */

/* 多选框参数（适配超窄行高度） */
#define CHECK_BOX_L_S       99
#define CHECK_BOX_U_S       0
#define CHECK_BOX_F_W       5
#define CHECK_BOX_F_H       5
#define CHECK_BOX_D_S       1

/* 弹窗参数 */
#define WIN_FONT            u8g2_font_HelvetiPixel_tr
#define WIN_H               32
#define WIN_W               102
#define WIN_BAR_W           92
#define WIN_BAR_H           7

/* UI 结构参数 */
#define UI_DEPTH            20    /* 最大菜单层级深度 */
#define UI_MNUMB            100   /* 最大菜单数量 */
#define UI_PARAM            11    /* 可调参数数量 */

/* ============================== 菜单枚举 ============================== */

/* 页面标签（缩进表示层级关系） */
enum UI_PAGE_e
{
    M_WINDOW,       /* 弹窗（虚拟页面，覆盖在其他页面上） */
    M_SLEEP,        /* 睡眠模式 */
        M_MAIN,         /* 主菜单 */
            M_EDITOR,       /* 编辑器 */
                M_KNOB,         /* 旋钮设置 */
                    M_KRF,          /* 旋钮旋转功能 */
                    /* M_KPF 旋钮点按功能已移除：手表项目无需 USB HID 键盘 */
            M_SETTING,      /* 设置 */
                M_ABOUT,        /* 关于本机 */
};

/* UI 状态标签 */
enum UI_STATE_e
{
    S_FADE,         /* 转场动画 */
    S_WINDOW,       /* 弹窗初始化 */
    S_LAYER_IN,     /* 进入更深层级 */
    S_LAYER_OUT,    /* 返回更浅层级 */
    S_NONE,         /* 正常显示 */
};

/* 参数索引 */
enum UI_PARAM_e
{
    DISP_BRI,       /* 屏幕亮度（0~255） */
    LIST_ANI,       /* 列表动画速度（10~100） */
    WIN_ANI,        /* 弹窗动画速度（10~100） */
    FADE_ANI,       /* 消失动画延迟（ms） */
    BTN_SPT,        /* 按键短按时长阈值 */
    BTN_LPT,        /* 按键长按时长阈值 */
    LIST_UFD,       /* 列表从头展开开关 */
    LIST_LOOP,      /* 列表循环模式开关 */
    WIN_BOK,        /* 弹窗背景虚化开关 */
    KNOB_DIR,       /* 旋钮方向反转开关 */
    DARK_MODE,      /* 黑暗模式开关 */
};

/* ============================== 菜单结构 ============================== */

typedef struct
{
    const char *m_select;
} MENU;

/* ============================== 对外接口 ============================== */

/**
 * @brief WouoUI 初始化（对应 Arduino 的 setup()）
 *        调用顺序：encoder_init() → ui_setup()
 */
void ui_setup(void);

/**
 * @brief WouoUI 主循环（对应 Arduino 的 loop()）
 *        需在 main() 的 while(1) 中持续调用
 */
void ui_loop(void);

#endif /* __UI_MAIN_H */
