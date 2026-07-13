#include "subghz_view_pocsag.h"

#include "types.h"
#include <input/input.h>
#include <gui/elements.h>
#include <assets_icons.h>
#include <m-array.h>

#define FRAME_HEIGHT 12
#define MAX_LEN_PX   112
#define MENU_ITEMS   4u

#define SUBGHZ_RAW_THRESHOLD_MIN -90.0f

typedef struct {
    FuriString* item_str;
} SubGhzPocsagMenuItem;

ARRAY_DEF(SubGhzPocsagMenuItemArray, SubGhzPocsagMenuItem, M_POD_OPLIST) //-V658

#define M_OPL_SubGhzPocsagMenuItemArray_t() ARRAY_OPLIST(SubGhzPocsagMenuItemArray, M_POD_OPLIST)

typedef struct {
    SubGhzPocsagMenuItemArray_t data;
} SubGhzPocsagHistory;

struct SubGhzViewPocsag {
    View* view;
    SubGhzViewPocsagCallback callback;
    void* context;
};

typedef struct {
    FuriString* frequency_str;
    FuriString* preset_str;
    FuriString* history_stat_str;
    SubGhzPocsagHistory* history;
    uint16_t idx;
    uint16_t list_offset;
    uint16_t history_item;
    uint8_t u_rssi;
    SubGhzRadioDeviceType device_type;
} SubGhzViewPocsagModel;

void subghz_pocsag_rssi(SubGhzViewPocsag* instance, float rssi) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        SubGhzViewPocsagModel * model,
        {
            if(rssi < SUBGHZ_RAW_THRESHOLD_MIN) {
                model->u_rssi = 0;
            } else {
                model->u_rssi = (uint8_t)(rssi - SUBGHZ_RAW_THRESHOLD_MIN);
            }
        },
        true);
}

void subghz_view_pocsag_set_callback(
    SubGhzViewPocsag* subghz_pocsag,
    SubGhzViewPocsagCallback callback,
    void* context) {
    furi_assert(subghz_pocsag);
    furi_assert(callback);
    subghz_pocsag->callback = callback;
    subghz_pocsag->context = context;
}

static void subghz_view_pocsag_update_offset(SubGhzViewPocsag* subghz_pocsag) {
    furi_assert(subghz_pocsag);

    with_view_model(
        subghz_pocsag->view,
        SubGhzViewPocsagModel * model,
        {
            size_t history_item = model->history_item;
            uint16_t bounds = history_item > 3 ? 2 : history_item;

            if(history_item > 3 && model->idx >= (int16_t)(history_item - 1)) {
                model->list_offset = model->idx - 3;
            } else if(model->list_offset < model->idx - bounds) {
                model->list_offset =
                    CLAMP(model->list_offset + 1, (int16_t)(history_item - bounds), 0);
            } else if(model->list_offset > model->idx - bounds) {
                model->list_offset = CLAMP(model->idx - 1, (int16_t)(history_item - bounds), 0);
            }
        },
        true);
}

void subghz_view_pocsag_add_item_to_menu(SubGhzViewPocsag* subghz_pocsag, const char* name) {
    furi_assert(subghz_pocsag);
    with_view_model(
        subghz_pocsag->view,
        SubGhzViewPocsagModel * model,
        {
            SubGhzPocsagMenuItem* item_menu =
                SubGhzPocsagMenuItemArray_push_raw(model->history->data);
            item_menu->item_str = furi_string_alloc_set(name);
            if(model->idx == model->history_item - 1) {
                model->history_item++;
                model->idx++;
            } else {
                model->history_item++;
            }
        },
        true);
    subghz_view_pocsag_update_offset(subghz_pocsag);
}

void subghz_view_pocsag_add_data_statusbar(
    SubGhzViewPocsag* subghz_pocsag,
    const char* frequency_str,
    const char* preset_str,
    const char* history_stat_str) {
    furi_assert(subghz_pocsag);
    with_view_model(
        subghz_pocsag->view,
        SubGhzViewPocsagModel * model,
        {
            furi_string_set(model->frequency_str, frequency_str);
            furi_string_set(model->preset_str, preset_str);
            furi_string_set(model->history_stat_str, history_stat_str);
        },
        true);
}

bool subghz_view_pocsag_get_selected_item(
    SubGhzViewPocsag* subghz_pocsag,
    FuriString* item_text) {
    furi_assert(subghz_pocsag);
    furi_assert(item_text);

    bool result = false;
    with_view_model(
        subghz_pocsag->view,
        SubGhzViewPocsagModel * model,
        {
            if(model->history_item != 0 && model->idx < model->history_item) {
                SubGhzPocsagMenuItem* item_menu =
                    SubGhzPocsagMenuItemArray_get(model->history->data, model->idx);
                furi_string_set(item_text, item_menu->item_str);
                result = true;
            }
        },
        false);

    return result;
}

