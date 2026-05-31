#include "page.h"
#include "../Data/data_provider.h"
#include "../Framework/page_manager.h"
#include "../Framework/gesture.h"

static lv_obj_t *root;
static lv_obj_t *spo2_icon;
static lv_obj_t *label_spo2;
static lv_obj_t *label_pct;
static lv_obj_t *label_status;
static lv_obj_t *btn_measure;
static lv_obj_t *label_btn;
static lv_timer_t *measure_timer;
static lv_timer_t *refresh_timer;
static bool measuring;
static bool s_swiping;

/* ── 测量覆盖层 ── */
static lv_obj_t   *overlay;
static lv_obj_t   *overlay_hint;
static lv_obj_t   *overlay_arc;
static lv_obj_t   *overlay_countdown;
static lv_obj_t   *overlay_status;
static lv_obj_t   *overlay_result;
static lv_obj_t   *overlay_result_pct;
static lv_timer_t *countdown_timer;
static lv_timer_t *result_timer;
static int32_t     countdown;
static int32_t     arc_display;

extern const lv_image_dsc_t xueyang_64;
extern const lv_font_t montserrat_48_digits;

#define SPO2_COLOR  lv_color_hex(0x00C9E0)

/* ── 动画回调 ── */
static void opa_cb(void *var, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)var, v, 0);
}

static void text_opa_cb(void *var, int32_t v)
{
    lv_obj_set_style_text_opa((lv_obj_t *)var, v, 0);
}

static void arc_value_cb(void *var, int32_t v)
{
    (void)var;
    lv_arc_set_value(overlay_arc, v);
}

static void arc_rotation_cb(void *var, int32_t v)
{
    (void)var;
    lv_arc_set_rotation(overlay_arc, v);
}

/* ── 数据刷新 ── */
static void on_refresh(lv_timer_t *timer)
{
    (void)timer;
    int32_t sp = watch_data_get_spo2();
    if (sp > 0) {
        lv_label_set_text_fmt(label_spo2, "%d", (int)sp);
        lv_label_set_text_fmt(label_status, "%d%%, Just now", (int)sp);
    }
    lv_obj_align_to(label_spo2, label_status, LV_ALIGN_OUT_TOP_MID, 0, -10);
    lv_obj_align_to(label_pct, label_spo2, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, 0);
}

/* ── 退出覆盖层，恢复 dashboard ── */
static void restore_dashboard(void)
{
    if (overlay) { lv_obj_delete(overlay); overlay = NULL; }
    overlay_hint = NULL;
    overlay_arc = NULL;
    overlay_countdown = NULL;
    overlay_status = NULL;
    overlay_result = NULL;
    overlay_result_pct = NULL;
    if (countdown_timer) { lv_timer_delete(countdown_timer); countdown_timer = NULL; }
    if (result_timer) { lv_timer_delete(result_timer); result_timer = NULL; }

    measuring = false;
    lv_obj_clear_state(btn_measure, LV_STATE_DISABLED);
    lv_label_set_text(label_btn, "MEASURE");

    int32_t sp = 98;
    lv_label_set_text_fmt(label_spo2, "%d", (int)sp);
    lv_obj_set_style_text_font(label_spo2, &montserrat_48_digits, 0);
    lv_label_set_text_fmt(label_status, "%d%%, Just now", (int)sp);
    lv_obj_align_to(label_spo2, label_status, LV_ALIGN_OUT_TOP_MID, 0, -10);
    lv_obj_align_to(label_pct, label_spo2, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, 0);

    /* dashboard 淡入 */
    lv_obj_t *dash[] = { spo2_icon, label_spo2, label_pct, label_status, btn_measure };
    for (int i = 0; i < 5; i++) {
        if (lv_obj_has_flag(dash[i], LV_OBJ_FLAG_HIDDEN))
            lv_obj_remove_flag(dash[i], LV_OBJ_FLAG_HIDDEN);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, dash[i]);
        lv_anim_set_values(&a, 0, LV_OPA_COVER);
        lv_anim_set_exec_cb(&a, opa_cb);
        lv_anim_set_duration(&a, 350);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_start(&a);
    }

    refresh_timer = lv_timer_create(on_refresh, 1000, NULL);
}

