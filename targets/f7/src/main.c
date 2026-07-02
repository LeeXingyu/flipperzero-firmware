#include <furi.h>
#include <furi_hal.h>
#include <flipper.h>
#include <alt_boot.h>
#include <update_util/update_operation.h>
#include <gui/canvas_i.h>
#include <u8g2_glue.h>

int main(void) {
    furi_init();

    furi_hal_init_early();
    furi_hal_init();
    //flipper_init();

    // furi_hal_power_enable_external_3_3v();
    furi_delay_ms(200);
    u8g2_t u8g2;
    u8g2_Setup_st756x_flipper(&u8g2, U8G2_R0, u8x8_hw_spi_stm32, u8g2_gpio_and_delay_stm32);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    // u8g2_t* u8g2 = &canvas->fb;
    uint8_t pos = 0;
    while(true) {
        u8g2_ClearBuffer(&u8g2);
        u8g2_SetDrawColor(&u8g2, 1);

        // Simple moving test pattern.
        u8g2_DrawFrame(&u8g2, 8, 4, 112, 56);
        u8g2_DrawBox(&u8g2, 16 + pos, 16, 16, 16);
        u8g2_DrawCircle(&u8g2, 72, 28, 12, U8G2_DRAW_ALL);
        u8g2_DrawLine(&u8g2, 8, 48, 119, 48);
        u8g2_SetFont(&u8g2, u8g2_font_profont11_mr);
        u8g2_DrawStr(&u8g2, 45, 61, "TEST");

        u8g2_SendBuffer(&u8g2);

        pos += 4;
        if(pos > 88) {
            pos = 0;
        }

        furi_delay_ms(120);
    }

    //canvas_free(canvas);

    return 0;
}

void Error_Handler(void) {
    furi_crash("ErrorHandler");
}

void abort(void) {
    furi_crash("AbortHandler");
}
