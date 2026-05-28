#include "page.h"
#include "../Data/data_provider.h"

static lv_obj_t *root;
static lv_obj_t *arc;
static lv_obj_t *arc_shadow;
static lv_obj_t *label_steps;
static lv_obj_t *label_steps_shadow;
static lv_obj_t *label_target;
static lv_obj_t *label_target_shadow;
static lv_timer_t *refresh_timer;
static int32_t arc_current;
static int32_t anim_dummy1;
static int32_t anim_dummy2;
static bool    arc_intro_done;

extern const lv_image_dsc_t The_little_person_is_running;
extern const lv_font_t montserrat_48_digits;

static void arc_anim_cb(void *var, int32_t v)
{
    (void)var;
    lv_arc_set_value(arc_shadow, v);
    lv_arc_set_value(arc, v);
}

static void number_anim_cb(void *var, int32_t v)
{
    (void)var;
    lv_label_set_text_fmt(label_steps_shadow, "%d", v);
    lv_label_set_text_fmt(label_steps, "%d", v);
}

static void arc_intro_finish(lv_anim_t *a)
{
    (void)a;
    arc_intro_done = true;
}

static void on_refresh(lv_timer_t *timer)
{
    int steps = (int)watch_data_get_steps();
    lv_label_set_text_fmt(label_steps_shadow, "%d", steps);
    lv_label_set_text_fmt(label_steps, "%d", steps);

    if (!arc_intro_done) return;

    int32_t target = steps;
    if (target > 10000) target = 10000;
    if (arc_current == target) return;
    arc_current = target;
    lv_arc_set_value(arc_shadow, target);
    lv_arc_set_value(arc, target);
}

static lv_obj_t *create(lv_obj_t *parent)
{
    root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);

    /* ── 阴影底弧：INDICATOR 略宽，暗色过渡 ── */
    arc_shadow = lv_arc_create(root);
    lv_obj_set_size(arc_shadow, 210, 210);
    lv_obj_center(arc_shadow);
    lv_obj_remove_style(arc_shadow, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(arc_shadow, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_width(arc_shadow, 0, 0);
    lv_obj_set_style_bg_opa(arc_shadow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_arc_width(arc_shadow, 18, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_shadow, lv_color_hex(0x005566), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc_shadow, LV_OPA_40, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_shadow, 22, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_shadow, lv_color_hex(0x000000), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc_shadow, LV_OPA_30, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc_shadow, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc_shadow, true, LV_PART_INDICATOR);
    lv_arc_set_bg_angles(arc_shadow, 135, 45);
    lv_arc_set_range(arc_shadow, 0, 10000);
    lv_arc_set_value(arc_shadow, 0);

    /* ── 圆弧进度条 ── */
    arc = lv_arc_create(root);
    lv_obj_set_size(arc, 210, 210);
    lv_obj_center(arc);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_width(arc, 0, 0);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, 0);
    lv_obj_set_style_arc_width(arc, 18, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 18, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x005566), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_40, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x00E5FF), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);

    lv_arc_set_bg_angles(arc, 135, 45);
    lv_arc_set_range(arc, 0, 10000);
    lv_arc_set_value(arc, 0);
    arc_current = 0;
    arc_intro_done = false;

    /* ── 步数 ── */
    label_steps_shadow = lv_label_create(root);
    lv_label_set_text(label_steps_shadow, "0");
    lv_obj_set_style_text_font(label_steps_shadow, &montserrat_48_digits, 0);
    lv_obj_set_style_text_color(label_steps_shadow, lv_color_black(), 0);
    lv_obj_set_style_text_opa(label_steps_shadow, LV_OPA_50, 0);
    lv_obj_align(label_steps_shadow, LV_ALIGN_CENTER, 2, -2);

    label_steps = lv_label_create(root);
    lv_label_set_text(label_steps, "0");
    lv_obj_set_style_text_font(label_steps, &montserrat_48_digits, 0);
    lv_obj_set_style_text_color(label_steps, lv_color_white(), 0);
    lv_obj_align(label_steps, LV_ALIGN_CENTER, 0, -4);

    /* ── 入场动画：圆弧充能 + 数字滚动，EASE_OUT 路径 ── */
    int32_t initial_steps = (int)watch_data_get_steps();
    if (initial_steps > 10000) initial_steps = 10000;
    if (initial_steps > 0) {
        int32_t duration = 600 + (initial_steps * 1400) / 10000;
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, &anim_dummy1);
        lv_anim_set_values(&a, 0, initial_steps);
        lv_anim_set_exec_cb(&a, arc_anim_cb);
        lv_anim_set_duration(&a, duration);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_set_completed_cb(&a, arc_intro_finish);
        lv_anim_start(&a);

        /* 数字同步滚动 */
        lv_anim_t b;
        lv_anim_init(&b);
        lv_anim_set_var(&b, &anim_dummy2);
        lv_anim_set_values(&b, 0, initial_steps);
        lv_anim_set_exec_cb(&b, number_anim_cb);
        lv_anim_set_duration(&b, duration);
        lv_anim_set_path_cb(&b, lv_anim_path_ease_out);
        lv_anim_start(&b);

        arc_current = initial_steps;
    } else {
        arc_intro_done = true;
    }

    /* ── 小人图标 ── */
    lv_obj_t *icon = lv_image_create(root);
    lv_image_set_src(icon, &The_little_person_is_running);
    lv_obj_set_style_image_recolor(icon, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, 36);

    /* ── 目标 ── */
    label_target_shadow = lv_label_create(root);
    lv_label_set_text(label_target_shadow, "/ 10000");
    lv_obj_set_style_text_font(label_target_shadow, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_target_shadow, lv_color_black(), 0);
    lv_obj_set_style_text_opa(label_target_shadow, LV_OPA_50, 0);
    lv_obj_align(label_target_shadow, LV_ALIGN_CENTER, 1, 61);

    label_target = lv_label_create(root);
    lv_label_set_text(label_target, "/ 10000");
    lv_obj_set_style_text_font(label_target, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_target, lv_color_hex(0x606060), 0);
    lv_obj_align(label_target, LV_ALIGN_CENTER, 0, 60);

    refresh_timer = lv_timer_create(on_refresh, 1000, NULL);
    return root;
}

static void destroy(void)
{
    if (refresh_timer) { lv_timer_delete(refresh_timer); refresh_timer = NULL; }
    lv_anim_delete(NULL, arc_anim_cb);
    lv_anim_delete(NULL, number_anim_cb);
}

static void update(void) {}

const page_t page_activity = {
    .name = "activity",
    .type = PAGE_TYPE_SPOKE,
    .create = create,
    .destroy = destroy,
    .update = update,
};
