#include "../subghz_i.h"
#include <lib/toolbox/value_index.h>

enum SubGhzPocsagSettingIndex {
    SubGhzPocsagSettingIndexFrequency,
    SubGhzPocsagSettingIndexModulation,
    SubGhzPocsagSettingIndexHopping,
    SubGhzPocsagSettingIndexLock,
};

#define POCSAG_HOPPING_COUNT 2
static const char* const pocsag_hopping_text[POCSAG_HOPPING_COUNT] = {
    "OFF",
    "ON",
};
static const uint32_t pocsag_hopping_value[POCSAG_HOPPING_COUNT] = {
    SubGhzHopperStateOFF,
    SubGhzHopperStateRunnig,
};

static uint8_t subghz_scene_pocsag_config_next_frequency(const uint32_t value, void* context) {
    furi_assert(context);
    SubGhz* subghz = context;
    SubGhzSetting* setting = subghz_txrx_get_setting(subghz->txrx);

    uint8_t index = 0;
    for(uint8_t i = 0; i < subghz_setting_get_frequency_count(setting); i++) {
        if(value == subghz_setting_get_frequency(setting, i)) {
            index = i;
            break;
        } else {
            index = subghz_setting_get_frequency_default_index(setting);
        }
    }
    return index;
}

static uint8_t subghz_scene_pocsag_config_next_preset(const char* preset_name, void* context) {
    furi_assert(context);
    SubGhz* subghz = context;
    SubGhzSetting* setting = subghz_txrx_get_setting(subghz->txrx);

    uint8_t index = 0;
    for(uint8_t i = 0; i < subghz_setting_get_preset_count(setting); i++) {
        if(!strcmp(subghz_setting_get_preset_name(setting, i), preset_name)) {
            index = i;
            break;
        }
    }
    return index;
}

static void subghz_scene_pocsag_config_set_frequency(VariableItem* item) {
    SubGhz* subghz = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    SubGhzSetting* setting = subghz_txrx_get_setting(subghz->txrx);
    SubGhzRadioPreset preset = subghz_txrx_get_preset(subghz->txrx);
    uint32_t frequency = subghz_setting_get_frequency(setting, index);
    char text_buf[10] = {0};

    snprintf(
        text_buf,
        sizeof(text_buf),
        "%lu.%02lu",
        frequency / 1000000,
        (frequency % 1000000) / 10000);
    variable_item_set_current_value_text(item, text_buf);
    subghz_txrx_set_preset(
        subghz->txrx,
        furi_string_get_cstr(preset.name),
        frequency,
        preset.data,
        preset.data_size);
}

static void subghz_scene_pocsag_config_set_preset(VariableItem* item) {
    SubGhz* subghz = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    SubGhzSetting* setting = subghz_txrx_get_setting(subghz->txrx);
    SubGhzRadioPreset preset = subghz_txrx_get_preset(subghz->txrx);

    variable_item_set_current_value_text(item, subghz_setting_get_preset_name(setting, index));
    subghz_txrx_set_preset(
        subghz->txrx,
        subghz_setting_get_preset_name(setting, index),
        preset.frequency,
        subghz_setting_get_preset_data(setting, index),
        subghz_setting_get_preset_data_size(setting, index));
}

static void subghz_scene_pocsag_config_set_hopping_running(VariableItem* item) {
    SubGhz* subghz = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    SubGhzSetting* setting = subghz_txrx_get_setting(subghz->txrx);
    VariableItem* frequency_item = (VariableItem*)scene_manager_get_scene_state(
        subghz->scene_manager, SubGhzScenePocsagConfig);

    variable_item_set_current_value_text(item, pocsag_hopping_text[index]);
    if(pocsag_hopping_value[index] == SubGhzHopperStateOFF) {
        char text_buf[10] = {0};
        uint32_t frequency = subghz_setting_get_default_frequency(setting);
        SubGhzRadioPreset preset = subghz_txrx_get_preset(subghz->txrx);

        snprintf(
            text_buf,
            sizeof(text_buf),
            "%lu.%02lu",
            frequency / 1000000,
            (frequency % 1000000) / 10000);
        variable_item_set_current_value_text(frequency_item, text_buf);

        subghz_txrx_set_preset(
            subghz->txrx,
            furi_string_get_cstr(preset.name),
            frequency,
            preset.data,
            preset.data_size);
        variable_item_set_current_value_index(
            frequency_item, subghz_setting_get_frequency_default_index(setting));
    } else {
        variable_item_set_current_value_text(frequency_item, " -----");
        variable_item_set_current_value_index(
            frequency_item, subghz_setting_get_frequency_default_index(setting));
    }

    subghz_txrx_hopper_set_state(subghz->txrx, pocsag_hopping_value[index]);
}

static void subghz_scene_pocsag_config_var_list_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    SubGhz* subghz = context;
    if(index == SubGhzPocsagSettingIndexLock) {
        view_dispatcher_send_custom_event(
            subghz->view_dispatcher, SubGhzCustomEventSceneSettingLock);
    }
}

void subghz_scene_pocsag_config_on_enter(void* context) {
    SubGhz* subghz = context;
    VariableItem* item;
    uint8_t value_index;
    SubGhzSetting* setting = subghz_txrx_get_setting(subghz->txrx);
    SubGhzRadioPreset preset = subghz_txrx_get_preset(subghz->txrx);

    item = variable_item_list_add(
        subghz->variable_item_list,
        "Frequency:",
        subghz_setting_get_frequency_count(setting),
        subghz_scene_pocsag_config_set_frequency,
        subghz);
    value_index = subghz_scene_pocsag_config_next_frequency(preset.frequency, subghz);
    scene_manager_set_scene_state(
        subghz->scene_manager, SubGhzScenePocsagConfig, (uint32_t)item);
    variable_item_set_current_value_index(item, value_index);
    char text_buf[10] = {0};
    uint32_t frequency = subghz_setting_get_frequency(setting, value_index);
    snprintf(
        text_buf,
        sizeof(text_buf),
        "%lu.%02lu",
        frequency / 1000000,
        (frequency % 1000000) / 10000);
    variable_item_set_current_value_text(item, text_buf);

    item = variable_item_list_add(
        subghz->variable_item_list,
        "Hopping:",
        POCSAG_HOPPING_COUNT,
        subghz_scene_pocsag_config_set_hopping_running,
        subghz);
    value_index = subghz_txrx_hopper_get_state(subghz->txrx);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, pocsag_hopping_text[value_index]);

    item = variable_item_list_add(
        subghz->variable_item_list,
        "Modulation:",
        subghz_setting_get_preset_count(setting),
        subghz_scene_pocsag_config_set_preset,
        subghz);
    value_index = subghz_scene_pocsag_config_next_preset(furi_string_get_cstr(preset.name), subghz);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, subghz_setting_get_preset_name(setting, value_index));

    variable_item_list_add(subghz->variable_item_list, "Lock Keyboard", 1, NULL, NULL);
    variable_item_list_set_enter_callback(
        subghz->variable_item_list,
        subghz_scene_pocsag_config_var_list_enter_callback,
        subghz);

    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdVariableItemList);
}

bool subghz_scene_pocsag_config_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubGhzCustomEventSceneSettingLock) {
            subghz_lock(subghz);
            scene_manager_previous_scene(subghz->scene_manager);
            consumed = true;
        }
    }

    return consumed;
}

void subghz_scene_pocsag_config_on_exit(void* context) {
    SubGhz* subghz = context;
    variable_item_list_set_selected_item(subghz->variable_item_list, 0);
    variable_item_list_reset(subghz->variable_item_list);
}
