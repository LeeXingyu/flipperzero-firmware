#include <furi.h>
#include <furi_hal.h>
#include "flipper.h"
#include <furi_hal_cortex.h>
#include <furi_hal_resources.h>
#include <furi_hal_serial_control.h>
#include <furi_hal_subghz.h>
#include <furi_hal_bt.h>
#include <gui/canvas.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <storage/storage_sd_api.h>
#include <lib/subghz/devices/cc1101_configs.h>

#define TAG "LcdTest"

#define SD_TEST             1
#define ENABLE_GUI_TEST     0
#define ENABLE_BT_TEST      0
#define ENABLE_CC1101_TEST  0
#define ENABLE_RAW_GUI_TEST 0
#define ENABLE_KEY_TEST     0
#define ENABLE_FLIPPER_TEST 0

#define CC1101_FREQ_HZ              433920000UL
#define CC1101_SIGNAL_THRESHOLD_DBM (-50.0f)
#define RAW_GUI_HISTORY_SIZE        100
#define RAW_GUI_THRESHOLD_MIN       (-90.0f)

#if ENABLE_BT_TEST
#include <bt/bt_service/bt_keys_filename.h>
#include <bt/bt_service/bt_keys_storage.h>
#include <profiles/serial_profile.h>

#ifndef BT_KEYS_STORAGE_PATH
#define BT_KEYS_STORAGE_PATH INT_PATH(BT_KEYS_STORAGE_FILE_NAME)
#endif
#endif
#if SD_TEST
static int32_t sd_test_thread(void) {
    FURI_LOG_I(TAG, "SD test begin");
    FURI_LOG_I(TAG, "Hardware note: no CD pin, presence check is skipped");
    FURI_LOG_I(TAG, "Polling until CMD0 probe succeeds");

    uint32_t probe_attempt = 0;
    while(true) {
        probe_attempt++;
        bool power_reset = (probe_attempt == 1U) || ((probe_attempt % 2U) == 1U);
        FURI_LOG_I(
            TAG,
            "Probe poll %lu: furi_hal_sd_probe(power_reset=%s) begin",
            (unsigned long)probe_attempt,
            power_reset ? "true" : "false");

        bool card_present = furi_hal_sd_probe(power_reset);
        FURI_LOG_I(
            TAG,
            "Probe poll %lu: furi_hal_sd_probe(power_reset=%s) result=%s",
            (unsigned long)probe_attempt,
            power_reset ? "true" : "false",
            card_present ? "true" : "false");

        if(card_present) {
            FURI_LOG_I(TAG, "CMD0 responded, card likely present");
            break;
        }

        FURI_LOG_W(TAG, "CMD0 no response, waiting 1s before retry");
        furi_delay_ms(1000);
    }

    FURI_LOG_I(TAG, "Polling until SD init succeeds");

    uint32_t init_attempt = 0;
    while(true) {
        init_attempt++;
        FURI_LOG_I(
            TAG,
            "Init poll %lu: furi_hal_sd_init(power_reset=false) begin",
            (unsigned long)init_attempt);

        FuriStatus status = furi_hal_sd_init(false);
        FURI_LOG_I(
            TAG,
            "Init poll %lu: furi_hal_sd_init(power_reset=false) result=%ld",
            (unsigned long)init_attempt,
            (long)status);

        if(status == FuriStatusOk) {
            FURI_LOG_I(TAG, "SD HAL init ok on poll %lu", (unsigned long)init_attempt);
            break;
        }

        FURI_LOG_W(TAG, "SD init failed, waiting 1s before retry");
        furi_delay_ms(1000);
    }

    FURI_LOG_I(TAG, "Polling until SD info succeeds");

    uint32_t info_attempt = 0;
    while(true) {
        info_attempt++;
        FURI_LOG_I(TAG, "Info poll %lu: furi_hal_sd_info begin", (unsigned long)info_attempt);

        FuriHalSdInfo sd_info = {0};
        FuriStatus status = furi_hal_sd_info(&sd_info);
        FURI_LOG_I(
            TAG,
            "Info poll %lu: furi_hal_sd_info result=%ld",
            (unsigned long)info_attempt,
            (long)status);

        if(status == FuriStatusOk) {
            FURI_LOG_I(
                TAG,
                "SD card ready: manuf=%02x oem=%s name=%s rev=%u.%u sn=%08lx total=%lu KiB block=%lu logical=%lu",
                sd_info.manufacturer_id,
                sd_info.oem_id,
                sd_info.product_name,
                sd_info.product_revision_major,
                sd_info.product_revision_minor,
                (unsigned long)sd_info.product_serial_number,
                (unsigned long)(sd_info.capacity / 1024UL),
                (unsigned long)sd_info.block_size,
                (unsigned long)sd_info.logical_block_count);

            FURI_LOG_I(TAG, "SD test success");
            break;
        }

        FURI_LOG_W(TAG, "SD info failed, waiting 1s before retry");
        furi_delay_ms(1000);
    }

    return 0;
}
#endif
typedef struct {
    volatile bool signal_active;
    volatile float rssi;
    volatile uint32_t edge_count;
    volatile uint32_t last_duration_us;
    volatile bool last_level;
    volatile float idle_baseline_rssi;
    volatile uint8_t rssi_history[RAW_GUI_HISTORY_SIZE];
    volatile uint8_t rssi_current;
    volatile uint8_t rssi_write;
    volatile bool rssi_history_full;
#if ENABLE_KEY_TEST
    volatile uint32_t key_event_count;
    volatile bool exit_requested;
#endif
} LcdTestState;

