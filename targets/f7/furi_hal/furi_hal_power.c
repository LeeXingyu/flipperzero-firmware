#include <furi_hal_power.h>
#include <furi.h>
#include <furi_hal_rtc.h>
#include <stm32wbxx_ll_cortex.h>

#define TAG "FuriHalPower"

#define POWER_SIM_BATTERY_PCT           100U
#define POWER_SIM_BATTERY_HEALTH        100U
#define POWER_SIM_BATTERY_VOLTAGE       3.7f
#define POWER_SIM_USB_VOLTAGE           5.0f
#define POWER_SIM_BATTERY_CURRENT       0.0f
#define POWER_SIM_BATTERY_TEMPERATURE   25.0f
#define POWER_SIM_CHARGE_VOLTAGE_LIMIT  4.2f
#define POWER_SIM_REMAINING_CAPACITY    2200U
#define POWER_SIM_FULL_CAPACITY         2200U
#define POWER_SIM_DESIGN_CAPACITY       2400U

typedef struct {
    volatile uint8_t insomnia;
    volatile uint8_t suppress_charge;
    bool gauge_ok;
    bool charger_ok;
    bool charging;
    bool charging_done;
    bool otg_enabled;
    float battery_charge_voltage_limit;
    uint8_t battery_pct;
    uint8_t battery_health_pct;
    uint32_t battery_remaining_capacity;
    uint32_t battery_full_capacity;
    uint32_t battery_design_capacity;
    float battery_voltage_v;
    float battery_current_a;
    float usb_voltage_v;
    float battery_temperature_c;
} FuriHalPower;

static FuriHalPower furi_hal_power = {
    .insomnia = 0,
    .suppress_charge = 0,
    .gauge_ok = false,
    .charger_ok = false,
    .charging = false,
    .charging_done = false,
    .otg_enabled = false,
    .battery_charge_voltage_limit = POWER_SIM_CHARGE_VOLTAGE_LIMIT,
    .battery_pct = POWER_SIM_BATTERY_PCT,
    .battery_health_pct = POWER_SIM_BATTERY_HEALTH,
    .battery_remaining_capacity = POWER_SIM_REMAINING_CAPACITY,
    .battery_full_capacity = POWER_SIM_FULL_CAPACITY,
    .battery_design_capacity = POWER_SIM_DESIGN_CAPACITY,
    .battery_voltage_v = POWER_SIM_BATTERY_VOLTAGE,
    .battery_current_a = POWER_SIM_BATTERY_CURRENT,
    .usb_voltage_v = POWER_SIM_USB_VOLTAGE,
    .battery_temperature_c = POWER_SIM_BATTERY_TEMPERATURE,
};

static void furi_hal_power_set_defaults(void) {
    furi_hal_power.gauge_ok = true;
    furi_hal_power.charger_ok = true;
    furi_hal_power.charging = false;
    furi_hal_power.charging_done = false;
    furi_hal_power.otg_enabled = false;
    furi_hal_power.battery_charge_voltage_limit = POWER_SIM_CHARGE_VOLTAGE_LIMIT;
    furi_hal_power.battery_pct = POWER_SIM_BATTERY_PCT;
    furi_hal_power.battery_health_pct = POWER_SIM_BATTERY_HEALTH;
    furi_hal_power.battery_remaining_capacity = POWER_SIM_REMAINING_CAPACITY;
    furi_hal_power.battery_full_capacity = POWER_SIM_FULL_CAPACITY;
    furi_hal_power.battery_design_capacity = POWER_SIM_DESIGN_CAPACITY;
    furi_hal_power.battery_voltage_v = POWER_SIM_BATTERY_VOLTAGE;
    furi_hal_power.battery_current_a = POWER_SIM_BATTERY_CURRENT;
    furi_hal_power.usb_voltage_v = POWER_SIM_USB_VOLTAGE;
    furi_hal_power.battery_temperature_c = POWER_SIM_BATTERY_TEMPERATURE;
}

void furi_hal_power_init(void) {
    furi_hal_power_set_defaults();
    FURI_LOG_I(TAG, "Init OK (simulated backend)");
}

bool furi_hal_power_gauge_is_ok(void) {
    return furi_hal_power.gauge_ok;
}

bool furi_hal_power_is_shutdown_requested(void) {
    return false;
}

uint16_t furi_hal_power_insomnia_level(void) {
    return furi_hal_power.insomnia;
}

void furi_hal_power_insomnia_enter(void) {
    FURI_CRITICAL_ENTER();
    furi_check(furi_hal_power.insomnia < UINT8_MAX);
    furi_hal_power.insomnia++;
    FURI_CRITICAL_EXIT();
}

void furi_hal_power_insomnia_exit(void) {
    FURI_CRITICAL_ENTER();
    furi_check(furi_hal_power.insomnia > 0);
    furi_hal_power.insomnia--;
    FURI_CRITICAL_EXIT();
}

