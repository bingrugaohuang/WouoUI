/**
 * @file    ui_main.c
 * @brief   WouoUI Lite_General STM32 移植版主实现
 *
 * 适配平台：STM32F103C8T6 + HAL 库
 *
 * 移植自 WouoUI-Lite_General v2.3（Arduino .ino 版本）
 * 原作者：RQNG（B 站 UID：9182439）
 *
 * 主要改动：
 *   1. 所有 u8g2.xxx() C++ 调用改为 u8g2_Xxx(&u8g2, ...) C 调用
 *   2. delay() → HAL_Delay()
 *   3. EEPROM.read/write → flash_storage_read/write
 *   4. 删除 USB HID 相关代码
 *   5. 浮点圆角半径改为整数（×10 再除以 10，避免 Keil C99 下的精度问题）
 */

#include "ui_main.h"

/* =====================================================================
 * 菜单内容定义
 * ===================================================================== */

static const MENU main_menu[] =
{
    {"[ Main Menu ]"},
    {"- Editor"},
    {"- Setting"},
};

static const MENU editor_menu[] =
{
    {"[ Editor ]"},
    {"- Function 0"},
    {"- Function 1"},
    {"- Function 2"},
    {"- Function 3"},
    {"- Function 4"},
    {"- Function 5"},
    {"- Function 6"},
    {"- Function 7"},
    {"- Function 8"},
    {"- Function 9"},
    {"- Knob"},
};

static const MENU knob_menu[] =
{
    {"[ Knob ]"},
    {"# Rotate Func"},
    /* 手表版已移除 Press Func（USB HID 功能，手表项目不需要） */
};

static const MENU krf_menu[] =
{
    {"[ Rotate Function ]"},
    {"--------------------------"},
    {"= Disable"},
    {"--------------------------"},
    {"= Volume"},
    {"= Brightness"},
    {"--------------------------"},
};

static const MENU setting_menu[] =
{
    {"[ Setting ]"},
    {"~ Disp Bri"},
    {"~ List Ani"},
    {"~ Win Ani"},
    {"~ Fade Ani"},
    {"~ Btn SPT"},
    {"~ Btn LPT"},
    {"+ L Ufd Fm Scr"},
    {"+ L Loop Mode"},
    {"+ Win Bokeh Bg"},
    {"+ Knob Rot Dir"},
    {"+ Dark Mode"},
    {"- [ About ]"},
};

static const MENU about_menu[] =
{
    {"[ WouoUI ]"},
    {"- Version: v2.3"},
    {"- Board: STM32F103"},
    {"- Ram: 20k"},
    {"- Flash: 64k"},
    {"- Freq: 72MHz"},
    {"- Creator: RQNG"},
    {"- Port: STM32"},
};

/* =====================================================================
 * 页面变量
 * ===================================================================== */

/* 屏幕缓冲区指针（指向 U8g2 内部显存） */
static uint8_t  *buf_ptr;
static uint16_t  buf_len;

/* UI 状态 */
static struct
{
    bool    init;
    uint8_t num[UI_MNUMB];
    uint8_t select[UI_DEPTH];
    uint8_t layer;
    uint8_t index;
    uint8_t state;
    bool    sleep;
    uint8_t fade;
    uint8_t param[UI_PARAM];
} ui;

/* 列表状态 */
static struct
{
    int16_t line_n;
    int16_t temp;
    bool    loop;
    float   y;
    float   y_trg;
    float   box_x;
    float   box_x_trg;
    float   box_y;
    float   box_y_trg[UI_DEPTH];
    float   bar_y;
    float   bar_y_trg;
} list;

/* 多选框 / 单选框辅助指针 */
static struct
{
    uint8_t *v;     /* 数值显示 */
    uint8_t *m;     /* 多选框 */
    uint8_t *s;     /* 单选框当前值 */
    uint8_t *s_p;   /* 单选框当前值在列表中的位置 */
} check_box;

