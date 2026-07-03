#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_resources.h>
#include <furi_hal_spi_config.h>
#include <furi_hal_serial_control.h>
#include <furi_hal_cortex.h>
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

#define CC1101_RX_FREQUENCY 433920000U

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

static const GpioPin cc1101_board_g0 = {.port = GPIOA, .pin = LL_GPIO_PIN_1};
static const GpioPin cc1101_board_cs = {.port = GPIOD, .pin = LL_GPIO_PIN_0};
static const GpioPin cc1101_board_miso = {.port = GPIOB, .pin = LL_GPIO_PIN_4};
static const GpioPin cc1101_board_mosi = {.port = GPIOB, .pin = LL_GPIO_PIN_5};
static const GpioPin cc1101_board_sck = {.port = GPIOA, .pin = LL_GPIO_PIN_5};

static volatile uint32_t cc1101_rx_edge_count = 0;
static volatile uint32_t cc1101_rx_last_duration_us = 0;
static volatile uint32_t cc1101_rx_last_edge_cyccnt = 0;
static volatile bool cc1101_rx_last_level = false;

static const FuriHalSpiBusHandle cc1101_board_spi;

static void cc1101_board_g0_exti_callback(void* context) {
    UNUSED(context);

    static uint32_t low_start_cyccnt = 0;
    static bool have_low_start = false;
    const bool level = furi_hal_gpio_read(&cc1101_board_g0);
    const uint32_t now = DWT->CYCCNT;

    cc1101_rx_last_level = level;
    cc1101_rx_last_edge_cyccnt = now;
    cc1101_rx_edge_count++;

    if(level) {
        if(have_low_start) {
            cc1101_rx_last_duration_us =
                (now - low_start_cyccnt) / furi_hal_cortex_instructions_per_microsecond();
        }
    } else {
        low_start_cyccnt = now;
        have_low_start = true;
    }
}

static void cc1101_board_load_preset(const uint8_t* preset_data) {
    uint32_t i = 0;
    while(preset_data[i]) {
        cc1101_write_reg(&cc1101_board_spi, preset_data[i], preset_data[i + 1]);
        i += 2;
    }

    uint8_t pa[8] = {0};
    memcpy(pa, &preset_data[i + 2], sizeof(pa));
    cc1101_set_pa_table(&cc1101_board_spi, pa);
}

static float cc1101_board_get_rssi_dbm(void) {
    int32_t rssi_raw = cc1101_get_rssi(&cc1101_board_spi);
    float rssi = rssi_raw;

    if(rssi_raw >= 128) {
        rssi = ((rssi - 256.0f) / 2.0f) - 74.0f;
    } else {
        rssi = (rssi / 2.0f) - 74.0f;
    }

    return rssi;
}

