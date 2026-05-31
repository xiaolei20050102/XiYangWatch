#include "page.h"
#include "pages_config.h"
#include "../Framework/page_manager.h"
#include "../Framework/gesture.h"

static lv_obj_t *root;

extern const lv_image_dsc_t activity_32;
extern const lv_image_dsc_t aixin_32;
extern const lv_image_dsc_t blood_oxygen_32;
extern const lv_image_dsc_t huwai_32;
extern const lv_image_dsc_t concentration_32;
extern const lv_image_dsc_t second_chronograph_32;
extern const lv_image_dsc_t alarm_clock_32;
extern const lv_image_dsc_t counter_32;
extern const lv_image_dsc_t flashlight_32;
extern const lv_image_dsc_t set_32;

typedef struct {
    const lv_image_dsc_t *icon;
    const char           *name;
    page_id_t             target;
} menu_item_t;

static const menu_item_t items[] = {
    { &activity_32,            "Activity",     PAGE_APP_ACTIVITY    },
    { &aixin_32,               "Heart Rate",   PAGE_APP_HEARTRATE   },
    { &blood_oxygen_32,        "Blood Oxygen", PAGE_APP_SPO2        },
    { &huwai_32,              "Outdoors",     PAGE_APP_ENVIRONMENT },
    { &concentration_32,       "Focus",        PAGE_APP_FOCUS       },
    { &second_chronograph_32,  "Stopwatch",    PAGE_APP_STOPWATCH   },
    { &alarm_clock_32,         "Alarm",        PAGE_APP_ALARM       },
    { &counter_32,             "Calculator",   PAGE_APP_CALCULATOR  },
    { &flashlight_32,          "Flashlight",   PAGE_APP_FLASHLIGHT  },
    { &set_32,                 "Settings",     PAGE_APP_SETTINGS    },
};

#define ITEM_COUNT (sizeof(items) / sizeof(items[0]))
#define ITEM_H     44
#define LIST_W     224
#define LIST_PAD   8

static lv_obj_t *list;
static lv_obj_t *rows[ITEM_COUNT];
static lv_obj_t *divs[ITEM_COUNT - 1];
static lv_timer_t *sb_timer;
static int32_t saved_scroll_y;

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
    /* 终止正在进行的淡出动画，立即恢复可见 */
    lv_anim_delete(list, sb_opa_cb);
    lv_obj_set_style_opa(list, LV_OPA_COVER, LV_PART_SCROLLBAR);

    /* 重置 1 秒隐藏定时器 */
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
    /* 滚动条样式：极窄、圆角、半透明 */
    lv_obj_set_style_width(list, 3, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(list, 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(list, lv_color_hex(0xAAAAAA), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(list, LV_OPA_30, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_right(list, 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_opa(list, LV_OPA_TRANSP, LV_PART_SCROLLBAR);  /* 初始隐藏 */
    lv_obj_add_event_cb(list, on_list_scroll, LV_EVENT_SCROLL, NULL);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 20);  /* 20px = 状态栏高度，避免重叠 */

    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    #define DIV_W   (LIST_W - LIST_PAD * 2 - 52)  /* 156: from icon-right to row-right */
    bool returning = (saved_scroll_y > 0);

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
        if (i == 0) {
            lv_obj_set_style_image_recolor(icon, lv_color_hex(0x00E676), 0);
            lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
        }
        if (i == 2) {
            lv_obj_set_style_image_recolor(icon, lv_color_hex(0x00C9E0), 0);
            lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
        }

        lv_obj_t *label = lv_label_create(row);
        lv_label_set_text(label, items[i].name);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_pad_left(label, 12, 0);

        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, item_click_cb, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)items[i].target);

        /* 首次进入：初始偏移+透明，等入场动画；返回时同样初始状态 */
        lv_obj_set_style_translate_y(row, 35, 0);
        lv_obj_set_style_opa(row, LV_OPA_TRANSP, 0);
        rows[i] = row;

        /* divider (except after last item) */
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

    int32_t content_h = ITEM_COUNT * ITEM_H;
    if (content_h > (280 - 20)) {
        lv_obj_set_height(list, 280 - 20);
    } else {
        lv_obj_set_height(list, content_h);
    }

    /* 级联入场动画 */
    {
        int center_idx;
        if (returning) {
            /* 返回时：以可见中心为基准，向外扩散 */
            center_idx = (saved_scroll_y + lv_obj_get_content_height(list) / 2) / ITEM_H;
        } else {
            /* 首次：从顶部向下级联 */
            center_idx = 0;
        }
        for (int i = 0; i < ITEM_COUNT; i++) {
            uint32_t delay = returning
                ? (uint32_t)(50 + LV_ABS(i - center_idx) * 60)
                : (uint32_t)(200 + i * 100);

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

            /* divider slides up + fades in */
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
    }

    /* 恢复上次滚动位置 */
    if (saved_scroll_y > 0) {
        lv_obj_scroll_to_y(list, saved_scroll_y, LV_ANIM_OFF);
    }

    return root;
}

static void destroy(void)
{
    saved_scroll_y = lv_obj_get_scroll_y(list);
    if (sb_timer) { lv_timer_delete(sb_timer); sb_timer = NULL; }
}
static void update(void) {}

const page_t page_menu = {
    .name = "menu",
    .type = PAGE_TYPE_OVERLAY,
    .create = create,
    .destroy = destroy,
    .update = update,
};
