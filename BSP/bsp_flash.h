#ifndef BSP_FLASH_H
#define BSP_FLASH_H

#include "app_types.h"

/* SystemConfig_t persistence in internal flash, sector 8 (0x08080000).
 * Layout: magic + version + fields + CRC16-CCITT-FALSE over everything
 * before the crc member. Boot loads and validates; on any mismatch the
 * caller falls back to defaults. Writes happen only on the CLI `save`
 * command, never periodically (flash endurance).
 */

#define CONFIG_FLASH_ADDR 0x08080000u
#define CONFIG_VERSION    2u   /* v2: sensor_1 = light %, threshold in % */

int BSP_Flash_SaveConfig(const SystemConfig_t *cfg);  /* 0 ok */
int BSP_Flash_LoadConfig(SystemConfig_t *cfg);        /* 0 valid, -1 invalid */

#endif
