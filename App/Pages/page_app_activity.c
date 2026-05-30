#include "page.h"
#include "../Data/data_provider.h"
#include "../Framework/page_manager.h"
#include "../Framework/gesture.h"

static lv_obj_t *root;
static lv_obj_t *arc;
static lv_obj_t *label_steps;
static lv_obj_t *label_unit;
static lv_obj_t *label_target;
static lv_obj_t *label_accomplish;
static lv_obj_t *label_distance;
static lv_obj_t *label_active;
static lv_timer_t *refresh_timer;
static int32_t anim_val;

extern const lv_image_dsc_t yundongxie_32;
extern const lv_image_dsc_t mubiao_16;
extern const lv_image_dsc_t jvli_16;
extern const lv_image_dsc_t huoyueshijian_16;
extern const lv_font_t montserrat_48_digits;

#define STEPS_GOAL   10000
#define ACCENT_COLOR lv_color_hex(0x00E676)

static void on_refresh(lv_timer_t *t)
{
    (void)t;
    int32_t steps = watch_data_get_steps();
    if (steps > STEPS_GOAL) steps = STEPS_GOAL;
    lv_label_set_text_fmt(label_steps, "%d", (int)steps);
    lv_arc_set_value(arc, steps);

    /* card 1: index % */
    int32_t pct = (steps * 100) / STEPS_GOAL;
    lv_label_set_text_fmt(label_accomplish, "%d%%", (int)pct);

    /* card 2: distance */
    int32_t dist_m = watch_data_get_distance_m();
    if (dist_m >= 1000) {
        int32_t km = dist_m / 1000;
        int32_t frac = (dist_m % 1000) / 100;
        lv_label_set_text_fmt(label_distance, "%d.%d km", (int)km, (int)frac);
    } else
        lv_label_set_text_fmt(label_distance, "%d m", (int)dist_m);

    /* card 3: active time */
    static int32_t active_min;
    active_min += 1;
    if (active_min >= 60) {
        int32_t h = active_min / 60;
        int32_t frac = (active_min % 60) / 6;
        lv_label_set_text_fmt(label_active, "%d.%d h", (int)h, (int)frac);
    } else
        lv_label_set_text_fmt(label_active, "%d min", (int)active_min);
}

/* ── 入场滚动动画 ── */
static void roll_cb(void *var, int32_t v)
{
    (void)var;
    lv_label_set_text_fmt(label_steps, "%d", v);
    lv_arc_set_value(arc, v);
}

static void on_entry_done(lv_anim_t *a)
{
    (void)a;
    refresh_timer = lv_timer_create(on_refresh, 1000, NULL);
}

static bool gesture_intercept(gesture_t g)
{
    if (g == GESTURE_UP) {
        page_manager_push_up(PAGE_APP_ACTIVITY_DETAIL);
        return true;
    }
    return false;
}

/* ── 创建单个卡片：图标 + 描述 + 数值 ── */
static void make_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                      const lv_image_dsc_t *icon_src, const char *desc,
                      lv_obj_t **out_value)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 74, 70);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, 0);

    /* icon */
    lv_obj_t *icon = lv_image_create(card);
    lv_image_set_src(icon, icon_src);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 6);

    /* description */
    lv_obj_t *lbl_desc = lv_label_create(card);
    lv_label_set_text(lbl_desc, desc);
    lv_obj_set_style_text_font(lbl_desc, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_desc, lv_color_hex(0x888888), 0);
    lv_obj_align(lbl_desc, LV_ALIGN_CENTER, 0, 0);

    /* value */
    lv_obj_t *lbl_val = lv_label_create(card);
    lv_obj_set_style_text_font(lbl_val, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_val, lv_color_white(), 0);
    lv_obj_align(lbl_val, LV_ALIGN_BOTTOM_MID, 0, -6);
    *out_value = lbl_val;
}

static lv_obj_t *create(lv_obj_t *parent)
{
    root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_clip_corner(root, true, 0);

    /* 全圆进度弧 */
    arc = lv_arc_create(root);
    lv_obj_set_size(arc, 180, 180);
    lv_obj_align(arc, LV_ALIGN_CENTER, 0, -30);
    lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
    lv_arc_set_range(arc, 0, STEPS_GOAL);
    lv_arc_set_value(arc, 0);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_rotation(arc, 270);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x2A2A2A), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, ACCENT_COLOR, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(arc, 0, 0);
    lv_obj_set_style_pad_all(arc, 0, 0);

    /* 图标 32×32 */
    lv_obj_t *icon = lv_image_create(root);
    lv_image_set_src(icon, &yundongxie_32);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -75);

    /* 步数大数字，初始 0 */
    label_steps = lv_label_create(root);
    lv_label_set_text(label_steps, "0");
    lv_obj_set_style_text_font(label_steps, &montserrat_48_digits, 0);
    lv_obj_set_style_text_color(label_steps, lv_color_white(), 0);
    lv_obj_align(label_steps, LV_ALIGN_CENTER, 0, -38);

    /* "steps" */
    label_unit = lv_label_create(root);
    lv_label_set_text(label_unit, "steps");
    lv_obj_set_style_text_font(label_unit, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_unit, ACCENT_COLOR, 0);
    lv_obj_align(label_unit, LV_ALIGN_CENTER, 0, -6);

    /* 目标 "/ 10000" */
    label_target = lv_label_create(root);
    lv_label_set_text(label_target, "/ 10000");
    lv_obj_set_style_text_font(label_target, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_target, lv_color_hex(0x666666), 0);
    lv_obj_align(label_target, LV_ALIGN_CENTER, 0, 20);

    /* 三张卡片 */
    make_card(root, 5, 206, &mubiao_16, "index",      &label_accomplish);
    make_card(root, 83, 206, &jvli_16,   "distance",   &label_distance);
    make_card(root, 161, 206, &huoyueshijian_16, "active", &label_active);

    /* 初始值 — 直接用真实数据，避免卡片显示 0 */
    int32_t init_steps = watch_data_get_steps();
    if (init_steps > STEPS_GOAL) init_steps = STEPS_GOAL;
    int32_t init_pct = (init_steps * 100) / STEPS_GOAL;
    lv_label_set_text_fmt(label_accomplish, "%d%%", (int)init_pct);

    int32_t init_dist = watch_data_get_distance_m();
    if (init_dist >= 1000) {
        int32_t km = init_dist / 1000;
        int32_t frac = (init_dist % 1000) / 100;
        lv_label_set_text_fmt(label_distance, "%d.%d km", (int)km, (int)frac);
    } else
        lv_label_set_text_fmt(label_distance, "%d m", (int)init_dist);

    lv_label_set_text(label_active, "0 min");

    gesture_set_intercept(gesture_intercept);

    /* 入场动画 */
    int32_t target = watch_data_get_steps();
    if (target > STEPS_GOAL) target = STEPS_GOAL;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, &anim_val);
    lv_anim_set_values(&a, 0, target);
    lv_anim_set_exec_cb(&a, roll_cb);
    lv_anim_set_duration(&a, 1200);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&a, on_entry_done);
    lv_anim_start(&a);

    return root;
}

static void destroy(void)
{
    if (gesture_get_intercept() == gesture_intercept)
        gesture_set_intercept(NULL);
    lv_anim_delete(&anim_val, roll_cb);
    if (refresh_timer) { lv_timer_delete(refresh_timer); refresh_timer = NULL; }
}

static void update(void) {}

const page_t page_app_activity = {
    .name    = "activity",
    .type    = PAGE_TYPE_OVERLAY,
    .create  = create,
    .destroy = destroy,
    .update  = update,
};