/* 弹窗状态 */
static struct
{
    uint8_t   *value;
    uint8_t    max;
    uint8_t    min;
    uint8_t    step;
    const MENU *bg;
    uint8_t    index;
    char       title[20];
    uint8_t    select;
    uint8_t    l;       /* 弹窗左边距 */
    uint8_t    u;       /* 弹窗上边距 */
    float      bar;
    float      bar_trg;
    float      y;
    float      y_trg;
} win;

/* 旋钮功能参数 */
#define KNOB_PARAM      4
#define KNOB_DISABLE    0
static struct
{
    uint8_t param[KNOB_PARAM];
} knob;

enum KNOB_PARAM_e
{
    KNOB_ROT,       /* 旋转功能 */
    KNOB_COD,       /* 点按功能 */
    KNOB_ROT_P,     /* 旋转选项在列表中的位置 */
    KNOB_COD_P,     /* 点按选项在列表中的位置 */
};

/* Flash 存储数据缓冲区（序列化后的参数） */
#define FLASH_DATA_LEN  (UI_PARAM + KNOB_PARAM)
static uint8_t flash_data_buf[FLASH_DATA_LEN];

/* 参数是否已更改（需要在下次进入睡眠时写 Flash） */
static bool param_changed = false;

/* =====================================================================
 * 内部宏
 * ===================================================================== */

/* 弹窗出场 / 退场位置 */
#define WIN_Y_INIT      (-(WIN_H) - 2)
#define WIN_Y_TARGET    (-(WIN_H) - 2)

/* =====================================================================
 * Flash 参数存储
 * ===================================================================== */

static void ui_param_default(void)
{
    ui.param[DISP_BRI]  = 255;
    ui.param[LIST_ANI]  = 60;
    ui.param[WIN_ANI]   = 25;
    ui.param[FADE_ANI]  = 30;
    ui.param[BTN_SPT]   = 25;
    ui.param[BTN_LPT]   = 150;
    ui.param[LIST_UFD]  = 1;
    ui.param[LIST_LOOP] = 0;
    ui.param[WIN_BOK]   = 0;
    ui.param[KNOB_DIR]  = 0;
    ui.param[DARK_MODE] = 1;

    knob.param[KNOB_ROT]   = KNOB_DISABLE;
    knob.param[KNOB_COD]   = KNOB_DISABLE;
    knob.param[KNOB_ROT_P] = 2;
    knob.param[KNOB_COD_P] = 2;
}

static void params_save(void)
{
    for (int i = 0; i < UI_PARAM;   i++) flash_data_buf[i]             = ui.param[i];
    for (int i = 0; i < KNOB_PARAM; i++) flash_data_buf[UI_PARAM + i]  = knob.param[i];
    flash_storage_write(flash_data_buf, FLASH_DATA_LEN);
}

static void params_load(void)
{
    if (flash_storage_read(flash_data_buf, FLASH_DATA_LEN))
    {
        for (int i = 0; i < UI_PARAM;   i++) ui.param[i]   = flash_data_buf[i];
        for (int i = 0; i < KNOB_PARAM; i++) knob.param[i] = flash_data_buf[UI_PARAM + i];
    }
    else
    {
        ui_param_default();
    }
}

/* =====================================================================
 * 选择框辅助函数
 * ===================================================================== */

static void check_box_v_init(uint8_t *param)   { check_box.v = param; }
static void check_box_m_init(uint8_t *param)   { check_box.m = param; }
static void check_box_s_init(uint8_t *param, uint8_t *param_p)
{
    check_box.s   = param;
    check_box.s_p = param_p;
}

static void check_box_m_select(uint8_t param)
{
    check_box.m[param] = !check_box.m[param];
    param_changed = true;
}

static void check_box_s_select(uint8_t val, uint8_t pos)
{
    *check_box.s   = val;
    *check_box.s_p = pos;
    param_changed  = true;
}

/* =====================================================================
 * 弹窗初始化
 * ===================================================================== */

static void window_value_init(const char *title, uint8_t select,
                               uint8_t *value, uint8_t max, uint8_t min,
                               uint8_t step, const MENU *bg, uint8_t index)
{
    strncpy(win.title, title, sizeof(win.title) - 1);
    win.title[sizeof(win.title) - 1] = '\0';
    win.select  = select;
    win.value   = value;
    win.max     = max;
    win.min     = min;
    win.step    = step;
    win.bg      = bg;
    win.index   = index;
    ui.index    = M_WINDOW;
    ui.state    = S_WINDOW;
}

