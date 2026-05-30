#include "page.h"
#include "../Data/data_provider.h"
#include "../Framework/page_manager.h"
#include "../Framework/gesture.h"

static lv_obj_t *root;
static lv_obj_t *heart_icon;
static lv_obj_t *label_hr;
static lv_obj_t *label_bpm;
static lv_obj_t *label_status;
static lv_obj_t *label_measuring;
static lv_timer_t *measure_timer;
static lv_timer_t *refresh_timer;
static bool pulse_running;
static bool blink_running;

extern const lv_image_dsc_t cexinlv_aixin_64;
extern const lv_font_t montserrat_48_digits;

/* ── 心跳呼吸动画（opacity，不吃变换层缓冲区）── */
static void pulse_cb(void *var, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)var, v, LV_PART_MAIN);
}

static void start_pulse(void)
{
    if (pulse_running) return;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, heart_icon);
    lv_anim_set_values(&a, 217, LV_OPA_COVER);
    lv_anim_set_exec_cb(&a, pulse_cb);
    lv_anim_set_duration(&a, 120);
    lv_anim_set_playback_duration(&a, 600);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
    pulse_running = true;
}

static void stop_pulse(void)
{
    if (!pulse_running) return;
    lv_anim_delete(NULL, pulse_cb);
    pulse_running = false;
}

/* ── 测量中提示文字的缓慢闪烁 ── */
static void blink_cb(void *var, int32_t v)
{
    lv_obj_set_style_text_opa((lv_obj_t *)var, v, 0);
}

static void start_blink(void)
{
    if (blink_running) return;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, label_measuring);
    lv_anim_set_values(&a, LV_OPA_40, LV_OPA_COVER);
    lv_anim_set_exec_cb(&a, blink_cb);
    lv_anim_set_duration(&a, 800);
    lv_anim_set_playback_duration(&a, 800);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
    blink_running = true;
}

static void stop_blink(void)
{
    if (!blink_running) return;
    lv_anim_delete(NULL, blink_cb);
    blink_running = false;
}

static void on_refresh(lv_timer_t *timer);

/* ── 模拟测量完成 → 显示当前数据 + 启动定期刷新 ── */
static void measure_done_cb(lv_timer_t *timer)
{
    (void)timer;
    measure_timer = NULL;
    stop_blink();
    lv_obj_add_flag(label_measuring, LV_OBJ_FLAG_HIDDEN);
    int32_t hr = watch_data_get_heart_rate();
    lv_label_set_text_fmt(label_hr, "%d", (int)hr);
    lv_label_set_text_fmt(label_status, "%d BPM, Just now", (int)hr);
    lv_obj_align_to(label_hr, label_status, LV_ALIGN_OUT_TOP_LEFT, 0, -10);
    lv_obj_align_to(label_bpm, label_hr, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, 0);
    refresh_timer = lv_timer_create(on_refresh, 1000, NULL);
}

/* ── 数据刷新 ── */
static void on_refresh(lv_timer_t *timer)
{
    (void)timer;
    int32_t hr = watch_data_get_heart_rate();
    lv_label_set_text_fmt(label_hr, "%d", (int)hr);
    lv_label_set_text_fmt(label_status, "%d BPM, Just now", (int)hr);
    lv_obj_align_to(label_hr, label_status, LV_ALIGN_OUT_TOP_LEFT, 0, -10);
    lv_obj_align_to(label_bpm, label_hr, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, 0);
}

/* ── 手势：上滑 → 详情页 ── */
static bool gesture_intercept(gesture_t g)
{
    if (g == GESTURE_UP) {
        page_manager_push_up(PAGE_APP_HEARTRATE_DETAIL);
        return true;
    }
    return false;
}

/* ═══════════════════════════════════════════════════════════════
   布局（240×280）
   y=0..160   爱心图标 64×64，中心偏上
   y=160..240 大数字 + 单位 bpm
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

    /* 爱心图标 64×64，中心偏上 */
    heart_icon = lv_image_create(root);
    lv_image_set_src(heart_icon, &cexinlv_aixin_64);
    lv_obj_set_size(heart_icon, 64, 64);
    lv_obj_align(heart_icon, LV_ALIGN_CENTER, 0, -30);

    /* 状态小字：立即显示上次测量值（模拟），居中作为锚点 */
    label_status = lv_label_create(root);
    lv_label_set_text_fmt(label_status, "%d BPM, Just now",
                          (int)watch_data_get_heart_rate());
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_status, lv_color_white(), 0);
    lv_obj_align(label_status, LV_ALIGN_CENTER, 0, 70);

    /* 心率数值，初始显示 "--"（等待测量） */
    label_hr = lv_label_create(root);
    lv_label_set_text(label_hr, "--");
    lv_obj_set_style_text_font(label_hr, &montserrat_48_digits, 0);
    lv_obj_set_style_text_color(label_hr, lv_color_white(), 0);
    lv_obj_align_to(label_hr, label_status, LV_ALIGN_OUT_TOP_LEFT, 0, -10);

    /* 单位 bpm，数字右下 */
    label_bpm = lv_label_create(root);
    lv_label_set_text(label_bpm, "bpm");
    lv_obj_set_style_text_font(label_bpm, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_bpm, lv_color_hex(0xFF3B4A), 0);
    lv_obj_align_to(label_bpm, label_hr, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, 0);

    /* 测量中提示文字，底部居中，缓慢闪烁 */
    label_measuring = lv_label_create(root);
    lv_label_set_text(label_measuring, "Measuring...");
    lv_obj_set_style_text_font(label_measuring, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_measuring, lv_color_white(), 0);
    lv_obj_align(label_measuring, LV_ALIGN_BOTTOM_MID, 0, -16);

    gesture_set_intercept(gesture_intercept);
    measure_timer = lv_timer_create(measure_done_cb, 3000, NULL);
    lv_timer_set_repeat_count(measure_timer, 1);
    start_pulse();
    start_blink();
    return root;
}

static void destroy(void)
{
    if (gesture_get_intercept() == gesture_intercept)
        gesture_set_intercept(NULL);
    if (measure_timer) { lv_timer_delete(measure_timer); measure_timer = NULL; }
    if (refresh_timer) { lv_timer_delete(refresh_timer); refresh_timer = NULL; }
    stop_pulse();
    stop_blink();
}

static void update(void) {}

const page_t page_app_heartrate = {
    .name    = "app_heartrate",
    .type    = PAGE_TYPE_OVERLAY,
    .create  = create,
    .destroy = destroy,
    .update  = update,
};
