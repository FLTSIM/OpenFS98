/* SIMSUI's small members: accessors, the plain control classes (CSimsSpinButtonCtrl,
 * CSimsScrollBar and CSimsStatic, each a bare MFC control with SIMSUI's vtable), CNotify,
 * and the module's static start-up and atexit. */
#include <stdarg.h>
#include "simsui_internal.h"

#define VTABLE_SPIN 0x15048
#define VTABLE_SCROLL 0x15108
#define VTABLE_STATIC 0x151c8
#define VTABLE_NOTIFY 0x16660
#define CONTROL_SIZE 0x40
#define NOTIFY_SIZE 0x29c
#define FONTS IMAGE(0x21390)
#define ATEXIT_BEGIN 0x213f4
#define ATEXIT_END 0x213f0

/* 0x204712c0: the classes' operator new (CDIBPal's, CSimsTextArt's, ...) */
void *SC fs98_simsui_class_new(uint32_t size)
{
    return fs98_simsui_ui_new(size);
}
/* 0x20471310: their operator delete */
void SC fs98_simsui_class_free(void *block)
{
    fs98_simsui_ui_delete(block);
}
/* 0x204712d0: CNotify's default constructor closure */
void *TC fs98_simsui_notify_default(void *notify)
{
    return fs98_simsui_notify_construct(notify, 0x30, 1);
}
/* 0x204712e0, CSimsPicture::GetChar; 0x204712f0, GetRepCnt; 0x20471300, GetFlags */
uint32_t TC fs98_simsui_picture_char(void *picture) { return D32(picture, 0xc4); }
uint32_t TC fs98_simsui_picture_repeat(void *picture) { return D32(picture, 0xc8); }
uint32_t TC fs98_simsui_picture_flags(void *picture) { return D32(picture, 0xcc); }
/* 0x20471320, CSimsRadioButton::IsChecked: the button's user data is its own handle */
int32_t TC fs98_simsui_radio_is_checked(void *button)
{
    int32_t data = CALL_STD(GetWindowLongA, int32_t, (uint32_t, int32_t))(D32(button, 0x20), -21);
    return D32(button, 0x20) == (uint32_t)data;
}
/* 0x20471340, GetHeight; 0x20471350, GetWidth (CSimsButton's and the radio button's) */
int32_t TC fs98_simsui_button_height(void *button) { return I32(button, 0xb4); }
int32_t TC fs98_simsui_button_width(void *button) { return I32(button, 0xb8); }
/* 0x20471360, CSimsCheckButton::IsChecked; 0x20471370, ToggleChecked */
int32_t TC fs98_simsui_check_is_checked(void *button) { return I32(button, 0xbc); }
void TC fs98_simsui_check_toggle(void *button) { D32(button, 0xbc) = !D32(button, 0xbc); }
/* 0x204713b0: an empty member taking one argument */
void TC fs98_simsui_nothing_with(void *object, uint32_t argument) { (void)object; (void)argument; }
/* 0x204713c0, 0x204713d0: the window disabled, enabled */
void TC fs98_simsui_window_disable(void *window)
{
    CALL_STD(EnableWindow, int32_t, (uint32_t, int32_t))(D32(window, 0x20), 0);
}
void TC fs98_simsui_window_enable(void *window)
{
    CALL_STD(EnableWindow, int32_t, (uint32_t, int32_t))(D32(window, 0x20), 1);
}
/* 0x20471390, 0x204713e0, 0x204713f0: CSimsSpinButtonCtrl */
void *TC fs98_simsui_spin_construct(void *control)
{
    mfc_wnd_construct(control);
    PTR(control, 0) = IMAGE(VTABLE_SPIN);
    return control;
}
void TC fs98_simsui_spin_destroy(void *control)
{
    PTR(control, 0) = IMAGE(VTABLE_SPIN);
    CALL_THIS(MFC_793, void, (void *))(control);
}
void *TC fs98_simsui_spin_delete(void *control, uint32_t flags)
{
    return fs98_simsui_delete_object(control, flags, CONTROL_SIZE, fs98_simsui_spin_destroy, fs98_simsui_mfc_free);
}
/* 0x20471440, 0x20471460, 0x20471470: CSimsScrollBar */
void *TC fs98_simsui_scroll_construct(void *control)
{
    mfc_wnd_construct(control);
    PTR(control, 0) = IMAGE(VTABLE_SCROLL);
    return control;
}
/* 0x2047afd0, CSimsScrollBar::GetMessageMap */
const void *TC fs98_simsui_scroll_message_map(const void *control) { (void)control; return IMAGE(0x16640); }
void TC fs98_simsui_scroll_destroy(void *control)
{
    PTR(control, 0) = IMAGE(VTABLE_SCROLL);
    CALL_THIS(MFC_783, void, (void *))(control);
}
void *TC fs98_simsui_scroll_delete(void *control, uint32_t flags)
{
    return fs98_simsui_delete_object(control, flags, CONTROL_SIZE, fs98_simsui_scroll_destroy, fs98_simsui_mfc_free);
}
/* 0x204714c0, 0x204714e0, 0x20471530: CSimsStatic */
void *TC fs98_simsui_static_construct(void *control)
{
    mfc_wnd_construct(control);
    PTR(control, 0) = IMAGE(VTABLE_STATIC);
    return control;
}
void TC fs98_simsui_static_destroy(void *control)
{
    PTR(control, 0) = IMAGE(VTABLE_STATIC);
    CALL_THIS(MFC_795, void, (void *))(control);
}
void *TC fs98_simsui_static_delete(void *control, uint32_t flags)
{
    return fs98_simsui_delete_object(control, flags, CONTROL_SIZE, fs98_simsui_static_destroy, fs98_simsui_mfc_free);
}
/* 0x204714f0, Init (CSimsStatic's and CSimsScrollBar's): moved to its remapped rectangle */
void TC fs98_simsui_control_init(void *control)
{
    int32_t rect[4];
    fs98_simsui_remap_rect(rect, control);
    CALL_THIS(MFC_4299, int32_t, (void *, int32_t, int32_t, int32_t, int32_t, int32_t))(
        control, rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1], 1);
}
/* 0x20471580, CSimsEdit::SetCharLimit */
void TC fs98_simsui_edit_set_limit(void *edit, int32_t limit) { I32(edit, 0x64) = limit; }
/* 0x20471590, CSimsDialog::SetTabCtrl; 0x204715a0, GetTabCtrl */
void TC fs98_simsui_dialog_set_tabs(void *dialog, void *tabs) { PTR(dialog, 0x7c) = tabs; }
void *TC fs98_simsui_dialog_tabs(void *dialog) { return PTR(dialog, 0x7c); }
/* 0x204715b0, SafeSetFocus(HWND): the focus moved later, by a posted 0x5fa */
void TC fs98_simsui_safe_set_focus(void *window, uint32_t focus)
{
    CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(window, 0x20), 0x5fa, focus, 0);
}
/* 0x204715d0, CSimsDialog::GetLastEditFieldNotValidated: the focused edit's id, or the
 * last one kept */
