#include "page.h"
#include "../Framework/page_manager.h"
#include "../Framework/gesture.h"

/* ── 24 小时模拟血氧数据 ── */
#define SP_N   24
#define CURVE_N 24
static const int sp_24h[SP_N] = {
    97, 96, 94, 91, 88, 86, 89, 93,
    97, 99,100, 98, 96, 94, 91, 87,
    85, 89, 93, 96, 98, 99, 97, 95
};
static lv_point_precise_t sp_pts[CURVE_N];

#define SPO2_COLOR  lv_color_hex(0x00C9E0)

/* ── 创建无背景无边距的 label ── */
static lv_obj_t *mk_label(lv_obj_t *parent)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_bg_opa(l, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(l, 0, 0);
    lv_obj_set_style_pad_all(l, 0, 0);
    return l;
}

/* ── 样条插值 ── */
static void gen_curve(const int *data, int n,
                      lv_point_precise_t *pts, int pts_n,
                      int cx, int cy, int cw, int ch,
                      int y_min, int y_range)
{
    for (int i = 0; i < pts_n; i++) {
        int pos_q8  = (i * (n - 1) * 256) / (pts_n - 1);
        int idx     = pos_q8 >> 8;
        int frac_q8 = pos_q8 & 0xFF;

        if (idx < 0)      { idx = 0;      frac_q8 = 0; }
        if (idx >= n - 1) { idx = n - 2;  frac_q8 = 256; }

        int v_q8 = data[idx] * (256 - frac_q8) + data[idx + 1] * frac_q8;
        int v    = v_q8 >> 8;

        pts[i].x = (lv_value_precise_t)(cx + (i * cw) / (pts_n - 1));
        pts[i].y = (lv_value_precise_t)(cy + ch - ((v - y_min) * ch) / y_range);
    }
}

/* ── min / max / avg ── */
static void calc_stats(const int *data, int n, int *min, int *max, int *avg)
{
    *min = 999; *max = 0;
    int sum = 0;
    for (int i = 0; i < n; i++) {
        if (data[i] < *min) *min = data[i];
        if (data[i] > *max) *max = data[i];
        sum += data[i];
    }
    *avg = sum / n;
}

