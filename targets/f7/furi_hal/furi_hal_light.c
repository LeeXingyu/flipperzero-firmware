#include <core/common_defines.h>
#include <furi.h>
#include <furi_hal_light.h>
#include <stdint.h>

#define TAG "FuriHalLight"

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t backlight;
} FuriHalLightState;

static FuriHalLightState light_state;

void furi_hal_light_init(void) {
    light_state.red = 0;
    light_state.green = 0;
    light_state.blue = 0;
    light_state.backlight = 0;
    FURI_LOG_I(TAG, "Init OK (simulated backend)");
}

void furi_hal_light_set(Light light, uint8_t value) {
    if(light & LightRed) {
        light_state.red = value;
    }
    if(light & LightRed) {
        FURI_LOG_T(TAG, "Red=%u", value);
    }
    if(light & LightGreen) {
        light_state.green = value;
        FURI_LOG_T(TAG, "Green=%u", value);
    }
    if(light & LightBlue) {
        light_state.blue = value;
        FURI_LOG_T(TAG, "Blue=%u", value);
    }
    if(light & LightBacklight) {
        light_state.backlight = value;
        FURI_LOG_T(TAG, "Backlight=%u", value);
    }
}

void furi_hal_light_blink_start(Light light, uint8_t brightness, uint16_t on_time, uint16_t period) {
    UNUSED(light);
    UNUSED(brightness);
    UNUSED(on_time);
    UNUSED(period);
    FURI_LOG_T(TAG, "Blink start ignored (simulated backend)");
}

void furi_hal_light_blink_stop(void) {
    FURI_LOG_T(TAG, "Blink stop ignored (simulated backend)");
}

void furi_hal_light_blink_set_color(Light light) {
    UNUSED(light);
    FURI_LOG_T(TAG, "Blink color ignored (simulated backend)");
}

void furi_hal_light_sequence(const char* sequence) {
    furi_check(sequence);
    do {
        if(*sequence == 'R') {
            furi_hal_light_set(LightRed, 0xFF);
        } else if(*sequence == 'r') {
            furi_hal_light_set(LightRed, 0x00);
        } else if(*sequence == 'G') {
            furi_hal_light_set(LightGreen, 0xFF);
        } else if(*sequence == 'g') {
            furi_hal_light_set(LightGreen, 0x00);
        } else if(*sequence == 'B') {
            furi_hal_light_set(LightBlue, 0xFF);
        } else if(*sequence == 'b') {
            furi_hal_light_set(LightBlue, 0x00);
        } else if(*sequence == 'W') {
            furi_hal_light_set(LightBacklight, 0xFF);
        } else if(*sequence == 'w') {
            furi_hal_light_set(LightBacklight, 0x00);
        } else if(*sequence == '.') {
            furi_delay_ms(250);
        } else if(*sequence == '-') {
            furi_delay_ms(500);
        }
        sequence++;
    } while(*sequence != 0);
}
