#include "page.h"
#include "../Framework/page_manager.h"
#include "../Framework/gesture.h"

static lv_obj_t *root;

static void card_click_cb(lv_event_t *e)
{
    page_id_t id = (page_id_t)(uintptr_t)lv_event_get_user_data(e);
    page_manager_push_fade(id);
}

static bool gesture_intercept(gesture_t g)
{
    if (g == GESTURE_UP) {
        page_manager_pop(g);
        return true;
    }
    if (g == GESTURE_LEFT || g == GESTURE_RIGHT)
        return true;
    return false;
}

#define CARD_W    68
#define CARD_H    74
#define CARD_GAP  6
#define GRID_X    12
#define GRID_Y1   24
#define GRID_Y2   (GRID_Y1 + CARD_H + 8)

static lv_obj_t *make_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                            const lv_image_dsc_t *icon_src, const char *label_text)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, CARD_W, CARD_H);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_border_opa(card, LV_OPA_60, 0);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1C1C1E), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_60, 0);

    lv_obj_t *icon = lv_image_create(card);
    lv_image_set_src(icon, icon_src);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *label = lv_label_create(card);
    lv_label_set_text(label, label_text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -6);

    return card;
}

#define SLIDER_W 124

static lv_obj_t *create(lv_obj_t *parent)
{
    root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

    extern const lv_image_dsc_t lanya_line_32;
    extern const lv_image_dsc_t wurao_line_32;
    extern const lv_image_dsc_t jieneng_line_32;
    extern const lv_image_dsc_t taiwan_line_32;
    extern const lv_image_dsc_t shoudiantong_line_32;
    extern const lv_image_dsc_t shezhi_line_32;

    /* ── 3×2 功能卡片网格 ── */
    /* 第一行 */
    make_card(root, GRID_X + 0 * (CARD_W + CARD_GAP), GRID_Y1,
              &lanya_line_32, "BT");
    make_card(root, GRID_X + 1 * (CARD_W + CARD_GAP), GRID_Y1,
              &taiwan_line_32, "Raise");
    make_card(root, GRID_X + 2 * (CARD_W + CARD_GAP), GRID_Y1,
              &jieneng_line_32, "Power");
    /* 第二行 */
    make_card(root, GRID_X + 0 * (CARD_W + CARD_GAP), GRID_Y2,
              &wurao_line_32, "DND");
    lv_obj_t *card_light = make_card(root, GRID_X + 1 * (CARD_W + CARD_GAP), GRID_Y2,
              &shoudiantong_line_32, "Light");
    lv_obj_add_flag(card_light, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card_light, card_click_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)PAGE_APP_FLASHLIGHT);

    lv_obj_t *card_settings = make_card(root, GRID_X + 2 * (CARD_W + CARD_GAP), GRID_Y2,
              &shezhi_line_32, "Settings");
    lv_obj_add_flag(card_settings, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card_settings, card_click_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)PAGE_APP_SETTINGS);

    extern const lv_image_dsc_t pingmuliangdu_32;

    /* ── 亮度调节卡片 ── */
    lv_obj_t *slider_card = lv_obj_create(root);
    lv_obj_set_size(slider_card, 204, 56);
    lv_obj_set_pos(slider_card, 18, GRID_Y2 + CARD_H + 16);
    lv_obj_set_style_pad_all(slider_card, 0, 0);
    lv_obj_set_style_border_width(slider_card, 1, 0);
    lv_obj_set_style_border_color(slider_card, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_border_opa(slider_card, LV_OPA_60, 0);
    lv_obj_set_style_radius(slider_card, 14, 0);
    lv_obj_set_style_bg_color(slider_card, lv_color_hex(0x1C1C1E), 0);
    lv_obj_set_style_bg_opa(slider_card, LV_OPA_60, 0);

    /* 亮度图标 */
    lv_obj_t *icon = lv_image_create(slider_card);
    lv_image_set_src(icon, &pingmuliangdu_32);
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 16, 0);

    /* slider */
    lv_obj_t *slider = lv_slider_create(slider_card);
    lv_obj_set_size(slider, SLIDER_W, 32);
    lv_obj_align(slider, LV_ALIGN_RIGHT_MID, -16, 0);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, 75, LV_ANIM_OFF);

    /* slider 轨道样式 */
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, 6, LV_PART_INDICATOR);

    /* slider 无圆球 */
    lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 0, LV_PART_KNOB);

    gesture_set_intercept(gesture_intercept);

    return root;
}

static void destroy(void)
{
    if (gesture_get_intercept() == gesture_intercept)
        gesture_set_intercept(NULL);
}

static void update(void) {}

const page_t page_control_center = {
    .name = "control_center",
    .type = PAGE_TYPE_OVERLAY,
    .create = create,
    .destroy = destroy,
    .update = update,
};