static void cc1101_board_spi_handle_event_callback(
    const FuriHalSpiBusHandle* handle,
    FuriHalSpiBusHandleEvent event) {
    if(event == FuriHalSpiBusHandleEventInit) {
        furi_hal_gpio_write(handle->cs, true);
        furi_hal_gpio_init(handle->cs, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    } else if(event == FuriHalSpiBusHandleEventDeinit) {
        furi_hal_gpio_write(handle->cs, true);
        furi_hal_gpio_init(handle->cs, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    } else if(event == FuriHalSpiBusHandleEventActivate) {
        LL_SPI_Init(handle->bus->spi, (LL_SPI_InitTypeDef*)&furi_hal_spi_preset_1edge_low_8m);
        LL_SPI_SetRxFIFOThreshold(handle->bus->spi, LL_SPI_RX_FIFO_TH_QUARTER);
        LL_SPI_Enable(handle->bus->spi);

        furi_hal_gpio_init_ex(
            handle->miso,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn5SPI1);
        furi_hal_gpio_init_ex(
            handle->mosi,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn5SPI1);
        furi_hal_gpio_init_ex(
            handle->sck,
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn5SPI1);

        furi_hal_gpio_write(handle->cs, false);
    } else if(event == FuriHalSpiBusHandleEventDeactivate) {
        furi_hal_gpio_write(handle->cs, true);
        furi_hal_gpio_init(handle->miso, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        furi_hal_gpio_init(handle->mosi, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        furi_hal_gpio_init(handle->sck, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        LL_SPI_Disable(handle->bus->spi);
    }
}

static const FuriHalSpiBusHandle cc1101_board_spi = {
    .bus = &furi_hal_spi_bus_r,
    .callback = cc1101_board_spi_handle_event_callback,
    .miso = &cc1101_board_miso,
    .mosi = &cc1101_board_mosi,
    .sck = &cc1101_board_sck,
    .cs = &cc1101_board_cs,
};

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
        furi_hal_gpio_init(&cc1101_board_g0, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        FURI_LOG_I(TAG, "furi_hal_gpio_init ended");
        furi_hal_spi_bus_handle_init(&cc1101_board_spi);
        FURI_LOG_I(TAG, "furi_hal_spi_bus_handle_init ended");
        furi_hal_bus_enable(FuriHalBusSPI1);
        FURI_LOG_I(TAG, "FuriHalBusSPI1 enabled");
        cc1101_board_spi.bus->current_handle = &cc1101_board_spi;
        cc1101_board_spi.callback(&cc1101_board_spi, FuriHalSpiBusHandleEventActivate);
        FURI_LOG_I(TAG, "cc1101 spi handle activated");
        cc1101_reset(&cc1101_board_spi);
        FURI_LOG_I(TAG, "ENABLE_CC1101_cc1101_reset started");
        CC1101StatusRaw status = {.status = cc1101_get_status(&cc1101_board_spi)};
        uint8_t partnumber = cc1101_get_partnumber(&cc1101_board_spi);
        uint8_t version = cc1101_get_version(&cc1101_board_spi);
        FURI_LOG_I(
            TAG,
            "CC1101 status raw=0x%02X state=%u rdyn=%u",
            status.status_raw,
            status.status.STATE,
            status.status.CHIP_RDYn);
        FURI_LOG_I(TAG, "CC1101 raw partnumber=0x%02X version=0x%02X", partnumber, version);

        if((partnumber == CC1101_EXPECTED_PARTNUMBER) && (version == CC1101_EXPECTED_VERSION)) {
            FURI_LOG_I(TAG, "CC1101 board detected");
            FURI_LOG_I(TAG, "CC1101 board chip %u, version %u", partnumber, version);
        } else {
            FURI_LOG_E(TAG, "CC1101 board not detected");
        }

#if ENABLE_CC1101_RX_TEST
        cc1101_reset(&cc1101_board_spi);
        FURI_LOG_I(TAG, "CC1101 RX test reset done");

        cc1101_board_load_preset(cc1101_ook_650khz_async_regs);
        FURI_LOG_I(TAG, "CC1101 OOK async preset loaded");

        cc1101_set_frequency(&cc1101_board_spi, CC1101_RX_FREQUENCY);
        FURI_LOG_I(TAG, "CC1101 frequency set to %lu Hz", (unsigned long)CC1101_RX_FREQUENCY);

        furi_hal_gpio_add_int_callback(&cc1101_board_g0, cc1101_board_g0_exti_callback, NULL);
        furi_hal_gpio_write(&cc1101_board_g0, true);
        furi_hal_gpio_init(&cc1101_board_g0, GpioModeInterruptRiseFall, GpioPullNo, GpioSpeedLow);
        furi_hal_gpio_enable_int_callback(&cc1101_board_g0);
        FURI_LOG_I(TAG, "CC1101 GDO0 interrupt armed");

        cc1101_switch_to_rx(&cc1101_board_spi);
        FURI_LOG_I(TAG, "CC1101 switched to RX");

        uint32_t last_edge_count = 0;
        while(true) {
            uint32_t edge_count = cc1101_rx_edge_count;
            float rssi = cc1101_board_get_rssi_dbm();

            if(edge_count != last_edge_count) {
                last_edge_count = edge_count;
                FURI_LOG_I(
                    TAG,
                    "433.92MHz activity edges=%lu last_level=%u last_pulse=%luus rssi=%.1fdBm",
                    (unsigned long)edge_count,
                    (unsigned int)cc1101_rx_last_level,
                    (unsigned long)cc1101_rx_last_duration_us,
                    (double)rssi);
            } else {
                FURI_LOG_I(
                    TAG,
                    "433.92MHz idle edges=%lu last_pulse=%luus rssi=%.1fdBm",
                    (unsigned long)edge_count,
                    (unsigned long)cc1101_rx_last_duration_us,
                    (double)rssi);
            }

            furi_delay_ms(1000);
        }
#endif

        cc1101_board_spi.callback(&cc1101_board_spi, FuriHalSpiBusHandleEventDeactivate);
        cc1101_board_spi.bus->current_handle = NULL;
        furi_hal_bus_disable(FuriHalBusSPI1);
        FURI_LOG_I(TAG, "cc1101 spi handle deactivated");
        furi_hal_spi_bus_handle_deinit(&cc1101_board_spi);
        FURI_LOG_I(TAG, "furi_hal_spi_bus_handle_deinit ended");
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
