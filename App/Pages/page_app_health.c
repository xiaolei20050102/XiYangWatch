#include "page.h"

extern const lv_image_dsc_t heart_rate_32;
extern const lv_image_dsc_t blood_oxygen_32;

#define DATA_N  10
static const int hr_raw[DATA_N] = { 68, 72, 76, 73, 69, 71, 78, 75, 70, 73 };
static const int o2_raw[DATA_N] = { 97, 98, 98, 97, 99, 98, 97, 98, 99, 98 };

#define CURVE_N 40
static lv_point_precise_t hr_pts[CURVE_N];
static lv_point_precise_t o2_pts[CURVE_N];

static void gen_curve(const int *data, int n,
                      lv_point_precise_t *pts, int pts_n,
                      int cx, int cy, int cw, int ch,
                      int y_min, int y_range, int span_pct_i)
{
    int span_w = (cw * span_pct_i) / 100;
    int margin = (cw - span_w) / 2;

    for (int i = 0; i < pts_n; i++) {
        int pos_q8  = (i * (n - 1) * 256) / (pts_n - 1);
        int idx     = pos_q8 >> 8;
        int frac_q8 = pos_q8 & 0xFF;

        if (idx < 0)      { idx = 0;      frac_q8 = 0; }
        if (idx >= n - 1) { idx = n - 2;  frac_q8 = 256; }

        int v_q8 = data[idx] * (256 - frac_q8) + data[idx + 1] * frac_q8;
        int v    = v_q8 >> 8;

        pts[i].x = (lv_value_precise_t)(cx + margin + (i * span_w) / (pts_n - 1));
        pts[i].y = (lv_value_precise_t)(cy + ch - ((v - y_min) * ch) / y_range);
    }
}

static void add_card(lv_obj_t *root, lv_coord_t y,
                     const lv_image_dsc_t *icon_src, lv_color_t color,
                     const char *value, const char *status,
                     const int *data, lv_point_precise_t *pts,
                     int y_min, int y_range)
{
    lv_obj_t *card = lv_obj_create(root);
    lv_obj_set_size(card, 216, 124);
    lv_obj_set_pos(card, 12, y);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xEAEAEA), 0);
    lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_bg_color(card, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);

    /* icon */
    lv_obj_t *icon = lv_image_create(card);
    lv_image_set_src(icon, icon_src);
    lv_obj_set_style_image_recolor(icon, color, 0);
    lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
    lv_obj_set_pos(icon, 14, 14);

    /* value */
    lv_obj_t *vl = lv_label_create(card);
    lv_label_set_text(vl, value);
    lv_obj_set_style_text_font(vl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(vl, color, 0);
    lv_obj_set_pos(vl, 54, 16);

    /* status subtitle */
    lv_obj_t *sl = lv_label_create(card);
    lv_label_set_text(sl, status);
    lv_obj_set_style_text_font(sl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sl, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(sl, 54, 36);

    /* chart area */
    int cx = 14, cw = 188, chart_h = 44;
    int chart_y = 56;

    /* 2 grid lines (top split, bottom split) */
    for (int g = 1; g <= 2; g++) {
        lv_obj_t *grid = lv_obj_create(card);
        lv_obj_set_size(grid, cw, 1);
        lv_obj_set_pos(grid, cx, chart_y + g * (chart_h / 3));
        lv_obj_set_style_bg_color(grid, lv_color_hex(0xF0F0F0), 0);
        lv_obj_set_style_bg_opa(grid, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(grid, 0, 0);
        lv_obj_set_style_pad_all(grid, 0, 0);
        lv_obj_set_style_radius(grid, 0, 0);
    }

    /* curve */
    gen_curve(data, DATA_N, pts, CURVE_N,
              cx, chart_y, cw, chart_h, y_min, y_range, 82);

    lv_obj_t *line = lv_line_create(card);
    lv_line_set_points_mutable(line, pts, CURVE_N);
    lv_obj_set_style_line_color(line, color, 0);
    lv_obj_set_style_line_width(line, 2, 0);

    /* endpoint dot */
    lv_point_precise_t last = pts[CURVE_N - 1];
    lv_obj_t *dot = lv_obj_create(card);
    lv_obj_set_size(dot, 6, 6);
    lv_obj_set_pos(dot, (lv_coord_t)last.x - 3, (lv_coord_t)last.y - 3);
    lv_obj_set_style_radius(dot, 3, 0);
    lv_obj_set_style_bg_color(dot, color, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_pad_all(dot, 0, 0);

    /* time labels */
    lv_obj_t *t;
    t = lv_label_create(card);
    lv_label_set_text(t, "8AM");
    lv_obj_set_style_text_font(t, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_pos(t, cx, chart_y + chart_h + 2);

    t = lv_label_create(card);
    lv_label_set_text(t, "12PM");
    lv_obj_set_style_text_font(t, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(t, LV_ALIGN_BOTTOM_MID, 0, -4);

    t = lv_label_create(card);
    lv_label_set_text(t, "4PM");
    lv_obj_set_style_text_font(t, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(t, LV_ALIGN_BOTTOM_RIGHT, -14, -4);
}

static lv_obj_t *create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

    add_card(root, 22, &heart_rate_32, lv_color_hex(0xFF5A7A),
             "72 BPM Avg", "Stable Rhythm",
             hr_raw, hr_pts, 60, 25);

    add_card(root, 152, &blood_oxygen_32, lv_color_hex(0x00B8D9),
             "98% Avg", "Excellent Oxygen",
             o2_raw, o2_pts, 96, 5);

    return root;
}

const page_t page_app_health = {
    .name    = "health",
    .type    = PAGE_TYPE_OVERLAY,
    .create  = create,
    .destroy = NULL,
    .update  = NULL,
};
