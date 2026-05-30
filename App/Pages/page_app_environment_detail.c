#include "page.h"
#include "../Framework/page_manager.h"
#include "../Framework/gesture.h"

/* ── 24 小时逐时海拔数据 ── */
#define N   24
#define PTS 24
static const int alt_24h[N] = {
     100,   50, -200, -500,  800, 2500, 4800, 6800,
    8000, 8600, 8800, 8400, 7600, 6200, 4500, 2800,
    1500,  600,  200, -100, -400, -600, -300,    0
};
static lv_point_precise_t alt_pts[PTS];

#define ALT_COLOR  lv_color_hex(0x0099FF)

static lv_obj_t *mk_label(lv_obj_t *parent)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_bg_opa(l, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(l, 0, 0);
    lv_obj_set_style_pad_all(l, 0, 0);
    return l;
}

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

static void draw_chart_card(lv_obj_t *parent, lv_coord_t card_y,
                            const int *data, lv_color_t color,
                            lv_point_precise_t *pts,
                            int y_min, int y_range,
                            const char *title, const char *unit,
                            const char *y0, const char *y1, const char *y2)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 218, 112);
    lv_obj_set_pos(card, 11, card_y);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);

    int last = data[N - 1];

    /* 标题 + 最新值 */
    lv_obj_t *lbl_title = mk_label(card);
    lv_label_set_text_fmt(lbl_title, "%s  %d %s", title, last, unit);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_title, color, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_LEFT, 12, 3);

    /* 图表区域 */
    int cx = 44, cw = 160, ch = 58;
    int cy = 34;

    /* 网格线 */
    for (int g = 1; g <= 2; g++) {
        lv_obj_t *grid = lv_obj_create(card);
        lv_obj_set_size(grid, cw, 1);
        lv_obj_set_pos(grid, cx, cy + g * (ch / 3));
        lv_obj_set_style_bg_color(grid, lv_color_hex(0x1A1A1A), 0);
        lv_obj_set_style_bg_opa(grid, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(grid, 0, 0);
        lv_obj_set_style_pad_all(grid, 0, 0);
        lv_obj_set_style_radius(grid, 0, 0);
    }

    /* Y 轴标签 */
    lv_obj_t *yl0 = mk_label(card);
    lv_label_set_text(yl0, y0);
    lv_obj_set_style_text_font(yl0, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(yl0, lv_color_hex(0x888888), 0);
    lv_obj_align(yl0, LV_ALIGN_TOP_LEFT, 2, cy - 7);

    lv_obj_t *yl1 = mk_label(card);
    lv_label_set_text(yl1, y1);
    lv_obj_set_style_text_font(yl1, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(yl1, lv_color_hex(0x888888), 0);
    lv_obj_align(yl1, LV_ALIGN_TOP_LEFT, 2, cy + ch / 2 - 8);

    lv_obj_t *yl2 = mk_label(card);
    lv_label_set_text(yl2, y2);
    lv_obj_set_style_text_font(yl2, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(yl2, lv_color_hex(0x888888), 0);
    lv_obj_align(yl2, LV_ALIGN_TOP_LEFT, 2, cy + ch - 9);

    /* 曲线 */
    gen_curve(data, N, pts, PTS, cx, cy, cw, ch, y_min, y_range);
    lv_obj_t *line = lv_line_create(card);
    lv_line_set_points_mutable(line, pts, PTS);
    lv_obj_set_style_line_color(line, color, 0);
    lv_obj_set_style_line_width(line, 2, 0);

    /* 终点圆点 */
    lv_point_precise_t lp = pts[PTS - 1];
    lv_obj_t *dot = lv_obj_create(card);
    lv_obj_set_size(dot, 6, 6);
    lv_obj_set_pos(dot, (lv_coord_t)lp.x - 3, (lv_coord_t)lp.y - 3);
    lv_obj_set_style_radius(dot, 3, 0);
    lv_obj_set_style_bg_color(dot, color, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_pad_all(dot, 0, 0);

    /* 时间轴 */
    int ty = cy + ch + 4;
    lv_obj_t *t0 = mk_label(card);
    lv_label_set_text(t0, "0:00");
    lv_obj_set_style_text_font(t0, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(t0, lv_color_hex(0x666666), 0);
    lv_obj_set_pos(t0, cx, ty);

    lv_obj_t *t12 = mk_label(card);
    lv_label_set_text(t12, "12:00");
    lv_obj_set_style_text_font(t12, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(t12, lv_color_hex(0x666666), 0);
    lv_obj_set_pos(t12, cx + cw / 2 - 18, ty);

    lv_obj_t *t24 = mk_label(card);
    lv_label_set_text(t24, "24:00");
    lv_obj_set_style_text_font(t24, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(t24, lv_color_hex(0x666666), 0);
    lv_obj_set_pos(t24, cx + cw - 28, ty);
}

static bool gesture_intercept(gesture_t g)
{
    if (g == GESTURE_DOWN) {
        page_manager_pop(GESTURE_DOWN);
        return true;
    }
    if (g == GESTURE_UP) {
        page_manager_push_up(PAGE_APP_ENVIRONMENT_PRESSURE_DETAIL);
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
    lv_label_set_text(title, "Altitude | 24h");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    /* 返回提示 */
    lv_obj_t *hint = mk_label(root);
    lv_label_set_text(hint, "Down: back  |  Up: pressure");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x666666), 0);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 46);

    /* 海拔卡片，居中 */
    draw_chart_card(root, 84, alt_24h, ALT_COLOR, alt_pts,
                    -800, 9600, "Alt", "m", "8800", "4000", "-800");

    gesture_set_intercept(gesture_intercept);
    return root;
}

static void destroy(void)
{
    if (gesture_get_intercept() == gesture_intercept)
        gesture_set_intercept(NULL);
}

const page_t page_app_environment_detail = {
    .name    = "environment_detail",
    .type    = PAGE_TYPE_OVERLAY,
    .create  = create,
    .destroy = destroy,
    .update  = NULL,
};
