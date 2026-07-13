#include "../subghz_i.h"
#include "../helpers/subghz_custom_event.h"

void subghz_scene_pocsag_info_callback(GuiButtonType result, InputType type, void* context) {
    furi_assert(context);
    SubGhz* subghz = context;

    if((result == GuiButtonTypeLeft) && (type == InputTypeShort)) {
        view_dispatcher_send_custom_event(
            subghz->view_dispatcher, SubGhzCustomEventViewReceiverBack);
    }
}

void subghz_scene_pocsag_info_on_enter(void* context) {
    SubGhz* subghz = context;
    FuriString* text = furi_string_alloc();
    FuriString* header = furi_string_alloc();
    FuriString* preset_text = furi_string_alloc();
    SubGhzRadioPreset preset = subghz_txrx_get_preset(subghz->txrx);

    if(subghz_view_pocsag_get_selected_item(subghz->subghz_pocsag, text)) {
        furi_string_printf(
            header,
            "\e#POCSAG %02u/%02u\e#\n",
            subghz_view_pocsag_get_idx_menu(subghz->subghz_pocsag) + 1,
            subghz_view_pocsag_get_count(subghz->subghz_pocsag));
        furi_string_printf(
            preset_text,
            "%03lu.%02lu %s",
            (unsigned long)(preset.frequency / 1000000 % 1000),
            (unsigned long)(preset.frequency / 10000 % 100),
            furi_string_get_cstr(preset.name));
        furi_string_cat_printf(
            header,
            "%s\n\n%s",
            furi_string_get_cstr(preset_text),
            furi_string_get_cstr(text));
    } else {
        furi_string_set_str(header, "\e#POCSAG Info\e#\n\nNo message selected");
    }

    widget_add_text_box_element(
        subghz->widget,
        0,
        0,
        128,
        54,
        AlignLeft,
        AlignTop,
        furi_string_get_cstr(header),
        false);

    widget_add_button_element(
        subghz->widget,
        GuiButtonTypeLeft,
        "Back",
        subghz_scene_pocsag_info_callback,
        subghz);

    furi_string_free(preset_text);
    furi_string_free(header);
    furi_string_free(text);

    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdWidget);
}

bool subghz_scene_pocsag_info_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;
    if(event.type == SceneManagerEventTypeBack) {
        return scene_manager_previous_scene(subghz->scene_manager);
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubGhzCustomEventViewReceiverBack) {
            return scene_manager_previous_scene(subghz->scene_manager);
        }
    }

    UNUSED(subghz);
    return false;
}

void subghz_scene_pocsag_info_on_exit(void* context) {
    SubGhz* subghz = context;
    widget_reset(subghz->widget);
}
