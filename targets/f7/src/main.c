#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_resources.h>
#include <furi_hal_spi_config.h>
#include <furi_hal_serial_control.h>
#include <furi_hal_cortex.h>
#include <furi_hal_subghz.h>
#include <cc1101.h>
#include <string.h>
#include <gui/canvas.h>
#include <gui/canvas_i.h>

// 1 = use u8g2 directly
// 2 = use GUI/Canvas direct draw
#define TAG                     "LcdTest"
#define LCD_RENDER_BACKEND_U8G2 1
#define LCD_RENDER_BACKEND_GUI  2
#define ENABLE_LCD_TEST         0
#define ENABLE_BT_TEST          0
#define ENABLE_CC1101_TEST      1
#define ENABLE_CC1101_RX_TEST   1
#define ENABLE_CC1101_RAW_PROBE 1
#define ENABLE_CC1101_SIGNAL_FILTER 1

#define CC1101_RX_FREQUENCY 433920000U
#define CC1101_EDGE_DELTA_THRESHOLD    6U
#define CC1101_RSSI_ABS_THRESHOLD_DBM   (-60.0f)
#define CC1101_RSSI_DELTA_THRESHOLD_DB  8.0f
#define CC1101_RSSI_BASELINE_SAMPLES    5U

static const uint8_t cc1101_ook_650khz_async_regs[] = {
    CC1101_IOCFG0,
    0x0D,
    CC1101_FIFOTHR,
    0x07,
    CC1101_PKTCTRL0,
    0x32,
    CC1101_FSCTRL1,
    0x06,
    CC1101_MDMCFG0,
    0x00,
    CC1101_MDMCFG1,
    0x00,
    CC1101_MDMCFG2,
    0x30,
    CC1101_MDMCFG3,
    0x32,
    CC1101_MDMCFG4,
    0x17,
    CC1101_MCSM0,
    0x18,
    CC1101_FOCCFG,
    0x18,
    CC1101_AGCCTRL0,
    0x91,
    CC1101_AGCCTRL1,
    0x00,
    CC1101_AGCCTRL2,
    0x07,
    CC1101_WORCTRL,
    0xFB,
    CC1101_FREND0,
    0x11,
    CC1101_FREND1,
    0xB6,
    0,
    0,
    0x00,
    0xC0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
};

// CC1101 raw identity reported by the chip status registers
// The current board returns 0x00 / 0x14, which is a valid CC1101 signature.
#define CC1101_EXPECTED_PARTNUMBER 0x00
#define CC1101_EXPECTED_VERSION    0x14

#ifndef LCD_RENDER_BACKEND
#define LCD_RENDER_BACKEND LCD_RENDER_BACKEND_GUI
#endif

#if ENABLE_BT_TEST
#include <bt/bt_service/bt_keys_storage.h>
#include <bt/bt_service/bt_keys_filename.h>
#include <profiles/serial_profile.h>
#include <storage/storage.h>

#ifndef BT_KEYS_STORAGE_PATH
#define BT_KEYS_STORAGE_PATH INT_PATH(BT_KEYS_STORAGE_FILE_NAME)
#endif

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

#if ENABLE_LCD_TEST
#if LCD_RENDER_BACKEND == LCD_RENDER_BACKEND_U8G2
#include <u8g2_glue.h>
#elif LCD_RENDER_BACKEND == LCD_RENDER_BACKEND_GUI
#else
#error "Invalid LCD_RENDER_BACKEND"
#endif
#endif

typedef struct {
    uint32_t counter;
    uint8_t pos;
} LcdTestContext;

static volatile uint32_t cc1101_rx_edge_count = 0;
static volatile uint32_t cc1101_rx_last_duration_us = 0;
static volatile bool cc1101_rx_last_level = false;

static void cc1101_board_load_preset(const uint8_t* preset_data) {
    furi_hal_subghz_load_custom_preset(preset_data);
}

static float cc1101_board_get_rssi_dbm(void) {
    return furi_hal_subghz_get_rssi();
}

static void cc1101_board_capture_callback(bool level, uint32_t duration, void* context) {
    UNUSED(context);
    cc1101_rx_last_level = level;
    cc1101_rx_last_duration_us = duration;
    cc1101_rx_edge_count++;
}