/* ── 覆盖层淡出 → 恢复 ── */
static void overlay_fade_out(lv_timer_t *t)
{
    (void)t;
    result_timer = NULL;
    if (!overlay) return;

    lv_anim_delete(NULL, arc_rotation_cb);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, overlay);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_exec_cb(&a, opa_cb);
    lv_anim_set_duration(&a, 400);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&a, (lv_anim_completed_cb_t)restore_dashboard);
    lv_anim_start(&a);
}

/* ── 显示测量结果 ── */
static void show_result(void)
{
    lv_anim_delete(NULL, text_opa_cb);

    /* 隐藏倒计时 */
    lv_obj_add_flag(overlay_countdown, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlay_hint, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlay_status, LV_OBJ_FLAG_HIDDEN);

    /* 圆弧收拢到满 */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, &arc_display);
    lv_anim_set_values(&a, lv_arc_get_value(overlay_arc), 300);
    lv_anim_set_exec_cb(&a, arc_value_cb);
    lv_anim_set_duration(&a, 400);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    lv_obj_set_style_arc_color(overlay_arc, SPO2_COLOR, LV_PART_INDICATOR);

    /* 急速旋转 */
    lv_anim_t a_rot;
    lv_anim_init(&a_rot);
    lv_anim_set_var(&a_rot, &arc_display);
    lv_anim_set_values(&a_rot, 270, 630);
    lv_anim_set_exec_cb(&a_rot, arc_rotation_cb);
    lv_anim_set_duration(&a_rot, 200);
    lv_anim_set_repeat_count(&a_rot, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a_rot, lv_anim_path_linear);
    lv_anim_start(&a_rot);

    /* 结果数字，偏左补偿 % 宽度 */
    overlay_result = lv_label_create(overlay);
    lv_label_set_text(overlay_result, "98");
    lv_obj_set_style_text_font(overlay_result, &montserrat_48_digits, 0);
    lv_obj_set_style_text_color(overlay_result, lv_color_white(), 0);
    lv_obj_set_style_text_opa(overlay_result, 0, 0);
    lv_obj_align(overlay_result, LV_ALIGN_CENTER, -8, 0);

    lv_anim_set_var(&a, overlay_result);
    lv_anim_set_values(&a, 0, LV_OPA_COVER);
    lv_anim_set_exec_cb(&a, text_opa_cb);
    lv_anim_set_duration(&a, 450);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    /* 单位 % */
    overlay_result_pct = lv_label_create(overlay);
    lv_label_set_text(overlay_result_pct, "%");
    lv_obj_set_style_text_font(overlay_result_pct, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(overlay_result_pct, SPO2_COLOR, 0);
    lv_obj_set_style_text_opa(overlay_result_pct, 0, 0);
    lv_obj_align_to(overlay_result_pct, overlay_result, LV_ALIGN_OUT_RIGHT_MID, 6, 0);

    lv_anim_set_var(&a, overlay_result_pct);
    lv_anim_set_delay(&a, 140);
    lv_anim_start(&a);

    /* 完成文字 */
    overlay_status = lv_label_create(overlay);
    lv_label_set_text(overlay_status, "Measurement complete");
    lv_obj_set_style_text_font(overlay_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(overlay_status, lv_color_hex(0xBBBBBB), 0);
    lv_obj_set_style_text_opa(overlay_status, 0, 0);
    lv_obj_align(overlay_status, LV_ALIGN_CENTER, 0, 106);

    lv_anim_set_var(&a, overlay_status);
    lv_anim_set_delay(&a, 280);
    lv_anim_start(&a);

    result_timer = lv_timer_create(overlay_fade_out, 1500, NULL);
    lv_timer_set_repeat_count(result_timer, 1);
}

/* ── 倒计时滴答 ── */
static void on_countdown_tick(lv_timer_t *t)
{
    (void)t;
    if (countdown > 1) {
        lv_label_set_text_fmt(overlay_countdown, "%d", countdown);
        countdown--;
    } else if (countdown == 1) {
        lv_label_set_text(overlay_countdown, "1");
        countdown--;
        lv_timer_delete(countdown_timer);
        countdown_timer = NULL;
        lv_timer_t *delay = lv_timer_create((lv_timer_cb_t)show_result, 200, NULL);
        lv_timer_set_repeat_count(delay, 1);
    }
}

/* ── 构建测量覆盖层 ── */
static void build_overlay(void)
{
    overlay = lv_obj_create(root);
    lv_obj_set_size(overlay, 240, 280);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    /* 淡入 */
    lv_obj_set_style_opa(overlay, 0, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, overlay);
    lv_anim_set_values(&a, 0, LV_OPA_COVER);
    lv_anim_set_exec_cb(&a, opa_cb);
    lv_anim_set_duration(&a, 250);
    lv_anim_start(&a);

    /* "Keep still..." */
    overlay_hint = lv_label_create(overlay);
    lv_label_set_text(overlay_hint, "Keep still...");
    lv_obj_set_style_text_font(overlay_hint, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(overlay_hint, lv_color_hex(0xDDDDDD), 0);
    lv_obj_align(overlay_hint, LV_ALIGN_CENTER, 0, -96);

    lv_anim_set_var(&a, overlay_hint);
    lv_anim_set_values(&a, 140, 220);
    lv_anim_set_exec_cb(&a, text_opa_cb);
    lv_anim_set_duration(&a, 1500);
    lv_anim_set_playback_duration(&a, 1500);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    /* 圆弧 */
    overlay_arc = lv_arc_create(overlay);
    lv_obj_set_size(overlay_arc, 152, 152);
    lv_obj_align(overlay_arc, LV_ALIGN_CENTER, 0, 0);
    lv_arc_set_mode(overlay_arc, LV_ARC_MODE_NORMAL);
    lv_arc_set_range(overlay_arc, 0, 300);
    lv_arc_set_value(overlay_arc, 0);
    lv_arc_set_bg_angles(overlay_arc, 0, 360);
    lv_arc_set_rotation(overlay_arc, 270);
    lv_obj_remove_flag(overlay_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(overlay_arc, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_color(overlay_arc, lv_color_hex(0x2A2A2A), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(overlay_arc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(overlay_arc, 5, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(overlay_arc, SPO2_COLOR, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(overlay_arc, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(overlay_arc, true, LV_PART_INDICATOR);
    lv_obj_remove_style(overlay_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(overlay_arc, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(overlay_arc, 0, 0);
    lv_obj_set_style_pad_all(overlay_arc, 0, 0);
    lv_obj_set_style_shadow_width(overlay_arc, 0, 0);

    /* 圆弧进度 */
    lv_anim_set_var(&a, &arc_display);
    lv_anim_set_values(&a, 0, 300);
    lv_anim_set_exec_cb(&a, arc_value_cb);
    lv_anim_set_duration(&a, 9800);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_start(&a);

    /* 倒计时数字 */
    overlay_countdown = lv_label_create(overlay);
    lv_label_set_text(overlay_countdown, "10");
    lv_obj_set_style_text_font(overlay_countdown, &montserrat_48_digits, 0);
    lv_obj_set_style_text_color(overlay_countdown, lv_color_white(), 0);
    lv_obj_align(overlay_countdown, LV_ALIGN_CENTER, 0, 0);

    /* "Scanning..." */
    overlay_status = lv_label_create(overlay);
    lv_label_set_text(overlay_status, "Scanning...");
    lv_obj_set_style_text_font(overlay_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(overlay_status, lv_color_hex(0xBBBBBB), 0);
    lv_obj_align(overlay_status, LV_ALIGN_CENTER, 0, 106);

    /* 启动倒计时定时器 */
    countdown = 10;
    countdown_timer = lv_timer_create(on_countdown_tick, 1000, NULL);
}

/* ── 点击 MEASURE 按钮 ── */
static void on_measure_click(lv_event_t *e)
{
    (void)e;
    if (s_swiping) { s_swiping = false; return; }
    if (measuring) return;
    measuring = true;

    if (refresh_timer) { lv_timer_delete(refresh_timer); refresh_timer = NULL; }
    watch_data_spo2_start();

    /* dashboard 淡出 */
    lv_obj_t *dash[] = { spo2_icon, label_spo2, label_pct, label_status, btn_measure };
    for (int i = 0; i < 5; i++) {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, dash[i]);
        lv_anim_set_values(&a, LV_OPA_COVER, 0);
        lv_anim_set_exec_cb(&a, opa_cb);
        lv_anim_set_duration(&a, 300);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_set_completed_cb(&a, (i == 4) ? (lv_anim_completed_cb_t)build_overlay : NULL);
        lv_anim_start(&a);
    }

    /* 禁用按钮，防止重复点击 */
    lv_obj_add_state(btn_measure, LV_STATE_DISABLED);
    lv_label_set_text(label_btn, "Measuring...");
}

/* ── 手势：上滑 → 详情页 ── */
static bool gesture_intercept(gesture_t g)
{
    if (measuring) return true;
    if (g == GESTURE_UP) {
        s_swiping = true;
        page_manager_push_up(PAGE_APP_SPO2_DETAIL);
        return true;
    }
    return false;
}

/* ═══════════════════════════════════════════════════════════════
   布局（240×280）
   y=0..100   血氧图标 64×64，中心偏上
   y=100..200 大数字 + 单位 %
   y=200..280 测量按钮
   ═══════════════════════════════════════════════════════════════ */

static lv_obj_t *create(lv_obj_t *parent)
{
    root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_clip_corner(root, true, 0);

    /* 血氧图标 64×64，中心偏上 */
    spo2_icon = lv_image_create(root);
    lv_image_set_src(spo2_icon, &xueyang_64);
    lv_obj_set_size(spo2_icon, 64, 64);
    lv_obj_align(spo2_icon, LV_ALIGN_CENTER, 0, -50);

    /* 状态小字：立即显示上次测量值，居中作为锚点 */
    label_status = lv_label_create(root);
    int32_t last_sp = watch_data_get_spo2();
    if (last_sp > 0)
        lv_label_set_text_fmt(label_status, "%d%%, Just now", (int)last_sp);
    else
        lv_label_set_text(label_status, "No data");
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_status, lv_color_white(), 0);
    lv_obj_align(label_status, LV_ALIGN_CENTER, 0, 70);

    /* 血氧数值，初始显示 "--"（等待测量） */
    label_spo2 = lv_label_create(root);
    lv_label_set_text(label_spo2, "--");
    lv_obj_set_style_text_font(label_spo2, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_spo2, lv_color_white(), 0);
    lv_obj_align_to(label_spo2, label_status, LV_ALIGN_OUT_TOP_MID, 0, -10);

    /* 单位 %，数字右下 */
    label_pct = lv_label_create(root);
    lv_label_set_text(label_pct, "%");
    lv_obj_set_style_text_font(label_pct, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_pct, SPO2_COLOR, 0);
    lv_obj_align_to(label_pct, label_spo2, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, 0);

    /* MEASURE 按钮 */
    btn_measure = lv_button_create(root);
    lv_obj_set_size(btn_measure, 186, 40);
    lv_obj_set_style_bg_color(btn_measure, lv_color_hex(0x1C1C1E), 0);
    lv_obj_set_style_bg_opa(btn_measure, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(btn_measure, SPO2_COLOR, 0);
    lv_obj_set_style_border_width(btn_measure, 2, 0);
    lv_obj_set_style_border_opa(btn_measure, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn_measure, 20, 0);
    lv_obj_set_style_shadow_width(btn_measure, 0, 0);
    lv_obj_set_style_outline_width(btn_measure, 0, 0);
    lv_obj_align(btn_measure, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_add_event_cb(btn_measure, on_measure_click, LV_EVENT_CLICKED, NULL);

    label_btn = lv_label_create(btn_measure);
    lv_label_set_text(label_btn, "MEASURE");
    lv_obj_set_style_text_font(label_btn, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_btn, SPO2_COLOR, 0);
    lv_obj_center(label_btn);

    gesture_set_intercept(gesture_intercept);
    return root;
}

static void destroy(void)
{
    if (gesture_get_intercept() == gesture_intercept)
        gesture_set_intercept(NULL);

    /* 清理覆盖层 */
    lv_anim_delete(NULL, opa_cb);
    lv_anim_delete(NULL, text_opa_cb);
    lv_anim_delete(NULL, arc_value_cb);
    lv_anim_delete(NULL, arc_rotation_cb);
    if (countdown_timer) { lv_timer_delete(countdown_timer); countdown_timer = NULL; }
    if (result_timer)   { lv_timer_delete(result_timer);   result_timer   = NULL; }
    if (overlay)        { lv_obj_delete(overlay);          overlay        = NULL; }

    if (measure_timer)  { lv_timer_delete(measure_timer);  measure_timer  = NULL; }
    if (refresh_timer)  { lv_timer_delete(refresh_timer);  refresh_timer  = NULL; }
    if (measuring) watch_data_spo2_abort();
}

static void update(void) {}

const page_t page_app_spo2 = {
    .name    = "app_spo2",
    .type    = PAGE_TYPE_OVERLAY,
    .create  = create,
    .destroy = destroy,
    .update  = update,
};