/* =====================================================================
 * UI 初始化（列表行数）
 * ===================================================================== */

static void ui_init_menus(void)
{
    ui.num[M_MAIN]    = sizeof(main_menu)    / sizeof(MENU);
    ui.num[M_EDITOR]  = sizeof(editor_menu)  / sizeof(MENU);
    ui.num[M_KNOB]    = sizeof(knob_menu)    / sizeof(MENU);
    ui.num[M_KRF]     = sizeof(krf_menu)     / sizeof(MENU);
    ui.num[M_SETTING] = sizeof(setting_menu) / sizeof(MENU);
    ui.num[M_ABOUT]   = sizeof(about_menu)   / sizeof(MENU);
}

/* =====================================================================
 * 分页面初始化
 * ===================================================================== */

static void sleep_param_init(void)
{
    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_DrawBox(&u8g2, 0, 0, DISP_W, DISP_H);
    u8g2_SetPowerSave(&u8g2, 1);
    ui.state = S_NONE;
    ui.sleep = true;
    if (param_changed)
    {
        params_save();
        param_changed = false;
    }
}

static void knob_param_init(void)    { check_box_v_init(knob.param); }
static void krf_param_init(void)     { check_box_s_init(&knob.param[KNOB_ROT], &knob.param[KNOB_ROT_P]); }

static void setting_param_init(void)
{
    check_box_v_init(ui.param);
    check_box_m_init(ui.param);
}

static void window_param_init(void)
{
    win.bar   = 0.0f;
    win.y     = (float)WIN_Y_INIT;
    win.y_trg = (float)win.u;
    ui.state  = S_NONE;
}

static void layer_init_in(void)
{
    ui.layer++;
    ui.init         = 0;
    list.y          = 0.0f;
    list.y_trg      = (float)LIST_LINE_H;
    list.box_x      = 0.0f;
    list.box_y      = 0.0f;
    list.bar_y      = 0.0f;
    ui.state        = S_FADE;
    switch (ui.index)
    {
        case M_KNOB:    knob_param_init();    break;
        case M_KRF:     krf_param_init();     break;
        case M_SETTING: setting_param_init(); break;
        default: break;
    }
}

static void layer_init_out(void)
{
    ui.select[ui.layer]         = 0;
    list.box_y_trg[ui.layer]    = 0.0f;
    ui.layer--;
    ui.init    = 0;
    list.y     = 0.0f;
    list.y_trg = (float)LIST_LINE_H;
    list.bar_y = 0.0f;
    ui.state   = S_FADE;
    switch (ui.index)
    {
        case M_SLEEP: sleep_param_init(); break;
        default: break;
    }
}

/* =====================================================================
 * 动画函数
 * ===================================================================== */

static void animation(float *a, float *a_trg, uint8_t n)
{
    if (*a != *a_trg)
    {
        float diff = *a_trg - *a;
        if (diff < 0.0f) diff = -diff;
        if (diff < 0.15f)
        {
            *a = *a_trg;
        }
        else
        {
            *a += (*a_trg - *a) / ((float)ui.param[n] / 10.0f);
        }
    }
}

/* 转场动画：对缓冲区像素按奇偶位置施加掩码 */
static void fade_apply_mask(bool odd_bytes, uint8_t mask)
{
    for (uint16_t i = 0; i < buf_len; ++i)
    {
        if (((i % 2) != 0) == odd_bytes)
            buf_ptr[i] = ui.param[DARK_MODE] ? (buf_ptr[i] & mask) : (buf_ptr[i] | mask);
    }
}

static void fade(void)
{
    HAL_Delay(ui.param[FADE_ANI]);
    switch (ui.fade)
    {
        case 1: fade_apply_mask(true,  0xAAU); break;
        case 2: fade_apply_mask(true,  ui.param[DARK_MODE] ? 0x00U : 0xFFU); break;
        case 3: fade_apply_mask(false, 0x55U); break;
        case 4: fade_apply_mask(false, ui.param[DARK_MODE] ? 0x00U : 0xFFU); break;
        default: ui.state = S_NONE; ui.fade = 0; break;
    }
    ui.fade++;
}

