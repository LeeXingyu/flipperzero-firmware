#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <stdio.h>
#include <string.h>
#include <u8g2_glue.h>

static int32_t lcd_test_thread(void* context) {
    UNUSED(context);

    furi_hal_init();

    furi_delay_ms(200);

    FuriHalSerialHandle* serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(!serial) {
        serial = furi_hal_serial_control_acquire(FuriHalSerialIdLpuart);
    }

    if(serial) {
        furi_hal_serial_init(serial, 115200);
        furi_hal_serial_configure_framing(
            serial, FuriHalSerialDataBits8, FuriHalSerialParityNone, FuriHalSerialStopBits1);
    }

    u8g2_t u8g2;
    u8g2_Setup_st756x_flipper(&u8g2, U8G2_R0, u8x8_hw_spi_stm32, u8g2_gpio_and_delay_stm32);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);

    if(serial) {
        const char* banner = "Serial LCD test started\r\n";
        furi_hal_serial_tx(serial, (const uint8_t*)banner, strlen(banner));
    }

    uint8_t pos = 0;
    uint32_t counter = 0;
    while(true) {
        u8g2_ClearBuffer(&u8g2);
        u8g2_SetDrawColor(&u8g2, 1);

        u8g2_DrawFrame(&u8g2, 8, 4, 112, 56);
        u8g2_DrawBox(&u8g2, 16 + pos, 16, 16, 16);
        u8g2_DrawCircle(&u8g2, 72, 28, 12, U8G2_DRAW_ALL);
        u8g2_DrawLine(&u8g2, 8, 48, 119, 48);
        u8g2_SetFont(&u8g2, u8g2_font_profont11_mr);
        u8g2_DrawStr(&u8g2, 45, 61, "TEST");

        u8g2_SendBuffer(&u8g2);

        if(serial) {
            char line[64];
            int len = snprintf(
                line,
                sizeof(line),
                "counter=%lu pos=%u\r\n",
                (unsigned long)counter,
                (unsigned int)pos);
            if(len > 0) {
                furi_hal_serial_tx(serial, (const uint8_t*)line, (size_t)len);
            }
        }

        pos += 4;
        if(pos > 88) {
            pos = 0;
        }
        counter++;

        furi_delay_ms(120);
    }

    return 0;
}

int main(void) {
    furi_init();
    furi_hal_init_early();

    FuriThread* thread = furi_thread_alloc_ex("LcdTest", 4096, lcd_test_thread, NULL);
    furi_thread_start(thread);

    furi_run();

    return 0;
}

void Error_Handler(void) {
    furi_crash("ErrorHandler");
}

void abort(void) {
    furi_crash("AbortHandler");
}
