#include "page.h"
#include "../Data/data_provider.h"
#include "../Framework/gesture.h"

static lv_obj_t *root;
static lv_obj_t *divider;
static lv_obj_t *label_hr;
static lv_obj_t *label_hr_shadow;
static lv_obj_t *label_bpm;
static lv_obj_t *label_bpm_shadow;
static lv_obj_t *heart_icon;
static lv_obj_t *spo2_icon;
static lv_obj_t *label_spo2_num;
static lv_obj_t *label_spo2_num_shadow;
static lv_obj_t *label_spo2_pct;
static lv_obj_t *label_spo2_pct_shadow;
static lv_obj_t *btn_spo2;
static lv_obj_t *label_btn;
static lv_timer_t *refresh_timer;

static int32_t hr_value;
static int32_t spo2_value = -1;
static int32_t spo2_countdown;
static int32_t spo2_measuring;
static int32_t arc_display;

/* ── 测量模式控件 ── */
static lv_obj_t   *meas_bg;
static lv_obj_t   *meas_hint;
static lv_obj_t   *meas_hint_shadow;
static lv_obj_t   *meas_arc;
static lv_obj_t   *meas_countdown_label;
static lv_obj_t   *meas_countdown_label_shadow;
static lv_obj_t   *meas_status;
static lv_obj_t   *meas_status_shadow;
static lv_obj_t   *meas_result_num;
static lv_obj_t   *meas_result_num_shadow;
static lv_obj_t   *meas_result_pct;
static lv_obj_t   *meas_result_pct_shadow;
static lv_timer_t *meas_timer;
static lv_timer_t *result_timer;
static bool        idle_animations_running;

extern const lv_image_dsc_t heart_rate_48;
extern const lv_image_dsc_t blood_oxygen_48;
extern const lv_font_t montserrat_48_digits;

static void on_refresh(lv_timer_t *timer);

/* ==================== 通用动画回调 ==================== */

static void translate_x_cb(void *var, int32_t v) {
	lv_obj_set_style_translate_x((lv_obj_t *)var, v, 0);
}
static void translate_y_cb(void *var, int32_t v) {
	lv_obj_set_style_translate_y((lv_obj_t *)var, v, 0);
}
static void opa_cb(void *var, int32_t v) {
	lv_obj_set_style_opa((lv_obj_t *)var, v, 0);
}
static void text_opa_cb(void *var, int32_t v) {
	lv_obj_set_style_text_opa((lv_obj_t *)var, v, 0);
}
static void image_opa_cb(void *var, int32_t v) {
	lv_obj_set_style_image_opa((lv_obj_t *)var, v, 0);
}
static void scale_cb(void *var, int32_t v) {
	lv_obj_set_style_transform_scale((lv_obj_t *)var, v, 0);
}
static void border_opa_cb(void *var, int32_t v) {
	lv_obj_set_style_border_opa((lv_obj_t *)var, v, 0);
}
static void line_opa_cb(void *var, int32_t v) {
	lv_obj_set_style_line_opa((lv_obj_t *)var, v, 0);
}
static void arc_anim_cb(void *var, int32_t v) {
	(void)var;
	lv_arc_set_value(meas_arc, v);
}
static void arc_indicator_opa_cb(void *var, int32_t v) {
	lv_obj_set_style_arc_opa((lv_obj_t *)var, v, LV_PART_INDICATOR);
}
static void arc_rotation_cb(void *var, int32_t v) {
	(void)var;
	lv_arc_set_rotation(meas_arc, v);
}
static void spo2_roll_cb(void *var, int32_t v) {
	(void)var;
	lv_label_set_text_fmt(label_spo2_num_shadow, "%d", v);
	lv_label_set_text_fmt(label_spo2_num, "%d", v);
	lv_obj_align_to(label_spo2_num_shadow, label_spo2_num, LV_ALIGN_TOP_LEFT, 2, 2);
	lv_obj_align_to(label_spo2_pct, label_spo2_num, LV_ALIGN_OUT_RIGHT_TOP, 6, 8);
	lv_obj_align_to(label_spo2_pct_shadow, label_spo2_pct, LV_ALIGN_TOP_LEFT, 1, 1);
}

/* ==================== 辅助函数 ==================== */

static void set_spo2_shadow_font(void)
{
	lv_obj_set_style_text_font(label_spo2_num_shadow, lv_obj_get_style_text_font(label_spo2_num, 0), 0);
}

static void align_spo2_shadows(void)
{
	lv_obj_align_to(label_spo2_num_shadow, label_spo2_num, LV_ALIGN_TOP_LEFT, 2, 2);
	lv_obj_align_to(label_spo2_pct_shadow, label_spo2_pct, LV_ALIGN_TOP_LEFT, 1, 1);
}

