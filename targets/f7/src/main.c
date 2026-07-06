#include <furi.h>
#include <furi_hal.h>
#include "flipper.h"
#include <furi_hal_cortex.h>
#include <furi_hal_resources.h>
#include <furi_hal_serial_control.h>
#include <gui/canvas.h>
#include <gui/gui.h>

#define TAG "LcdTest"

#define ENABLE_LCD_TEST    1
#define ENABLE_BT_TEST     0
#define ENABLE_CC1101_TEST 0

typedef struct {
    uint32_t counter;
} LcdTestState;

static void lcd_draw_callback(Canvas* canvas, void* context) {
    furi_assert(canvas);
    furi_assert(context);

    const LcdTestState* state = context;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "LCD TEST");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 26, "Official GUI path");

    char line[32];
    snprintf(line, sizeof(line), "CNT:%lu", (unsigned long)state->counter);
    canvas_draw_str(canvas, 2, 42, line);
}

static int32_t lcd_test_thread(void* context) {
    UNUSED(context);

    furi_hal_init();
    // furi_hal_rtc_set_log_device(FuriHalRtcLogDeviceUsart);
    // furi_hal_serial_control_set_logging_config(FuriHalSerialIdUsart, 230400);

    FURI_LOG_I(TAG, "LCD GUI test started");

    LcdTestState state = {
        .counter = 0,
    };

    ViewPort* view_port = view_port_alloc();
    furi_check(view_port);
    view_port_draw_callback_set(view_port, lcd_draw_callback, &state);
    FURI_LOG_I(TAG, "furi_check(view_port) done");
    Gui* gui = NULL;
    for(uint32_t i = 0; i < 100U; i++) {
        if(furi_record_exists(RECORD_GUI)) {
            FURI_LOG_I(TAG, "furi_record_exists");
            gui = furi_record_open(RECORD_GUI);
            break;
        }
        furi_delay_ms(10);
    }

    if(!gui) {
        FURI_LOG_W(TAG, "GUI not ready");
        view_port_free(view_port);
        return 0;
    }

    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    FURI_LOG_I(TAG, "GUI ready");

    while(true) {
        state.counter++;
        view_port_update(view_port);
        furi_delay_ms(100);
    }

    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    return 0;
}

int main(void) {
    furi_init();
    furi_hal_init_early();
    flipper_init();
    FuriThread* thread = furi_thread_alloc_ex("LcdTest", 2048, lcd_test_thread, NULL);
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