/* =====================================================================
 * 列表绘制辅助函数
 * ===================================================================== */

static void list_draw_value(int n)
{
    char tmp[8];
    snprintf(tmp, sizeof(tmp), "%u", (unsigned)check_box.v[n - 1]);
    u8g2_DrawStr(&u8g2, (u8g2_uint_t)u8g2_GetCursorX(&u8g2),
                 (u8g2_uint_t)u8g2_GetCursorY(&u8g2), tmp);
}

static void list_draw_krf(int n)
{
    const char *s;
    switch (check_box.v[n - 1])
    {
        case 0: s = "OFF"; break;
        case 1: s = "VOL"; break;
        case 2: s = "BRI"; break;
        default: s = "?";  break;
    }
    u8g2_DrawStr(&u8g2, (u8g2_uint_t)u8g2_GetCursorX(&u8g2),
                 (u8g2_uint_t)u8g2_GetCursorY(&u8g2), s);
}

static void list_draw_check_box_frame(void)
{
    u8g2_DrawRFrame(&u8g2, CHECK_BOX_L_S, (u8g2_uint_t)(list.temp + CHECK_BOX_U_S),
                    CHECK_BOX_F_W, CHECK_BOX_F_H, 1);
}

static void list_draw_check_box_dot(void)
{
    u8g2_DrawBox(&u8g2,
                 CHECK_BOX_L_S + CHECK_BOX_D_S + 1,
                 (u8g2_uint_t)(list.temp + CHECK_BOX_U_S + CHECK_BOX_D_S + 1),
                 CHECK_BOX_F_W - (CHECK_BOX_D_S + 1) * 2,
                 CHECK_BOX_F_H - (CHECK_BOX_D_S + 1) * 2);
}

static void list_draw_text_and_check_box(const MENU arr[], int i)
{
    u8g2_DrawStr(&u8g2, LIST_TEXT_S,
                 (u8g2_uint_t)(list.temp + LIST_TEXT_H + LIST_TEXT_S),
                 arr[i].m_select);
    u8g2_SetCursor(&u8g2, CHECK_BOX_L_S,
                   (u8g2_uint_t)(list.temp + LIST_TEXT_H + LIST_TEXT_S));
    switch (arr[i].m_select[0])
    {
        case '~': list_draw_value(i); break;
        case '+':
            list_draw_check_box_frame();
            if (check_box.m[i - 1] == 1) list_draw_check_box_dot();
            break;
        case '=':
            list_draw_check_box_frame();
            if (*check_box.s_p == (uint8_t)i) list_draw_check_box_dot();
            break;
        case '#': list_draw_krf(i);   break;
        default: break;
    }
}

/* =====================================================================
 * 列表通用显示函数
 * ===================================================================== */