int32_t TC fs98_simsui_dialog_last_edit(void *dialog)
{
    void *focus = CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(GetFocus, uint32_t, (void))());
    if (focus && CALL_THIS(MFC_4083, int32_t, (void *, void *))(focus, fs98_simsui_imports[SIMSUI_IMPORT_MFC_1829]))
        return CALL_THIS(MFC_3089, int32_t, (void *))(focus);
    return I32(dialog, 0x7d0);
}
/* 0x20471610, ValidateData (and DisplayError): nothing to say */
int32_t TC fs98_simsui_no_error(void *dialog, int32_t code) { (void)dialog; (void)code; return 0; }
/* 0x20471620, GetPal; 0x20471630, GetBkgrndDib; 0x20471640, GetTabDib (CSimsDialog's) */
void *TC fs98_simsui_dialog_pal(void *dialog) { return AT(dialog, 0x7e8); }
void *TC fs98_simsui_dialog_background(void *dialog) { return AT(dialog, 0xff4); }
void *TC fs98_simsui_dialog_tab_dib(void *dialog) { return AT(dialog, 0x1064); }
/* 0x20471650, 0x20471660: CSimsTabPage's, which are its dialog's */
void *TC fs98_simsui_page_pal(void *page) { return PTR(page, 0x10dc); }
void *TC fs98_simsui_page_background(void *page) { return PTR(page, 0x10fc); }