static void set_spo2_placeholder(void)
{
	lv_obj_set_style_text_font(label_spo2_num, &lv_font_montserrat_20, 0);
	lv_label_set_text(label_spo2_num, "--");
	lv_obj_set_style_text_color(label_spo2_num, lv_color_hex(0x555555), 0);
	lv_obj_set_style_text_font(label_spo2_pct, &lv_font_montserrat_16, 0);
	lv_label_set_text(label_spo2_pct, "%");
	lv_obj_set_style_text_color(label_spo2_pct, lv_color_hex(0x555555), 0);
	lv_obj_align_to(label_spo2_pct, label_spo2_num, LV_ALIGN_OUT_RIGHT_TOP, 2, 2);

	lv_obj_set_style_text_font(label_spo2_num_shadow, &lv_font_montserrat_20, 0);
	lv_label_set_text(label_spo2_num_shadow, "--");
	lv_obj_set_style_text_color(label_spo2_num_shadow, lv_color_black(), 0);
	lv_obj_set_style_text_opa(label_spo2_num_shadow, LV_OPA_50, 0);
	lv_obj_set_style_text_font(label_spo2_pct_shadow, &lv_font_montserrat_16, 0);
	lv_label_set_text(label_spo2_pct_shadow, "%");
	lv_obj_set_style_text_color(label_spo2_pct_shadow, lv_color_black(), 0);
	lv_obj_set_style_text_opa(label_spo2_pct_shadow, LV_OPA_50, 0);
	align_spo2_shadows();
}

static void set_spo2_value(int32_t val)
{
	lv_obj_set_style_text_font(label_spo2_num, &montserrat_48_digits, 0);
	lv_label_set_text_fmt(label_spo2_num, "%d", val);
	lv_obj_set_style_text_color(label_spo2_num, lv_color_white(), 0);
	lv_obj_set_style_text_font(label_spo2_pct, &lv_font_montserrat_16, 0);
	lv_label_set_text(label_spo2_pct, "%");
	lv_obj_set_style_text_color(label_spo2_pct, lv_color_hex(0x808080), 0);
	lv_obj_align_to(label_spo2_pct, label_spo2_num, LV_ALIGN_OUT_RIGHT_TOP, 6, 8);

	lv_obj_set_style_text_font(label_spo2_num_shadow, &montserrat_48_digits, 0);
	lv_label_set_text_fmt(label_spo2_num_shadow, "%d", val);
	lv_obj_set_style_text_color(label_spo2_num_shadow, lv_color_black(), 0);
	lv_obj_set_style_text_opa(label_spo2_num_shadow, LV_OPA_50, 0);
	lv_obj_set_style_text_font(label_spo2_pct_shadow, &lv_font_montserrat_16, 0);
	lv_label_set_text(label_spo2_pct_shadow, "%");
	lv_obj_set_style_text_color(label_spo2_pct_shadow, lv_color_black(), 0);
	lv_obj_set_style_text_opa(label_spo2_pct_shadow, LV_OPA_50, 0);
	align_spo2_shadows();
}

static void animate_spo2_to(int32_t target, int32_t delay_ms)
{
	if (target <= 0) {
		set_spo2_placeholder();
		return;
	}
	set_spo2_value(0);
	lv_anim_t a;
	lv_anim_init(&a);
	lv_anim_set_var(&a, &arc_display);
	lv_anim_set_values(&a, 0, target);
	lv_anim_set_exec_cb(&a, spo2_roll_cb);
	lv_anim_set_duration(&a, 500 + (target * 400) / 100);
	lv_anim_set_delay(&a, delay_ms);
	lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
	lv_anim_start(&a);
}

static void exit_meas_mode(void)
{
	gesture_set_intercept(NULL);
	lv_anim_delete(NULL, arc_anim_cb);
	lv_anim_delete(NULL, arc_indicator_opa_cb);
	lv_anim_delete(NULL, arc_rotation_cb);
	if (meas_bg) { lv_obj_delete(meas_bg); meas_bg = NULL; }
	if (result_timer) { lv_timer_delete(result_timer); result_timer = NULL; }
	meas_hint = NULL; meas_hint_shadow = NULL; meas_arc = NULL;
	meas_countdown_label = NULL; meas_countdown_label_shadow = NULL;
	meas_status = NULL; meas_status_shadow = NULL;
	meas_result_num = NULL; meas_result_num_shadow = NULL;
	meas_result_pct = NULL; meas_result_pct_shadow = NULL;
	if (meas_timer) { lv_timer_delete(meas_timer); meas_timer = NULL; }
}

/* ==================== 停止 / 重启 idle 动画 ==================== */

static void stop_idle_animations(void)
{
	if (!idle_animations_running) return;
	lv_anim_delete(NULL, scale_cb);
	lv_anim_delete(NULL, border_opa_cb);
	idle_animations_running = false;
}