static void list_show(const MENU arr[], uint8_t ui_index)
{
    u8g2_SetFont(&u8g2, LIST_FONT);

    /* 更新目标值 */
    list.box_x_trg = (float)(u8g2_GetStrWidth(&u8g2, arr[ui.select[ui.layer]].m_select)
                             + LIST_TEXT_S * 2);
    list.bar_y_trg = (float)ceil((double)ui.select[ui.layer]
                                 * ((double)DISP_H / (double)(ui.num[ui_index] - 1)));

    /* 计算动画过渡值 */
    animation(&list.y,     &list.y_trg,                LIST_ANI);
    animation(&list.box_x, &list.box_x_trg,            LIST_ANI);
    animation(&list.box_y, &list.box_y_trg[ui.layer],  LIST_ANI);
    animation(&list.bar_y, &list.bar_y_trg,            LIST_ANI);

    if (list.loop && list.box_y == list.box_y_trg[ui.layer]) list.loop = false;

    u8g2_SetDrawColor(&u8g2, 1);

    /* 绘制进度条 */
    u8g2_DrawHLine(&u8g2, DISP_W - LIST_BAR_W, 0, LIST_BAR_W);
    u8g2_DrawHLine(&u8g2, DISP_W - LIST_BAR_W, DISP_H - 1, LIST_BAR_W);
    u8g2_DrawVLine(&u8g2, (u8g2_uint_t)(DISP_W - (int)ceil((double)LIST_BAR_W / 2.0)), 0, DISP_H);
    u8g2_DrawBox(&u8g2, DISP_W - LIST_BAR_W, 0, LIST_BAR_W, (u8g2_uint_t)list.bar_y);

    /* 绘制列表文字 */
    if (!ui.init)
    {
        for (int i = 0; i < ui.num[ui_index]; i++)
        {
            if (ui.param[LIST_UFD])
                list.temp = (int16_t)((float)i * list.y
                            - (float)(LIST_LINE_H * ui.select[ui.layer])
                            + list.box_y_trg[ui.layer]);
            else
                list.temp = (int16_t)(((float)i - (float)ui.select[ui.layer]) * list.y
                            + list.box_y_trg[ui.layer]);
            list_draw_text_and_check_box(arr, i);
        }
        if (list.y == list.y_trg)
        {
            ui.init   = true;
            list.y    = (float)(-(LIST_LINE_H * ui.select[ui.layer]))
                       + list.box_y_trg[ui.layer];
            list.y_trg = list.y;
        }
    }
    else
    {
        for (int i = 0; i < ui.num[ui_index]; i++)
        {
            list.temp = (int16_t)((float)(LIST_LINE_H * i) + list.y);
            list_draw_text_and_check_box(arr, i);
        }
    }

    /* 绘制选择框（反色） */
    u8g2_SetDrawColor(&u8g2, 2);
    u8g2_DrawRBox(&u8g2, 0, (u8g2_uint_t)(int16_t)list.box_y,
                  (u8g2_uint_t)list.box_x, LIST_LINE_H,
                  (u8g2_uint_t)(LIST_BOX_R_X10 / 10));

    /* 白天模式遮罩 */
    if (!ui.param[DARK_MODE])
    {
        u8g2_DrawBox(&u8g2, 0, 0, DISP_W, DISP_H);
    }
}

/* =====================================================================
 * 弹窗显示函数
 * ===================================================================== */

static void window_show(void)
{
    list_show(win.bg, win.index);

    if (ui.param[WIN_BOK])
    {
        for (uint16_t i = 0; i < buf_len; ++i)
            buf_ptr[i] &= (i % 2 == 0 ? 0x55U : 0xAAU);
    }

    u8g2_SetFont(&u8g2, WIN_FONT);
    win.bar_trg = (float)(*win.value - win.min)
                 / (float)(win.max - win.min)
                 * (float)(WIN_BAR_W - 4);

    animation(&win.bar, &win.bar_trg, WIN_ANI);
    animation(&win.y,   &win.y_trg,  WIN_ANI);

    /* 绘制弹窗 */
    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_DrawRBox(&u8g2, win.l, (u8g2_uint_t)(int16_t)win.y, WIN_W, WIN_H, 2);

    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_DrawRFrame(&u8g2, win.l, (u8g2_uint_t)(int16_t)win.y, WIN_W, WIN_H, 2);
    u8g2_DrawRFrame(&u8g2, win.l + 5, (u8g2_uint_t)((int16_t)win.y + 20), WIN_BAR_W, WIN_BAR_H, 1);
    u8g2_DrawBox(&u8g2, win.l + 7, (u8g2_uint_t)((int16_t)win.y + 22), (u8g2_uint_t)(int16_t)win.bar, WIN_BAR_H - 4);

    u8g2_SetCursor(&u8g2, win.l + 5, (u8g2_uint_t)((int16_t)win.y + 14));
    u8g2_DrawStr(&u8g2, win.l + 5, (u8g2_uint_t)((int16_t)win.y + 14), win.title);

    char num_str[8];
    snprintf(num_str, sizeof(num_str), "%u", (unsigned)*win.value);
    u8g2_DrawStr(&u8g2, win.l + 78, (u8g2_uint_t)((int16_t)win.y + 14), num_str);

    /* 亮度调节实时生效 */
    if (strncmp(win.title, "Disp Bri", 8) == 0)
    {
        u8g2_SetContrast(&u8g2, ui.param[DISP_BRI]);
    }

    /* 白天模式遮罩 */
    u8g2_SetDrawColor(&u8g2, 2);
    if (!ui.param[DARK_MODE]) u8g2_DrawBox(&u8g2, 0, 0, DISP_W, DISP_H);
}

