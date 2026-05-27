#include "page.h"
#include "../Framework/gesture.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


static lv_obj_t *root;
static lv_obj_t *disp_cont;
static lv_obj_t *display;
static char disp[32];
static int  dlen;
static bool err_flag;
static bool disp_active;

/* ── string → double, no stdlib dependency ── */
static double my_atof(const char *s, int len)
{
    double v = 0, frac = 0, div = 1;
    int sign = 1, i = 0, dot = 0;
    if (len > 0 && s[0] == '-') { sign = -1; i = 1; }
    for (; i < len; i++) {
        if (s[i] == '.') { dot = 1; continue; }
        if (dot) { frac = frac * 10 + (s[i] - '0'); div *= 10; }
        else     { v = v * 10 + (s[i] - '0'); }
    }
    return sign * (v + frac / div);
}

/* ── block gesture nav while dragging display ── */
static bool intercept_cb(gesture_t g)
{
    (void)g;
    return disp_active;
}

static void disp_evt_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING)
        disp_active = true;
    else if (code == LV_EVENT_RELEASED)
        disp_active = false;
}

static void show(void)
{
    lv_label_set_text(display, dlen ? disp : "0");
    lv_obj_scroll_to_x(disp_cont, LV_COORD_MAX, LV_ANIM_OFF);
}

static void add(const char *s)
{
    int sl = (int)strlen(s);
    if (dlen + sl >= (int)sizeof(disp) - 1) return;
    memcpy(disp + dlen, s, sl);
    dlen += sl;
    disp[dlen] = '\0';
    show();
}

/* ── double → string ── */
static void set_result(double v)
{
    int ip, istart, i, j, fstart, d;
    double frac;

    if (v < 0) { disp[0] = '-'; dlen = 1; v = -v; }
    else dlen = 0;

    istart = dlen;
    ip = (int)v;
    if (ip == 0) { disp[dlen++] = '0'; }
    else {
        while (ip > 0 && dlen < 31) {
            disp[dlen++] = '0' + (ip % 10);
            ip /= 10;
        }
        for (i = istart, j = dlen - 1; i < j; i++, j--) {
            char t = disp[i]; disp[i] = disp[j]; disp[j] = t;
        }
    }

    frac = v - (int)v;
    if (frac > 0.000001 && dlen < 30) {
        disp[dlen++] = '.';
        fstart = dlen;
        for (i = 0; i < 6; i++) {
            frac *= 10;
            d = (int)frac;
            frac -= d;
            disp[dlen++] = '0' + d;
        }
        while (dlen > fstart && disp[dlen - 1] == '0') dlen--;
        if (dlen > fstart && disp[dlen - 1] == '.') dlen--;
    }

    disp[dlen] = '\0';
    show();
}

/* ── evaluate with operator precedence (two-stack) ── */
static int is_op(char c)
{
    return c == '+' || c == '-' || c == '*' || c == '/';
}

static double eval(void)
{
    double vstack[16], a, b;
    char   ostack[16];
    int    vtop, otop, i, len, sign, istart, nl, prec;
    char   op, to;
    int    tp;

    vtop = 0; otop = 0; i = 0;
    len = dlen;

    while (len > 0 && is_op(disp[len - 1])) len--;
    if (len == 0) return 0;

    while (i < len) {
        sign = 1;
        if (disp[i] == '-') {
            if (i == 0 || (i > 0 && is_op(disp[i-1]))) {
                sign = -1; i++;
            }
        }

        istart = i;
        while (i < len && ((disp[i] >= '0' && disp[i] <= '9') || disp[i] == '.')) i++;
        if (i == istart) return 0;

        nl = i - istart;
        vstack[vtop++] = my_atof(disp + istart, nl) * sign;

        if (i >= len) break;

        op = disp[i++];
        prec = (op == '*' || op == '/') ? 2 : 1;

        while (otop > 0) {
            to = ostack[otop - 1];
            tp = (to == '*' || to == '/') ? 2 : 1;
            if (tp < prec) break;
            otop--;
            b = vstack[--vtop]; a = vstack[--vtop];
            switch (to) {
            case '+': vstack[vtop++] = a + b; break;
            case '-': vstack[vtop++] = a - b; break;
            case '*': vstack[vtop++] = a * b; break;
            case '/':
                if (b == 0) { err_flag = true; return 0; }
                vstack[vtop++] = a / b; break;
            }
        }
        ostack[otop++] = op;
    }

    while (otop > 0) {
        otop--;
        b = vstack[--vtop]; a = vstack[--vtop];
        switch (ostack[otop]) {
        case '+': vstack[vtop++] = a + b; break;
        case '-': vstack[vtop++] = a - b; break;
        case '*': vstack[vtop++] = a * b; break;
        case '/':
            if (b == 0) { err_flag = true; return 0; }
            vstack[vtop++] = a / b; break;
        }
    }

    return vtop > 0 ? vstack[0] : 0;
}

/* ── callbacks ── */
static void on_digit(lv_event_t *e)
{
    int d;
    char s[2];
    if (gesture_was_recent()) return;
    d = (int)(uintptr_t)lv_event_get_user_data(e);
    s[0] = '0' + d; s[1] = '\0';
    add(s);
}