static void start_idle_animations(void)
{
	if (idle_animations_running) return;
	lv_anim_t a;

	lv_anim_init(&a);
	lv_anim_set_var(&a, heart_icon);
	lv_anim_set_values(&a, 210, 256);
	lv_anim_set_exec_cb(&a, scale_cb);
	lv_anim_set_duration(&a, 100);
	lv_anim_set_playback_duration(&a, 500);
	lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
	lv_anim_start(&a);

	lv_anim_init(&a);
	lv_anim_set_var(&a, btn_spo2);
	lv_anim_set_values(&a, 180, 220);
	lv_anim_set_exec_cb(&a, border_opa_cb);
	lv_anim_set_duration(&a, 1500);
	lv_anim_set_playback_duration(&a, 1500);
	lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
	lv_anim_start(&a);

	idle_animations_running = true;
}

static void on_entry_complete(lv_anim_t *a)
{
	(void)a;
	start_idle_animations();
	refresh_timer = lv_timer_create(on_refresh, 1000, NULL);
}

/* ==================== 仪表盘淡入/淡出 ==================== */

static void dashboard_set_opa(int32_t target, int32_t duration, lv_anim_completed_cb_t cb)
{
	lv_obj_t *dash[] = {
		heart_icon, label_hr, label_hr_shadow, label_bpm, label_bpm_shadow, divider,
		spo2_icon, label_spo2_num, label_spo2_num_shadow, label_spo2_pct, label_spo2_pct_shadow, btn_spo2
	};
	lv_anim_t a;
	for (int i = 0; i < 12; i++) {
		lv_anim_init(&a);
		lv_anim_set_var(&a, dash[i]);
		lv_anim_set_values(&a, target == 0 ? 255 : 0, target);
		lv_anim_set_exec_cb(&a, opa_cb);
		lv_anim_set_duration(&a, duration);
		lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
		lv_anim_set_completed_cb(&a, (i == 11) ? cb : NULL);
		lv_anim_start(&a);
	}
}

/* ==================== 测量模式 ==================== */

static void start_countdown_pulse(void)
{
	lv_anim_t a;
	lv_anim_init(&a);
	lv_anim_set_var(&a, meas_countdown_label);
	lv_anim_set_values(&a, 100, 255);
	lv_anim_set_exec_cb(&a, text_opa_cb);
	lv_anim_set_duration(&a, 200);
	lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
	lv_anim_start(&a);
}

static void restore_dashboard_cb(lv_anim_t *a)
{
	(void)a;
	exit_meas_mode();
	lv_obj_clear_state(btn_spo2, LV_STATE_DISABLED);
	lv_label_set_text(label_btn, "MEASURE");
	animate_spo2_to(spo2_value, 150);
	start_idle_animations();
}

static void result_fade_out(lv_timer_t *t)
{
	result_timer = NULL;
	if (!meas_bg) return;

	lv_anim_t a;
	lv_anim_init(&a);
	lv_anim_set_var(&a, meas_bg);
	lv_anim_set_values(&a, 255, 0);
	lv_anim_set_exec_cb(&a, opa_cb);
	lv_anim_set_duration(&a, 450);
	lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
	lv_anim_set_completed_cb(&a, restore_dashboard_cb);
	lv_anim_start(&a);

	dashboard_set_opa(255, 380, NULL);
}