/* =====================================================================
 * 列表旋转通用处理
 * ===================================================================== */

static void list_rotate_switch(void)
{
    uint8_t btn_id = encoder_event.id;

    if (list.loop) return;

    switch (btn_id)
    {
        case BTN_ID_CC:
            if (ui.select[ui.layer] == 0)
            {
                if (ui.param[LIST_LOOP] && ui.init)
                {
                    list.loop = true;
                    ui.select[ui.layer] = ui.num[ui.index] - 1;
                    if (ui.num[ui.index] > (uint8_t)list.line_n)
                    {
                        list.box_y_trg[ui.layer] = (float)(DISP_H - LIST_LINE_H);
                        list.y_trg = (float)(DISP_H - ui.num[ui.index] * LIST_LINE_H);
                    }
                    else list.box_y_trg[ui.layer] = (float)((ui.num[ui.index] - 1) * LIST_LINE_H);
                }
                break;
            }
            if (ui.init)
            {
                ui.select[ui.layer]--;
                int16_t threshold = -(int16_t)(list.y_trg / LIST_LINE_H);
                if (ui.select[ui.layer] < (uint8_t)threshold)
                {
                    if (!(DISP_H % LIST_LINE_H)) list.y_trg += LIST_LINE_H;
                    else
                    {
                        if ((int16_t)list.box_y_trg[ui.layer] == DISP_H - LIST_LINE_H * list.line_n)
                        {
                            list.y_trg += (float)((list.line_n + 1) * LIST_LINE_H - DISP_H);
                            list.box_y_trg[ui.layer] = 0.0f;
                        }
                        else if ((int16_t)list.box_y_trg[ui.layer] == LIST_LINE_H)
                            list.box_y_trg[ui.layer] = 0.0f;
                        else list.y_trg += LIST_LINE_H;
                    }
                }
                else list.box_y_trg[ui.layer] -= LIST_LINE_H;
            }
            break;

        case BTN_ID_CW:
            if (ui.select[ui.layer] == (ui.num[ui.index] - 1))
            {
                if (ui.param[LIST_LOOP] && ui.init)
                {
                    list.loop = true;
                    ui.select[ui.layer] = 0;
                    list.y_trg = 0.0f;
                    list.box_y_trg[ui.layer] = 0.0f;
                }
                break;
            }
            if (ui.init)
            {
                ui.select[ui.layer]++;
                if ((ui.select[ui.layer] + 1) > (uint8_t)(list.line_n - (int16_t)(list.y_trg / LIST_LINE_H)))
                {
                    if (!(DISP_H % LIST_LINE_H)) list.y_trg -= LIST_LINE_H;
                    else
                    {
                        if ((int16_t)list.box_y_trg[ui.layer] == LIST_LINE_H * (list.line_n - 1))
                        {
                            list.y_trg -= (float)((list.line_n + 1) * LIST_LINE_H - DISP_H);
                            list.box_y_trg[ui.layer] = (float)(DISP_H - LIST_LINE_H);
                        }
                        else if ((int16_t)list.box_y_trg[ui.layer] == DISP_H - LIST_LINE_H * 2)
                            list.box_y_trg[ui.layer] = (float)(DISP_H - LIST_LINE_H);
                        else list.y_trg -= LIST_LINE_H;
                    }
                }
                else list.box_y_trg[ui.layer] += LIST_LINE_H;
            }
            break;

        default: break;
    }
}

/* =====================================================================
 * 弹窗处理
 * ===================================================================== */

