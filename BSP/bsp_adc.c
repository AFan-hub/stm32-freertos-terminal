#include "bsp_adc.h"

#include <stdio.h>

#include "bsp_uart.h"
#include "main.h"

extern ADC_HandleTypeDef hadc1;

/* ADC common registers: CSR at +0x300, CCR at +0x304. TSVREFE (bit 23,
 * enables internal temp sensor + VREFINT) lives in CCR - the old +0x300
 * address silently wrote into CSR and left both sources powered down. */
#define ADC_COMMON_CCR     (*(volatile uint32_t *)0x40012304u)
#define ADC_CCR_TSVREFE    (1u << 23)

static uint16_t adc_read_channel(uint32_t channel)
{
    ADC_ChannelConfTypeDef cfg = { 0 };

    cfg.Channel      = channel;
    cfg.Rank         = 1;
    cfg.SamplingTime = ADC_SAMPLETIME_144CYCLES;

    if (HAL_ADC_ConfigChannel(&hadc1, &cfg) != HAL_OK)
    {
        return 0;
    }

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

uint16_t BSP_ADC_ReadRaw(void)
{
    return adc_read_channel(ADC_CHANNEL_1);   /* photoresistor module AO */
}

float BSP_ADC_ReadLightPercent(void)
{
    return (float)BSP_ADC_ReadRaw() * 100.0f / 4095.0f;
}

float BSP_ADC_ReadTempC(void)
{
    /* internal temperature sensor = ADC1 channel 16 on F407 */
    ADC_COMMON_CCR |= ADC_CCR_TSVREFE;

    /* Self-calibrate the reference: factory VREFINT calibration word
     * (measured at 3.3 V, 25 C) lets us recover the real VDDA instead of
     * assuming 3.3 V - board supply sag otherwise shifts temp by 10+ C. */
    const uint16_t vrefint_cal = *(const uint16_t *)0x1FFF7A2Au;
    uint16_t raw_ref = adc_read_channel(ADC_CHANNEL_17);

    if (raw_ref == 0)
    {
        return -273.0f;
    }

    float vdda = 3.3f * (float)vrefint_cal / (float)raw_ref;

    uint16_t raw = adc_read_channel(ADC_CHANNEL_16);

    /* one-shot raw dump for the Day-2 temperature forensics */
    static int reported = 0;
    if (!reported)
    {
        reported = 1;
        char line[72];
        int n = snprintf(line, sizeof(line),
                         "[adc] r16=%u r17=%u cal=%u vdda=%ldmV\r\n",
                         (unsigned)raw, (unsigned)raw_ref,
                         (unsigned)vrefint_cal, (long)(vdda * 1000.0f));
        BSP_UART1_SendBuffer((const uint8_t *)line, (uint16_t)n);
        BSP_UART2_SendBuffer((const uint8_t *)line, (uint16_t)n);
    }

    float v = (float)raw * vdda / 4095.0f;

    /* F407 datasheet typical: V25 = 0.76 V, avg slope = 2.5 mV/degC.
     * Chip-internal grade: +/- a few degrees, fine for a demo channel. */
    return (0.76f - v) / 0.0025f + 25.0f;
}