bool furi_hal_power_sleep_available(void) {
    return furi_hal_power.insomnia == 0;
}

void furi_hal_power_sleep(void) {
    __WFI();
}

uint8_t furi_hal_power_get_pct(void) {
    return furi_hal_power.battery_pct;
}

uint8_t furi_hal_power_get_bat_health_pct(void) {
    return furi_hal_power.battery_health_pct;
}

bool furi_hal_power_is_charging(void) {
    return furi_hal_power.charging;
}

bool furi_hal_power_is_charging_done(void) {
    return furi_hal_power.charging_done;
}

void furi_hal_power_shutdown(void) {
    FURI_LOG_W(TAG, "Shutdown requested on simulated power backend");
}

void furi_hal_power_off(void) {
    FURI_LOG_W(TAG, "Power off requested on simulated power backend");
}

FURI_NORETURN void furi_hal_power_reset(void) {
    NVIC_SystemReset();
}

bool furi_hal_power_enable_otg(void) {
    furi_hal_power.otg_enabled = true;
    return true;
}

void furi_hal_power_disable_otg(void) {
    furi_hal_power.otg_enabled = false;
}

bool furi_hal_power_check_otg_fault(void) {
    return false;
}

void furi_hal_power_check_otg_status(void) {
    // No hardware OTG backend on this board.
}

bool furi_hal_power_is_otg_enabled(void) {
    return furi_hal_power.otg_enabled;
}

float furi_hal_power_get_battery_charge_voltage_limit(void) {
    return furi_hal_power.battery_charge_voltage_limit;
}

void furi_hal_power_set_battery_charge_voltage_limit(float voltage) {
    furi_hal_power.battery_charge_voltage_limit = voltage;
}

uint32_t furi_hal_power_get_battery_remaining_capacity(void) {
    return furi_hal_power.battery_remaining_capacity;
}

uint32_t furi_hal_power_get_battery_full_capacity(void) {
    return furi_hal_power.battery_full_capacity;
}

uint32_t furi_hal_power_get_battery_design_capacity(void) {
    return furi_hal_power.battery_design_capacity;
}

float furi_hal_power_get_battery_voltage(FuriHalPowerIC ic) {
    UNUSED(ic);
    return furi_hal_power.battery_voltage_v;
}

float furi_hal_power_get_battery_current(FuriHalPowerIC ic) {
    UNUSED(ic);
    return furi_hal_power.battery_current_a;
}

float furi_hal_power_get_battery_temperature(FuriHalPowerIC ic) {
    UNUSED(ic);
    return furi_hal_power.battery_temperature_c;
}

float furi_hal_power_get_usb_voltage(void) {
    return furi_hal_power.usb_voltage_v;
}

void furi_hal_power_enable_external_3_3v(void) {
    // No external power switch on this board.
}

void furi_hal_power_disable_external_3_3v(void) {
    // No external power switch on this board.
}

void furi_hal_power_suppress_charge_enter(void) {
    FURI_CRITICAL_ENTER();
    furi_hal_power.suppress_charge++;
    FURI_CRITICAL_EXIT();
}

void furi_hal_power_suppress_charge_exit(void) {
    FURI_CRITICAL_ENTER();
    furi_check(furi_hal_power.suppress_charge > 0);
    furi_hal_power.suppress_charge--;
    FURI_CRITICAL_EXIT();
}

void furi_hal_power_info_get(PropertyValueCallback out, char sep, void* context) {
    furi_check(out);

    FuriString* value = furi_string_alloc();
    FuriString* key = furi_string_alloc();

    PropertyValueContext property_context = {
        .key = key,
        .value = value,
        .out = out,
        .sep = sep,
        .last = false,
        .context = context,
    };

    if(sep == '.') {
        property_value_out(&property_context, NULL, 2, "format", "major", "2");
        property_value_out(&property_context, NULL, 2, "format", "minor", "1");
    } else {
        property_value_out(&property_context, NULL, 3, "power", "info", "major", "2");
        property_value_out(&property_context, NULL, 3, "power", "info", "minor", "1");
    }

    uint8_t charge = furi_hal_power_get_pct();
    property_value_out(&property_context, "%u", 2, "charge", "level", charge);

    const char* charge_state = furi_hal_power_is_charging() ? "charging" : "discharging";
    property_value_out(&property_context, NULL, 2, "charge", "state", charge_state);

    uint16_t charge_voltage_limit =
        (uint16_t)(furi_hal_power_get_battery_charge_voltage_limit() * 1000.0f);
    property_value_out(
        &property_context, "%u", 3, "charge", "voltage", "limit", charge_voltage_limit);

    uint16_t voltage = (uint16_t)(furi_hal_power_get_battery_voltage(FuriHalPowerICFuelGauge) * 1000.0f);
    property_value_out(&property_context, "%u", 2, "battery", "voltage", voltage);

    int16_t current = (int16_t)(furi_hal_power_get_battery_current(FuriHalPowerICFuelGauge) * 1000.0f);
    property_value_out(&property_context, "%d", 2, "battery", "current", current);

    int16_t temperature = (int16_t)furi_hal_power_get_battery_temperature(FuriHalPowerICFuelGauge);
    property_value_out(&property_context, "%d", 2, "battery", "temp", temperature);

    property_value_out(&property_context, "%u", 2, "battery", "health", furi_hal_power_get_bat_health_pct());
    property_value_out(
        &property_context,
        "%lu",
        2,
        "capacity",
        "remain",
        furi_hal_power_get_battery_remaining_capacity());
    property_value_out(
        &property_context,
        "%lu",
        2,
        "capacity",
        "full",
        furi_hal_power_get_battery_full_capacity());
    property_context.last = true;
    property_value_out(
        &property_context,
        "%lu",
        2,
        "capacity",
        "design",
        furi_hal_power_get_battery_design_capacity());

    furi_string_free(key);
    furi_string_free(value);
}