#if ENABLE_BT_TEST
static bool bt_test_gap_event_callback(GapEvent event, void* context) {
    UNUSED(context);

    switch(event.type) {
    case GapEventTypeStartAdvertising:
        FURI_LOG_I(TAG, "BLE advertising started");
        break;
    case GapEventTypeStopAdvertising:
        FURI_LOG_I(TAG, "BLE advertising stopped");
        break;
    default:
        break;
    }

    return true;
}
#endif

#if ENABLE_RAW_GUI_TEST
static uint8_t lcd_raw_map_rssi(float rssi) {
    if(rssi < RAW_GUI_THRESHOLD_MIN) {
        return 0;
    }

    float value = (rssi - RAW_GUI_THRESHOLD_MIN) / 2.7f;
    if(value < 0.0f) value = 0.0f;
    if(value > 34.0f) value = 34.0f;
    return (uint8_t)value;
}
#endif

#if ENABLE_CC1101_TEST
static void lcd_capture_callback(bool level, uint32_t duration_us, void* context) {
    furi_assert(context);

    LcdTestState* state = context;
    state->edge_count++;
    state->last_level = level;
    state->last_duration_us = duration_us;
}
#endif

#if ENABLE_RAW_GUI_TEST
static void lcd_raw_add_rssi_sample(LcdTestState* state, float rssi) {
    uint8_t sample = lcd_raw_map_rssi(rssi);
    state->rssi_current = sample;
    state->rssi_history[state->rssi_write] = sample;
    state->rssi_write++;
    if(state->rssi_write >= RAW_GUI_HISTORY_SIZE) {
        state->rssi_write = 0;
        state->rssi_history_full = true;
    }
}

