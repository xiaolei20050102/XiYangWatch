#include "page.h"
#include "pages_config.h"
#include "../Framework/page_manager.h"
#include "../Framework/gesture.h"

static lv_obj_t *root;

extern const lv_image_dsc_t taiwanliangping_32;
extern const lv_image_dsc_t pingmuliangdu_32;
extern const lv_image_dsc_t lanya_32;
extern const lv_image_dsc_t guanyv_32;

typedef struct {
    const lv_image_dsc_t *icon;
    const char           *name;
    page_id_t             target;
} menu_item_t;

static const menu_item_t items[] = {
    { &taiwanliangping_32, "Raise to Wake", PAGE_DISPLAY    },
    { &pingmuliangdu_32,   "Brightness",    PAGE_DISPLAY    },
    { &lanya_32,           "Bluetooth",     PAGE_BLUETOOTH  },
    { &guanyv_32,          "About",         PAGE_ABOUT      },
};

#define ITEM_COUNT (sizeof(items) / sizeof(items[0]))
#define ITEM_H     44
#define LIST_W     224
#define LIST_PAD   8

static lv_obj_t *list;
static lv_obj_t *rows[ITEM_COUNT];
static lv_obj_t *divs[ITEM_COUNT - 1];
static lv_timer_t *sb_timer;

/* ── 滚动条 淡入/淡出 ── */
static void sb_opa_cb(void *var, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)var, v, LV_PART_SCROLLBAR);
}

static void sb_hide_cb(lv_timer_t *t)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, list);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_exec_cb(&a, sb_opa_cb);
    lv_anim_set_duration(&a, 300);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
    sb_timer = NULL;
}

static void on_list_scroll(lv_event_t *e)
{
    lv_anim_delete(list, sb_opa_cb);
    lv_obj_set_style_opa(list, LV_OPA_COVER, LV_PART_SCROLLBAR);

    if (sb_timer)
        lv_timer_reset(sb_timer);
    else {
        sb_timer = lv_timer_create(sb_hide_cb, 1000, NULL);
        lv_timer_set_repeat_count(sb_timer, 1);
    }
}

static void row_translate_cb(void *var, int32_t v)
{
    lv_obj_set_style_translate_y((lv_obj_t *)var, v, 0);
}

static void row_opa_cb(void *var, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)var, v, 0);
}

static void item_click_cb(lv_event_t *e)
{
    page_id_t target = (page_id_t)(uintptr_t)lv_event_get_user_data(e);
    page_manager_push_fade(target);
}

static lv_obj_t *create(lv_obj_t *parent)
{
    root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

    /* scrollable list */
    list = lv_obj_create(root);
    lv_obj_set_size(list, LIST_W, 280 - 20);
    lv_obj_set_style_pad_all(list, LIST_PAD, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(list, 3, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(list, 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(list, lv_color_hex(0xAAAAAA), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(list, LV_OPA_30, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_right(list, 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_opa(list, LV_OPA_TRANSP, LV_PART_SCROLLBAR);
    lv_obj_add_event_cb(list, on_list_scroll, LV_EVENT_SCROLL, NULL);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    #define DIV_W (LIST_W - LIST_PAD * 2 - 52)

    for (int i = 0; i < ITEM_COUNT; i++) {
        lv_obj_t *row = lv_obj_create(list);
        lv_obj_set_size(row, LIST_W - LIST_PAD * 2, ITEM_H);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *icon = lv_image_create(row);
        lv_image_set_src(icon, items[i].icon);
        lv_obj_set_style_pad_left(icon, 12, 0);

        lv_obj_t *label = lv_label_create(row);
        lv_label_set_text(label, items[i].name);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_pad_left(label, 12, 0);

        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, item_click_cb, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)items[i].target);

        lv_obj_set_style_translate_y(row, 35, 0);
        lv_obj_set_style_opa(row, LV_OPA_TRANSP, 0);
        rows[i] = row;

        /* divider */
        if (i < ITEM_COUNT - 1) {
            lv_obj_t *div = lv_obj_create(list);
            lv_obj_set_size(div, DIV_W, 1);
            lv_obj_set_style_pad_all(div, 0, 0);
            lv_obj_set_style_border_width(div, 0, 0);
            lv_obj_set_style_bg_color(div, lv_color_hex(0x2A2A2A), 0);
            lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(div, 0, 0);
            lv_obj_set_style_translate_x(div, (LIST_W - LIST_PAD * 2 - DIV_W) / 2, 0);
            lv_obj_set_style_translate_y(div, 35, 0);
            lv_obj_set_style_opa(div, LV_OPA_TRANSP, 0);
            divs[i] = div;
        }
    }

    /* 级联入场动画 */
    for (int i = 0; i < ITEM_COUNT; i++) {
        uint32_t delay = 200 + i * 100;

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, rows[i]);
        lv_anim_set_values(&a, 35, 0);
        lv_anim_set_exec_cb(&a, row_translate_cb);
        lv_anim_set_duration(&a, 350);
        lv_anim_set_delay(&a, delay);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_start(&a);

        lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_exec_cb(&a, row_opa_cb);
        lv_anim_set_duration(&a, 280);
        lv_anim_start(&a);

        if (i < ITEM_COUNT - 1) {
            lv_anim_set_var(&a, divs[i]);
            lv_anim_set_values(&a, 35, 0);
            lv_anim_set_exec_cb(&a, row_translate_cb);
            lv_anim_set_duration(&a, 350);
            lv_anim_start(&a);

            lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
            lv_anim_set_exec_cb(&a, row_opa_cb);
            lv_anim_set_duration(&a, 280);
            lv_anim_start(&a);
        }
    }

    return root;
}

static void destroy(void)
{
    if (sb_timer) { lv_timer_delete(sb_timer); sb_timer = NULL; }
}

static void update(void) {}

const page_t page_app_settings = {
    .name    = "settings",
    .type    = PAGE_TYPE_OVERLAY,
    .create  = create,
    .destroy = destroy,
    .update  = update,
};