void furi_hal_power_debug_get(PropertyValueCallback out, void* context) {
    furi_check(out);

    FuriString* value = furi_string_alloc();
    FuriString* key = furi_string_alloc();

    PropertyValueContext property_context = {
        .key = key,
        .value = value,
        .out = out,
        .sep = '.',
        .last = false,
        .context = context,
    };

    property_value_out(&property_context, NULL, 2, "format", "major", "1");
    property_value_out(&property_context, NULL, 2, "format", "minor", "0");

    property_value_out(
        &property_context,
        "%d",
        2,
        "charger",
        "vbus",
        (int)furi_hal_power.usb_voltage_v * 1000);
    property_value_out(
        &property_context,
        "%d",
        2,
        "charger",
        "vsys",
        (int)furi_hal_power.battery_voltage_v * 1000);
    property_value_out(
        &property_context,
        "%d",
        2,
        "charger",
        "vbat",
        (int)furi_hal_power.battery_voltage_v * 1000);
    property_value_out(
        &property_context,
        "%d",
        2,
        "charger",
        "vreg",
        (int)(furi_hal_power.battery_charge_voltage_limit * 1000.0f));
    property_value_out(
        &property_context,
        "%d",
        2,
        "charger",
        "current",
        (int)(furi_hal_power.battery_current_a * 1000.0f));

    property_value_out(&property_context, "%lu", 2, "charger", "ntc", 25000UL);
    property_value_out(&property_context, "%d", 2, "gauge", "calmd", 1);
    property_value_out(&property_context, "%d", 2, "gauge", "sec", 1);
    property_value_out(&property_context, "%d", 2, "gauge", "edv2", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "vdq", 1);
    property_value_out(&property_context, "%d", 2, "gauge", "initcomp", 1);
    property_value_out(&property_context, "%d", 2, "gauge", "smth", 1);
    property_value_out(&property_context, "%d", 2, "gauge", "btpint", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "cfgupdate", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "chginh", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "fc", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "otd", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "otc", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "sleep", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "ocvfail", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "ocvcomp", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "fd", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "dsg", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "sysdwn", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "tda", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "battpres", 1);
    property_value_out(&property_context, "%d", 2, "gauge", "authgd", 1);
    property_value_out(&property_context, "%d", 2, "gauge", "ocvgd", 1);
    property_value_out(&property_context, "%d", 2, "gauge", "tca", 0);
    property_value_out(&property_context, "%d", 2, "gauge", "rsvd", 0);
    property_value_out(
        &property_context,
        "%d",
        3,
        "gauge",
        "capacity",
        "full",
        (int)furi_hal_power.battery_full_capacity);
    property_value_out(
        &property_context,
        "%d",
        3,
        "gauge",
        "capacity",
        "design",
        (int)furi_hal_power.battery_design_capacity);
    property_value_out(
        &property_context,
        "%d",
        3,
        "gauge",
        "capacity",
        "remain",
        (int)furi_hal_power.battery_remaining_capacity);
    property_value_out(
        &property_context,
        "%d",
        3,
        "gauge",
        "state",
        "charge",
        (int)furi_hal_power.battery_pct);
    property_value_out(&property_context, "%d", 3, "gauge", "state", "health", 100);
    property_value_out(
        &property_context,
        "%d",
        2,
        "gauge",
        "voltage",
        (int)(furi_hal_power.battery_voltage_v * 1000.0f));
    property_value_out(
        &property_context,
        "%d",
        2,
        "gauge",
        "current",
        (int)(furi_hal_power.battery_current_a * 1000.0f));
    property_context.last = true;
    property_value_out(&property_context, "%d", 2, "gauge", "temperature", 25);

    furi_string_free(key);
    furi_string_free(value);
}
