#include "bsp_adc.h"

#include "main.h"

extern ADC_HandleTypeDef hadc1;

uint16_t BSP_ADC_ReadRaw(void)
{
    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        return 0;
    }

    if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK)
    {
        HAL_ADC_Stop(&hadc1);
        return 0;
    }

    uint16_t value = (uint16_t)HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    return value;
}

float BSP_ADC_ReadLightPercent(void)
{
    return (float)BSP_ADC_ReadRaw() * 100.0f / 4095.0f;
}