static void window_proc(void)
{
    window_show();
    if ((int16_t)win.y == WIN_Y_TARGET) ui.index = win.index;
    if (encoder_event.pressed && win.y == win.y_trg && (int16_t)win.y != WIN_Y_TARGET)
    {
        encoder_clear_event();
        switch (encoder_event.id)
        {
            case BTN_ID_CW:
                if (*win.value < win.max) { *win.value += win.step; param_changed = true; }
                break;
            case BTN_ID_CC:
                if (*win.value > win.min) { *win.value -= win.step; param_changed = true; }
                break;
            case BTN_ID_SP:
            case BTN_ID_LP:
                win.y_trg = (float)WIN_Y_TARGET;
                break;
            default: break;
        }
    }
}

/* =====================================================================
 * 睡眠处理
 * ===================================================================== */

static void sleep_proc(void)
{
    while (ui.sleep)
    {
        if (encoder_event.pressed)
        {
            uint8_t id = encoder_event.id;
            encoder_clear_event();
            switch (id)
            {
                case BTN_ID_LP:
                    /* 长按旋钮退出睡眠，进入主菜单 */
                    ui.index = M_MAIN;
                    ui.state = S_LAYER_IN;
                    u8g2_SetPowerSave(&u8g2, 0);
                    ui.sleep = false;
                    break;
                default:
                    /* 手表版本：短按和旋转可在此添加自定义功能 */
                    break;
            }
        }
        /* 低功耗等待（可选）：等待中断唤醒 */
        /* HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI); */
    }
}

/* =====================================================================
 * 分页面处理函数
 * ===================================================================== */

/* 通用列表处理宏（减少重复代码） */
#define LIST_PROC_BEGIN(arr, idx) \
    list_show((arr), (idx)); \
    if (encoder_event.pressed) { \
        uint8_t _btn = encoder_event.id; \
        encoder_clear_event(); \
        switch (_btn) { \
            case BTN_ID_CW: case BTN_ID_CC: \
                encoder_event.id = _btn; \
                list_rotate_switch(); \
                break; \
            case BTN_ID_LP: ui.select[ui.layer] = 0; /* fall through */ \
            case BTN_ID_SP: \
                switch (ui.select[ui.layer]) {

#define LIST_PROC_END() \
                    default: break; \
                } break; \
            default: break; \
        } \
    }

static void main_proc(void)
{
    LIST_PROC_BEGIN(main_menu, M_MAIN)
        case 0: ui.index = M_SLEEP;   ui.state = S_LAYER_OUT; break;
        case 1: ui.index = M_EDITOR;  ui.state = S_LAYER_IN;  break;
        case 2: ui.index = M_SETTING; ui.state = S_LAYER_IN;  break;
    LIST_PROC_END()
}

static void editor_proc(void)
{
    LIST_PROC_BEGIN(editor_menu, M_EDITOR)
        case 0:  ui.index = M_MAIN; ui.state = S_LAYER_OUT; break;
        case 11: ui.index = M_KNOB; ui.state = S_LAYER_IN;  break;
    LIST_PROC_END()
}

static void knob_proc(void)
{
    LIST_PROC_BEGIN(knob_menu, M_KNOB)
        case 0: ui.index = M_EDITOR; ui.state = S_LAYER_OUT; break;
        case 1: ui.index = M_KRF;   ui.state = S_LAYER_IN;
                check_box_s_init(&knob.param[KNOB_ROT], &knob.param[KNOB_ROT_P]); break;
        /* case 2 (Press Func) 已移除：手表项目无需 USB HID */
    LIST_PROC_END()
}

static void krf_proc(void)
{
    LIST_PROC_BEGIN(krf_menu, M_KRF)
        case 0: ui.index = M_KNOB;  ui.state = S_LAYER_OUT; break;
        case 2: check_box_s_select(KNOB_DISABLE, ui.select[ui.layer]); break;
        case 4: check_box_s_select(1, ui.select[ui.layer]); break; /* KNOB_ROT_VOL */
        case 5: check_box_s_select(2, ui.select[ui.layer]); break; /* KNOB_ROT_BRI */
    LIST_PROC_END()
}