void subghz_view_pocsag_set_radio_device_type(
    SubGhzViewPocsag* subghz_pocsag,
    SubGhzRadioDeviceType device_type) {
    furi_assert(subghz_pocsag);
    with_view_model(
        subghz_pocsag->view,
        SubGhzViewPocsagModel * model,
        { model->device_type = device_type; },
        true);
}

static void subghz_view_pocsag_draw_frame(Canvas* canvas, uint16_t idx, bool scrollbar) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0 + idx * FRAME_HEIGHT, scrollbar ? 122 : 127, FRAME_HEIGHT);

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_dot(canvas, 0, 0 + idx * FRAME_HEIGHT);
    canvas_draw_dot(canvas, 1, 0 + idx * FRAME_HEIGHT);
    canvas_draw_dot(canvas, 0, (0 + idx * FRAME_HEIGHT) + 1);

    canvas_draw_dot(canvas, 0, (0 + idx * FRAME_HEIGHT) + 11);
    canvas_draw_dot(canvas, scrollbar ? 121 : 126, 0 + idx * FRAME_HEIGHT);
    canvas_draw_dot(canvas, scrollbar ? 121 : 126, (0 + idx * FRAME_HEIGHT) + 11);
}

static void subghz_view_pocsag_rssi_draw(Canvas* canvas, SubGhzViewPocsagModel* model) {
    for(uint8_t i = 1; i < model->u_rssi; i++) {
        if(i % 5) {
            canvas_draw_dot(canvas, 46 + i, 52);
            canvas_draw_dot(canvas, 47 + i, 53);
            canvas_draw_dot(canvas, 46 + i, 54);
        }
    }
}

void subghz_view_pocsag_draw(Canvas* canvas, SubGhzViewPocsagModel* model) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    elements_button_left(canvas, "Config");
    elements_button_center(canvas, "Info");

    bool scrollbar = model->history_item > 4;
    FuriString* str_buff = furi_string_alloc();

    for(size_t i = 0; i < MIN(model->history_item, MENU_ITEMS); ++i) {
        size_t idx = CLAMP((uint16_t)(i + model->list_offset), model->history_item, 0);
        SubGhzPocsagMenuItem* item_menu =
            SubGhzPocsagMenuItemArray_get(model->history->data, idx);
        furi_string_set(str_buff, item_menu->item_str);
        elements_string_fit_width(canvas, str_buff, scrollbar ? MAX_LEN_PX - 7 : MAX_LEN_PX);
        if(model->idx == idx) {
            subghz_view_pocsag_draw_frame(canvas, i, scrollbar);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }
        canvas_draw_icon(canvas, 4, 2 + i * FRAME_HEIGHT, &I_Unlock_7x8);
        canvas_draw_str(canvas, 15, 9 + i * FRAME_HEIGHT, furi_string_get_cstr(str_buff));
        furi_string_reset(str_buff);
    }
    if(scrollbar) {
        elements_scrollbar_pos(canvas, 128, 0, 49, model->idx, model->history_item);
    }
    furi_string_free(str_buff);

    canvas_set_color(canvas, ColorBlack);

    if(model->history_item == 0) {
        canvas_draw_icon(canvas, 0, 0, &I_Scanning_short_96x52);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 66, 34, AlignLeft, AlignTop, "Waiting POC");
        canvas_set_font(canvas, FontSecondary);
    }

    if(model->device_type == SubGhzRadioDeviceTypeInternal) {
        canvas_draw_icon(canvas, 109, 0, &I_Internal_ant_1_9x11);
    } else {
        canvas_draw_icon(canvas, 109, 0, &I_External_ant_1_9x11);
    }

    subghz_view_pocsag_rssi_draw(canvas, model);
    canvas_draw_str(canvas, 44, 62, furi_string_get_cstr(model->frequency_str));
    canvas_draw_str(canvas, 80, 62, furi_string_get_cstr(model->preset_str));
    canvas_draw_str(canvas, 104, 62, furi_string_get_cstr(model->history_stat_str));
}

