#include "page.h"
#include "../Data/data_provider.h"

static lv_obj_t *root;
static lv_obj_t *label_hr;
static lv_obj_t *label_spo2_num;
static lv_obj_t *label_spo2_pct;
static lv_obj_t *btn_spo2;
static lv_obj_t *label_btn;
static lv_timer_t *refresh_timer;

static int32_t hr_value;
static int32_t spo2_value = -1;
static int32_t spo2_countdown;
static int32_t spo2_measuring;

/* 测量弹窗 */
static lv_obj_t *meas_overlay;
static lv_obj_t *meas_arc;
static lv_obj_t *meas_countdown_label;
static int32_t arc_display;

static void arc_anim_cb(void *var, int32_t v)
{
    (void)var;
    lv_arc_set_value(meas_arc, v);
}

extern const lv_image_dsc_t heart_rate_32;
extern const lv_image_dsc_t blood_oxygen_32;
extern const lv_font_t montserrat_48_digits;

static void close_meas_popup(void)
{
    if (meas_overlay) {
        lv_obj_delete(meas_overlay);
        meas_overlay = NULL;
        meas_arc = NULL;
        meas_countdown_label = NULL;
    }
}

static void on_spo2_click(lv_event_t *e)
{
    if (spo2_measuring) return;

    spo2_measuring = 1;
    spo2_countdown = 3;
    arc_display = 0;

    watch_data_spo2_start();

    /* 隐藏数值区域 */
    lv_obj_set_style_text_font(label_spo2_num, &lv_font_montserrat_16, 0);
    lv_label_set_text(label_spo2_num, "");
    lv_label_set_text(label_spo2_pct, "");
    lv_obj_add_state(btn_spo2, LV_STATE_DISABLED);
    lv_label_set_text(label_btn, "MEASURE");

    /* 全屏半透明遮罩 */
    meas_overlay = lv_obj_create(root);
    lv_obj_set_size(meas_overlay, 240, 280);
    lv_obj_set_style_bg_color(meas_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(meas_overlay, LV_OPA_60, 0);
    lv_obj_set_style_border_width(meas_overlay, 0, 0);
    lv_obj_set_style_pad_all(meas_overlay, 0, 0);
    lv_obj_remove_flag(meas_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(meas_overlay, LV_ALIGN_CENTER, 0, 0);

    /* 弹窗卡片 */
    lv_obj_t *card = lv_obj_create(meas_overlay);
    lv_obj_set_size(card, 180, 120);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x3A3A3C), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 24, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_center(card);

    /* 提示文字 */
    lv_obj_t *hint = lv_label_create(card);
    lv_label_set_text(hint, "Keep still...");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0xCCCCCC), 0);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 2);

    /* 圆弧进度 */
    meas_arc = lv_arc_create(card);
    lv_obj_set_size(meas_arc, 72, 72);
    lv_obj_align(meas_arc, LV_ALIGN_CENTER, 0, 4);
    lv_arc_set_mode(meas_arc, LV_ARC_MODE_NORMAL);
    lv_arc_set_range(meas_arc, 0, 300);
    lv_arc_set_value(meas_arc, 0);
    lv_arc_set_bg_angles(meas_arc, 0, 360);
    lv_arc_set_rotation(meas_arc, 270);
    lv_obj_remove_flag(meas_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(meas_arc, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_color(meas_arc, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(meas_arc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(meas_arc, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(meas_arc, lv_color_hex(0x3AFFB6), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(meas_arc, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_remove_style(meas_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(meas_arc, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(meas_arc, 0, 0);
    lv_obj_set_style_pad_all(meas_arc, 0, 0);
    lv_obj_set_style_shadow_width(meas_arc, 0, 0);

    /* 倒计时数字 — 叠在圆弧中心 */
    meas_countdown_label = lv_label_create(card);
    lv_label_set_text(meas_countdown_label, "3");
    lv_obj_set_style_text_font(meas_countdown_label, &montserrat_48_digits, 0);
    lv_obj_set_style_text_color(meas_countdown_label, lv_color_white(), 0);
    lv_obj_center(meas_countdown_label);
}

static void on_refresh(lv_timer_t *timer)
{
    static int32_t tick;
    tick++;

    hr_value = 75 + ((tick % 7) - 3);
    lv_label_set_text_fmt(label_hr, "%d", hr_value);

    if (spo2_measuring) {
        if (spo2_countdown > 0) {
            lv_label_set_text_fmt(meas_countdown_label, "%d", spo2_countdown);
            spo2_countdown--;
            {
                int32_t target = (3 - spo2_countdown) * 100;
                int32_t from = target > 0 ? target - 100 : 0;
                lv_anim_t a;
                lv_anim_init(&a);
                lv_anim_set_var(&a, &arc_display);
                lv_anim_set_values(&a, from, target);
                lv_anim_set_exec_cb(&a, arc_anim_cb);
                lv_anim_set_duration(&a, 950);
                lv_anim_set_path_cb(&a, lv_anim_path_linear);
                lv_anim_start(&a);
            }
        } else {
            /* 上一 tick 已显示 "1"，本 tick 完成 */
            close_meas_popup();
            spo2_measuring = 0;
            spo2_value = 98;

            lv_obj_set_style_text_font(label_spo2_num, &montserrat_48_digits, 0);
            lv_label_set_text_fmt(label_spo2_num, "%d", spo2_value);
            lv_obj_set_style_text_font(label_spo2_pct, &lv_font_montserrat_16, 0);
            lv_label_set_text(label_spo2_pct, "%");
            lv_obj_align_to(label_spo2_pct, label_spo2_num, LV_ALIGN_OUT_RIGHT_TOP, 6, 8);
            lv_obj_clear_state(btn_spo2, LV_STATE_DISABLED);
        }
    }
}

static lv_obj_t *create(lv_obj_t *parent)
{
    root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

    /* ==================== 中心分割线 ==================== */
    lv_obj_t *divider = lv_obj_create(root);
    lv_obj_set_size(divider, 200, 1);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_set_style_pad_all(divider, 0, 0);
    lv_obj_align(divider, LV_ALIGN_CENTER, 0, 0);

    /* ==================== 上半区：心率 ==================== */

    /* 心率图标 */
    lv_obj_t *heart = lv_image_create(root);
    lv_image_set_src(heart, &heart_rate_32);
    lv_obj_align(heart, LV_ALIGN_CENTER, -84, -46);

    /* 数字锚定中心，图标在数字左边 */
    label_hr = lv_label_create(root);
    lv_label_set_text(label_hr, "75");
    lv_obj_set_style_text_font(label_hr, &montserrat_48_digits, 0);
    lv_obj_set_style_text_color(label_hr, lv_color_white(), 0);
    lv_obj_align(label_hr, LV_ALIGN_CENTER, 0, -54);

    /* bpm 单位 */
    lv_obj_t *label_bpm = lv_label_create(root);
    lv_label_set_text(label_bpm, "bpm");
    lv_obj_set_style_text_font(label_bpm, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_bpm, lv_color_hex(0x808080), 0);
    lv_obj_align_to(label_bpm, label_hr, LV_ALIGN_OUT_RIGHT_MID, 8, 6);

    /* ==================== 下半区：血氧 ==================== */

    /* 血氧图标 */
    lv_obj_t *spo2_icon = lv_image_create(root);
    lv_image_set_src(spo2_icon, &blood_oxygen_32);
    lv_obj_align(spo2_icon, LV_ALIGN_CENTER, -80, 54);

    /* 数字锚定中心 */
    label_spo2_num = lv_label_create(root);
    lv_label_set_text(label_spo2_num, "");
    lv_obj_set_style_text_font(label_spo2_num, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_spo2_num, lv_color_white(), 0);
    lv_obj_align(label_spo2_num, LV_ALIGN_CENTER, 0, 54);

    /* % 符号 */
    label_spo2_pct = lv_label_create(root);
    lv_label_set_text(label_spo2_pct, "");
    lv_obj_set_style_text_font(label_spo2_pct, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_spo2_pct, lv_color_hex(0x808080), 0);
    lv_obj_align_to(label_spo2_pct, label_spo2_num, LV_ALIGN_OUT_RIGHT_TOP, 6, 8);

    /* Ghost 按钮 */
    btn_spo2 = lv_button_create(root);
    lv_obj_set_size(btn_spo2, 180, 36);
    lv_obj_set_style_bg_opa(btn_spo2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(btn_spo2, lv_color_hex(0xFF5500), 0);
    lv_obj_set_style_border_color(btn_spo2, lv_color_hex(0xFF8833), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn_spo2, 2, 0);
    lv_obj_set_style_border_opa(btn_spo2, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn_spo2, 18, 0);
    lv_obj_set_style_shadow_width(btn_spo2, 0, 0);
    lv_obj_set_style_outline_width(btn_spo2, 0, 0);
    lv_obj_align(btn_spo2, LV_ALIGN_CENTER, 0, 108);
    lv_obj_add_event_cb(btn_spo2, on_spo2_click, LV_EVENT_CLICKED, NULL);

    label_btn = lv_label_create(btn_spo2);
    lv_label_set_text(label_btn, "MEASURE");
    lv_obj_set_style_text_font(label_btn, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_btn, lv_color_hex(0xFF5500), 0);
    lv_obj_center(label_btn);

    refresh_timer = lv_timer_create(on_refresh, 1000, NULL);
    return root;
}

static void destroy(void)
{
    if (spo2_measuring) watch_data_spo2_abort();
    if (refresh_timer) { lv_timer_delete(refresh_timer); refresh_timer = NULL; }
    lv_anim_delete(NULL, arc_anim_cb);
    close_meas_popup();
    spo2_measuring = 0;
    spo2_countdown = 0;
}

static void update(void) {}

const page_t page_heartrate = {
    .name = "heartrate",
    .type = PAGE_TYPE_SPOKE,
    .create = create,
    .destroy = destroy,
    .update = update,
};