static void lcd_draw_raw_graph(Canvas* canvas, const LcdTestState* state) {
    const uint8_t graph_left = 0;
    const uint8_t graph_right = 115;
    const uint8_t graph_top = 14;
    const uint8_t graph_bottom = 48;

    canvas_draw_line(canvas, graph_left, graph_top, graph_right, graph_top);
    canvas_draw_line(canvas, graph_left, graph_bottom, graph_right, graph_bottom);
    canvas_draw_line(canvas, graph_right, graph_top, graph_right, graph_bottom);

    const uint8_t capacity = RAW_GUI_HISTORY_SIZE;
    const uint8_t visible = graph_right - graph_left + 1;
    const uint8_t sample_count = state->rssi_history_full ? capacity : state->rssi_write;
    const uint8_t left_padding = (visible > sample_count) ? (visible - sample_count) : 0;
    const uint8_t history_start = state->rssi_history_full ? state->rssi_write : 0;

    for(uint8_t x = 0; x < visible; x++) {
        uint8_t sample = 0;

        if(x >= left_padding) {
            uint8_t history_index = x - left_padding;
            if(history_index < sample_count) {
                uint8_t index = (history_start + history_index) % capacity;
                sample = state->rssi_history[index];
            }
        }

        canvas_draw_line(
            canvas, graph_left + x, graph_bottom, graph_left + x, graph_bottom - sample);
    }

    const uint8_t threshold_y =
        graph_bottom - (uint8_t)((CC1101_SIGNAL_THRESHOLD_DBM - RAW_GUI_THRESHOLD_MIN) / 2.7f);
    for(uint8_t x = 0; x < visible; x += 6) {
        canvas_draw_line(canvas, graph_left + x, threshold_y, graph_left + x + 3, threshold_y);
    }
    canvas_draw_dot(canvas, graph_right, threshold_y);

    canvas_set_font_direction(canvas, CanvasDirectionBottomToTop);
    canvas_draw_str(canvas, 126, 40, "RSSI");
    canvas_set_font_direction(canvas, CanvasDirectionLeftToRight);
}
#endif
#if ENABLE_CC1101_TEST
static void lcd_draw_callback(Canvas* canvas, void* context) {
    furi_assert(canvas);
    furi_assert(context);

    const LcdTestState* state = context;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 9, "RAW");

    char line[24];
    snprintf(line, sizeof(line), "RSSI:%5.1f", (double)state->rssi);
    canvas_draw_str(canvas, 18, 9, line);
    canvas_draw_str(canvas, 74, 9, state->signal_active ? "SIGNAL" : "IDLE");

#if ENABLE_RAW_GUI_TEST
    lcd_draw_raw_graph(canvas, state);
#endif
}
#endif

#if ENABLE_KEY_TEST
static void lcd_input_events_callback(const void* message, void* context) {
    furi_assert(message);
    furi_assert(context);

    const InputEvent* event = message;
    LcdTestState* state = context;

    state->key_event_count++;
    FURI_LOG_I(
        TAG,
        "Key event #%lu: %s %s",
        (unsigned long)state->key_event_count,
        input_get_key_name(event->key),
        input_get_type_name(event->type));

    if((event->key == InputKeyBack) && (event->type == InputTypeLong)) {
        state->exit_requested = true;
        FURI_LOG_I(TAG, "Back long pressed, exit requested");
    }
}
#endif

