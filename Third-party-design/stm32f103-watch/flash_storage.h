/**
 * @file    flash_storage.h
 * @brief   Flash 参数存储头文件（模拟 EEPROM）
 *
 * 适配平台：STM32F103C8T6
 * Flash 信息：64KB，页大小 1KB，最后一页地址 0x0800FC00
 *
 * 原理：
 *   将 UI 参数保存到 Flash 的最后一个页中，
 *   读取时直接从 Flash 地址读取，
 *   写入时先擦除整页再写入所有数据。
 *
 * 注意：
 *   - Flash 擦写寿命约 1 万次，请勿频繁写入
 *   - 本驱动只在 UI 参数发生变化时（进入睡眠前）写入一次
 */

#ifndef __FLASH_STORAGE_H
#define __FLASH_STORAGE_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/* ============================== 用户配置 ============================== */

/* Flash 参数存储起始地址
 * STM32F103C8T6（64KB Flash）最后一页：0x0800FC00
 * STM32F103CBT6（128KB Flash）最后一页：0x0801FC00
 * 根据实际芯片修改 */
#define FLASH_PARAM_ADDRESS     0x0800FC00UL

/* 存储空间大小（不超过一个 Flash 页：1024 字节） */
#define FLASH_PARAM_MAX_SIZE    128U

/* 校验魔数（用于判断 Flash 是否有有效数据） */
#define FLASH_MAGIC_WORD        0xA5A5A5A5UL

/* ====================================================================== */

/**
 * @brief 检查 Flash 中是否有有效参数
 * @return true  有效（魔数匹配）
 *         false 无效（首次使用或数据损坏）
 */
bool flash_storage_is_valid(void);

/**
 * @brief 从 Flash 读取参数
 * @param buf    读取缓冲区指针
 * @param len    读取字节数（不超过 FLASH_PARAM_MAX_SIZE - 4）
 * @return true  成功
 *         false 失败（魔数不匹配）
 */
bool flash_storage_read(uint8_t *buf, uint16_t len);

/**
 * @brief 将参数写入 Flash（擦除后写入）
 * @param buf    数据缓冲区指针
 * @param len    写入字节数（不超过 FLASH_PARAM_MAX_SIZE - 4）
 * @return true  成功
 *         false 失败（擦除或写入错误）
 */
bool flash_storage_write(const uint8_t *buf, uint16_t len);

/**
 * @brief 擦除参数存储区（设置为无效状态）
 * @return true  成功
 *         false 失败
 */
bool flash_storage_erase(void);

#endif /* __FLASH_STORAGE_H */
