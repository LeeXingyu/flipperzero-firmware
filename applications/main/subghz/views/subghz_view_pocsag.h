#pragma once

#include <gui/view.h>
#include "../helpers/subghz_types.h"
#include "../helpers/subghz_custom_event.h"

typedef struct SubGhzViewPocsag SubGhzViewPocsag;

typedef void (*SubGhzViewPocsagCallback)(SubGhzCustomEvent event, void* context);

void subghz_pocsag_rssi(SubGhzViewPocsag* instance, float rssi);

void subghz_view_pocsag_set_callback(
    SubGhzViewPocsag* subghz_pocsag,
    SubGhzViewPocsagCallback callback,
    void* context);

SubGhzViewPocsag* subghz_view_pocsag_alloc(void);

void subghz_view_pocsag_free(SubGhzViewPocsag* subghz_pocsag);

View* subghz_view_pocsag_get_view(SubGhzViewPocsag* subghz_pocsag);

void subghz_view_pocsag_add_data_statusbar(
    SubGhzViewPocsag* subghz_pocsag,
    const char* frequency_str,
    const char* preset_str,
    const char* history_stat_str);

void subghz_view_pocsag_add_item_to_menu(SubGhzViewPocsag* subghz_pocsag, const char* name);

uint16_t subghz_view_pocsag_get_idx_menu(SubGhzViewPocsag* subghz_pocsag);

uint16_t subghz_view_pocsag_get_count(SubGhzViewPocsag* subghz_pocsag);

bool subghz_view_pocsag_get_selected_item(
    SubGhzViewPocsag* subghz_pocsag,
    FuriString* item_text);

void subghz_view_pocsag_set_idx_menu(SubGhzViewPocsag* subghz_pocsag, uint16_t idx);

void subghz_view_pocsag_set_radio_device_type(
    SubGhzViewPocsag* subghz_pocsag,
    SubGhzRadioDeviceType device_type);

void subghz_view_pocsag_clear_history(SubGhzViewPocsag* subghz_pocsag);

void subghz_view_pocsag_exit(void* context);