static void show_result(void)
{
	lv_anim_delete(NULL, text_opa_cb);
	lv_anim_delete(NULL, arc_indicator_opa_cb);

	lv_anim_t a;
	lv_anim_init(&a);
	lv_anim_set_var(&a, meas_countdown_label);
	lv_anim_set_values(&a, 255, 0);
	lv_anim_set_exec_cb(&a, opa_cb);
	lv_anim_set_duration(&a, 200);
	lv_anim_start(&a);

	lv_anim_set_var(&a, meas_countdown_label_shadow);
	lv_anim_start(&a);

	lv_anim_set_var(&a, meas_hint);
	lv_anim_set_values(&a, 255, 0);
	lv_anim_set_exec_cb(&a, opa_cb);
	lv_anim_set_duration(&a, 200);
	lv_anim_start(&a);

	lv_anim_set_var(&a, meas_hint_shadow);
	lv_anim_start(&a);

	/* 阴影 "98" */
	meas_result_num_shadow = lv_label_create(meas_bg);
	lv_label_set_text(meas_result_num_shadow, "98");
	lv_obj_set_style_text_font(meas_result_num_shadow, &montserrat_48_digits, 0);
	lv_obj_set_style_text_color(meas_result_num_shadow, lv_color_black(), 0);
	lv_obj_set_style_text_opa(meas_result_num_shadow, 0, 0);
	lv_obj_set_style_translate_y(meas_result_num_shadow, 8, 0);
	lv_obj_align(meas_result_num_shadow, LV_ALIGN_CENTER, -8, -8);

	lv_anim_set_var(&a, meas_result_num_shadow);
	lv_anim_set_values(&a, 8, 0);
	lv_anim_set_exec_cb(&a, translate_y_cb);
	lv_anim_set_duration(&a, 450);
	lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
	lv_anim_start(&a);

	lv_anim_set_values(&a, 0, 255);
	lv_anim_set_exec_cb(&a, text_opa_cb);
	lv_anim_set_duration(&a, 400);
	lv_anim_start(&a);

	/* "98" */
	meas_result_num = lv_label_create(meas_bg);
	lv_label_set_text(meas_result_num, "98");
	lv_obj_set_style_text_font(meas_result_num, &montserrat_48_digits, 0);
	lv_obj_set_style_text_color(meas_result_num, lv_color_white(), 0);
	lv_obj_set_style_text_opa(meas_result_num, 0, 0);
	lv_obj_set_style_translate_y(meas_result_num, 8, 0);
	lv_obj_align(meas_result_num, LV_ALIGN_CENTER, -10, -10);

	lv_anim_set_var(&a, meas_result_num);
	lv_anim_set_values(&a, 8, 0);
	lv_anim_set_exec_cb(&a, translate_y_cb);
	lv_anim_set_duration(&a, 450);
	lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
	lv_anim_start(&a);

	lv_anim_set_values(&a, 0, 255);
	lv_anim_set_exec_cb(&a, text_opa_cb);
	lv_anim_set_duration(&a, 400);
	lv_anim_start(&a);

	/* 阴影 "%" */
	meas_result_pct_shadow = lv_label_create(meas_bg);
	lv_label_set_text(meas_result_pct_shadow, "%");
	lv_obj_set_style_text_font(meas_result_pct_shadow, &lv_font_montserrat_20, 0);
	lv_obj_set_style_text_color(meas_result_pct_shadow, lv_color_black(), 0);
	lv_obj_set_style_text_opa(meas_result_pct_shadow, 0, 0);
	lv_obj_align_to(meas_result_pct_shadow, meas_result_num, LV_ALIGN_OUT_RIGHT_TOP, 5, 7);

	lv_anim_set_var(&a, meas_result_pct_shadow);
	lv_anim_set_values(&a, 0, 255);
	lv_anim_set_exec_cb(&a, text_opa_cb);
	lv_anim_set_duration(&a, 400);
	lv_anim_set_delay(&a, 140);
	lv_anim_start(&a);

	/* "%" */
	meas_result_pct = lv_label_create(meas_bg);
	lv_label_set_text(meas_result_pct, "%");
	lv_obj_set_style_text_font(meas_result_pct, &lv_font_montserrat_20, 0);
	lv_obj_set_style_text_color(meas_result_pct, lv_color_hex(0x808080), 0);
	lv_obj_set_style_text_opa(meas_result_pct, 0, 0);
	lv_obj_align_to(meas_result_pct, meas_result_num, LV_ALIGN_OUT_RIGHT_TOP, 4, 6);

	lv_anim_set_var(&a, meas_result_pct);
	lv_anim_set_values(&a, 0, 255);
	lv_anim_set_exec_cb(&a, text_opa_cb);
	lv_anim_set_duration(&a, 400);
	lv_anim_set_delay(&a, 140);
	lv_anim_start(&a);

	lv_arc_set_value(meas_arc, 55);
	lv_obj_set_style_arc_color(meas_arc, lv_color_hex(0x3AFFB6), LV_PART_INDICATOR);
	lv_obj_set_style_arc_opa(meas_arc, LV_OPA_COVER, LV_PART_INDICATOR);

	lv_anim_t a_rot;
	lv_anim_init(&a_rot);
	lv_anim_set_var(&a_rot, &arc_display);
	lv_anim_set_values(&a_rot, 270, 630);
	lv_anim_set_exec_cb(&a_rot, arc_rotation_cb);
	lv_anim_set_duration(&a_rot, 500);
	lv_anim_set_repeat_count(&a_rot, LV_ANIM_REPEAT_INFINITE);
	lv_anim_set_path_cb(&a_rot, lv_anim_path_linear);
	lv_anim_start(&a_rot);

	lv_label_set_text(meas_status, "Measurement complete");
	lv_label_set_text(meas_status_shadow, "Measurement complete");

	result_timer = lv_timer_create(result_fade_out, 1000, NULL);
	lv_timer_set_repeat_count(result_timer, 1);
}

static void on_meas_tick(lv_timer_t *t)
{
	if (spo2_countdown > 1) {
		lv_label_set_text_fmt(meas_countdown_label_shadow, "%d", spo2_countdown);
		lv_label_set_text_fmt(meas_countdown_label, "%d", spo2_countdown);
		start_countdown_pulse();
		spo2_countdown--;
	} else if (spo2_countdown == 1) {
		lv_label_set_text(meas_countdown_label_shadow, "1");
		lv_label_set_text(meas_countdown_label, "1");
		start_countdown_pulse();
		spo2_countdown--;

		spo2_value = 98;
		spo2_measuring = 0;

		lv_timer_delete(meas_timer);
		meas_timer = NULL;

		lv_timer_t *delay = lv_timer_create((lv_timer_cb_t)show_result, 200, NULL);
		lv_timer_set_repeat_count(delay, 1);
	}
}