static int32_t lcd_test_thread(void* context) {
#if ENABLE_LCD_TEST
    furi_assert(context);
    LcdTestContext* ctx = context;
#else
    UNUSED(context);
#endif
    furi_hal_init();
    furi_hal_rtc_set_log_device(FuriHalRtcLogDeviceUsart);
    furi_hal_serial_control_set_logging_config(FuriHalSerialIdUsart, 230400);
    FURI_LOG_I(TAG, "Main started");

#if ENABLE_CC1101_TEST
    {
        FURI_LOG_I(TAG, "CC1101 HAL init");
        furi_hal_subghz_init();
        FURI_LOG_I(TAG, "CC1101 HAL ready");

        cc1101_board_load_preset(cc1101_ook_650khz_async_regs);
        FURI_LOG_I(TAG, "CC1101 OOK async preset loaded");

        uint32_t real_frequency = furi_hal_subghz_set_frequency(CC1101_RX_FREQUENCY);
        FURI_LOG_I(TAG, "CC1101 frequency set to %lu Hz", (unsigned long)real_frequency);

        furi_hal_subghz_start_async_rx(cc1101_board_capture_callback, NULL);
        FURI_LOG_I(TAG, "CC1101 async RX started");

        uint32_t last_edge_count = 0;
        float rssi_baseline_sum = 0.0f;
        float rssi_baseline = 0.0f;
        uint32_t rssi_baseline_count = 0;
        while(true) {
            uint32_t edge_count = cc1101_rx_edge_count;
            float rssi = cc1101_board_get_rssi_dbm();
            uint32_t edge_delta = edge_count - last_edge_count;

            if(rssi_baseline_count < CC1101_RSSI_BASELINE_SAMPLES) {
                rssi_baseline_sum += rssi;
                rssi_baseline_count++;
                if(rssi_baseline_count == CC1101_RSSI_BASELINE_SAMPLES) {
                    rssi_baseline = rssi_baseline_sum / (float)CC1101_RSSI_BASELINE_SAMPLES;
                    FURI_LOG_I(TAG, "CC1101 idle RSSI baseline=%.1fdBm", (double)rssi_baseline);
                }
            } else {
                bool strong_rssi = (rssi >= CC1101_RSSI_ABS_THRESHOLD_DBM) ||
                                   ((rssi - rssi_baseline) >= CC1101_RSSI_DELTA_THRESHOLD_DB);
                bool edge_burst = edge_delta >= CC1101_EDGE_DELTA_THRESHOLD;

                if(ENABLE_CC1101_SIGNAL_FILTER && (strong_rssi || edge_burst)) {
                    FURI_LOG_I(
                        TAG,
                        "433.92MHz possible signal edges=%lu(+%lu) last_level=%u last_pulse=%luus rssi=%.1fdBm baseline=%.1fdBm",
                        (unsigned long)edge_count,
                        (unsigned long)edge_delta,
                        (unsigned int)cc1101_rx_last_level,
                        (unsigned long)cc1101_rx_last_duration_us,
                        (double)rssi,
                        (double)rssi_baseline);
                } else {
                    FURI_LOG_D(
                        TAG,
                        "433.92MHz idle edges=%lu(+%lu) last_pulse=%luus rssi=%.1fdBm baseline=%.1fdBm",
                        (unsigned long)edge_count,
                        (unsigned long)edge_delta,
                        (unsigned long)cc1101_rx_last_duration_us,
                        (double)rssi,
                        (double)rssi_baseline);
                }
            }

            last_edge_count = edge_count;

            furi_delay_ms(1000);
        }
    }
#endif

#if ENABLE_BT_TEST
    furi_check(furi_hal_bt_start_radio_stack());

    BtKeysStorage* bt_keys = bt_keys_storage_alloc(BT_KEYS_STORAGE_PATH);
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

#if ENABLE_LCD_TEST
    furi_delay_ms(200);

#if LCD_RENDER_BACKEND == LCD_RENDER_BACKEND_U8G2
    u8g2_t u8g2;
    u8g2_Setup_st756x_flipper(&u8g2, U8G2_R0, u8x8_hw_spi_stm32, u8g2_gpio_and_delay_stm32);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);

    FURI_LOG_I(TAG, "Serial LCD test started (u8g2)");
#elif LCD_RENDER_BACKEND == LCD_RENDER_BACKEND_GUI
    Canvas* canvas = canvas_init();

    FURI_LOG_I(TAG, "Serial LCD test started (gui)");
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

        //FURI_LOG_I(TAG, "counter=%lu pos=%u", (unsigned long)ctx->counter, (unsigned int)ctx->pos);

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
#else
    while(true) {
        furi_delay_ms(1000);
    }
#endif

    return 0;
}

int main(void) {
    furi_init();
    furi_hal_init_early();

#if ENABLE_LCD_TEST
    LcdTestContext* context = malloc(sizeof(LcdTestContext));
    context->counter = 0;
    context->pos = 0;
#else
    LcdTestContext* context = NULL;
#endif

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
