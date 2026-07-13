#include <string.h>
#include "../subghz_i.h"
#include "../helpers/subghz_pocsag_settings.h"
#include "../views/subghz_view_pocsag.h"
#include <dolphin/dolphin.h>
#include <lib/flipper_format/flipper_format.h>
#include <lib/subghz/protocols/base.h>
#include <lib/subghz/protocols/pocsag.h>

#define SUBGHZ_POCSAG_PROTOCOL_NAME        "POCSAG"

static bool subghz_scene_pocsag_load_custom_preset(SubGhz* subghz) {
    SubGhzSetting* setting = subghz_txrx_get_setting(subghz->txrx);
    subghz_setting_delete_custom_preset(setting, SUBGHZ_POCSAG_CUSTOM_PRESET_NAME);

    FlipperFormat* temp_fm_preset = flipper_format_string_alloc();
    flipper_format_write_string_cstr(
        temp_fm_preset, "Custom_preset_data", subghz_pocsag_custom_preset_data);
    flipper_format_rewind(temp_fm_preset);
    bool ok = subghz_setting_load_custom_preset(
        setting, SUBGHZ_POCSAG_CUSTOM_PRESET_NAME, temp_fm_preset);
    flipper_format_free(temp_fm_preset);

    return ok;
}

static void subghz_scene_pocsag_update_statusbar(void* context) {
    SubGhz* subghz = context;
    FuriString* count_str = furi_string_alloc();
    furi_string_printf(count_str, "%02u msgs", subghz_view_pocsag_get_count(subghz->subghz_pocsag));

    FuriString* frequency_str = furi_string_alloc();
    FuriString* preset_str = furi_string_alloc();
    SubGhzRadioPreset preset = subghz_txrx_get_preset(subghz->txrx);
    furi_string_printf(
        frequency_str,
        "%03lu.%02lu",
        (unsigned long)(preset.frequency / 1000000 % 1000),
        (unsigned long)(preset.frequency / 10000 % 100));
    furi_string_set_str(preset_str, furi_string_get_cstr(preset.name));

    subghz_view_pocsag_add_data_statusbar(
        subghz->subghz_pocsag,
        furi_string_get_cstr(frequency_str),
        furi_string_get_cstr(preset_str),
        furi_string_get_cstr(count_str));

    furi_string_free(frequency_str);
    furi_string_free(preset_str);
    furi_string_free(count_str);
}

void subghz_scene_pocsag_callback(SubGhzCustomEvent event, void* context) {
    furi_assert(context);
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, event);
}

static void subghz_scene_pocsag_add_to_history_callback(
    SubGhzReceiver* receiver,
    SubGhzProtocolDecoderBase* decoder_base,
    void* context) {
    furi_assert(context);
    SubGhz* subghz = context;

    if(decoder_base->protocol &&
       !strcmp(decoder_base->protocol->name, SUBGHZ_POCSAG_PROTOCOL_NAME)) {
        FuriString* text = furi_string_alloc();
        subghz_protocol_decoder_base_get_string(decoder_base, text);
        furi_string_replace_all(text, "\e#", "");
        furi_string_replace_all(text, "\r\n", " | ");
        subghz_view_pocsag_add_item_to_menu(subghz->subghz_pocsag, furi_string_get_cstr(text));
        subghz_scene_pocsag_update_statusbar(subghz);
        notification_message(subghz->notifications, &sequence_blink_green_10);
        furi_string_free(text);
    }

    subghz_receiver_reset(receiver);
    subghz->pocsag_rx_key_state = SubGhzRxKeyStateAddKey;
}

void subghz_scene_pocsag_on_enter(void* context) {
    SubGhz* subghz = context;

    if(subghz->pocsag_rx_key_state == SubGhzRxKeyStateIDLE) {
        if(!subghz_scene_pocsag_load_custom_preset(subghz)) {
            view_dispatcher_send_custom_event(
                subghz->view_dispatcher, SubGhzCustomEventSceneShowErrorSub);
            return;
        }
        subghz_txrx_set_preset(
        subghz->txrx,
        SUBGHZ_POCSAG_CUSTOM_PRESET_NAME,
        SUBGHZ_POCSAG_DEFAULT_FREQUENCY_HZ,
        NULL,
        0);
        subghz_view_pocsag_clear_history(subghz->subghz_pocsag);
        subghz->pocsag_rx_key_state = SubGhzRxKeyStateStart;
    }

    subghz_view_pocsag_set_radio_device_type(
        subghz->subghz_pocsag, subghz_txrx_radio_device_get(subghz->txrx));
    subghz_view_pocsag_set_callback(
        subghz->subghz_pocsag, subghz_scene_pocsag_callback, subghz);
    subghz_scene_pocsag_update_statusbar(subghz);
    subghz_txrx_set_rx_calback(subghz->txrx, subghz_scene_pocsag_add_to_history_callback, subghz);

    if(subghz_txrx_hopper_get_state(subghz->txrx) != SubGhzHopperStateOFF) {
        subghz_txrx_hopper_set_state(subghz->txrx, SubGhzHopperStateOFF);
    }

    if(subghz->pocsag_rx_key_state == SubGhzRxKeyStateStart) {
        subghz_txrx_rx_start(subghz->txrx);
    }

    subghz->state_notifications = SubGhzNotificationStateRx;

    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdPocsag);
}

bool subghz_scene_pocsag_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubGhzCustomEventViewReceiverBack) {
            subghz_txrx_stop(subghz->txrx);
            subghz_txrx_hopper_set_state(subghz->txrx, SubGhzHopperStateOFF);
            subghz_txrx_set_rx_calback(subghz->txrx, NULL, subghz);
            subghz_set_default_preset(subghz);
            subghz_view_pocsag_clear_history(subghz->subghz_pocsag);
            subghz->pocsag_rx_key_state = SubGhzRxKeyStateIDLE;
            subghz->state_notifications = SubGhzNotificationStateIDLE;
            scene_manager_search_and_switch_to_previous_scene(
                subghz->scene_manager, SubGhzSceneStart);
            consumed = true;
        } else if(event.event == SubGhzCustomEventViewReceiverConfig) {
            subghz->idx_menu_chosen = subghz_view_pocsag_get_idx_menu(subghz->subghz_pocsag);
            scene_manager_next_scene(subghz->scene_manager, SubGhzScenePocsagConfig);
            consumed = true;
        } else if(event.event == SubGhzCustomEventViewReceiverOK) {
            subghz->idx_menu_chosen = subghz_view_pocsag_get_idx_menu(subghz->subghz_pocsag);
            scene_manager_next_scene(subghz->scene_manager, SubGhzScenePocsagInfo);
            consumed = true;
        } else if(event.event == SubGhzCustomEventSceneShowErrorSub) {
            furi_string_set_str(subghz->error_str, "POCSAG preset load failed.");
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneShowErrorSub);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        SubGhzThresholdRssiData ret_rssi = subghz_threshold_get_rssi_data(
            subghz->threshold_rssi, subghz_txrx_radio_device_get_rssi(subghz->txrx));

        subghz_pocsag_rssi(subghz->subghz_pocsag, ret_rssi.rssi);
        subghz_view_pocsag_set_radio_device_type(
            subghz->subghz_pocsag, subghz_txrx_radio_device_get(subghz->txrx));
        if(subghz->state_notifications == SubGhzNotificationStateRx) {
            notification_message(subghz->notifications, &sequence_blink_cyan_10);
        }
    }

    return consumed;
}

void subghz_scene_pocsag_on_exit(void* context) {
    SubGhz* subghz = context;
    subghz_view_pocsag_exit(subghz->subghz_pocsag);
}
