#include "bsp_flash.h"

#include <stddef.h>
#include <string.h>

#include "crc16.h"
#include "main.h"

#define CONFIG_FLASH_SECTOR FLASH_SECTOR_8

static uint16_t config_crc(const SystemConfig_t *cfg)
{
    return CRC16_CCITT((const uint8_t *)cfg,
                       (uint16_t)offsetof(SystemConfig_t, crc));
}

int BSP_Flash_SaveConfig(const SystemConfig_t *cfg)
{
    SystemConfig_t image = *cfg;
    FLASH_EraseInitTypeDef erase = { 0 };
    uint32_t sector_error = 0;
    uint32_t addr = CONFIG_FLASH_ADDR;

    image.magic = 0xA55AA55Au;
    image.version = 1;
    image.crc = config_crc(&image);

    HAL_FLASH_Unlock();

    erase.TypeErase     = FLASH_TYPEERASE_SECTORS;
    erase.Sector        = CONFIG_FLASH_SECTOR;
    erase.NbSectors     = 1;
    erase.VoltageRange  = FLASH_VOLTAGE_RANGE_3;

    if (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return -1;
    }

    const uint32_t *src = (const uint32_t *)(const void *)&image;

    for (uint32_t i = 0; i < sizeof(image) / 4u; ++i)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, src[i]) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return -1;
        }
        addr += 4u;
    }

    HAL_FLASH_Lock();

    /* read-back verify */
    return (memcmp((const void *)CONFIG_FLASH_ADDR, &image, sizeof(image)) == 0)
               ? 0 : -1;
}

int BSP_Flash_LoadConfig(SystemConfig_t *cfg)
{
    SystemConfig_t image;

    memcpy(&image, (const void *)CONFIG_FLASH_ADDR, sizeof(image));

    if (image.magic != 0xA55AA55Au || image.version != 1 ||
        image.crc != config_crc(&image))
    {
        return -1;
    }

    *cfg = image;
    return 0;
}
