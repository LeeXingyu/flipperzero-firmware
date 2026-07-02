#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <gui/canvas.h>
#include <gui/canvas_i.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// 1 = use u8g2 directly
// 2 = use GUI/Canvas direct draw
#define LCD_RENDER_BACKEND_U8G2 1
#define LCD_RENDER_BACKEND_GUI  2

#ifndef LCD_RENDER_BACKEND
#define LCD_RENDER_BACKEND LCD_RENDER_BACKEND_GUI
#endif

#if LCD_RENDER_BACKEND == LCD_RENDER_BACKEND_U8G2
#include <u8g2_glue.h>
#elif LCD_RENDER_BACKEND == LCD_RENDER_BACKEND_GUI
#else
#error "Invalid LCD_RENDER_BACKEND"
#endif

typedef struct {
    FuriHalSerialHandle* serial;
    uint32_t counter;
    uint8_t pos;
} LcdTestContext;

static void lcd_test_serial_init(LcdTestContext* ctx) {
    ctx->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(!ctx->serial) {
        ctx->serial = furi_hal_serial_control_acquire(FuriHalSerialIdLpuart);
    }

    if(ctx->serial) {
        furi_hal_serial_init(ctx->serial, 115200);
        furi_hal_serial_configure_framing(
            ctx->serial, FuriHalSerialDataBits8, FuriHalSerialParityNone, FuriHalSerialStopBits1);
    }
}

static void lcd_test_serial_write(LcdTestContext* ctx, const char* text) {
    if(ctx->serial && text) {
        furi_hal_serial_tx(ctx->serial, (const uint8_t*)text, strlen(text));
    }
}

static void lcd_test_serial_writef(LcdTestContext* ctx, const char* fmt, ...) {
    if(!ctx->serial) return;

    char line[64];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    if(len > 0) {
        furi_hal_serial_tx(ctx->serial, (const uint8_t*)line, (size_t)len);
    }
}

static int32_t lcd_test_thread(void* context) {
    furi_assert(context);
    LcdTestContext* ctx = context;

    furi_hal_init();
    furi_hal_serial_control_set_logging_config(FuriHalSerialIdMax, 115200);

    furi_delay_ms(200);

    lcd_test_serial_init(ctx);

#if LCD_RENDER_BACKEND == LCD_RENDER_BACKEND_U8G2
    u8g2_t u8g2;
    u8g2_Setup_st756x_flipper(&u8g2, U8G2_R0, u8x8_hw_spi_stm32, u8g2_gpio_and_delay_stm32);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);

    lcd_test_serial_write(ctx, "Serial LCD test started (u8g2)\r\n");
#elif LCD_RENDER_BACKEND == LCD_RENDER_BACKEND_GUI
    Canvas* canvas = canvas_init();

    lcd_test_serial_write(ctx, "Serial LCD test started (gui)\r\n");
#endif

    while(true) {
#if LCD_RENDER_BACKEND == LCD_RENDER_BACKEND_U8G2
        u8g2_ClearBuffer(&u8g2);
        u8g2_SetDrawColor(&u8g2, 1);

        u8g2_DrawFrame(&u8g2, 8, 4, 112, 56);
        u8g2_DrawBox(&u8g2, 16 + ctx->pos, 16, 16, 16);
        u8g2_DrawCircle(&u8g2, 72, 28, 12, U8G2_DRAW_ALL);
        u8g2_DrawLine(&u8g2, 8, 48, 119, 48);
        u8g2_SetFont(&u8g2, u8g2_font_profont11_mr);
        u8g2_DrawStr(&u8g2, 45, 61, "TEST");

        u8g2_SendBuffer(&u8g2);
#elif LCD_RENDER_BACKEND == LCD_RENDER_BACKEND_GUI
        canvas_reset(canvas);
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);

        canvas_draw_frame(canvas, 8, 4, 112, 56);
        canvas_draw_box(canvas, 16 + ctx->pos, 16, 16, 16);
        canvas_draw_circle(canvas, 72, 28, 12);
        canvas_draw_line(canvas, 8, 48, 119, 48);
        canvas_draw_str(canvas, 45, 61, "TEST");
        canvas_commit(canvas);
#endif

        lcd_test_serial_writef(
            ctx, "counter=%lu pos=%u\r\n", (unsigned long)ctx->counter, (unsigned int)ctx->pos);

        ctx->pos += 4;
        if(ctx->pos > 88) {
            ctx->pos = 0;
        }
        ctx->counter++;

        furi_delay_ms(120);
    }

#if LCD_RENDER_BACKEND == LCD_RENDER_BACKEND_GUI
    canvas_free(canvas);
#endif

    return 0;
}

int main(void) {
    furi_init();
    furi_hal_init_early();

    LcdTestContext* context = malloc(sizeof(LcdTestContext));
    context->serial = NULL;
    context->counter = 0;
    context->pos = 0;

    FuriThread* thread = furi_thread_alloc_ex("LcdTest", 4096, lcd_test_thread, context);
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
