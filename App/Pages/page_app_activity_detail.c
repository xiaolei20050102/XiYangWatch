#include "page.h"
#include "../Framework/page_manager.h"
#include "../Framework/gesture.h"

/* ── 7 天模拟步数数据 ── */
#define DAYS 7
static const int32_t steps_7d[DAYS] = {
    8200, 10500, 6800, 9300, 11000, 7200, 5400
};
#define ACCENT_COLOR lv_color_hex(0x00E676)
#define BAR_W       18
#define BAR_GAP     8
#define CHART_X     22
#define CHART_Y     56
#define CHART_W     (DAYS * BAR_W + (DAYS - 1) * BAR_GAP)
#define CHART_H     128
#define Y_MAX       10000

static bool gesture_intercept(gesture_t g)
{
    if (g == GESTURE_DOWN) {
        page_manager_pop(GESTURE_DOWN);
        return true;
    }
    if (g == GESTURE_LEFT || g == GESTURE_RIGHT)
        return true;
    return false;
}

static lv_obj_t *mk_label(lv_obj_t *parent)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_bg_opa(l, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(l, 0, 0);
    lv_obj_set_style_pad_all(l, 0, 0);
    return l;
}

static lv_obj_t *create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

    /* 标题 */
    lv_obj_t *title = mk_label(root);
    lv_label_set_text(title, "7-Day Steps");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    /* 返回提示 */
    lv_obj_t *hint = mk_label(root);
    lv_label_set_text(hint, "Swipe down to return");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x666666), 0);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 46);

    /* ── 卡片背景 ── */
    lv_obj_t *card = lv_obj_create(root);
    lv_obj_set_size(card, 218, 210);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 6);

    /* Y 轴网格线 + 标签: 0, 5k, 10k */
    for (int g = 1; g <= 1; g++) {
        lv_obj_t *grid = lv_obj_create(card);
        lv_obj_set_size(grid, CHART_W, 1);
        lv_obj_set_pos(grid, CHART_X, CHART_Y + CHART_H / 2);
        lv_obj_set_style_bg_color(grid, lv_color_hex(0x1A1A1A), 0);
        lv_obj_set_style_bg_opa(grid, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(grid, 0, 0);
        lv_obj_set_style_pad_all(grid, 0, 0);
        lv_obj_set_style_radius(grid, 0, 0);
    }

    lv_obj_t *y0 = mk_label(card);
    lv_label_set_text(y0, "0");
    lv_obj_set_style_text_font(y0, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(y0, lv_color_hex(0x666666), 0);
    lv_obj_align(y0, LV_ALIGN_TOP_LEFT, 6, CHART_Y + CHART_H - 8);

    lv_obj_t *y5 = mk_label(card);
    lv_label_set_text(y5, "5k");
    lv_obj_set_style_text_font(y5, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(y5, lv_color_hex(0x666666), 0);
    lv_obj_align(y5, LV_ALIGN_TOP_LEFT, 2, CHART_Y + CHART_H / 2 - 8);

    lv_obj_t *y10 = mk_label(card);
    lv_label_set_text(y10, "10k");
    lv_obj_set_style_text_font(y10, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(y10, lv_color_hex(0x666666), 0);
    lv_obj_align(y10, LV_ALIGN_TOP_LEFT, 0, CHART_Y - 8);

    /* ── 柱状图 ── */
    for (int i = 0; i < DAYS; i++) {
        int32_t s = steps_7d[i];
        if (s > Y_MAX) s = Y_MAX;
        int32_t bar_h = (s * CHART_H) / Y_MAX;
        if (bar_h < 2) bar_h = 2;

        lv_obj_t *bar = lv_obj_create(card);
        lv_obj_set_size(bar, BAR_W, bar_h);
        lv_obj_set_pos(bar, CHART_X + i * (BAR_W + BAR_GAP),
                       CHART_Y + CHART_H - bar_h);
        lv_obj_set_style_bg_color(bar, ACCENT_COLOR, 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_set_style_pad_all(bar, 0, 0);
        lv_obj_set_style_radius(bar, 4, 0);

        /* 数值标签 */
        lv_obj_t *val = mk_label(card);
        if (s >= 1000)
            lv_label_set_text_fmt(val, "%d.%dk",
                                  (int)(s / 1000), (int)((s % 1000) / 100));
        else
            lv_label_set_text_fmt(val, "%d", (int)s);
        lv_obj_set_style_text_font(val, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(val, lv_color_white(), 0);
        lv_obj_align_to(val, bar, LV_ALIGN_OUT_TOP_MID, 0, -4);

        /* 天数标签 1~7 */
        lv_obj_t *day = mk_label(card);
        lv_label_set_text_fmt(day, "%d", i + 1);
        lv_obj_set_style_text_font(day, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(day, lv_color_hex(0x888888), 0);
        lv_obj_align_to(day, bar, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);
    }

    gesture_set_intercept(gesture_intercept);
    return root;
}

static void destroy(void)
{
    if (gesture_get_intercept() == gesture_intercept)
        gesture_set_intercept(NULL);
}

const page_t page_app_activity_detail = {
    .name    = "activity_detail",
    .type    = PAGE_TYPE_OVERLAY,
    .create  = create,
    .destroy = destroy,
    .update  = NULL,
};