/* 0x2047afe0, CNotify(mask, beep): mask's sound bits (0xf0) are MessageBeep's type */
void *TC fs98_simsui_notify_construct(void *notify, uint32_t mask, uint32_t beep)
{
    mfc_wnd_construct(notify);
    mask |= 1;
    PTR(notify, 0) = IMAGE(VTABLE_NOTIFY);
    D32(notify, 0x40) = mask;
    D32(notify, 0x4c) = mask & 0xf0;
    D32(notify, 0x48) = beep;
    return notify;
}
/* 0x2047b100, Set(mask, beep) */
void TC fs98_simsui_notify_set(void *notify, uint32_t mask, uint32_t beep)
{
    D32(notify, 0x40) = mask;
    D32(notify, 0x48) = beep;
    D32(notify, 0x4c) = mask & 0xf0;
}
/* 0x2047b120, GetMask */
uint32_t TC fs98_simsui_notify_mask(void *notify) { return D32(notify, 0x40); }
/* 0x2047b130 */
void TC fs98_simsui_notify_destroy(void *notify)
{
    PTR(notify, 0) = IMAGE(VTABLE_NOTIFY);
    CALL_THIS(MFC_818, void, (void *))(notify);
}
/* 0x2047b010 */
void *TC fs98_simsui_notify_delete(void *notify, uint32_t flags)
{
    return fs98_simsui_delete_object(notify, flags, NOTIFY_SIZE, fs98_simsui_notify_destroy, fs98_simsui_ui_delete);
}
/* 0x2047b070, Send(title, format, ...) (a cdecl member): the message formatted into +0x54
 * (the format kept at +0x1e4; the title is not used), the beep, and HOST's message box
 * (service 0x10c, 2): an error's (1) when the mask has 0x30, else a notice's (4). Whether
 * the answer was 2. */
int32_t fs98_simsui_notify_send(void *notify, const char *title, const char *format, ...)
{
    struct { uint32_t zero, also_zero, six; volatile uint32_t answer; uint32_t type; char *text; } box;
    va_list arguments;
    (void)title;
    CALL_STD(lstrcpyA, char *, (char *, const char *))(AT(notify, 0x1e4), format);
    va_start(arguments, format);
    CALL_C(vsprintf, int, (char *, const char *, va_list))(AT(notify, 0x54), AT(notify, 0x1e4), arguments);
    va_end(arguments);
    if (D32(notify, 0x48)) CALL_STD(MessageBeep, int32_t, (uint32_t))(D32(notify, 0x4c));
    box.zero = 0;
    box.also_zero = 0;
    box.six = 6;
    box.type = (D8(notify, 0x40) & 0x30) ? 1 : 4;
    box.text = AT(notify, 0x54);
    ((void (SC *)(uint32_t, void *))PTR(G_PTR(0x2002c), 0x10c))(2, &box);
    return box.answer == 2;
}
/* 0x204716b0: the font array's static constructor, and its destructor registered */
void fs98_simsui_fonts_static(void)
{
    CALL_THIS(MFC_414, void *, (void *))(FONTS);
    fs98_simsui_atexit(fs98_simsui_fonts_static_end);
}
/* 0x204716e0 */
void fs98_simsui_fonts_static_end(void)
{
    CALL_THIS(MFC_713, void, (void *))(FONTS);
}
/* 0x204832c0, _onexit for a DLL */
void *fs98_simsui_onexit(void (*handler)(void))
{
    if (I32(fs98_simsui_image, ATEXIT_BEGIN) == -1) return CALL_C(_onexit, void *, (void (*)(void)))(handler);
    return CALL_C(__dllonexit, void *, (void (*)(void), void *, void *))(handler, IMAGE(ATEXIT_BEGIN), IMAGE(ATEXIT_END));
}
/* 0x204832f0, atexit */
int32_t fs98_simsui_atexit(void (*handler)(void))
{
    return fs98_simsui_onexit(handler) ? 0 : -1;
}