static void on_dash_faded(lv_anim_t *a)
{
	(void)a;

	meas_bg = lv_obj_create(root);
	lv_obj_set_size(meas_bg, 240, 280);
	lv_obj_set_style_bg_color(meas_bg, lv_color_black(), 0);
	lv_obj_set_style_bg_opa(meas_bg, 0, 0);
	lv_obj_set_style_border_width(meas_bg, 0, 0);
	lv_obj_set_style_pad_all(meas_bg, 0, 0);
	lv_obj_remove_flag(meas_bg, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_align(meas_bg, LV_ALIGN_CENTER, 0, 0);

	lv_anim_t a2;
	lv_anim_init(&a2);
	lv_anim_set_var(&a2, meas_bg);
	lv_anim_set_values(&a2, 0, 255);
	lv_anim_set_exec_cb(&a2, opa_cb);
	lv_anim_set_duration(&a2, 250);
	lv_anim_set_path_cb(&a2, lv_anim_path_ease_out);
	lv_anim_start(&a2);

	/* "Keep still..." shadow */
	meas_hint_shadow = lv_label_create(meas_bg);
	lv_label_set_text(meas_hint_shadow, "Keep still...");
	lv_obj_set_style_text_font(meas_hint_shadow, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(meas_hint_shadow, lv_color_black(), 0);
	lv_obj_set_style_text_opa(meas_hint_shadow, LV_OPA_50, 0);
	lv_obj_align(meas_hint_shadow, LV_ALIGN_CENTER, 1, -97);

	/* "Keep still..." */
	meas_hint = lv_label_create(meas_bg);
	lv_label_set_text(meas_hint, "Keep still...");
	lv_obj_set_style_text_font(meas_hint, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(meas_hint, lv_color_hex(0xA8A8A8), 0);
	lv_obj_align(meas_hint, LV_ALIGN_CENTER, 0, -98);

	lv_anim_set_var(&a2, meas_hint);
	lv_anim_set_values(&a2, 140, 220);
	lv_anim_set_exec_cb(&a2, text_opa_cb);
	lv_anim_set_duration(&a2, 1500);
	lv_anim_set_playback_duration(&a2, 1500);
	lv_anim_set_repeat_count(&a2, LV_ANIM_REPEAT_INFINITE);
	lv_anim_start(&a2);

	/* 扫描弧 */
	meas_arc = lv_arc_create(meas_bg);
	lv_obj_set_size(meas_arc, 108, 108);
	lv_obj_align(meas_arc, LV_ALIGN_CENTER, 0, -10);
	lv_arc_set_mode(meas_arc, LV_ARC_MODE_NORMAL);
	lv_arc_set_range(meas_arc, 0, 300);
	lv_arc_set_value(meas_arc, 0);
	lv_arc_set_bg_angles(meas_arc, 0, 360);
	lv_arc_set_rotation(meas_arc, 270);
	lv_obj_remove_flag(meas_arc, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_set_style_arc_width(meas_arc, 4, LV_PART_MAIN);
	lv_obj_set_style_arc_color(meas_arc, lv_color_hex(0x2A2A2A), LV_PART_MAIN);
	lv_obj_set_style_arc_opa(meas_arc, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_arc_width(meas_arc, 5, LV_PART_INDICATOR);
	lv_obj_set_style_arc_color(meas_arc, lv_color_hex(0x3AFFB6), LV_PART_INDICATOR);
	lv_obj_set_style_arc_opa(meas_arc, LV_OPA_COVER, LV_PART_INDICATOR);
	lv_obj_set_style_arc_rounded(meas_arc, true, LV_PART_INDICATOR);
	lv_obj_remove_style(meas_arc, NULL, LV_PART_KNOB);
	lv_obj_set_style_bg_opa(meas_arc, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(meas_arc, 0, 0);
	lv_obj_set_style_pad_all(meas_arc, 0, 0);
	lv_obj_set_style_shadow_width(meas_arc, 0, 0);

	lv_anim_set_var(&a2, &arc_display);
	lv_anim_set_values(&a2, 0, 300);
	lv_anim_set_exec_cb(&a2, arc_anim_cb);
	lv_anim_set_duration(&a2, 2800);
	lv_anim_set_delay(&a2, 100);
	lv_anim_set_path_cb(&a2, lv_anim_path_ease_in_out);
	lv_anim_start(&a2);

	lv_anim_set_var(&a2, meas_arc);
	lv_anim_set_values(&a2, 200, 255);
	lv_anim_set_exec_cb(&a2, arc_indicator_opa_cb);
	lv_anim_set_duration(&a2, 600);
	lv_anim_set_playback_duration(&a2, 600);
	lv_anim_set_repeat_count(&a2, LV_ANIM_REPEAT_INFINITE);
	lv_anim_start(&a2);

	/* 倒计时阴影 */
	meas_countdown_label_shadow = lv_label_create(meas_bg);
	lv_label_set_text(meas_countdown_label_shadow, "3");
	lv_obj_set_style_text_font(meas_countdown_label_shadow, &montserrat_48_digits, 0);
	lv_obj_set_style_text_color(meas_countdown_label_shadow, lv_color_black(), 0);
	lv_obj_set_style_text_opa(meas_countdown_label_shadow, LV_OPA_50, 0);
	lv_obj_align(meas_countdown_label_shadow, LV_ALIGN_CENTER, 2, -8);

	/* 倒计时数字 */
	meas_countdown_label = lv_label_create(meas_bg);
	lv_label_set_text(meas_countdown_label, "3");
	lv_obj_set_style_text_font(meas_countdown_label, &montserrat_48_digits, 0);
	lv_obj_set_style_text_color(meas_countdown_label, lv_color_white(), 0);
	lv_obj_align(meas_countdown_label, LV_ALIGN_CENTER, 0, -10);
	start_countdown_pulse();

	/* "Scanning..." shadow */
	meas_status_shadow = lv_label_create(meas_bg);
	lv_label_set_text(meas_status_shadow, "Scanning...");
	lv_obj_set_style_text_font(meas_status_shadow, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(meas_status_shadow, lv_color_black(), 0);
	lv_obj_set_style_text_opa(meas_status_shadow, LV_OPA_50, 0);
	lv_obj_align(meas_status_shadow, LV_ALIGN_CENTER, 1, 61);

	/* "Scanning..." */
	meas_status = lv_label_create(meas_bg);
	lv_label_set_text(meas_status, "Scanning...");
	lv_obj_set_style_text_font(meas_status, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(meas_status, lv_color_hex(0x666666), 0);
	lv_obj_align(meas_status, LV_ALIGN_CENTER, 0, 60);
}

static bool meas_gesture_intercept(gesture_t g)
{
	if (g == GESTURE_NONE) return false;

	if (meas_timer) { lv_timer_delete(meas_timer); meas_timer = NULL; }
	if (result_timer) { lv_timer_delete(result_timer); result_timer = NULL; }
	spo2_countdown = 0;
	spo2_measuring = 0;
	watch_data_spo2_abort();

	lv_anim_delete(NULL, text_opa_cb);
	lv_anim_delete(NULL, arc_indicator_opa_cb);
	lv_anim_delete(NULL, opa_cb);

	exit_meas_mode();

	dashboard_set_opa(255, 250, NULL);
	start_idle_animations();

	if (spo2_value > 0) {
		set_spo2_value(spo2_value);
	} else {
		set_spo2_placeholder();
	}

	lv_obj_clear_state(btn_spo2, LV_STATE_DISABLED);
	lv_label_set_text(label_btn, "MEASURE");

	gesture_set_intercept(NULL);
	return true;
}

static void enter_meas_mode(void)
{
	stop_idle_animations();
	gesture_set_intercept(meas_gesture_intercept);
	dashboard_set_opa(0, 300, on_dash_faded);
}

static void on_spo2_click(lv_event_t *e)
{
	if (spo2_measuring) return;

	spo2_measuring = 1;
	spo2_countdown = 3;
	spo2_value = -1;
	arc_display = 0;

	watch_data_spo2_start();

	lv_obj_add_state(btn_spo2, LV_STATE_DISABLED);
	set_spo2_placeholder();

	enter_meas_mode();
	meas_timer = lv_timer_create(on_meas_tick, 1000, NULL);
}

static void on_refresh(lv_timer_t *timer)
{
	hr_value = watch_data_get_heart_rate();
	lv_label_set_text_fmt(label_hr_shadow, "%d", hr_value);
	lv_label_set_text_fmt(label_hr, "%d", hr_value);
}

/* ==================== 页面创建 ==================== */

static lv_obj_t *create(lv_obj_t *parent)
{
	root = lv_obj_create(parent);
	lv_obj_set_style_opa(root, 0, 0);
	lv_obj_set_size(root, 240, 280);
	lv_obj_set_style_pad_all(root, 0, 0);
	lv_obj_set_style_border_width(root, 0, 0);
	lv_obj_set_style_bg_color(root, lv_color_black(), 0);
	lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);

	/* ═══════════════ 分割线 ═══════════════ */
	{
		static const lv_point_precise_t div_pts[] = { {0, 0}, {192, 0} };
		divider = lv_line_create(root);
		lv_obj_set_size(divider, 192, 1);
		lv_line_set_points(divider, div_pts, 2);
		lv_obj_set_style_line_color(divider, lv_color_hex(0x2A2A2A), 0);
		lv_obj_set_style_line_width(divider, 1, 0);
		lv_obj_set_style_line_opa(divider, LV_OPA_COVER, 0);
		lv_obj_align(divider, LV_ALIGN_CENTER, 0, 0);
	}

	/* ═══════════════ 上半区: 心率 ═══════════════ */

	heart_icon = lv_image_create(root);
	lv_image_set_src(heart_icon, &heart_rate_48);
	lv_obj_align(heart_icon, LV_ALIGN_CENTER, -84, -51);

	label_hr_shadow = lv_label_create(root);
	lv_label_set_text_fmt(label_hr_shadow, "%d", watch_data_get_heart_rate());
	lv_obj_set_style_text_font(label_hr_shadow, &montserrat_48_digits, 0);
	lv_obj_set_style_text_color(label_hr_shadow, lv_color_black(), 0);
	lv_obj_set_style_text_opa(label_hr_shadow, LV_OPA_50, 0);
	lv_obj_align(label_hr_shadow, LV_ALIGN_CENTER, 2, -52);

	label_hr = lv_label_create(root);
	lv_label_set_text_fmt(label_hr, "%d", watch_data_get_heart_rate());
	lv_obj_set_style_text_font(label_hr, &montserrat_48_digits, 0);
	lv_obj_set_style_text_color(label_hr, lv_color_white(), 0);
	lv_obj_align(label_hr, LV_ALIGN_CENTER, 0, -54);

	label_bpm_shadow = lv_label_create(root);
	lv_label_set_text(label_bpm_shadow, "bpm");
	lv_obj_set_style_text_font(label_bpm_shadow, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(label_bpm_shadow, lv_color_black(), 0);
	lv_obj_set_style_text_opa(label_bpm_shadow, LV_OPA_50, 0);
	lv_obj_align_to(label_bpm_shadow, label_hr, LV_ALIGN_OUT_RIGHT_BOTTOM, 9, 1);

	label_bpm = lv_label_create(root);
	lv_label_set_text(label_bpm, "bpm");
	lv_obj_set_style_text_font(label_bpm, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(label_bpm, lv_color_hex(0x808080), 0);
	lv_obj_align_to(label_bpm, label_hr, LV_ALIGN_OUT_RIGHT_BOTTOM, 8, 0);

	/* ═══════════════ 下半区: 血氧 ═══════════════ */

	spo2_icon = lv_image_create(root);
	lv_image_set_src(spo2_icon, &blood_oxygen_48);
	lv_obj_align(spo2_icon, LV_ALIGN_CENTER, -80, 54);

	label_spo2_num_shadow = lv_label_create(root);
	lv_obj_align(label_spo2_num_shadow, LV_ALIGN_CENTER, 2, 56);

	label_spo2_num = lv_label_create(root);
	lv_obj_align(label_spo2_num, LV_ALIGN_CENTER, 0, 54);

	label_spo2_pct_shadow = lv_label_create(root);
	label_spo2_pct = lv_label_create(root);

	animate_spo2_to(spo2_value, 420);

	/* MEASURE 胶囊按钮 */
	btn_spo2 = lv_button_create(root);
	lv_obj_set_size(btn_spo2, 186, 40);
	lv_obj_set_style_bg_color(btn_spo2, lv_color_hex(0x1C1C1E), 0);
	lv_obj_set_style_bg_opa(btn_spo2, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_color(btn_spo2, lv_color_hex(0xFF7A4D), 0);
	lv_obj_set_style_border_color(btn_spo2, lv_color_hex(0xFF9A6B), LV_STATE_PRESSED);
	lv_obj_set_style_border_width(btn_spo2, 2, 0);
	lv_obj_set_style_border_opa(btn_spo2, LV_OPA_COVER, 0);
	lv_obj_set_style_radius(btn_spo2, 20, 0);
	lv_obj_set_style_shadow_width(btn_spo2, 0, 0);
	lv_obj_set_style_outline_width(btn_spo2, 0, 0);
	lv_obj_align(btn_spo2, LV_ALIGN_CENTER, 0, 108);
	lv_obj_add_event_cb(btn_spo2, on_spo2_click, LV_EVENT_CLICKED, NULL);

	label_btn = lv_label_create(btn_spo2);
	lv_label_set_text(label_btn, "MEASURE");
	lv_obj_set_style_text_font(label_btn, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(label_btn, lv_color_hex(0xFF7A4D), 0);
	lv_obj_center(label_btn);

	/* ═══════════════ 设置初始动画状态 ═══════════════ */

	lv_obj_set_style_translate_x(heart_icon, -80, 0);
	lv_obj_set_style_transform_scale(heart_icon, 210, 0);
	lv_obj_set_style_translate_x(label_hr_shadow, -120, 0);
	lv_obj_set_style_translate_x(label_hr, -120, 0);
	lv_obj_set_style_translate_x(label_bpm_shadow, -120, 0);
	lv_obj_set_style_translate_x(label_bpm, -120, 0);

	lv_obj_set_style_translate_x(spo2_icon, 160, 0);
	lv_obj_set_style_translate_x(label_spo2_num_shadow, 160, 0);
	lv_obj_set_style_translate_x(label_spo2_num, 160, 0);
	lv_obj_set_style_translate_x(label_spo2_pct_shadow, 160, 0);
	lv_obj_set_style_translate_x(label_spo2_pct, 160, 0);
	lv_obj_set_style_translate_y(btn_spo2, 40, 0);
	lv_obj_set_style_opa(btn_spo2, 0, 0);

	/* ═══════════════ 入场动画 ═══════════════ */

	lv_anim_t a;

	/* root 淡入 */
	lv_anim_init(&a);
	lv_anim_set_var(&a, root);
	lv_anim_set_values(&a, 0, 255);
	lv_anim_set_exec_cb(&a, opa_cb);
	lv_anim_set_duration(&a, 180);
	lv_anim_start(&a);

	/* ── 分隔线淡入 ── */
	lv_anim_set_var(&a, divider);
	lv_anim_set_values(&a, LV_OPA_0, LV_OPA_COVER);
	lv_anim_set_exec_cb(&a, line_opa_cb);
	lv_anim_set_duration(&a, 300);
	lv_anim_set_delay(&a, 60);
	lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
	lv_anim_start(&a);

	/* ═══ 上半区：左→右依次弹入 ═══ */

	/* label_bpm shadow + label_bpm */
	lv_anim_set_var(&a, label_bpm_shadow);
	lv_anim_set_values(&a, -120, 0);
	lv_anim_set_exec_cb(&a, translate_x_cb);
	lv_anim_set_duration(&a, 360);
	lv_anim_set_delay(&a, 200);
	lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
	lv_anim_start(&a);

	lv_anim_set_var(&a, label_bpm);
	lv_anim_start(&a);

	/* label_hr shadow + label_hr */
	lv_anim_set_var(&a, label_hr_shadow);
	lv_anim_set_values(&a, -120, 0);
	lv_anim_set_delay(&a, 280);
	lv_anim_start(&a);

	lv_anim_set_var(&a, label_hr);
	lv_anim_start(&a);

	/* heart_icon */
	lv_anim_set_var(&a, heart_icon);
	lv_anim_set_values(&a, -80, 0);
	lv_anim_set_delay(&a, 360);
	lv_anim_start(&a);

	/* ═══ 下半区：右→左依次弹入 ═══ */

	lv_anim_set_var(&a, spo2_icon);
	lv_anim_set_values(&a, 160, 0);
	lv_anim_set_delay(&a, 200);
	lv_anim_start(&a);

	lv_anim_set_var(&a, label_spo2_num_shadow);
	lv_anim_set_delay(&a, 280);
	lv_anim_start(&a);

	lv_anim_set_var(&a, label_spo2_num);
	lv_anim_start(&a);

	lv_anim_set_var(&a, label_spo2_pct_shadow);
	lv_anim_set_delay(&a, 360);
	lv_anim_start(&a);

	lv_anim_set_var(&a, label_spo2_pct);
	lv_anim_start(&a);

	/* btn_spo2 */
	lv_anim_set_var(&a, btn_spo2);
	lv_anim_set_values(&a, 40, 0);
	lv_anim_set_exec_cb(&a, translate_y_cb);
	lv_anim_set_duration(&a, 380);
	lv_anim_set_delay(&a, 700);
	lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
	lv_anim_start(&a);

	lv_anim_set_values(&a, 0, 255);
	lv_anim_set_exec_cb(&a, opa_cb);
	lv_anim_set_duration(&a, 280);
	lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
	lv_anim_set_completed_cb(&a, on_entry_complete);
	lv_anim_start(&a);

	return root;
}

/* ═══════════════ 销毁 ═══════════════ */

static void destroy(void)
{
	if (refresh_timer) { lv_timer_delete(refresh_timer); refresh_timer = NULL; }
	if (meas_timer)    { lv_timer_delete(meas_timer);    meas_timer    = NULL; }
	if (result_timer)  { lv_timer_delete(result_timer);  result_timer  = NULL; }
	if (spo2_measuring) { watch_data_spo2_abort(); }
	lv_anim_delete(NULL, opa_cb);
	lv_anim_delete(NULL, text_opa_cb);
	lv_anim_delete(NULL, image_opa_cb);
	lv_anim_delete(NULL, scale_cb);
	lv_anim_delete(NULL, border_opa_cb);
	lv_anim_delete(NULL, line_opa_cb);
	lv_anim_delete(NULL, translate_x_cb);
	lv_anim_delete(NULL, translate_y_cb);
	lv_anim_delete(NULL, arc_anim_cb);
	lv_anim_delete(NULL, arc_indicator_opa_cb);
	lv_anim_delete(NULL, arc_rotation_cb);
	lv_anim_delete(NULL, spo2_roll_cb);
	exit_meas_mode();
	idle_animations_running = false;
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