static void setting_proc(void)
{
    LIST_PROC_BEGIN(setting_menu, M_SETTING)
        case 0:  ui.index = M_MAIN; ui.state = S_LAYER_OUT; break;
        case 1:  window_value_init("Disp Bri", DISP_BRI, &ui.param[DISP_BRI],  255,  0,  5, setting_menu, M_SETTING); break;
        case 2:  window_value_init("List Ani", LIST_ANI, &ui.param[LIST_ANI],  100, 10,  1, setting_menu, M_SETTING); break;
        case 3:  window_value_init("Win Ani",  WIN_ANI,  &ui.param[WIN_ANI],   100, 10,  1, setting_menu, M_SETTING); break;
        case 4:  window_value_init("Fade Ani", FADE_ANI, &ui.param[FADE_ANI],  255,  0,  1, setting_menu, M_SETTING); break;
        case 5:  window_value_init("Btn SPT",  BTN_SPT,  &ui.param[BTN_SPT],   255,  0,  1, setting_menu, M_SETTING); break;
        case 6:  window_value_init("Btn LPT",  BTN_LPT,  &ui.param[BTN_LPT],   255,  0,  1, setting_menu, M_SETTING); break;
        case 7:  check_box_m_select(LIST_UFD);  break;
        case 8:  check_box_m_select(LIST_LOOP); break;
        case 9:  check_box_m_select(WIN_BOK);   break;
        case 10: check_box_m_select(KNOB_DIR);  break;
        case 11: check_box_m_select(DARK_MODE); break;
        case 12: ui.index = M_ABOUT; ui.state = S_LAYER_IN; break;
    LIST_PROC_END()
}

static void about_proc(void)
{
    LIST_PROC_BEGIN(about_menu, M_ABOUT)
        case 0: ui.index = M_SETTING; ui.state = S_LAYER_OUT; break;
    LIST_PROC_END()
}

/* =====================================================================
 * UI 主进程
 * ===================================================================== */

static void ui_proc(void)
{
    u8g2_SendBuffer(&u8g2);
    switch (ui.state)
    {
        case S_FADE:      fade();              return;
        case S_WINDOW:    window_param_init(); return;
        case S_LAYER_IN:  layer_init_in();     return;
        case S_LAYER_OUT: layer_init_out();    return;
        case S_NONE:
        default:
            u8g2_ClearBuffer(&u8g2);
            switch (ui.index)
            {
                case M_WINDOW:  window_proc();  break;
                case M_SLEEP:   sleep_proc();   break;
                case M_MAIN:    main_proc();    break;
                case M_EDITOR:  editor_proc();  break;
                case M_KNOB:    knob_proc();    break;
                case M_KRF:     krf_proc();     break;
                case M_SETTING: setting_proc(); break;
                case M_ABOUT:   about_proc();   break;
                default: break;
            }
            break;
    }
}

/* =====================================================================
 * 对外接口
 * ===================================================================== */

/**
 * @brief WouoUI 初始化（对应 Arduino 的 setup()）
 */
void ui_setup(void)
{
    /* 初始化 UI 状态 */
    ui.layer = 0;
    ui.index = M_SLEEP;
    ui.state = S_NONE;
    ui.sleep = true;
    ui.fade  = 1;
    ui.init  = false;

    /* 从 Flash 加载参数（或使用默认值） */
    params_load();

    /* 初始化菜单行数 */
    ui_init_menus();

    /* 初始化列表状态 */
    list.line_n = DISP_H / LIST_LINE_H;

    /* 初始化弹窗位置 */
    win.l = (DISP_W - WIN_W) / 2;
    win.u = (DISP_H - WIN_H) / 2;
    win.y = (float)WIN_Y_INIT;

    /* 初始化 OLED */
    oled_init(ui.param[DISP_BRI]);

    /* 获取缓冲区指针（用于转场动画直接操作像素） */
    buf_ptr = u8g2_GetBufferPtr(&u8g2);
    buf_len = (uint16_t)(8U * u8g2_GetBufferTileHeight(&u8g2)
                            * u8g2_GetBufferTileWidth(&u8g2));
}

/**
 * @brief WouoUI 主循环（对应 Arduino 的 loop()）
 */
void ui_loop(void)
{
    ui_proc();
}