static int32_t lcd_test_thread(void* context) {
    UNUSED(context);
    furi_hal_init();
    // flipper_init();
    FURI_LOG_I(TAG, "Main started");
#if SD_TEST
    return sd_test_thread();
#elif ENABLE_FLIPPER_TEST
    furi_background();
#else
#if ENABLE_BT_TEST
    BtKeysStorage* bt_keys = NULL;
#endif

    LcdTestState state = {
        .signal_active = false,
        .rssi = -127.0f,
        .edge_count = 0,
        .last_duration_us = 0,
        .last_level = false,
        .idle_baseline_rssi = -127.0f,
        .rssi_history = {0},
        .rssi_current = 0,
        .rssi_write = 0,
        .rssi_history_full = false,
#if ENABLE_KEY_TEST
        .key_event_count = 0,
        .exit_requested = false,
#endif
    };

    ViewPort* view_port = view_port_alloc();
    furi_check(view_port);
    view_port_draw_callback_set(view_port, lcd_draw_callback, &state);

    Gui* gui = NULL;
    for(uint32_t i = 0; i < 100U; i++) {
        if(furi_record_exists(RECORD_GUI)) {
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

#if ENABLE_BT_TEST
    furi_check(furi_hal_bt_start_radio_stack());

    bt_keys = bt_keys_storage_alloc(BT_KEYS_STORAGE_PATH);
    furi_check(bt_keys);

    FuriHalBleProfileBase* bt_profile = furi_hal_bt_start_app(
        ble_profile_serial,
        NULL,
        bt_keys_storage_get_root_keys(bt_keys),
        bt_test_gap_event_callback,
        NULL);
    furi_check(bt_profile);

    furi_hal_bt_start_advertising();
    FURI_LOG_I(TAG, "BLE profile started");
#endif

#if ENABLE_KEY_TEST
    FuriPubSub* input_events = NULL;
    for(uint32_t i = 0; i < 100U; i++) {
        if(furi_record_exists(RECORD_INPUT_EVENTS)) {
            input_events = furi_record_open(RECORD_INPUT_EVENTS);
            break;
        }
        furi_delay_ms(10);
    }

    if(!input_events) {
        FURI_LOG_W(TAG, "Input events not ready");
        gui_remove_view_port(gui, view_port);
        furi_record_close(RECORD_GUI);
        view_port_free(view_port);
        return 0;
    }

    FuriPubSubSubscription* input_subscription =
        furi_pubsub_subscribe(input_events, lcd_input_events_callback, &state);
    FURI_LOG_I(TAG, "Input events ready");
#endif

#if ENABLE_CC1101_TEST
    FURI_LOG_I(TAG, "CC1101 HAL init");
    furi_hal_subghz_init();
    FURI_LOG_I(TAG, "CC1101 HAL ready");

    furi_hal_subghz_load_custom_preset(subghz_device_cc1101_preset_ook_650khz_async_regs);
    FURI_LOG_I(TAG, "CC1101 OOK async preset loaded");

    uint32_t real_frequency = furi_hal_subghz_set_frequency(CC1101_FREQ_HZ);
    furi_hal_subghz_set_path(FuriHalSubGhzPath433);
    FURI_LOG_I(TAG, "CC1101 frequency set to %lu Hz", (unsigned long)real_frequency);

    furi_hal_subghz_start_async_rx(lcd_capture_callback, &state);
    FURI_LOG_I(TAG, "CC1101 async RX started");
#endif

    uint32_t last_edges = 0;
    uint32_t baseline_acc = 0;
    uint32_t baseline_count = 0;

    while(true) {
#if ENABLE_KEY_TEST
        if(state.exit_requested) {
            break;
        }
#endif
        state.rssi = furi_hal_subghz_get_rssi();
        state.signal_active = (state.rssi > CC1101_SIGNAL_THRESHOLD_DBM);

        if(!state.signal_active) {
            baseline_acc += (uint32_t)((state.rssi + 200.0f) * 10.0f);
            baseline_count++;
            if(baseline_count != 0) {
                state.idle_baseline_rssi =
                    ((float)baseline_acc / (float)baseline_count) / 10.0f - 200.0f;
            }
        }

#if ENABLE_RAW_GUI_TEST
        lcd_raw_add_rssi_sample(&state, state.rssi);
#endif

        if(state.signal_active && state.edge_count != last_edges) {
            FURI_LOG_I(
                TAG,
                "433.92MHz possible signal edges=%lu(+%lu) last_level=%u last_pulse=%luus rssi=%.1fdBm baseline=%.1fdBm",
                (unsigned long)state.edge_count,
                (unsigned long)(state.edge_count - last_edges),
                state.last_level ? 1U : 0U,
                (unsigned long)state.last_duration_us,
                (double)state.rssi,
                (double)state.idle_baseline_rssi);
            last_edges = state.edge_count;
        }

        view_port_update(view_port);
        furi_delay_ms(100);
    }

#if ENABLE_CC1101_TEST
    furi_hal_subghz_stop_async_rx();
    furi_hal_subghz_sleep();
#endif

#if ENABLE_BT_TEST
    furi_hal_bt_stop_advertising();
    bt_keys_storage_free(bt_keys);
#endif

    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
#if ENABLE_KEY_TEST
    furi_pubsub_unsubscribe(input_events, input_subscription);
    furi_record_close(RECORD_INPUT_EVENTS);
#endif
#endif
    return 0;
}

int main(void) {
    furi_init();
    furi_hal_init_early();

    FuriThread* thread = furi_thread_alloc_ex("LcdTest", 3072, lcd_test_thread, NULL);
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
