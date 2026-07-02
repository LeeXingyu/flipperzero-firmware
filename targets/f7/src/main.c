#include <furi.h>
#include <furi_hal.h>
#include <flipper.h>
#include <alt_boot.h>
#include <update_util/update_operation.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>

typedef struct {
    FuriMessageQueue* event_queue;
    Gui* gui;
    ViewPort* view_port;
    FuriTimer* timer;
    uint8_t pos;
    bool stop;
} BootScreenApp;

typedef enum {
    BootScreenEventTypeTick,
    BootScreenEventTypeInput,
} BootScreenEventType;

typedef struct {
    BootScreenEventType type;
    InputEvent input;
} BootScreenEvent;

static void boot_screen_render_callback(Canvas* canvas, void* ctx) {
    furi_assert(canvas);
    furi_assert(ctx);

    BootScreenApp* app = ctx;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_frame(canvas, 8, 4, 112, 56);
    canvas_draw_box(canvas, 16 + app->pos, 16, 16, 16);
    canvas_draw_circle(canvas, 72, 28, 12);
    canvas_draw_line(canvas, 8, 48, 119, 48);
    canvas_draw_str(canvas, 45, 61, "TEST");
}

static void boot_screen_timer_callback(void* ctx) {
    furi_assert(ctx);

    BootScreenApp* app = ctx;
    BootScreenEvent event = {.type = BootScreenEventTypeTick};
    furi_message_queue_put(app->event_queue, &event, 0);
}

static void boot_screen_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    furi_assert(input_event);

    FuriMessageQueue* event_queue = ctx;
    BootScreenEvent event = {.type = BootScreenEventTypeInput, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void boot_screen_app_free(BootScreenApp* app) {
    furi_assert(app);

    if(app->gui && app->view_port) {
        gui_remove_view_port(app->gui, app->view_port);
    }

    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
    }

    if(app->view_port) {
        view_port_free(app->view_port);
    }

    if(app->gui) {
        furi_record_close(RECORD_GUI);
    }

    if(app->event_queue) {
        furi_message_queue_free(app->event_queue);
    }

    free(app);
}

int32_t init_task(void* context) {
    UNUSED(context);

    BootScreenApp* app = malloc(sizeof(BootScreenApp));
    app->event_queue = furi_message_queue_alloc(8, sizeof(BootScreenEvent));
    app->gui = furi_record_open(RECORD_GUI);
    app->view_port = view_port_alloc();
    app->timer = furi_timer_alloc(boot_screen_timer_callback, FuriTimerTypePeriodic, app);
    app->pos = 0;
    app->stop = false;

    view_port_draw_callback_set(app->view_port, boot_screen_render_callback, app);
    view_port_input_callback_set(app->view_port, boot_screen_input_callback, app->event_queue);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    view_port_update(app->view_port);

    furi_timer_start(app->timer, furi_kernel_get_tick_frequency() / 8);

    while(!app->stop) {
        BootScreenEvent event;
        furi_check(
            furi_message_queue_get(app->event_queue, &event, FuriWaitForever) == FuriStatusOk);

        if(event.type == BootScreenEventTypeInput) {
            if((event.input.type == InputTypeShort) && (event.input.key == InputKeyBack)) {
                app->stop = true;
            }
        } else {
            app->pos += 4;
            if(app->pos > 88) {
                app->pos = 0;
            }
            view_port_update(app->view_port);
        }
    }

    boot_screen_app_free(app);

    return 0;
}

int main(void) {
    // Initialize FURI layer
    furi_init();

    // Flipper critical FURI HAL
    furi_hal_init_early();
    furi_hal_init();
    flipper_init();

    FuriThread* main_thread = furi_thread_alloc_ex("InitSrv", 2048, init_task, NULL);
    furi_thread_set_priority(main_thread, FuriThreadPriorityInit);
    furi_thread_start(main_thread);

    // Run Kernel
    furi_run();

    furi_crash("Kernel is Dead");
}

void Error_Handler(void) {
    furi_crash("ErrorHandler");
}

void abort(void) {
    furi_crash("AbortHandler");
}
