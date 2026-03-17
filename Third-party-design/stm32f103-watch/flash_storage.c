/**
 * @file    flash_storage.c
 * @brief   Flash 参数存储实现（模拟 EEPROM）
 *
 * 适配平台：STM32F103C8T6 + HAL 库
 *
 * Flash 存储布局（从 FLASH_PARAM_ADDRESS 开始）：
 *   偏移 0x00：魔数（4字节，FLASH_MAGIC_WORD = 0xA5A5A5A5）
 *   偏移 0x04：参数数据（最多 FLASH_PARAM_MAX_SIZE - 4 字节）
 */

#include "flash_storage.h"

/**
 * @brief 检查 Flash 中是否有有效参数
 */
bool flash_storage_is_valid(void)
{
    uint32_t magic = *(__IO uint32_t *)FLASH_PARAM_ADDRESS;
    return (magic == FLASH_MAGIC_WORD);
}

/**
 * @brief 从 Flash 读取参数
 */
bool flash_storage_read(uint8_t *buf, uint16_t len)
{
    if (!flash_storage_is_valid())
    {
        return false;
    }

    if (len > (FLASH_PARAM_MAX_SIZE - 4U))
    {
        return false;
    }

    /* 直接读取 Flash（从魔数之后开始） */
    const uint8_t *src = (const uint8_t *)(FLASH_PARAM_ADDRESS + 4U);
    for (uint16_t i = 0; i < len; i++)
    {
        buf[i] = src[i];
    }
    return true;
}

/**
 * @brief 将参数写入 Flash
 *
 * 步骤：
 * 1. 解锁 Flash
 * 2. 擦除目标页
 * 3. 以半字（16位）为单位写入魔数和数据
 * 4. 重新上锁 Flash
 */
bool flash_storage_write(const uint8_t *buf, uint16_t len)
{
    HAL_StatusTypeDef status;
    FLASH_EraseInitTypeDef erase_init;
    uint32_t page_error = 0;

    if (len > (FLASH_PARAM_MAX_SIZE - 4U))
    {
        return false;
    }

    /* 1. 解锁 Flash */
    status = HAL_FLASH_Unlock();
    if (status != HAL_OK)
    {
        return false;
    }

    /* 2. 擦除目标页 */
    erase_init.TypeErase   = FLASH_TYPEERASE_PAGES;
    erase_init.PageAddress = FLASH_PARAM_ADDRESS;
    erase_init.NbPages     = 1;

    status = HAL_FLASHEx_Erase(&erase_init, &page_error);
    if (status != HAL_OK || page_error != 0xFFFFFFFFU)
    {
        HAL_FLASH_Lock();
        return false;
    }

    /* 3. 写入魔数（以半字为单位）
     *    0xA5A5A5A5 → 先写低半字 0xA5A5，再写高半字 0xA5A5 */
    uint32_t addr = FLASH_PARAM_ADDRESS;

    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr,
                                (uint16_t)(FLASH_MAGIC_WORD & 0xFFFFU));
    if (status != HAL_OK) { HAL_FLASH_Lock(); return false; }
    addr += 2U;

    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr,
                                (uint16_t)((FLASH_MAGIC_WORD >> 16) & 0xFFFFU));
    if (status != HAL_OK) { HAL_FLASH_Lock(); return false; }
    addr += 2U;

    /* 4. 写入数据（每次写入 2 字节，不足 2 字节的末尾补 0xFF） */
    uint16_t i = 0;
    while (i < len)
    {
        uint16_t hword;
        if (i + 1U < len)
        {
            hword = (uint16_t)buf[i] | ((uint16_t)buf[i + 1U] << 8);
        }
        else
        {
            /* 最后一个字节，高字节填 0xFF（Flash 擦除后为 0xFF） */
            hword = (uint16_t)buf[i] | 0xFF00U;
        }

        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr, hword);
        if (status != HAL_OK)
        {
            HAL_FLASH_Lock();
            return false;
        }
        addr += 2U;
        i    += 2U;
    }

    /* 5. 重新上锁 */
    HAL_FLASH_Lock();
    return true;
}

/**
 * @brief 擦除参数存储区
 */
bool flash_storage_erase(void)
{
    FLASH_EraseInitTypeDef erase_init;
    uint32_t page_error = 0;
    HAL_StatusTypeDef status;

    status = HAL_FLASH_Unlock();
    if (status != HAL_OK) return false;

    erase_init.TypeErase   = FLASH_TYPEERASE_PAGES;
    erase_init.PageAddress = FLASH_PARAM_ADDRESS;
    erase_init.NbPages     = 1;

    status = HAL_FLASHEx_Erase(&erase_init, &page_error);
    HAL_FLASH_Lock();

    return (status == HAL_OK && page_error == 0xFFFFFFFFU);
}