static void on_op(lv_event_t *e)
{
    int o;
    char last, s[2];
    if (gesture_was_recent()) return;
    o = (int)(uintptr_t)lv_event_get_user_data(e);
    last = dlen > 0 ? disp[dlen - 1] : 0;

    if (dlen == 0 && (char)o != '-') return;
    if (last == '+' || last == '*' || last == '/') {
        if ((char)o != '-') return;
    }
    if (last == '-' && (char)o == '-') return;

    s[0] = (char)o; s[1] = '\0';
    add(s);
}

static void on_eq(lv_event_t *e)
{
    double res;
    if (gesture_was_recent()) return;
    err_flag = false;
    res = eval();
    if (err_flag) {
        dlen = 0; disp[0] = '\0';
        lv_label_set_text(display, "ERROR");
        return;
    }
    set_result(res);
}

static void on_C(lv_event_t *e)
{
    if (gesture_was_recent()) return;
    err_flag = false;
    dlen = 0; disp[0] = '\0'; show();
}

static void on_dot(lv_event_t *e)
{
    int last_dot, i;
    if (gesture_was_recent()) return;
    last_dot = -1;
    for (i = dlen - 1; i >= 0; i--) {
        if (disp[i] == '.') { last_dot = i; break; }
        if (is_op(disp[i])) break;
    }
    if (last_dot < 0) add(".");
}

#define UD(n) ((void*)(uintptr_t)(n))

/* ── UI ── */
static lv_obj_t *btn(lv_obj_t *p, const char *t, lv_event_cb_t cb,
                     lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                     lv_color_t bg, lv_color_t fg, uintptr_t data)
{
    lv_obj_t *b, *l;
    b = lv_button_create(p);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_radius(b, h / 3, 0);
    lv_obj_set_style_bg_color(b, bg, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    l = lv_label_create(b);
    lv_label_set_text(l, t);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l, fg, 0);
    lv_obj_center(l);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, UD(data));
    return b;
}

static lv_obj_t *create(lv_obj_t *parent)
{
    lv_color_t cd, cf, co, cb;

    root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 280);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x080808), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    /* scrollable display line */
    disp_cont = lv_obj_create(root);
    lv_obj_set_size(disp_cont, 210, 42);
    lv_obj_set_style_pad_all(disp_cont, 0, 0);
    lv_obj_set_style_border_width(disp_cont, 0, 0);
    lv_obj_set_style_bg_opa(disp_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_scroll_dir(disp_cont, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(disp_cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(disp_cont, disp_evt_cb, LV_EVENT_ALL, NULL);
    lv_obj_align(disp_cont, LV_ALIGN_TOP_RIGHT, -16, 24);

    display = lv_label_create(disp_cont);
    lv_obj_set_style_text_font(display, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(display, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_width(display, LV_SIZE_CONTENT);

    /* colors */
    cd = lv_color_hex(0x181818);
    cf = lv_color_hex(0xE8E8E8);
    co = lv_color_hex(0x00C2FF);
    cb = lv_color_hex(0x262626);

    /* ── digit pad 3x4 ── */
#define BW 64
#define BH 36
#define GX 7
#define MG ((240 - 3*BW - 2*GX) / 2)
#define X(c) (MG + (c)*(BW + GX))
#define Y(r) (72 + (r)*(BH + 3))

    btn(root, "7", on_digit, X(0), Y(0), BW, BH, cd, cf, 7);
    btn(root, "8", on_digit, X(1), Y(0), BW, BH, cd, cf, 8);
    btn(root, "9", on_digit, X(2), Y(0), BW, BH, cd, cf, 9);

    btn(root, "4", on_digit, X(0), Y(1), BW, BH, cd, cf, 4);
    btn(root, "5", on_digit, X(1), Y(1), BW, BH, cd, cf, 5);
    btn(root, "6", on_digit, X(2), Y(1), BW, BH, cd, cf, 6);

    btn(root, "1", on_digit, X(0), Y(2), BW, BH, cd, cf, 1);
    btn(root, "2", on_digit, X(1), Y(2), BW, BH, cd, cf, 2);
    btn(root, "3", on_digit, X(2), Y(2), BW, BH, cd, cf, 3);

    btn(root, "C", on_C,   X(0), Y(3), BW, BH, cb, lv_color_hex(0xFF6B6B), 0);
    btn(root, "0", on_digit, X(1), Y(3), BW, BH, cd, cf, 0);
    btn(root, ".", on_dot, X(2), Y(3), BW, BH, cd, cf, 0);

    /* ── operator bar ── */
#define OW 40
#define OH 34
#define OG 4
#define OMG ((240 - 5*OW - 4*OG) / 2)
#define OX(c) (OMG + (c)*(OW + OG))
#define OY   (280 - OH - 8)

    btn(root, "+", on_op, OX(0), OY, OW, OH, co, lv_color_black(), '+');
    btn(root, "-", on_op, OX(1), OY, OW, OH, co, lv_color_black(), '-');
    btn(root, "*", on_op, OX(2), OY, OW, OH, co, lv_color_black(), '*');
    btn(root, "/", on_op, OX(3), OY, OW, OH, co, lv_color_black(), '/');
    btn(root, "=", on_eq, OX(4), OY, OW, OH, co, lv_color_black(), 0);

    on_C(NULL);
    gesture_set_intercept(intercept_cb);
    return root;
}

static void destroy(void)
{
    gesture_set_intercept(NULL);
}

const page_t page_app_calculator = {
    .name = "calculator",
    .type = PAGE_TYPE_OVERLAY,
    .create = create,
    .destroy = destroy,
    .update = NULL,
};