static bool subghz_view_pocsag_input(InputEvent* event, void* context) {
    furi_assert(context);
    SubGhzViewPocsag* subghz_pocsag = context;

    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        subghz_pocsag->callback(SubGhzCustomEventViewReceiverBack, subghz_pocsag->context);
    } else if(event->key == InputKeyLeft && event->type == InputTypeShort) {
        subghz_pocsag->callback(SubGhzCustomEventViewReceiverConfig, subghz_pocsag->context);
    } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
        with_view_model(
            subghz_pocsag->view,
            SubGhzViewPocsagModel * model,
            {
                if(model->history_item != 0) {
                    subghz_pocsag->callback(
                        SubGhzCustomEventViewReceiverOK, subghz_pocsag->context);
                }
            },
            false);
    } else if(
        event->key == InputKeyUp &&
        (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        with_view_model(
            subghz_pocsag->view,
            SubGhzViewPocsagModel * model,
            {
                if(model->idx != 0) model->idx--;
            },
            true);
    } else if(
        event->key == InputKeyDown &&
        (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        with_view_model(
            subghz_pocsag->view,
            SubGhzViewPocsagModel * model,
            {
                if((model->history_item != 0) && (model->idx != model->history_item - 1))
                    model->idx++;
            },
            true);
    }

    subghz_view_pocsag_update_offset(subghz_pocsag);
    return true;
}

static void subghz_view_pocsag_enter(void* context) {
    UNUSED(context);
}

void subghz_view_pocsag_exit(void* context) {
    UNUSED(context);
}

void subghz_view_pocsag_clear_history(SubGhzViewPocsag* subghz_pocsag) {
    furi_assert(subghz_pocsag);
    with_view_model(
        subghz_pocsag->view,
        SubGhzViewPocsagModel * model,
        {
            furi_string_reset(model->frequency_str);
            furi_string_reset(model->preset_str);
            furi_string_reset(model->history_stat_str);
            for
                M_EACH(item_menu, model->history->data, SubGhzPocsagMenuItemArray_t) {
                    furi_string_free(item_menu->item_str);
                }
            SubGhzPocsagMenuItemArray_reset(model->history->data);
            model->idx = 0;
            model->list_offset = 0;
            model->history_item = 0;
        },
        true);
}

SubGhzViewPocsag* subghz_view_pocsag_alloc(void) {
    SubGhzViewPocsag* subghz_pocsag = malloc(sizeof(SubGhzViewPocsag));

    subghz_pocsag->view = view_alloc();
    view_allocate_model(subghz_pocsag->view, ViewModelTypeLocking, sizeof(SubGhzViewPocsagModel));
    view_set_context(subghz_pocsag->view, subghz_pocsag);
    view_set_draw_callback(subghz_pocsag->view, (ViewDrawCallback)subghz_view_pocsag_draw);
    view_set_input_callback(subghz_pocsag->view, subghz_view_pocsag_input);
    view_set_enter_callback(subghz_pocsag->view, subghz_view_pocsag_enter);
    view_set_exit_callback(subghz_pocsag->view, subghz_view_pocsag_exit);

    with_view_model(
        subghz_pocsag->view,
        SubGhzViewPocsagModel * model,
        {
            model->frequency_str = furi_string_alloc();
            model->preset_str = furi_string_alloc();
            model->history_stat_str = furi_string_alloc();
            model->history = malloc(sizeof(SubGhzPocsagHistory));
            SubGhzPocsagMenuItemArray_init(model->history->data);
            model->device_type = SubGhzRadioDeviceTypeInternal;
        },
        true);

    return subghz_pocsag;
}

void subghz_view_pocsag_free(SubGhzViewPocsag* subghz_pocsag) {
    furi_assert(subghz_pocsag);
    with_view_model(
        subghz_pocsag->view,
        SubGhzViewPocsagModel * model,
        {
            furi_string_free(model->frequency_str);
            furi_string_free(model->preset_str);
            furi_string_free(model->history_stat_str);
            for
                M_EACH(item_menu, model->history->data, SubGhzPocsagMenuItemArray_t) {
                    furi_string_free(item_menu->item_str);
                }
            SubGhzPocsagMenuItemArray_clear(model->history->data);
            free(model->history);
        },
        false);
    view_free(subghz_pocsag->view);
    free(subghz_pocsag);
}

View* subghz_view_pocsag_get_view(SubGhzViewPocsag* subghz_pocsag) {
    furi_assert(subghz_pocsag);
    return subghz_pocsag->view;
}

uint16_t subghz_view_pocsag_get_idx_menu(SubGhzViewPocsag* subghz_pocsag) {
    furi_assert(subghz_pocsag);
    uint32_t idx = 0;
    with_view_model(subghz_pocsag->view, SubGhzViewPocsagModel * model, { idx = model->idx; }, false);
    return idx;
}

uint16_t subghz_view_pocsag_get_count(SubGhzViewPocsag* subghz_pocsag) {
    furi_assert(subghz_pocsag);
    uint32_t count = 0;
    with_view_model(
        subghz_pocsag->view, SubGhzViewPocsagModel * model, { count = model->history_item; }, false);
    return count;
}

void subghz_view_pocsag_set_idx_menu(SubGhzViewPocsag* subghz_pocsag, uint16_t idx) {
    furi_assert(subghz_pocsag);
    with_view_model(
        subghz_pocsag->view,
        SubGhzViewPocsagModel * model,
        {
            model->idx = idx;
            if(model->idx > 2) model->list_offset = idx - 2;
        },
        true);
    subghz_view_pocsag_update_offset(subghz_pocsag);
}
