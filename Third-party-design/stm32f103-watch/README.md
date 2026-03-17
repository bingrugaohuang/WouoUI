# WouoUI 移植到 STM32F103C8T6 手表项目完整指南

## 作者
社区贡献 / WouoUI 维护团队

## 说明
本文档详细介绍如何将 WouoUI（Lite_General 版本）从 Arduino 移植到 STM32F103C8T6 手表项目中，
使用 STM32CubeMX 生成底层驱动代码，并使用 Keil MDK5 进行编译和烧录。

---

## 目录
1. [准备工作](#1-准备工作)
2. [硬件连接](#2-硬件连接)
3. [CubeMX 工程配置](#3-cubemx-工程配置)
4. [Keil MDK5 工程配置](#4-keil-mdk5-工程配置)
5. [添加 U8g2 C 库](#5-添加-u8g2-c-库)
6. [移植 WouoUI 代码](#6-移植-wououi-代码)
7. [代码适配要点](#7-代码适配要点)
8. [编译与烧录](#8-编译与烧录)
9. [常见问题](#9-常见问题)

---

## 1. 准备工作

### 1.1 所需软件

| 软件 | 版本要求 | 下载地址 |
|------|---------|---------|
| STM32CubeMX | 6.x 及以上 | https://www.st.com/en/development-tools/stm32cubemx.html |
| Keil MDK5 (MDK-ARM) | 5.x 及以上 | https://www.keil.com/download/product/ |
| STM32F1 器件包 | 最新版 | 通过 Keil Pack Installer 安装 |
| ST-Link 驱动 | 最新版 | https://www.st.com/en/development-tools/stsw-link009.html |
| U8g2 C 库源码 | 最新版 | https://github.com/olikraus/u8g2 |

> **注意：** Keil MDK5 需要激活许可证或使用社区版（32KB 代码限制）。WouoUI-Lite_General 编译后固件约 15-25 KB，在社区版限制范围内。

### 1.2 所需硬件

| 器件 | 说明 |
|------|------|
| STM32F103C8T6 最小系统板（蓝丸/黑丸） | 主控，72MHz Cortex-M3，64KB Flash，20KB RAM |
| SSD1306 OLED 显示屏（128×64 或 128×32） | I2C 接口，0.96 英寸 |
| EC11 旋转编码器（带按键） | 输入控制 |
| ST-Link V2 调试器 | 程序烧录和调试 |
| 面包板 + 杜邦线 | 连接硬件 |
| 3.3V 电源或 USB 供电 | 为系统供电 |

---

## 2. 硬件连接

### 2.1 OLED 屏幕连接（I2C 接口）

| OLED 引脚 | STM32F103C8T6 引脚 | 说明 |
|----------|-------------------|------|
| VCC | 3.3V | 供电 |
| GND | GND | 接地 |
| SCL | PB6 (I2C1_SCL) | I2C 时钟线，需接 4.7kΩ 上拉电阻到 3.3V |
| SDA | PB7 (I2C1_SDA) | I2C 数据线，需接 4.7kΩ 上拉电阻到 3.3V |

> **注意：** I2C 总线必须接上拉电阻，否则通信不稳定。SSD1306 的 I2C 地址通常为 `0x3C`（7 位）或 `0x78`（8 位含读写位），在初始化时注意区分。

### 2.2 EC11 旋转编码器连接

| EC11 引脚 | STM32F103C8T6 引脚 | 说明 |
|----------|-------------------|------|
| A 相 (CLK) | PA0 (支持外部中断) | 编码器 A 信号，内部上拉 |
| B 相 (DT) | PA1 | 编码器 B 信号，内部上拉 |
| SW（按键） | PA2 | 按键信号，内部上拉 |
| GND | GND | 接地 |
| + | 3.3V（可选） | 如使用有 VCC 引脚的模块 |

> **提示：** 如果旋转方向与预期相反，可以在软件中修改 `KNOB_DIR` 参数，或直接交换 A/B 相的引脚定义。

### 2.3 ST-Link 调试器连接

| ST-Link 引脚 | STM32F103C8T6 引脚 |
|-------------|-------------------|
| SWDIO | PA13 |
| SWCLK | PA14 |
| GND | GND |
| 3.3V | 3.3V（可为目标板供电） |

---

## 3. CubeMX 工程配置

### 3.1 新建工程

1. 打开 **STM32CubeMX**，点击 **File → New Project**。
2. 在搜索框输入 `STM32F103C8T6`，选中后点击 **Start Project**。
3. 进入 **Pinout & Configuration** 页面。

### 3.2 配置系统时钟

1. 在左侧 **System Core** 中点击 **RCC**。
2. 将 **High Speed Clock (HSE)** 设置为 `Crystal/Ceramic Resonator`（外部晶振，蓝丸板通常为 8MHz）。
3. 切换到 **Clock Configuration** 选项卡，按如下配置：
   - **Input frequency**：8 MHz（HSE）
   - **PLL Source Mux**：HSE
   - **PLLMul**：×9
   - **System Clock Mux**：PLLCLK
   - **HCLK（AHB）**：72 MHz
   - **APB1 Prescaler**：÷2（APB1 = 36 MHz，I2C 挂在 APB1 上）
   - **APB2 Prescaler**：÷1（APB2 = 72 MHz）

   配置完成后 **HCLK 应显示 72.000 MHz**。

### 3.3 配置 I2C1（OLED 显示屏）

1. 在左侧 **Connectivity** 中点击 **I2C1**。
2. 将模式设置为 **I2C**。
3. 在 **Configuration** 中：
   - **I2C Speed Mode**：Standard Mode（100 kHz）或 Fast Mode（400 kHz，推荐，刷新更快）
   - **I2C Clock Speed**：100000 或 400000
4. 此时 PB6（SCL）和 PB7（SDA）会自动配置为 I2C 功能引脚。

> **推荐使用 400kHz Fast Mode**，可以让 OLED 刷新率更高，UI 动画更流畅。

### 3.4 配置 GPIO（EC11 编码器引脚）

1. 在 Pinout 视图中，点击 **PA0** 引脚，选择 `GPIO_EXTI0`（外部中断）。
2. 点击 **PA1** 引脚，选择 `GPIO_Input`（普通输入）。
3. 点击 **PA2** 引脚，选择 `GPIO_Input`（普通输入）。
4. 在左侧 **System Core → GPIO** 中，对上述引脚进行详细配置：

   **PA0（编码器 A 相，外部中断）：**
   - GPIO Mode: External Interrupt Mode with Rising/Falling edge trigger detection
   - GPIO Pull-up/Pull-down: Pull-up
   - User Label: `ENC_A`

   **PA1（编码器 B 相）：**
   - GPIO Mode: Input mode
   - GPIO Pull-up/Pull-down: Pull-up
   - User Label: `ENC_B`

   **PA2（编码器按键）：**
   - GPIO Mode: Input mode
   - GPIO Pull-up/Pull-down: Pull-up
   - User Label: `ENC_SW`

### 3.5 配置外部中断（NVIC）

1. 在左侧 **System Core → NVIC** 中，启用 `EXTI line0 interrupt`。
2. 设置优先级：**Preemption Priority** = 1，**Sub Priority** = 0。
3. 确保已勾选 **Enabled**。

### 3.6 配置 TIM4（5ms 定时中断，用于按键扫描）

按键扫描需要周期性执行（每 5ms 一次），使用 TIM4 定时器产生中断：

1. 在左侧 **Timers → TIM4** 中：
   - **Clock Source**：Internal Clock
   - **Prescaler**：7199（即 72MHz / (7199+1) = 10kHz）
   - **Counter Period**：49（即 10kHz / (49+1) = 200Hz = 5ms 周期）
   - **Counter Mode**：Up
2. 在 **NVIC Settings** 选项卡中启用 `TIM4 global interrupt`。
   - Preemption Priority: 2
   - Sub Priority: 0

### 3.7 工程代码生成设置

1. 切换到 **Project Manager** 选项卡。
2. **Project 设置：**
   - **Project Name**：例如 `WouoUI_Watch`
   - **Project Location**：选择你的工作目录
   - **Toolchain / IDE**：选择 `MDK-ARM`（即 Keil MDK5）
   - **Min Version**：V5
3. **Code Generator 设置：**
   - 勾选 **Copy only the necessary library files**（减少文件数量）
   - 勾选 **Generate peripheral initialization as a pair of '.c/.h' files**（将外设初始化分离为独立文件）
4. 点击右上角 **GENERATE CODE** 按钮，选择工程保存目录，等待生成完成。

---

## 4. Keil MDK5 工程配置

### 4.1 打开工程

1. CubeMX 生成完成后，点击弹出窗口中的 **Open Project**，会自动用 Keil 打开生成的工程。
   - 或手动打开工程目录下 `MDK-ARM/WouoUI_Watch.uvprojx`。

### 4.2 安装器件包

若首次使用 STM32F1 系列，需安装器件支持包：

1. 在 Keil 菜单栏点击 **Pack Installer**（图标形似魔方）。
2. 搜索 `STM32F1`，安装 `Keil.STM32F1xx_DFP` 器件包。
3. 安装完成后重启 Keil 并重新打开工程。

### 4.3 添加用户代码文件夹

在工程目录下新建两个文件夹（以下步骤在 Windows 文件管理器中操作）：
- `WouoUI_Watch/User/` — 存放 WouoUI 移植代码
- `WouoUI_Watch/User/U8g2/` — 存放 U8g2 C 库文件

### 4.4 在 Keil 中添加文件到工程

1. 在 Keil 左侧 **Project** 窗格中，右键点击工程名，选择 **Manage Project Items**。
2. 在 **Project Items** 对话框中：
   - 点击 **New Group**，创建名为 `User` 的组。
   - 点击 **New Group**，创建名为 `U8g2` 的组。
3. 选中 `User` 组，点击 **Add Files**，依次添加 `User/` 目录下的所有 `.c` 文件。
4. 选中 `U8g2` 组，点击 **Add Files**，添加 `User/U8g2/` 下所有 `.c` 文件（见第 5 节）。

### 4.5 配置包含路径

1. 在 Keil 菜单栏点击 **Project → Options for Target**（快捷键 Alt+F7）。
2. 切换到 **C/C++** 选项卡。
3. 在 **Include Paths** 栏点击右侧 `...` 按钮，添加以下路径：
   - `../User`
   - `../User/U8g2`
   - `../Core/Inc`
4. 在 **Define** 栏添加宏（如果 U8g2 需要）：`USE_HAL_DRIVER,STM32F103xB`

### 4.6 配置编译优化

1. 在 **C/C++** 选项卡中：
   - **Optimization**：建议选 `Level 1 (-O1)` 以平衡代码大小和速度。
   - 勾选 **One ELF Section per Function**（有助于减小固件体积）。
2. 在 **Target** 选项卡中：
   - **Xtal**：8.0 MHz（与 CubeMX 设置一致）
   - 确认 **Use MicroLIB** 已勾选（减小 C 运行库大小）。

---

## 5. 添加 U8g2 C 库

### 5.1 获取 U8g2 源码

1. 访问 https://github.com/olikraus/u8g2 下载 ZIP 包（或 git clone）。
2. 在压缩包中找到 `csrc/` 目录，这是 U8g2 的 C 语言库。

### 5.2 复制必要文件

将 `csrc/` 目录下以下文件复制到工程的 `User/U8g2/` 目录：

**必须包含的 .c 文件（约 40 个）：**
```
u8g2_bitmap.c          u8g2_box.c             u8g2_buffer.c
u8g2_button.c          u8g2_circle.c          u8g2_cleardisplay.c
u8g2_d_memory.c        u8g2_d_setup.c         u8g2_font.c
u8g2_fonts.c           u8g2_hvline.c          u8g2_input_value.c
u8g2_intersection.c    u8g2_kerning.c         u8g2_line.c
u8g2_ll_hvline.c       u8g2_message.c         u8g2_polygon.c
u8g2_selection_list.c  u8g2_setup.c           u8g2_unicode.c
u8x8_byte.c            u8x8_cad.c             u8x8_capture.c
u8x8_d_ssd1306_128x64_noname.c               (根据你的屏幕型号选择)
u8x8_display.c         u8x8_fonts.c           u8x8_gpio.c
u8x8_input_value.c     u8x8_message.c         u8x8_selection_list.c
u8x8_setup.c           u8x8_string.c          u8x8_u16toa.c
u8x8_u8toa.c
```

**所有 .h 文件：**
```
u8g2.h    u8x8.h    mui.h    mui_u8g2.h
```

> **提示：** 根据你使用的 OLED 驱动芯片，必须包含对应的显示驱动 `.c` 文件：
> - SSD1306（最常见）：`u8x8_d_ssd1306_128x64_noname.c`
> - SH1106：`u8x8_d_sh1106_128x64_noname.c`

### 5.3 字体选择

本移植使用两种字体（在 `ui_main.h` 中定义）：
- `u8g2_font_4x6_tr`：列表字体（超小字体，适合行高 7px）
- `u8g2_font_HelvetiPixel_tr`：弹窗字体

字体数据已包含在 `u8g2_fonts.c` 中。如需其他字体，可在该文件中查找。

---

## 6. 移植 WouoUI 代码

本目录下已提供移植好的模板文件，直接复制到工程的 `User/` 目录下使用：

| 文件 | 说明 |
|------|------|
| `oled_driver.h` / `oled_driver.c` | OLED 显示屏驱动，封装 U8g2 的 I2C 初始化回调 |
| `encoder.h` / `encoder.c` | EC11 旋转编码器驱动，处理旋转检测和按键消抖 |
| `flash_storage.h` / `flash_storage.c` | Flash 参数存储（替代 Arduino EEPROM），保存 UI 配置 |
| `ui_main.h` / `ui_main.c` | WouoUI Lite_General 主逻辑，移植自 Arduino 版本 |

### 6.1 文件依赖关系

```
main.c
  ├── ui_main.h / ui_main.c     ← UI 主逻辑（含菜单定义、动画、状态机）
  │     ├── oled_driver.h        ← OLED 初始化和 U8g2 实例
  │     │     └── U8g2/u8g2.h
  │     ├── encoder.h            ← 编码器输入
  │     └── flash_storage.h      ← 参数持久化
  └── （CubeMX 生成的外设代码）
        ├── Core/Src/i2c.c
        ├── Core/Src/gpio.c
        └── Core/Src/tim.c
```

### 6.2 在 `main.c` 中集成

CubeMX 生成的 `Core/Src/main.c` 需要在用户代码区域添加调用：

```c
/* USER CODE BEGIN Includes */
#include "ui_main.h"
#include "encoder.h"
/* USER CODE END Includes */

/* USER CODE BEGIN 2 */
encoder_init();      // 初始化 EC11 编码器
ui_setup();          // 初始化 WouoUI
/* USER CODE END 2 */

/* USER CODE BEGIN WHILE */
while (1)
{
  encoder_scan();    // 轮询按键状态（在 5ms TIM4 中断中调用效果更好）
  ui_loop();         // UI 主循环
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */
}
/* USER CODE END 3 */
```

### 6.3 在中断处理中集成

打开 `Core/Src/stm32f1xx_it.c`，在对应中断处理函数中添加调用：

```c
/* USER CODE BEGIN Includes */
#include "encoder.h"
/* USER CODE END Includes */

// EC11 编码器 A 相外部中断（EXTI0）
void EXTI0_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI0_IRQn 0 */
  encoder_exti_handler();   // 在中断中处理旋转
  /* USER CODE END EXTI0_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(ENC_A_Pin);
  /* USER CODE BEGIN EXTI0_IRQn 1 */
  /* USER CODE END EXTI0_IRQn 1 */
}

// TIM4 定时中断（5ms 周期，用于按键扫描）
void TIM4_IRQHandler(void)
{
  /* USER CODE BEGIN TIM4_IRQn 0 */
  encoder_tim_handler();    // 5ms 定时扫描按键
  /* USER CODE END TIM4_IRQn 0 */
  HAL_TIM_IRQHandler(&htim4);
  /* USER CODE BEGIN TIM4_IRQn 1 */
  /* USER CODE END TIM4_IRQn 1 */
}
```

---

## 7. 代码适配要点

### 7.1 Arduino → STM32 HAL 函数对照

| Arduino 函数 | STM32 HAL 等效 | 说明 |
|-------------|---------------|------|
| `delay(ms)` | `HAL_Delay(ms)` | 毫秒延时 |
| `digitalRead(pin)` | `HAL_GPIO_ReadPin(GPIOx, pin)` | 读取 GPIO 电平 |
| `attachInterrupt(pin, isr, mode)` | CubeMX NVIC + `HAL_GPIO_EXTI_Callback` | 外部中断 |
| `EEPROM.read(addr)` | 见 `flash_storage.c` | Flash 模拟 EEPROM |
| `EEPROM.write(addr, val)` | 见 `flash_storage.c` | Flash 模拟 EEPROM |

### 7.2 U8g2 初始化差异

Arduino 版使用 C++ 对象：
```cpp
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, SCL, SDA, RST);
u8g2.begin();
```

STM32 C 移植版使用结构体和回调函数：
```c
u8g2_t u8g2;
u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0,
    u8x8_byte_hw_i2c,       // I2C 字节传输回调
    u8x8_gpio_and_delay);   // GPIO 和延时回调
u8g2_InitDisplay(&u8g2);
u8g2_SetPowerSave(&u8g2, 0);
```

### 7.3 U8g2 C++ API → C API 对照

| C++ 方法（Arduino） | C 函数（STM32） |
|--------------------|-----------------|
| `u8g2.clearBuffer()` | `u8g2_ClearBuffer(&u8g2)` |
| `u8g2.sendBuffer()` | `u8g2_SendBuffer(&u8g2)` |
| `u8g2.setFont(font)` | `u8g2_SetFont(&u8g2, font)` |
| `u8g2.drawStr(x,y,s)` | `u8g2_DrawStr(&u8g2, x, y, s)` |
| `u8g2.drawBox(x,y,w,h)` | `u8g2_DrawBox(&u8g2, x, y, w, h)` |
| `u8g2.drawRBox(x,y,w,h,r)` | `u8g2_DrawRBox(&u8g2, x, y, w, h, r)` |
| `u8g2.drawRFrame(x,y,w,h,r)` | `u8g2_DrawRFrame(&u8g2, x, y, w, h, r)` |
| `u8g2.drawHLine(x,y,w)` | `u8g2_DrawHLine(&u8g2, x, y, w)` |
| `u8g2.drawVLine(x,y,h)` | `u8g2_DrawVLine(&u8g2, x, y, h)` |
| `u8g2.setDrawColor(c)` | `u8g2_SetDrawColor(&u8g2, c)` |
| `u8g2.setContrast(v)` | `u8g2_SetContrast(&u8g2, v)` |
| `u8g2.setPowerSave(v)` | `u8g2_SetPowerSave(&u8g2, v)` |
| `u8g2.getStrWidth(s)` | `u8g2_GetStrWidth(&u8g2, s)` |
| `u8g2.getBufferPtr()` | `u8g2_GetBufferPtr(&u8g2)` |
| `u8g2.getBufferTileHeight()` | `u8g2_GetBufferTileHeight(&u8g2)` |
| `u8g2.getBufferTileWidth()` | `u8g2_GetBufferTileWidth(&u8g2)` |
| `u8g2.setCursor(x,y)` | `u8g2_SetCursor(&u8g2, x, y)` |
| `u8g2.print(val)` | `u8g2_uint_t u8g2_DrawUTF8(...)` 或格式化后用 `DrawStr` |

### 7.4 Flash 模拟 EEPROM

STM32F103C8T6 没有 EEPROM，需要使用 Flash 最后一页（地址 `0x0800FC00`，64KB 型号）模拟 EEPROM。

**关键注意事项：**
- Flash 写入前必须先擦除整页（页大小 1KB）
- 擦除操作会暂时关闭中断，避免在动画渲染时操作
- 每次只在退出睡眠前写入一次，避免频繁擦写损坏 Flash（擦除寿命约 1 万次）

### 7.5 USB HID 功能

WouoUI-Lite_General 原版 Arduino 代码包含 USB HID 控制音量/亮度的功能。
**手表项目中请删除或禁用此功能**（已在移植代码中通过条件编译禁用）。
相关代码在 `ui_main.c` 中用 `#if UI_ENABLE_USB_HID` 宏控制。

### 7.6 Sleep 模式适配

在手表项目中，睡眠模式下可以进入 STM32 的低功耗模式：
```c
// 在 sleep_proc() 中，等待中断时可进入 WFI 低功耗模式
HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
// EC11 的 EXTI 中断会唤醒 MCU
```
这样可以大幅降低手表的待机功耗。

---

## 8. 编译与烧录

### 8.1 编译

1. 在 Keil 中按 **F7** 或点击 **Project → Build Target**。
2. 等待编译完成，查看输出窗口。
3. 如有错误（Error），根据提示检查：
   - 头文件路径是否正确（Include Paths）
   - 文件是否都已添加到工程
   - 宏定义是否缺失
4. 编译成功后，输出类似：
   ```
   Program Size: Code=15432 RO-data=3210 RW-data=52 ZI-data=4096
   FromELF: creating hex file...
   ".\WouoUI_Watch.axf" - 0 Error(s), 0 Warning(s).
   ```

### 8.2 烧录

1. 连接 ST-Link V2 调试器到 STM32 的 SWD 接口（SWDIO/SWCLK/GND/3.3V）。
2. 在 Keil 菜单点击 **Flash → Configure Flash Tools**：
   - **Utilities** 选项卡中选择 **ST-Link Debugger**，点击 **Settings**。
   - 在 **Flash Download** 中确认已有 STM32F10x Flash 编程算法（若没有，点击 **Add** 添加）。
3. 按 **F8** 或点击 **Flash → Download** 进行烧录。
4. 烧录完成后按 **复位键**（NRST）或重新上电，程序开始运行。

### 8.3 调试

1. 按 **Ctrl+F5** 启动调试模式。
2. 利用 **Watch 窗口**观察 `ui.index`、`ui.state`、`btn` 等结构体变量。
3. 在 `ui_proc()`、`encoder_scan()` 等关键函数处设置断点排查问题。

---

## 9. 常见问题

### Q1：OLED 无显示

**可能原因和解决方法：**
- 检查 I2C 上拉电阻是否存在（SCL/SDA 需要 4.7kΩ 上拉到 3.3V）
- 确认 I2C 地址：OLED 模块上的 I2C 地址引脚（SA0）决定地址是 `0x3C` 还是 `0x3D`（7 位地址），在 `oled_driver.h` 中修改 `OLED_I2C_ADDRESS`
- 检查 CubeMX 中 I2C1 是否已正确启用
- 用示波器或逻辑分析仪检查 SDA/SCL 线上的波形

### Q2：旋转编码器方向相反

在 `ui_main.h` 中修改默认参数：
```c
// 将以下默认值从 0 改为 1
.param[KNOB_DIR] = 1,   // 旋钮方向反转
```
或在运行时通过 **Setting → Knob Rot Dir** 菜单项切换。

### Q3：菜单动画卡顿

- 确认 I2C 速度已设置为 400kHz Fast Mode
- 在 `ui_main.h` 中适当增大 `LIST_ANI_DEFAULT` 值（值越大动画越快，越小越慢）
- 检查 `TIM4` 中断是否正常触发（在中断函数中设置断点确认）

### Q4：参数无法保存（重启后恢复默认）

- 检查 `flash_storage.c` 中的 Flash 地址 `FLASH_PARAM_ADDRESS` 是否与实际 Flash 末页地址匹配
- STM32F103C8T6 (64KB Flash) 最后一页地址为 `0x0800FC00`
- 确认 Flash 解锁和重新上锁操作正确（见 `flash_storage.c`）

### Q5：Keil 提示"ROM size limit exceeded"

- 使用 Keil MDK5 社区版（免费）时，代码体积限制为 32KB
- 解决方法：
  1. 开启编译优化（O1 或 O2）
  2. 删除不需要的 U8g2 字体（在 `u8g2_fonts.c` 中注释掉未使用的字体数组）
  3. 购买 Keil MDK5 许可证，或使用 GCC（arm-none-eabi）编译（免费无限制）

### Q6：编译报 C99 语法错误

Keil 默认使用 C90，需启用 C99：
1. **Project → Options for Target → C/C++**
2. 在 **Misc Controls** 中添加 `-std=c99` 或勾选 **C99 Mode**（较新版本 Keil 有此选项）

---

## 移植代码文件说明

本目录下提供的所有源文件均为模板，方便开发者快速上手：

```
stm32f103-watch/
├── README.md               ← 本文档
├── oled_driver.h           ← OLED 驱动头文件
├── oled_driver.c           ← OLED 驱动实现（U8g2 I2C 回调）
├── encoder.h               ← EC11 编码器头文件
├── encoder.c               ← EC11 编码器实现
├── flash_storage.h         ← Flash 存储头文件
├── flash_storage.c         ← Flash 存储实现（模拟 EEPROM）
├── ui_main.h               ← WouoUI 主头文件（菜单定义、参数）
└── ui_main.c               ← WouoUI 主实现（动画、状态机）
```

将以上 `.c` 和 `.h` 文件复制到 CubeMX 生成工程的 `User/` 目录后，
按照本文档第 6 节在 `main.c` 和 `stm32f1xx_it.c` 中添加调用即可。
