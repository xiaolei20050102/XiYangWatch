#include "page.h"

static lv_obj_t *empty_create(lv_obj_t *parent)
{
    lv_obj_t *page = lv_obj_create(parent);
    lv_obj_set_size(page, 240, 280);
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_bg_color(page, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    return page;
}

const page_t page_app_focus       = { "focus",       PAGE_TYPE_OVERLAY, empty_create, NULL, NULL };