/* ── 手势：下滑返回，左右滑拦截掉不处理 ── */
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
    lv_label_set_text(title, "Blood Oxygen | 24h");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    /* 返回提示 */
    lv_obj_t *hint = mk_label(root);
    lv_label_set_text(hint, "Swipe down to return");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x666666), 0);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 48);

    /* ── 卡片 ── */
    int h_min, h_max, h_avg;
    calc_stats(sp_24h, SP_N, &h_min, &h_max, &h_avg);

    lv_obj_t *card = lv_obj_create(root);
    lv_obj_set_size(card, 216, 168);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, 0);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 10);

    /* Avg */
    lv_obj_t *stat_avg = mk_label(card);
    lv_label_set_text_fmt(stat_avg, "Avg  %d%%", h_avg);
    lv_obj_set_style_text_font(stat_avg, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(stat_avg, SPO2_COLOR, 0);
    lv_obj_align(stat_avg, LV_ALIGN_TOP_LEFT, 14, 14);

    /* Min / Max */
    lv_obj_t *stat_range = mk_label(card);
    lv_label_set_text_fmt(stat_range, "Min %d%%  |  Max %d%%", h_min, h_max);
    lv_obj_set_style_text_font(stat_range, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(stat_range, lv_color_hex(0x888888), 0);
    lv_obj_align(stat_range, LV_ALIGN_TOP_LEFT, 14, 40);

    /* ── 图表 ── */
    int cx = 36, cw = 166, chart_h = 64;
    int chart_y = 72;

    for (int g = 1; g <= 2; g++) {
        lv_obj_t *grid = lv_obj_create(card);
        lv_obj_set_size(grid, cw, 1);
        lv_obj_set_pos(grid, cx, chart_y + g * (chart_h / 3));
        lv_obj_set_style_bg_color(grid, lv_color_hex(0x1A1A1A), 0);
        lv_obj_set_style_bg_opa(grid, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(grid, 0, 0);
        lv_obj_set_style_pad_all(grid, 0, 0);
        lv_obj_set_style_radius(grid, 0, 0);
    }

    /* Y 轴标签 */
    lv_obj_t *yl0 = mk_label(card);
    lv_label_set_text(yl0, "100");
    lv_obj_set_style_text_font(yl0, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(yl0, lv_color_hex(0x888888), 0);
    lv_obj_align(yl0, LV_ALIGN_TOP_LEFT, 2, chart_y - 7);

    lv_obj_t *yl1 = mk_label(card);
    lv_label_set_text(yl1, "67");
    lv_obj_set_style_text_font(yl1, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(yl1, lv_color_hex(0x888888), 0);
    lv_obj_align(yl1, LV_ALIGN_TOP_LEFT, 2, chart_y + chart_h / 3 - 8);

    lv_obj_t *yl2 = mk_label(card);
    lv_label_set_text(yl2, "33");
    lv_obj_set_style_text_font(yl2, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(yl2, lv_color_hex(0x888888), 0);
    lv_obj_align(yl2, LV_ALIGN_TOP_LEFT, 2, chart_y + chart_h * 2 / 3 - 8);

    lv_obj_t *yl3 = mk_label(card);
    lv_label_set_text(yl3, "0");
    lv_obj_set_style_text_font(yl3, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(yl3, lv_color_hex(0x888888), 0);
    lv_obj_align(yl3, LV_ALIGN_TOP_LEFT, 2, chart_y + chart_h - 9);

    gen_curve(sp_24h, SP_N, sp_pts, CURVE_N,
              cx, chart_y, cw, chart_h, 0, 100);

    lv_obj_t *line = lv_line_create(card);
    lv_line_set_points_mutable(line, sp_pts, CURVE_N);
    lv_obj_set_style_line_color(line, SPO2_COLOR, 0);
    lv_obj_set_style_line_width(line, 2, 0);

    /* 终点圆点 */
    lv_point_precise_t last = sp_pts[CURVE_N - 1];
    lv_obj_t *dot = lv_obj_create(card);
    lv_obj_set_size(dot, 6, 6);
    lv_obj_set_pos(dot, (lv_coord_t)last.x - 3, (lv_coord_t)last.y - 3);
    lv_obj_set_style_radius(dot, 3, 0);
    lv_obj_set_style_bg_color(dot, SPO2_COLOR, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_pad_all(dot, 0, 0);

    /* 时间轴 */
    lv_obj_t *t0 = mk_label(card);
    lv_label_set_text(t0, "0:00");
    lv_obj_set_style_text_font(t0, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(t0, lv_color_hex(0x666666), 0);
    lv_obj_set_pos(t0, cx, chart_y + chart_h + 4);

    lv_obj_t *t12 = mk_label(card);
    lv_label_set_text(t12, "12:00");
    lv_obj_set_style_text_font(t12, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(t12, lv_color_hex(0x666666), 0);
    lv_obj_set_pos(t12, cx + cw / 2 - 20, chart_y + chart_h + 4);

    lv_obj_t *t24 = mk_label(card);
    lv_label_set_text(t24, "24:00");
    lv_obj_set_style_text_font(t24, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(t24, lv_color_hex(0x666666), 0);
    lv_obj_set_pos(t24, cx + cw - 32, chart_y + chart_h + 4);

    gesture_set_intercept(gesture_intercept);
    return root;
}

static void destroy(void)
{
    if (gesture_get_intercept() == gesture_intercept)
        gesture_set_intercept(NULL);
}

const page_t page_app_spo2_detail = {
    .name    = "spo2_detail",
    .type    = PAGE_TYPE_OVERLAY,
    .create  = create,
    .destroy = destroy,
    .update  = NULL,
};
