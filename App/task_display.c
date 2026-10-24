#include "task_display.h"

#include "app_main.h"
#include "lcd.h"

/* Backend: Alientek 4.3" TFT over FSMC (landscape, see app_main.c init).
 * The SSD1306 driver (bsp_oled.*) stays in the tree as a spare backend.
 */
void DisplayTask(void *argument)
{
    (void)argument;

    for (;;)
    {
        App_TaskHeartbeat(TASK_DISPLAY);

        /* Row 1: uptime. Row 2: one block per task (Day 4 makes them live
         * heartbeat indicators). Row 3: fault bitmask.
         */
        lcd_show_string(10, 8, 120, 24, 24, "UP:", WHITE);
        lcd_show_num(70, 8, xTaskGetTickCount() / 1000, 5, 24, WHITE);
        lcd_show_string(190, 8, 24, 24, 24, "s", WHITE);

        for (int i = 0; i < TASK_COUNT; ++i)
        {
            uint16_t x = (uint16_t)(10 + i * 30);
            lcd_fill(x, 50, (uint16_t)(x + 16), 66, GREEN);
        }

        lcd_show_string(10, 90, 60, 24, 24, "F:", WHITE);
        lcd_show_num(50, 90, gFaultFlags, 2, 24, RED);

        /* Live heartbeat counters: prove CommTask/ProcessTask are running
         * even when the serial path is under suspicion (Day 1 COM mixup). */
        lcd_show_string(10, 130, 60, 24, 24, "C:", WHITE);
        lcd_show_num(50, 130, gHeartbeat[TASK_COMM] % 1000, 3, 24, GREEN);
        lcd_show_string(120, 130, 60, 24, 24, "P:", WHITE);
        lcd_show_num(160, 130, gHeartbeat[TASK_PROCESS] % 1000, 3, 24, GREEN);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
