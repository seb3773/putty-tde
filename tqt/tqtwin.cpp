#include "tqtwin.h"
#include "tqtdlg.h"
#include "tqtask.h"
#include "putty_icons.h"

#include <tqpainter.h>
#include <tqapplication.h>
#include <tqclipboard.h>
#include <tqmessagebox.h>
#include <tqlayout.h>
#include <tqstyle.h>
#include <tqtextview.h>
#include <tqdragobject.h>
#include <tqcursor.h>
#include <tqstatusbar.h>
#include <tqlabel.h>
#include <tqpushbutton.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>

static Mouse_Button translate_button(Mouse_Button button, Conf *conf)
{
    int mouse_mode = conf ? conf_get_int(conf, CONF_mouse_is_xterm) : 1;
    if (button == MBT_LEFT)
        return MBT_SELECT;
    if (button == MBT_MIDDLE)
        return (mouse_mode == 0 || mouse_mode == 2) ? MBT_EXTEND : MBT_PASTE;
    if (button == MBT_RIGHT)
        return (mouse_mode == 0 || mouse_mode == 2) ? MBT_PASTE : MBT_EXTEND;
    return MBT_NOTHING;
}



// --------------------------------------------------------------------------
// Forward VTable declarations
// --------------------------------------------------------------------------
static bool tqt_setup_draw_ctx(TermWin *tw);
static void tqt_free_draw_ctx(TermWin *tw);
static void tqt_draw_text(TermWin *tw, int x, int y, wchar_t *text, int len,
                          unsigned long attr, int lattr, truecolour tc);
static void tqt_draw_cursor(TermWin *tw, int x, int y, wchar_t *text, int len,
                            unsigned long attr, int lattr, truecolour tc);
static void tqt_draw_trust_sigil(TermWin *tw, int x, int y);
static int tqt_char_width(TermWin *tw, int uc);
static void tqt_set_cursor_pos(TermWin *tw, int x, int y);
static void tqt_set_raw_mouse_mode(TermWin *tw, bool enable);
static void tqt_set_scrollbar(TermWin *tw, int total, int start, int page);
static void tqt_bell(TermWin *tw, int mode);
static void tqt_clip_write(TermWin *tw, int clipboard, wchar_t *text, int *attrs,
                           truecolour *colours, int len, bool must_deselect);
static void tqt_clip_request_paste(TermWin *tw, int clipboard);
static void tqt_refresh(TermWin *tw);
static void tqt_request_resize(TermWin *tw, int w, int h);
static void tqt_set_title(TermWin *tw, const char *title);
static void tqt_set_icon_title(TermWin *tw, const char *icontitle);
static void tqt_set_minimised(TermWin *tw, bool minimised);
static bool tqt_is_minimised(TermWin *tw);
static void tqt_set_maximised(TermWin *tw, bool maximised);
static void tqt_move(TermWin *tw, int x, int y);
static void tqt_set_zorder(TermWin *tw, bool top);
static bool tqt_palette_get(TermWin *tw, int n, int *r, int *g, int *b);
static void tqt_palette_set(TermWin *tw, int n, int r, int g, int b);
static void tqt_palette_reset(TermWin *tw);
static void tqt_get_pos(TermWin *tw, int *x, int *y);
static void tqt_get_pixels(TermWin *tw, int *x, int *y);
static const char *tqt_get_title(TermWin *tw, bool icon);
static bool tqt_tw_is_utf8(TermWin *tw);

static const TermWinVtable tqt_termwin_vt = {
    tqt_setup_draw_ctx,
    tqt_draw_text,
    tqt_draw_cursor,
    tqt_draw_trust_sigil,
    tqt_char_width,
    tqt_free_draw_ctx,
    tqt_set_cursor_pos,
    tqt_set_raw_mouse_mode,
    tqt_set_scrollbar,
    tqt_bell,
    tqt_clip_write,
    tqt_clip_request_paste,
    tqt_refresh,
    tqt_request_resize,
    tqt_set_title,
    tqt_set_icon_title,
    tqt_set_minimised,
    tqt_is_minimised,
    tqt_set_maximised,
    tqt_move,
    tqt_set_zorder,
    tqt_palette_get,
    tqt_palette_set,
    tqt_palette_reset,
    tqt_get_pos,
    tqt_get_pixels,
    tqt_get_title,
    tqt_tw_is_utf8,
};

static size_t tqt_seat_output(Seat *seat, bool is_stderr, const void *data, size_t len);
static bool tqt_seat_eof(Seat *seat);
static void tqt_seat_notify_remote_exit(Seat *seat);
static void tqt_seat_connection_fatal(Seat *seat, const char *message);
static void tqt_seat_update_specials_menu(Seat *seat);
static char *tqt_seat_get_ttymode(Seat *seat, const char *mode);
static void tqt_seat_set_busy_status(Seat *seat, BusyStatus status);
static bool tqt_seat_is_utf8(Seat *seat);
static void tqt_seat_echoedit_update(Seat *seat, bool echo, bool edit);
static const char *tqt_seat_get_x_display(Seat *seat);
static bool tqt_seat_get_windowid(Seat *seat, long *id_out);
static bool tqt_seat_get_window_pixel_size(Seat *seat, int *width, int *height);
static StripCtrlChars *tqt_seat_stripctrl_new(Seat *seat, BinarySink *bs_out, SeatInteractionContext sic);
static bool tqt_seat_set_trust_status(Seat *seat, bool trusted);
static bool tqt_seat_verbose(Seat *seat);
static bool tqt_seat_interactive(Seat *seat);
static bool tqt_seat_get_cursor_position(Seat *seat, int *x, int *y);

static const SeatVtable tqt_seat_vt = {
    tqt_seat_output,
    tqt_seat_eof,
    tqt_seat_get_userpass_input,
    tqt_seat_notify_remote_exit,
    tqt_seat_connection_fatal,
    tqt_seat_update_specials_menu,
    tqt_seat_get_ttymode,
    tqt_seat_set_busy_status,
    tqt_seat_verify_ssh_host_key,
    tqt_seat_confirm_weak_crypto_primitive,
    tqt_seat_confirm_weak_cached_hostkey,
    tqt_seat_is_utf8,
    tqt_seat_echoedit_update,
    tqt_seat_get_x_display,
    tqt_seat_get_windowid,
    tqt_seat_get_window_pixel_size,
    tqt_seat_stripctrl_new,
    tqt_seat_set_trust_status,
    tqt_seat_verbose,
    tqt_seat_interactive,
    tqt_seat_get_cursor_position,
};

static void tqt_log_event(LogPolicy *lp, const char *event)
{
    PuTTYTermWidget *w = ((TQtLogPolicy *)lp)->inst;
    if (w && event) {
        w->addEventLog(event);
    }
}
static int tqt_log_askappend(LogPolicy *lp, Filename *fn, void (*cb)(void*, int), void *ctx)
{
    (void)lp; (void)fn;
    if (cb) cb(ctx, 2); // 2 = overwrite
    return 1;
}
static void tqt_log_error(LogPolicy *lp, const char *event)
{
    PuTTYTermWidget *w = ((TQtLogPolicy *)lp)->inst;
    if (w && event) {
        w->addEventLog(event);
    }
}

static const LogPolicyVtable tqt_logpolicy_vt = {
    tqt_log_event,
    tqt_log_askappend,
    tqt_log_error,
    null_lp_verbose_yes,
};

// --------------------------------------------------------------------------
// PuTTYTermWidget Implementation
// --------------------------------------------------------------------------

PuTTYTermWidget::PuTTYTermWidget(TQWidget *parent, Conf *conf)
    : TQWidget(parent, "PuTTYTermWidget", TQt::WNoAutoErase),
      m_conf(conf),
      m_term(0),
      m_backend(0),
      m_ldisc(0),
      m_logctx(0),
      m_windowBorder(4),
      m_backbuffer(0),
      m_rawMouse(false),
      m_drawing(false),
      m_connected(false),
      m_cursorHidden(false)
{
    setFocusPolicy(TQWidget::StrongFocus);
    setMouseTracking(true);
    setAcceptDrops(true);

    m_lastClickAction = MA_NOTHING;
    m_lastClickButton = MBT_NOTHING;

    m_tqt_termwin.tw.vt = &tqt_termwin_vt;
    m_tqt_termwin.inst = this;

    m_tqt_seat.seat.vt = &tqt_seat_vt;
    m_tqt_seat.inst = this;

    m_tqt_logpolicy.lp.vt = &tqt_logpolicy_vt;
    m_tqt_logpolicy.inst = this;

    m_termCols = conf_get_int(m_conf, CONF_width);
    m_termRows = conf_get_int(m_conf, CONF_height);
    if (m_termCols <= 0) m_termCols = 80;
    if (m_termRows <= 0) m_termRows = 24;

    initFont();
    initPalette();

    init_ucs(&m_ucsdata,
             conf_get_str(m_conf, CONF_line_codepage),
             conf_get_bool(m_conf, CONF_utf8_override),
             CS_NONE,
             conf_get_int(m_conf, CONF_vtmode));

    m_term = term_init(m_conf, &m_ucsdata, &m_tqt_termwin.tw);
    term_size(m_term, m_termRows, m_termCols, conf_get_int(m_conf, CONF_savelines));

    assert(m_term->mouse_select_clipboards[0] == CLIP_LOCAL);
    m_term->n_mouse_select_clipboards = 1;
    m_term->mouse_select_clipboards[m_term->n_mouse_select_clipboards++] = CLIP_PRIMARY;
    if (conf_get_bool(m_conf, CONF_mouseautocopy)) {
        m_term->mouse_select_clipboards[m_term->n_mouse_select_clipboards++] = CLIP_CLIPBOARD;
    }
    switch (conf_get_int(m_conf, CONF_mousepaste)) {
      case CLIPUI_EXPLICIT:
        m_term->mouse_paste_clipboard = CLIP_CLIPBOARD;
        break;
      default:
        m_term->mouse_paste_clipboard = CLIP_PRIMARY;
        break;
    }

    int px = m_termCols * m_fontWidth + 2 * m_windowBorder;
    int py = m_termRows * m_fontHeight + 2 * m_windowBorder;
    setMinimumSize(px, py);
    resize(px, py);

    ensureBackbuffer(px, py);
}

PuTTYTermWidget::~PuTTYTermWidget()
{
    if (m_term) {
        term_free(m_term);
        m_term = 0;
    }
    delete m_backbuffer;
}

void PuTTYTermWidget::setUsername(const TQString &user)
{
    if (m_conf && !user.isEmpty()) {
        conf_set_str(m_conf, CONF_username, user.utf8());
    }
    emit usernameChanged(user);
}

void PuTTYTermWidget::notifyConnectionEstablished()
{
    if (!m_connected) {
        m_connected = true;
        emit connectionEstablished();
    }
}

void PuTTYTermWidget::resetConnection()
{
    m_connected = false;
}

void PuTTYTermWidget::initFont()
{
    FontSpec *fs = m_conf ? conf_get_fontspec(m_conf, CONF_font) : NULL;
    if (fs && fs->name && fs->name[0] && strcmp(fs->name, "server:fixed") != 0) {
        TQString desc = TQString::fromUtf8(fs->name);
        int lastSpace = desc.findRev(' ');
        if (lastSpace != -1) {
            TQString family = desc.left(lastSpace);
            int size = desc.mid(lastSpace + 1).toInt();
            if (size <= 0) size = 10;
            m_font = TQFont(family, size);
        } else {
            m_font = TQFont(desc, 10);
        }
    } else {
        m_font = TQFont("Monospace", 10);
    }
    m_font.setStyleHint(TQFont::TypeWriter);
    m_font.setFixedPitch(true);

    m_fontBold = m_font;
    m_fontBold.setBold(true);

    TQFontMetrics fm(m_font);
    m_fontWidth = fm.width('M');
    if (m_fontWidth <= 0) m_fontWidth = 8;
    m_fontHeight = fm.lineSpacing();
    if (m_fontHeight <= 0) m_fontHeight = 16;
    m_fontAscent = fm.ascent();
}

void PuTTYTermWidget::reconfigureFromConf()
{
    initFont();
    initPalette();
    if (m_term) {
        m_term->n_mouse_select_clipboards = 1;
        m_term->mouse_select_clipboards[m_term->n_mouse_select_clipboards++] = CLIP_PRIMARY;
        if (conf_get_bool(m_conf, CONF_mouseautocopy)) {
            m_term->mouse_select_clipboards[m_term->n_mouse_select_clipboards++] = CLIP_CLIPBOARD;
        }
        switch (conf_get_int(m_conf, CONF_mousepaste)) {
          case CLIPUI_EXPLICIT:
            m_term->mouse_paste_clipboard = CLIP_CLIPBOARD;
            break;
          default:
            m_term->mouse_paste_clipboard = CLIP_PRIMARY;
            break;
        }
    }
    refreshWindow();
}

void PuTTYTermWidget::addEventLog(const char *event)
{
    time_t now = time(NULL);
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S\t", localtime(&now));
    m_eventLog.append(TQString(timebuf) + TQString::fromUtf8(event));
}

void PuTTYTermWidget::initPalette()
{
    // Standard 16 ANSI colors
    static const unsigned int ansi_defaults[16] = {
        0x000000, 0xBB0000, 0x00BB00, 0xBBBB00,
        0x0000BB, 0xBB00BB, 0x00BBBB, 0xBBBBBB,
        0x555555, 0xFF5555, 0x55FF55, 0xFFFF55,
        0x5555FF, 0xFF55FF, 0x55FFFF, 0xFFFFFF
    };

    for (int i = 0; i < 16; ++i) {
        m_palette[i] = TQColor((ansi_defaults[i] >> 16) & 0xFF,
                              (ansi_defaults[i] >> 8) & 0xFF,
                              ansi_defaults[i] & 0xFF);
    }

    // 216 colour cube
    for (int r = 0; r < 6; r++) {
        for (int g = 0; g < 6; g++) {
            for (int b = 0; b < 6; b++) {
                int idx = 16 + (r * 36) + (g * 6) + b;
                m_palette[idx] = TQColor(r ? (r * 40 + 55) : 0,
                                         g ? (g * 40 + 55) : 0,
                                         b ? (b * 40 + 55) : 0);
            }
        }
    }

    // 24 greyscales
    for (int i = 0; i < 24; i++) {
        int val = i * 10 + 8;
        m_palette[232 + i] = TQColor(val, val, val);
    }

    // Special colors: 256=def_fg, 257=def_bold_fg, 258=def_bg, 259=def_bold_bg, 260=curs_text, 261=curs_bg
    m_palette[256] = TQColor(187, 187, 187); // Default FG
    m_palette[257] = TQColor(255, 255, 255); // Default Bold FG
    m_palette[258] = TQColor(0, 0, 0);       // Default BG
    m_palette[259] = TQColor(85, 85, 85);    // Default Bold BG
    m_palette[260] = TQColor(0, 0, 0);       // Cursor Text
    m_palette[261] = TQColor(0, 255, 0);     // Cursor BG

    // Load custom session colors from conf if available
    if (m_conf) {
        static const int ww[] = {
            256, 257, 258, 259, 260, 261,
            0, 8, 1, 9, 2, 10, 3, 11,
            4, 12, 5, 13, 6, 14, 7, 15
        };
        for (int i = 0; i < 22; i++) {
            int r = conf_get_int_int(m_conf, CONF_colours, i * 3 + 0);
            int g = conf_get_int_int(m_conf, CONF_colours, i * 3 + 1);
            int b = conf_get_int_int(m_conf, CONF_colours, i * 3 + 2);
            m_palette[ww[i]] = TQColor(r, g, b);
        }
    }
}

void PuTTYTermWidget::ensureBackbuffer(int w, int h)
{
    if (!m_backbuffer || m_backbuffer->width() < w || m_backbuffer->height() < h) {
        delete m_backbuffer;
        m_backbuffer = new TQPixmap(w > 0 ? w : 1, h > 0 ? h : 1);
        m_backbuffer->fill(m_palette[258]);
    }
}

bool PuTTYTermWidget::setupDrawCtx()
{
    m_drawing = true;
    return true;
}

void PuTTYTermWidget::freeDrawCtx()
{
    m_drawing = false;
}

void PuTTYTermWidget::drawText(int x, int y, wchar_t *text, int len,
                              unsigned long attr, int lattr, truecolour tc)
{
    doDrawText(x, y, text, len, attr, lattr, tc);

    int widefactor = (attr & ATTR_WIDE) ? 2 : 1;
    int px = x * m_fontWidth + m_windowBorder;
    int py = y * m_fontHeight + m_windowBorder;
    int pw = len * m_fontWidth * widefactor;
    int ph = m_fontHeight;

    update(px, py, pw, ph);
}

void PuTTYTermWidget::doDrawText(int x, int y, wchar_t *text, int len,
                                unsigned long attr, int lattr, truecolour tc)
{
    (void)lattr;
    if (!m_backbuffer || len <= 0) return;

    int widefactor = (attr & ATTR_WIDE) ? 2 : 1;
    int px = x * m_fontWidth + m_windowBorder;
    int py = y * m_fontHeight + m_windowBorder;
    int pw = len * m_fontWidth * widefactor;
    int ph = m_fontHeight;

    int nfg = ((attr & ATTR_FGMASK) >> ATTR_FGSHIFT);
    int nbg = ((attr & ATTR_BGMASK) >> ATTR_BGSHIFT);

    if (attr & ATTR_REVERSE) {
        int t = nfg; nfg = nbg; nbg = t;
        struct optionalrgb trgb = tc.fg;
        tc.fg = tc.bg;
        tc.bg = trgb;
    }

    int cursor_type = m_conf ? conf_get_int(m_conf, CONF_cursor_type) : 0;
    if ((attr & TATTR_ACTCURS) && cursor_type == 0) {
        tc.fg.enabled = false;
        tc.bg.enabled = false;
        nfg = 260; // Cursor text colour
        nbg = 261; // Cursor background colour
        attr &= ~ATTR_DIM;
    }

    TQColor fgColor = (nfg >= 0 && nfg < 262) ? m_palette[nfg] : m_palette[256];
    TQColor bgColor = (nbg >= 0 && nbg < 262) ? m_palette[nbg] : m_palette[258];

    if (tc.fg.enabled)
        fgColor = TQColor(tc.fg.r, tc.fg.g, tc.fg.b);
    if (tc.bg.enabled)
        bgColor = TQColor(tc.bg.r, tc.bg.g, tc.bg.b);

    TQPainter p(m_backbuffer);
    p.fillRect(px, py, pw, ph, bgColor);

    TQFont currentFont = (attr & ATTR_BOLD) ? m_fontBold : m_font;
    if (attr & ATTR_UNDER)
        currentFont.setUnderline(true);

    p.setFont(currentFont);
    p.setPen(fgColor);

    TQString str;
    str.setLength(len);
    for (int i = 0; i < len; ++i) {
        str[i] = (TQChar)(ushort)text[i];
    }

    p.drawText(px, py + m_fontAscent, str);
}

void PuTTYTermWidget::drawCursor(int x, int y, wchar_t *text, int len,
                                unsigned long attr, int lattr, truecolour tc)
{
    if (!m_backbuffer || len <= 0) return;

    int widefactor = (attr & ATTR_WIDE) ? 2 : 1;
    int px = x * m_fontWidth + m_windowBorder;
    int py = y * m_fontHeight + m_windowBorder;
    int pw = len * m_fontWidth * widefactor;
    int ph = m_fontHeight;

    int cursor_type = m_conf ? conf_get_int(m_conf, CONF_cursor_type) : 0;
    bool passive = (attr & TATTR_PASCURS) != 0;

    if (cursor_type == 0) {
        // Block cursor
        if (passive) {
            unsigned long norm_attr = attr & ~TATTR_ACTCURS;
            doDrawText(x, y, text, len, norm_attr, lattr, tc);

            TQPainter p(m_backbuffer);
            p.setPen(m_palette[261]);
            p.setBrush(TQt::NoBrush);
            p.drawRect(px, py, pw, ph);
        } else {
            unsigned long act_attr = attr | TATTR_ACTCURS;
            doDrawText(x, y, text, len, act_attr, lattr, tc);
        }
    } else {
        unsigned long norm_attr = attr & ~(TATTR_ACTCURS | TATTR_PASCURS);
        doDrawText(x, y, text, len, norm_attr, lattr, tc);

        TQPainter p(m_backbuffer);
        TQPen pen = passive ? TQPen(m_palette[261], 1, TQt::DotLine)
                            : TQPen(m_palette[261], 1, TQt::SolidLine);
        p.setPen(pen);

        if (cursor_type == 1) {
            // Underline cursor
            int uheight = m_fontAscent + 1;
            if (uheight >= m_fontHeight) uheight = m_fontHeight - 1;
            p.drawLine(px, py + uheight, px + pw - 1, py + uheight);
        } else if (cursor_type == 2) {
            // Vertical bar cursor
            int xadjust = (attr & TATTR_RIGHTCURS) ? (pw - 1) : 0;
            p.drawLine(px + xadjust, py, px + xadjust, py + ph - 1);
        }
    }

    update(px, py, pw, ph);
}

void PuTTYTermWidget::drawTrustSigil(int x, int y)
{
    (void)x; (void)y;
}

int PuTTYTermWidget::charWidth(int uc)
{
    (void)uc;
    return 1;
}

void PuTTYTermWidget::setCursorPos(int x, int y)
{
    (void)x; (void)y;
}

void PuTTYTermWidget::setRawMouseMode(bool enable)
{
    m_rawMouse = enable;
}

void PuTTYTermWidget::setScrollbar(int total, int start, int page)
{
    emit scrollbarUpdated(total, start, page);
}

void PuTTYTermWidget::bell(int mode)
{
    if (mode == BELL_DEFAULT || mode == BELL_PCSPEAKER) {
        TQApplication::beep();
    }
}

void PuTTYTermWidget::clipWrite(int clipboard, wchar_t *text, int *attrs,
                               truecolour *colours, int len, bool must_deselect)
{
    (void)attrs; (void)colours; (void)must_deselect;
    TQString str;
    str.setLength(len);
    for (int i = 0; i < len; ++i)
        str[i] = (TQChar)(ushort)text[i];

    TQClipboard *cb = TQApplication::clipboard();
    if (clipboard == CLIP_CLIPBOARD)
        cb->setText(str, TQClipboard::Clipboard);
    else
        cb->setText(str, TQClipboard::Selection);
}

void PuTTYTermWidget::clipRequestPaste(int clipboard)
{
    TQClipboard *cb = TQApplication::clipboard();
    TQString text = cb->text(clipboard == CLIP_CLIPBOARD ? TQClipboard::Clipboard : TQClipboard::Selection);

    if (!text.isEmpty() && m_term) {
        int len = text.length();
        wchar_t *wc = new wchar_t[len];
        for (int i = 0; i < len; ++i)
            wc[i] = text[i].unicode();
        term_do_paste(m_term, wc, len);
        delete[] wc;
    }
}

void PuTTYTermWidget::changeFontSize(int increment)
{
    int curSize = m_font.pointSize();
    if (curSize <= 0) curSize = 10;
    int newSize = curSize + increment;
    if (newSize < 6) newSize = 6;
    if (newSize > 48) newSize = 48;
    if (newSize == curSize) return;

    m_font.setPointSize(newSize);
    m_fontBold = m_font;
    m_fontBold.setBold(true);

    TQFontMetrics fm(m_font);
    m_fontWidth = fm.width('M');
    if (m_fontWidth <= 0) m_fontWidth = 8;
    m_fontHeight = fm.lineSpacing();
    if (m_fontHeight <= 0) m_fontHeight = 16;
    m_fontAscent = fm.ascent();

    int newCols = (width() - 2 * m_windowBorder) / m_fontWidth;
    int newRows = (height() - 2 * m_windowBorder) / m_fontHeight;
    if (newCols > 0 && newRows > 0) {
        m_termCols = newCols;
        m_termRows = newRows;
        if (m_term) {
            term_size(m_term, m_termRows, m_termCols, conf_get_int(m_conf, CONF_savelines));
            term_invalidate(m_term);
            term_update(m_term);
        }
    }
    emit fontSizeChanged();
    refreshWindow();
}

void PuTTYTermWidget::dragEnterEvent(TQDragEnterEvent *e)
{
    if (TQTextDrag::canDecode(e) || TQUriDrag::canDecode(e)) {
        e->accept();
    } else {
        e->ignore();
    }
}

void PuTTYTermWidget::dropEvent(TQDropEvent *e)
{
    TQString text;
    if (TQUriDrag::canDecode(e)) {
        TQStringList uriList;
        if (TQUriDrag::decodeLocalFiles(e, uriList)) {
            text = uriList.join(" ");
        }
    }
    if (text.isEmpty() && TQTextDrag::canDecode(e)) {
        TQTextDrag::decode(e, text);
    }
    if (!text.isEmpty() && m_term) {
        int len = text.length();
        wchar_t *wc = new wchar_t[len];
        for (int i = 0; i < len; ++i)
            wc[i] = text[i].unicode();
        term_do_paste(m_term, wc, len);
        delete[] wc;
    }
}

void PuTTYTermWidget::refreshWindow()
{
    if (m_term) {
        term_invalidate(m_term);
        term_update(m_term);
    }
    update();
}

void PuTTYTermWidget::requestResize(int w, int h)
{
    m_termCols = w;
    m_termRows = h;
    if (m_term) {
        term_size(m_term, m_termRows, m_termCols, conf_get_int(m_conf, CONF_savelines));
    }
    int px = m_termCols * m_fontWidth + 2 * m_windowBorder;
    int py = m_termRows * m_fontHeight + 2 * m_windowBorder;
    ensureBackbuffer(px, py);
    resize(px, py);
}

void PuTTYTermWidget::setWindowTitle(const char *title)
{
    emit windowTitleChanged(TQString::fromUtf8(title));
}

void PuTTYTermWidget::setIconTitle(const char *icontitle)
{
    (void)icontitle;
}

bool PuTTYTermWidget::paletteGet(int n, int *r, int *g, int *b)
{
    if (n >= 0 && n < 262) {
        *r = m_palette[n].red();
        *g = m_palette[n].green();
        *b = m_palette[n].blue();
        return true;
    }
    return false;
}

void PuTTYTermWidget::paletteSet(int n, int r, int g, int b)
{
    if (n >= 0 && n < 262) {
        m_palette[n] = TQColor(r, g, b);
    }
}

void PuTTYTermWidget::paletteReset()
{
    initPalette();
}

void PuTTYTermWidget::paintEvent(TQPaintEvent *e)
{
    if (!m_backbuffer) return;
    TQPainter p(this);
    p.drawPixmap(e->rect().topLeft(), *m_backbuffer, e->rect());
}

void PuTTYTermWidget::resizeEvent(TQResizeEvent *e)
{
    int newCols = (width() - 2 * m_windowBorder) / m_fontWidth;
    int newRows = (height() - 2 * m_windowBorder) / m_fontHeight;

    if (newCols > 0 && newRows > 0 && (newCols != m_termCols || newRows != m_termRows)) {
        m_termCols = newCols;
        m_termRows = newRows;
        ensureBackbuffer(width(), height());
        if (m_term) {
            term_size(m_term, m_termRows, m_termCols, conf_get_int(m_conf, CONF_savelines));
            if (m_backend) {
                backend_size(m_backend, m_termCols, m_termRows);
            }
        }
    }
}

void PuTTYTermWidget::keyPressEvent(TQKeyEvent *e)
{
    if (!m_term) return;

    term_seen_key_event(m_term);

    if (conf_get_bool(m_conf, CONF_hide_mouseptr) && !m_cursorHidden) {
        setCursor(TQt::blankCursor);
        m_cursorHidden = true;
    }

    bool shift = (e->state() & TQt::ShiftButton) != 0;
    bool ctrl = (e->state() & TQt::ControlButton) != 0;
    bool alt = (e->state() & TQt::AltButton) != 0;

    // Shift+Insert: paste from PRIMARY (or CLIPBOARD if configured)
    if (e->key() == TQt::Key_Insert && shift && !ctrl && !alt) {
        int clip = (conf_get_int(m_conf, CONF_ctrlshiftins) == CLIPUI_EXPLICIT)
                   ? CLIP_CLIPBOARD : CLIP_PRIMARY;
        term_request_paste(m_term, clip);
        return;
    }

    // Ctrl+Shift+Insert: paste from CLIPBOARD
    if (e->key() == TQt::Key_Insert && shift && ctrl && !alt) {
        term_request_paste(m_term, CLIP_CLIPBOARD);
        return;
    }

    // Shift+PageUp / Shift+PageDown: scroll half page
    if (e->key() == TQt::Key_Prior && shift && !ctrl && !alt) {
        term_scroll(m_term, 0, -m_termRows / 2);
        return;
    }
    if (e->key() == TQt::Key_Next && shift && !ctrl && !alt) {
        term_scroll(m_term, 0, +m_termRows / 2);
        return;
    }

    // Ctrl+Shift+PageUp / Ctrl+Shift+PageDown: scroll to top / bottom
    if (e->key() == TQt::Key_Prior && shift && ctrl && !alt) {
        term_scroll(m_term, 1, 0);
        return;
    }
    if (e->key() == TQt::Key_Next && shift && ctrl && !alt) {
        term_scroll(m_term, -1, 0);
        return;
    }

    // Ctrl+PageUp / Ctrl+PageDown: scroll 1 line up / down
    if (e->key() == TQt::Key_Prior && !shift && ctrl && !alt) {
        term_scroll(m_term, 0, -1);
        return;
    }
    if (e->key() == TQt::Key_Next && !shift && ctrl && !alt) {
        term_scroll(m_term, 0, +1);
        return;
    }

    // Alt+Enter: toggle fullscreen
    if ((e->key() == TQt::Key_Return || e->key() == TQt::Key_Enter) && alt && !ctrl && !shift) {
        if (conf_get_bool(m_conf, CONF_fullscreenonaltenter)) {
            emit toggleFullScreenRequested();
            return;
        }
    }

    // Ctrl + > / Ctrl + + / Ctrl + < / Ctrl + - : zoom in / out
    if (ctrl && !alt) {
        if (e->key() == TQt::Key_Greater || e->key() == TQt::Key_Plus || e->text() == "+" || e->text() == ">") {
            changeFontSize(+1);
            return;
        }
        if (e->key() == TQt::Key_Less || e->key() == TQt::Key_Minus || e->text() == "-" || e->text() == "<") {
            changeFontSize(-1);
            return;
        }
    }

    char buf[64];
    int len = 0;

    switch (e->key()) {
    case TQt::Key_Enter:
    case TQt::Key_Return:
        buf[0] = '\r'; len = 1; break;
    case TQt::Key_Backspace:
        buf[0] = conf_get_bool(m_conf, CONF_bksp_is_delete) ? '\x7f' : '\x08';
        len = 1; break;
    case TQt::Key_Tab:
        buf[0] = '\t'; len = 1; break;
    case TQt::Key_Escape:
        buf[0] = '\x1b'; len = 1; break;
    case TQt::Key_Up:
        len = format_arrow_key(buf, m_term, 0, ctrl); break;
    case TQt::Key_Down:
        len = format_arrow_key(buf, m_term, 1, ctrl); break;
    case TQt::Key_Right:
        len = format_arrow_key(buf, m_term, 2, ctrl); break;
    case TQt::Key_Left:
        len = format_arrow_key(buf, m_term, 3, ctrl); break;
    case TQt::Key_Home:
        len = format_small_keypad_key(buf, m_term, SKK_HOME); break;
    case TQt::Key_End:
        len = format_small_keypad_key(buf, m_term, SKK_END); break;
    case TQt::Key_Insert:
        len = format_small_keypad_key(buf, m_term, SKK_INSERT); break;
    case TQt::Key_Delete:
        len = format_small_keypad_key(buf, m_term, SKK_DELETE); break;
    case TQt::Key_Prior:
        len = format_small_keypad_key(buf, m_term, SKK_PGUP); break;
    case TQt::Key_Next:
        len = format_small_keypad_key(buf, m_term, SKK_PGDN); break;
    default:
        if (e->key() >= TQt::Key_F1 && e->key() <= TQt::Key_F12) {
            len = format_function_key(buf, m_term, e->key() - TQt::Key_F1 + 1, shift, ctrl);
        } else {
            TQString text = e->text();
            if (!text.isEmpty()) {
                if (alt) {
                    buf[0] = '\x1b';
                    int utf8len = text.utf8().length();
                    memcpy(buf + 1, text.utf8().data(), utf8len);
                    len = 1 + utf8len;
                } else if (ctrl && text[0].latin1() >= '@' && text[0].latin1() <= '_') {
                    buf[0] = text[0].latin1() & 0x1f;
                    len = 1;
                } else {
                    term_keyinput(m_term, CP_UTF8, text.utf8().data(), text.utf8().length());
                    return;
                }
            }
        }
        break;
    }

    if (len > 0) {
        term_keyinput(m_term, -1, buf, len);
    }
}

void PuTTYTermWidget::mousePressEvent(TQMouseEvent *e)
{
    if (!m_term) return;

    int col = (e->x() - m_windowBorder) / m_fontWidth;
    int row = (e->y() - m_windowBorder) / m_fontHeight;
    bool shift = (e->state() & TQt::ShiftButton) != 0;
    bool ctrl = (e->state() & TQt::ControlButton) != 0;
    bool alt = (e->state() & TQt::AltButton) != 0;

    if (e->button() == TQt::RightButton && ctrl) {
        emit showContextMenuRequested(e->globalPos());
        return;
    }

    Mouse_Button b = MBT_LEFT;
    if (e->button() == TQt::MidButton) b = MBT_MIDDLE;
    else if (e->button() == TQt::RightButton) b = MBT_RIGHT;

    int act = MA_CLICK;
    int dblInterval = TQApplication::doubleClickInterval();
    if (dblInterval <= 0) dblInterval = 400;

    if (b == m_lastClickButton && m_lastClickAction == MA_2CLK &&
        m_lastClickTime.elapsed() < dblInterval &&
        (e->pos() - m_lastClickPos).manhattanLength() < 5) {
        act = MA_3CLK;
    }

    m_lastClickButton = b;
    m_lastClickAction = act;
    m_lastClickTime.start();
    m_lastClickPos = e->pos();

    term_mouse(m_term, b, translate_button(b, m_conf), (Mouse_Action)act,
               col, row, shift, ctrl, alt);
}

void PuTTYTermWidget::mouseDoubleClickEvent(TQMouseEvent *e)
{
    if (!m_term) return;

    int col = (e->x() - m_windowBorder) / m_fontWidth;
    int row = (e->y() - m_windowBorder) / m_fontHeight;
    bool shift = (e->state() & TQt::ShiftButton) != 0;
    bool ctrl = (e->state() & TQt::ControlButton) != 0;
    bool alt = (e->state() & TQt::AltButton) != 0;

    if (e->button() == TQt::RightButton && ctrl) return;

    Mouse_Button b = MBT_LEFT;
    if (e->button() == TQt::MidButton) b = MBT_MIDDLE;
    else if (e->button() == TQt::RightButton) b = MBT_RIGHT;

    m_lastClickButton = b;
    m_lastClickAction = MA_2CLK;
    m_lastClickTime.start();
    m_lastClickPos = e->pos();

    term_mouse(m_term, b, translate_button(b, m_conf), MA_2CLK,
               col, row, shift, ctrl, alt);
}

void PuTTYTermWidget::mouseReleaseEvent(TQMouseEvent *e)
{
    if (!m_term) return;

    int col = (e->x() - m_windowBorder) / m_fontWidth;
    int row = (e->y() - m_windowBorder) / m_fontHeight;
    bool shift = (e->state() & TQt::ShiftButton) != 0;
    bool ctrl = (e->state() & TQt::ControlButton) != 0;
    bool alt = (e->state() & TQt::AltButton) != 0;

    if (e->button() == TQt::RightButton && ctrl) {
        return;
    }

    Mouse_Button b = MBT_LEFT;
    if (e->button() == TQt::MidButton) b = MBT_MIDDLE;
    else if (e->button() == TQt::RightButton) b = MBT_RIGHT;

    term_mouse(m_term, b, translate_button(b, m_conf), MA_RELEASE,
               col, row, shift, ctrl, alt);
}

void PuTTYTermWidget::mouseMoveEvent(TQMouseEvent *e)
{
    if (m_cursorHidden) {
        setCursor(TQt::ibeamCursor);
        m_cursorHidden = false;
    }

    if (!m_term) return;

    int col = (e->x() - m_windowBorder) / m_fontWidth;
    int row = (e->y() - m_windowBorder) / m_fontHeight;
    bool shift = (e->state() & TQt::ShiftButton) != 0;
    bool ctrl = (e->state() & TQt::ControlButton) != 0;
    bool alt = (e->state() & TQt::AltButton) != 0;

    Mouse_Button b = MBT_NOTHING;
    if (e->state() & TQt::LeftButton)
        b = MBT_LEFT;
    else if (e->state() & TQt::MidButton)
        b = MBT_MIDDLE;
    else if (e->state() & TQt::RightButton)
        b = MBT_RIGHT;

    if (b != MBT_NOTHING) {
        term_mouse(m_term, b, translate_button(b, m_conf), MA_DRAG,
                   col, row, shift, ctrl, alt);
    }
}

void PuTTYTermWidget::wheelEvent(TQWheelEvent *e)
{
    if (!m_term) return;
    int lines = (e->delta() < 0) ? 3 : -3;
    term_scroll(m_term, 0, lines);
}

void PuTTYTermWidget::contextMenuEvent(TQContextMenuEvent *e)
{
    if (e->reason() == TQContextMenuEvent::Keyboard) {
        emit showContextMenuRequested(e->globalPos());
    }
}

void PuTTYTermWidget::focusInEvent(TQFocusEvent *e)
{
    TQWidget::focusInEvent(e);
    if (m_term)
        term_set_focus(m_term, true);
    term_update(m_term);
}

void PuTTYTermWidget::focusOutEvent(TQFocusEvent *e)
{
    TQWidget::focusOutEvent(e);
    if (m_term)
        term_set_focus(m_term, false);
    term_update(m_term);
}

// --------------------------------------------------------------------------
// PuTTYSessionWindow Implementation
static int s_active_session_count = 0;

int get_active_session_count()
{
    return s_active_session_count;
}

PuTTYSessionWindow::PuTTYSessionWindow(Conf *conf, const char *geometry_string)
    : TQWidget(0, "PuTTYSessionWindow"),
      m_conf(conf),
      m_termWidget(0),
      m_scrollbar(0),
      m_statusBar(0),
      m_statusHost(0),
      m_statusHint(0),
      m_popup(0),
      m_sessionsMenu(0),
      m_specialsMenu(0),
      m_restartItemId(-1),
      m_specialsItemId(-1),
      m_statusBarItemId(-1),
      m_showStatusBar(true),
      m_sessionCounted(true),
      m_sessionActive(false),
      m_connState(CONN_CONNECTING)
{
    (void)geometry_string;
    prepare_session(m_conf);
    s_active_session_count++;
    setWFlags(getWFlags() | TQt::WDestructiveClose);
    setCaption("PuTTY-TDE");
    setIcon(get_putty_icon());

    TQVBoxLayout *mainLayout = new TQVBoxLayout(this, 0, 0);

    TQHBoxLayout *termLayout = new TQHBoxLayout(mainLayout, 0);

    m_termWidget = new PuTTYTermWidget(this, m_conf);
    m_scrollbar = new TQScrollBar(TQt::Vertical, this);

    bool showSb = conf_get_bool(m_conf, CONF_scrollbar);
    bool sbLeft = conf_get_bool(m_conf, CONF_scrollbar_on_left);

    if (sbLeft) {
        termLayout->addWidget(m_scrollbar, 0);
        termLayout->addWidget(m_termWidget, 1);
    } else {
        termLayout->addWidget(m_termWidget, 1);
        termLayout->addWidget(m_scrollbar, 0);
    }

    if (showSb)
        m_scrollbar->show();
    else
        m_scrollbar->hide();

    m_statusBar = new TQStatusBar(this);
    m_statusHost = new TQLabel(m_statusBar);
    m_statusHint = new TQLabel("Ctrl+Right Click for options  ", m_statusBar);
    m_statusBar->addWidget(m_statusHost, 1);
    m_statusBar->addWidget(m_statusHint, 0, true);

    mainLayout->addWidget(m_statusBar, 0);

    connect(m_termWidget, TQ_SIGNAL(scrollbarUpdated(int, int, int)),
            this, TQ_SLOT(onScrollBarUpdated(int, int, int)));
    connect(m_scrollbar, TQ_SIGNAL(valueChanged(int)),
            this, TQ_SLOT(onScrollBarValueChanged(int)));
    connect(m_termWidget, TQ_SIGNAL(windowTitleChanged(const TQString &)),
            this, TQ_SLOT(onTitleChanged(const TQString &)));
    connect(m_termWidget, TQ_SIGNAL(usernameChanged(const TQString &)),
            this, TQ_SLOT(updateStatusText()));
    connect(m_termWidget, TQ_SIGNAL(connectionEstablished()),
            this, TQ_SLOT(onConnectionEstablished()));
    connect(m_termWidget, TQ_SIGNAL(sessionClosed()),
            this, TQ_SLOT(onSessionClosed()));
    connect(m_termWidget, TQ_SIGNAL(showContextMenuRequested(const TQPoint &)),
            this, TQ_SLOT(onShowContextMenu(const TQPoint &)));
    connect(m_termWidget, TQ_SIGNAL(fontSizeChanged()),
            this, TQ_SLOT(updateSizeHints()));
    connect(m_termWidget, TQ_SIGNAL(toggleFullScreenRequested()),
            this, TQ_SLOT(toggleFullScreen()));

    // System Context Menu (matching legacy PuTTY structure)
    m_popup = new TQPopupMenu(this);
    m_popup->insertItem("&New Session...", this, TQ_SLOT(menuNewSession()));
    m_restartItemId = m_popup->insertItem("&Restart Session", this, TQ_SLOT(menuRestartSession()));
    m_popup->insertItem("&Duplicate Session", this, TQ_SLOT(menuDuplicateSession()));

    m_sessionsMenu = new TQPopupMenu(m_popup);
    connect(m_sessionsMenu, TQ_SIGNAL(activated(int)), this, TQ_SLOT(onSavedSessionActivated(int)));
    m_popup->insertItem("&Saved Sessions", m_sessionsMenu);

    m_popup->insertSeparator();
    m_popup->insertItem("&Change Settings...", this, TQ_SLOT(menuSettings()));
    m_popup->insertSeparator();

    m_statusBarItemId = m_popup->insertItem("Show Status &Bar", this, TQ_SLOT(toggleStatusBar()));
    m_popup->setItemChecked(m_statusBarItemId, true);

    m_popup->insertItem("&Event Log", this, TQ_SLOT(menuEventLog()));

    m_specialsMenu = new TQPopupMenu(m_popup);
    connect(m_specialsMenu, TQ_SIGNAL(activated(int)), this, TQ_SLOT(onSpecialCmdActivated(int)));
    m_specialsItemId = m_popup->insertItem("S&pecial Commands", m_specialsMenu);

    m_popup->insertSeparator();
    m_popup->insertItem("Clear &Scrollback", this, TQ_SLOT(menuClearScrollback()));
    m_popup->insertItem("Reset &Terminal", this, TQ_SLOT(menuResetTerminal()));

    m_popup->insertSeparator();
    m_popup->insertItem("Copy to &CLIPBOARD", this, TQ_SLOT(menuCopyClipboard()));
    m_popup->insertItem("Paste from CLIP&BOARD", this, TQ_SLOT(menuPasteClipboard()));
    m_popup->insertItem("Copy &All", this, TQ_SLOT(menuCopyAll()));

    m_popup->insertSeparator();
    m_popup->insertItem("&About PuTTY-TDE", this, TQ_SLOT(menuAbout()));

    updateStatusText();
    startSession();
    updateSizeHints();
}

PuTTYSessionWindow::~PuTTYSessionWindow()
{
    clear_cached_auth_password();
    if (m_sessionCounted) {
        m_sessionCounted = false;
        s_active_session_count--;
    }
    if (m_conf) {
        conf_free(m_conf);
        m_conf = 0;
    }
}

void PuTTYSessionWindow::closeEvent(TQCloseEvent *e)
{
    if (m_sessionActive && conf_get_bool(m_conf, CONF_warn_on_close)) {
        int ret = TQMessageBox::warning(
            this,
            "PuTTY-TDE Exit Confirmation",
            "Are you sure you want to close this session?",
            TQMessageBox::Yes | TQMessageBox::Default,
            TQMessageBox::No | TQMessageBox::Escape);
        if (ret != TQMessageBox::Yes) {
            e->ignore();
            return;
        }
    }

    m_sessionActive = false;
    clear_cached_auth_password();
    e->accept();
    if (m_sessionCounted) {
        m_sessionCounted = false;
        s_active_session_count--;
    }

    if (s_active_session_count <= 0) {
        PuTTYConfigDialog *mainDlg = get_main_config_dialog();
        if (mainDlg) {
            mainDlg->restorePosition();
            mainDlg->show();
            mainDlg->restorePosition();
            mainDlg->raise();
            mainDlg->setActiveWindow();
            mainDlg->setInitialFocus();
        } else {
            cleanup_exit(0);
        }
    }
}

void PuTTYSessionWindow::startSession()
{
    clear_cached_auth_password();
    m_termWidget->resetConnection();
    m_connState = CONN_CONNECTING;
    const struct BackendVtable *vt = select_backend(m_conf);
    assert(vt != NULL);

    seat_set_trust_status(m_termWidget->seat(), true);

    char *realhost = NULL;
    Backend *backend = NULL;

    LogContext *logctx = log_init(m_termWidget->logPolicy(), m_conf);
    m_termWidget->setLogContext(logctx);
    term_provide_logctx(m_termWidget->terminal(), logctx);

    char *error = backend_init(vt, m_termWidget->seat(), &backend,
                              logctx, m_conf,
                              conf_get_str(m_conf, CONF_host),
                              conf_get_int(m_conf, CONF_port),
                              &realhost,
                              conf_get_bool(m_conf, CONF_tcp_nodelay),
                              conf_get_bool(m_conf, CONF_tcp_keepalives));

    if (error) {
        m_sessionActive = false;
        TQMessageBox::critical(this, "PuTTY-TDE Fatal Error",
                              TQString("Unable to open connection:\n%1").arg(error));
        sfree(error);
        close();
        return;
    }

    m_sessionActive = true;
    updateStatusText();

    if (realhost) {
        char *title = make_default_wintitle(realhost);
        setCaption(title);
        sfree(title);
        sfree(realhost);
    }

    m_termWidget->setBackend(backend);
    term_provide_backend(m_termWidget->terminal(), backend);

    void *ldisc = ldisc_create(m_conf, m_termWidget->terminal(), backend, m_termWidget->seat());
    m_termWidget->setLdisc(ldisc);

    m_termWidget->setFocus();
}

void PuTTYSessionWindow::onScrollBarUpdated(int total, int start, int page)
{
    m_scrollbar->blockSignals(true);
    m_scrollbar->setRange(0, total - page);
    m_scrollbar->setValue(start);
    m_scrollbar->setPageStep(page);
    m_scrollbar->blockSignals(false);
}

void PuTTYSessionWindow::onScrollBarValueChanged(int value)
{
    if (m_termWidget->terminal()) {
        term_scroll(m_termWidget->terminal(), 1, value);
    }
}

void PuTTYSessionWindow::onTitleChanged(const TQString &title)
{
    setCaption(title);
}

void PuTTYSessionWindow::onConnectionEstablished()
{
    m_connState = CONN_CONNECTED;
    updateStatusText();
}

void PuTTYSessionWindow::onSessionClosed()
{
    m_connState = CONN_CLOSED;
    m_sessionActive = false;
    clear_cached_auth_password();
    updateStatusText();
    close();
}

void PuTTYSessionWindow::menuNewSession()
{
    PuTTYConfigDialog *mainDlg = get_main_config_dialog();
    if (mainDlg) {
        mainDlg->restorePosition();
        mainDlg->show();
        mainDlg->restorePosition();
        mainDlg->raise();
        mainDlg->setActiveWindow();
        mainDlg->setInitialFocus();
    } else {
        initial_config_box(conf_new(), NULL, NULL);
    }
}

void PuTTYSessionWindow::menuDuplicateSession()
{
    Conf *newconf = conf_copy(m_conf);
    new_session_window(newconf, NULL);
}

void PuTTYSessionWindow::menuResetTerminal()
{
    if (m_termWidget->terminal())
        term_pwron(m_termWidget->terminal(), true);
}

void PuTTYSessionWindow::menuClearScrollback()
{
    if (m_termWidget->terminal())
        term_clrsb(m_termWidget->terminal());
}

void PuTTYSessionWindow::menuRestartSession()
{
    if (!m_sessionActive) {
        if (m_termWidget && m_termWidget->terminal()) {
            term_pwron(m_termWidget->terminal(), false);
        }
        startSession();
    }
}

void PuTTYSessionWindow::menuCopyClipboard()
{
    if (m_termWidget && m_termWidget->terminal()) {
        static const int clips[] = { CLIP_CLIPBOARD };
        term_request_copy(m_termWidget->terminal(), clips, 1);
    }
}

void PuTTYSessionWindow::menuPasteClipboard()
{
    if (m_termWidget && m_termWidget->terminal()) {
        term_request_paste(m_termWidget->terminal(), CLIP_CLIPBOARD);
    }
}

void PuTTYSessionWindow::menuCopyAll()
{
    if (m_termWidget && m_termWidget->terminal()) {
        static const int clips[] = { CLIP_CLIPBOARD };
        term_copyall(m_termWidget->terminal(), clips, 1);
    }
}

void show_about_dialog(TQWidget *parent)
{
    TQDialog dlg(parent, "About PuTTY-TDE", true);
    dlg.setCaption("About PuTTY-TDE");
    dlg.setIcon(get_putty_icon());

    TQVBoxLayout *mainLayout = new TQVBoxLayout(&dlg, 14, 12);

    TQHBoxLayout *contentLayout = new TQHBoxLayout(mainLayout, 16);

    // Left side: custom About image (100x110)
    TQLabel *iconLabel = new TQLabel(&dlg);
    iconLabel->setPixmap(get_about_puttytde_icon());
    iconLabel->setAlignment(TQt::AlignTop | TQt::AlignHCenter);
    contentLayout->addWidget(iconLabel, 0, TQt::AlignTop);

    // Right side: version, build info, copyright
    char *buildinfo_text = buildinfo("\n");
    TQString infoHtml = TQString(buildinfo_text).replace("\n", "<br>");
    sfree(buildinfo_text);

    TQString aboutHtml = TQString(
        "<p style='margin-bottom: 4px;'>"
        "<b><font size='+1'>PuTTY-TDE</font></b><br>"
        "<b>TQt3 Edition &bull; %1</b>"
        "</p>"
        "<p style='margin-top: 6px; margin-bottom: 8px; line-height: 130%;'>"
        "%2"
        "</p>"
        "<p style='margin-top: 6px; color: #555555; font-size: 9pt;'>"
        "Copyright &copy; 1997-2020 Simon Tatham<br>"
        "All rights reserved."
        "</p>"
    ).arg(ver).arg(infoHtml);

    TQLabel *textLabel = new TQLabel(&dlg);
    textLabel->setTextFormat(TQt::RichText);
    textLabel->setText(aboutHtml);
    textLabel->setAlignment(TQt::AlignTop | TQt::AlignLeft);
    contentLayout->addWidget(textLabel, 1);

    mainLayout->addSpacing(4);

    // Bottom action: Close button
    TQHBoxLayout *btnLayout = new TQHBoxLayout(mainLayout, 8);
    btnLayout->addStretch(1);
    TQPushButton *btnClose = new TQPushButton("&Close", &dlg);
    btnClose->setDefault(true);
    btnClose->setMinimumWidth(80);
    TQObject::connect(btnClose, TQ_SIGNAL(clicked()), &dlg, TQ_SLOT(accept()));
    btnLayout->addWidget(btnClose);
    btnLayout->addStretch(1);

    dlg.exec();
}

void PuTTYSessionWindow::menuAbout()
{
    show_about_dialog(this);
}

void PuTTYSessionWindow::menuEventLog()
{
    TQDialog dlg(this, "PuTTY-TDE Event Log", true);
    dlg.setCaption("PuTTY-TDE Event Log");
    dlg.setIcon(get_putty_icon());
    dlg.resize(600, 360);

    TQVBoxLayout *layout = new TQVBoxLayout(&dlg, 10, 8);
    TQTextView *tv = new TQTextView(&dlg);
    tv->setFamily("Monospace");
    tv->setPointSize(9);
    tv->setWordWrap(TQTextView::NoWrap);

    TQString text;
    if (m_termWidget) {
        text = m_termWidget->eventLog().join("\n");
    }
    if (text.isEmpty()) {
        text = "(No events logged yet)";
    }
    tv->setText(text);
    layout->addWidget(tv, 1);

    TQHBoxLayout *btnLayout = new TQHBoxLayout(layout);
    TQPushButton *btnCopy = new TQPushButton("&Copy to Clipboard", &dlg);
    TQPushButton *btnClose = new TQPushButton("&Close", &dlg);
    btnClose->setDefault(true);
    btnLayout->addStretch(1);
    btnLayout->addWidget(btnCopy);
    btnLayout->addWidget(btnClose);

    connect(btnCopy, TQ_SIGNAL(clicked()), tv, TQ_SLOT(selectAll()));
    connect(btnCopy, TQ_SIGNAL(clicked()), tv, TQ_SLOT(copy()));
    connect(btnClose, TQ_SIGNAL(clicked()), &dlg, TQ_SLOT(accept()));

    dlg.exec();
}

void PuTTYSessionWindow::updateSavedSessionsMenu()
{
    if (!m_sessionsMenu) return;
    m_sessionsMenu->clear();
    m_savedSessionMap.clear();

    struct sesslist sl;
    get_sesslist(&sl, true);
    int count = 0;
    for (int i = 1; i < sl.nsessions; i++) {
        if (sl.sessions[i] && sl.sessions[i][0]) {
            int id = m_sessionsMenu->insertItem(TQString::fromUtf8(sl.sessions[i]));
            m_savedSessionMap[id] = TQString::fromUtf8(sl.sessions[i]);
            count++;
        }
    }
    get_sesslist(&sl, false);

    if (count == 0) {
        int id = m_sessionsMenu->insertItem("(No sessions)");
        m_sessionsMenu->setItemEnabled(id, false);
    }
}

void PuTTYSessionWindow::onSavedSessionActivated(int id)
{
    if (m_savedSessionMap.contains(id)) {
        TQString name = m_savedSessionMap[id];
        Conf *newconf = conf_new();
        do_defaults(name.utf8().data(), newconf);
        new_session_window(newconf, NULL);
    }
}

void PuTTYSessionWindow::updateSpecialsMenu()
{
    if (!m_specialsMenu) return;
    m_specialsMenu->clear();
    m_specialCmdMap.clear();

    const SessionSpecial *specials = NULL;
    if (m_termWidget && m_termWidget->m_backend) {
        specials = backend_get_specials(m_termWidget->m_backend);
    }

    if (!specials) {
        if (m_specialsItemId != -1) {
            m_popup->setItemEnabled(m_specialsItemId, false);
        }
        return;
    }

    if (m_specialsItemId != -1) {
        m_popup->setItemEnabled(m_specialsItemId, true);
    }

    TQPopupMenu *curMenu = m_specialsMenu;
    TQPopupMenu *parentMenu = NULL;
    int nesting = 1;

    for (int i = 0; nesting > 0; i++) {
        switch (specials[i].code) {
        case SS_SUBMENU: {
            parentMenu = curMenu;
            curMenu = new TQPopupMenu(parentMenu);
            connect(curMenu, TQ_SIGNAL(activated(int)), this, TQ_SLOT(onSpecialCmdActivated(int)));
            parentMenu->insertItem(TQString::fromUtf8(specials[i].name), curMenu);
            nesting++;
            break;
        }
        case SS_EXITMENU: {
            nesting--;
            if (nesting > 0 && parentMenu) {
                curMenu = parentMenu;
                parentMenu = NULL;
            }
            break;
        }
        case SS_SEP: {
            curMenu->insertSeparator();
            break;
        }
        default: {
            int id = curMenu->insertItem(TQString::fromUtf8(specials[i].name));
            SpecialCmdItem item;
            item.code = specials[i].code;
            item.arg = specials[i].arg;
            m_specialCmdMap[id] = item;
            break;
        }
        }
    }
}

void PuTTYSessionWindow::onSpecialCmdActivated(int id)
{
    if (m_specialCmdMap.contains(id) && m_termWidget && m_termWidget->m_backend) {
        SpecialCmdItem item = m_specialCmdMap[id];
        backend_special(m_termWidget->m_backend, item.code, item.arg);
    }
}

void PuTTYSessionWindow::onShowContextMenu(const TQPoint &pos)
{
    updateSavedSessionsMenu();
    updateSpecialsMenu();
    if (m_restartItemId != -1) {
        m_popup->setItemEnabled(m_restartItemId, !m_sessionActive);
    }
    if (m_popup) {
        m_popup->exec(pos);
    }
}

struct ReconfigContext {
    PuTTYSessionWindow *win;
    Conf *newconf;
};

static void after_change_settings_dialog(void *vctx, int retval)
{
    ReconfigContext *ctx = (ReconfigContext *)vctx;
    if (!ctx) return;
    if (retval > 0 && ctx->win) {
        ctx->win->applyReconfiguration(ctx->newconf);
    } else if (ctx->newconf) {
        conf_free(ctx->newconf);
    }
    delete ctx;
}

void PuTTYSessionWindow::menuSettings()
{
    ReconfigContext *ctx = new ReconfigContext();
    ctx->win = this;
    ctx->newconf = conf_copy(m_conf);

    PuTTYConfigDialog *dlg = new PuTTYConfigDialog(
        this, "PuTTY-TDE Reconfiguration", ctx->newconf, true,
        conf_get_int(ctx->newconf, CONF_protocol),
        after_change_settings_dialog, ctx);
    dlg->show();
}

void PuTTYSessionWindow::applyReconfiguration(Conf *newconf)
{
    if (m_conf) {
        conf_free(m_conf);
    }
    m_conf = newconf;

    if (m_termWidget) {
        m_termWidget->m_conf = m_conf;
        if (m_termWidget->terminal()) {
            term_reconfig(m_termWidget->terminal(), m_conf);
        }
        m_termWidget->reconfigureFromConf();
    }

    bool showSb = conf_get_bool(m_conf, CONF_scrollbar);
    if (isFullScreen() && !conf_get_bool(m_conf, CONF_scrollbar_in_fullscreen))
        showSb = false;

    if (showSb)
        m_scrollbar->show();
    else
        m_scrollbar->hide();

    if (m_showStatusBar && !isFullScreen())
        m_statusBar->show();
    else
        m_statusBar->hide();

    updateStatusText();
    updateSizeHints();
}

void PuTTYSessionWindow::updateSizeHints()
{
    if (!m_termWidget) return;
    int fw = m_termWidget->fontWidth();
    int fh = m_termWidget->fontHeight();
    if (fw > 0 && fh > 0) {
        int sbWidth = (m_scrollbar && m_scrollbar->isVisible()) ? m_scrollbar->sizeHint().width() : 0;
        int statusHeight = (m_statusBar && m_statusBar->isVisible()) ? m_statusBar->sizeHint().height() : 0;
        int baseW = 2 * m_termWidget->windowBorder() + sbWidth;
        int baseH = 2 * m_termWidget->windowBorder() + statusHeight;
        setBaseSize(baseW, baseH);
        setSizeIncrement(fw, fh);
        setMinimumSize(baseW + fw * 10, baseH + fh * 4);
    }
}

void PuTTYSessionWindow::toggleFullScreen()
{
    if (isFullScreen()) {
        showNormal();
        if (conf_get_bool(m_conf, CONF_scrollbar))
            m_scrollbar->show();
        else
            m_scrollbar->hide();
        if (m_showStatusBar)
            m_statusBar->show();
    } else {
        showFullScreen();
        if (!conf_get_bool(m_conf, CONF_scrollbar_in_fullscreen))
            m_scrollbar->hide();
        else if (conf_get_bool(m_conf, CONF_scrollbar))
            m_scrollbar->show();
        m_statusBar->hide();
    }
    updateSizeHints();
}

void PuTTYSessionWindow::toggleStatusBar()
{
    m_showStatusBar = !m_showStatusBar;
    if (m_statusBarItemId != -1) {
        m_popup->setItemChecked(m_statusBarItemId, m_showStatusBar);
    }
    if (m_showStatusBar && !isFullScreen()) {
        m_statusBar->show();
    } else {
        m_statusBar->hide();
    }
    updateSizeHints();
}

void PuTTYSessionWindow::updateStatusText()
{
    if (!m_statusHost) return;

    int proto = conf_get_int(m_conf, CONF_protocol);
    if (proto == PROT_SERIAL) {
        TQString serLine = conf_get_str(m_conf, CONF_serline);
        int baud = conf_get_int(m_conf, CONF_serspeed);
        TQString status;
        if (m_connState == CONN_CONNECTING)
            status = TQString("Opening %1 (%2 baud)...").arg(serLine).arg(baud);
        else if (m_connState == CONN_CONNECTED)
            status = TQString("%1, %2 baud").arg(serLine).arg(baud);
        else
            status = TQString("%1, %2 baud [Closed]").arg(serLine).arg(baud);
        m_statusHost->setText("  " + status);
        return;
    }

    TQString hostStr = conf_get_str(m_conf, CONF_host);
    int port = conf_get_int(m_conf, CONF_port);
    const struct BackendVtable *vt = backend_vt_from_proto(proto);
    int default_port = vt ? vt->default_port : 22;
    TQString portSuffix;
    if (port > 0 && port != default_port) {
        portSuffix = TQString(":%1").arg(port);
    }

    if (m_connState == CONN_CONNECTING) {
        m_statusHost->setText(TQString("  Connecting to %1%2...").arg(hostStr).arg(portSuffix));
        return;
    }

    TQString userStr = conf_get_str(m_conf, CONF_username);
    if (userStr.isEmpty()) {
        char *remoteUser = get_remote_username(m_conf);
        if (remoteUser && *remoteUser) {
            userStr = remoteUser;
            sfree(remoteUser);
        }
    }

    TQString connText;
    if (!userStr.isEmpty() && !hostStr.isEmpty())
        connText = userStr + "@" + hostStr + portSuffix;
    else if (!hostStr.isEmpty())
        connText = hostStr + portSuffix;
    else
        connText = userStr;

    if (m_connState == CONN_CLOSED || !m_sessionActive) {
        connText += " [Closed]";
    }

    m_statusHost->setText("  " + connText);
}

// --------------------------------------------------------------------------
// TermWin C VTable trampoline implementations
// --------------------------------------------------------------------------

#define GET_TW_INST(tw) (((TQtTermWin *)(tw))->inst)

static bool tqt_setup_draw_ctx(TermWin *tw) { return GET_TW_INST(tw)->setupDrawCtx(); }
static void tqt_free_draw_ctx(TermWin *tw) { GET_TW_INST(tw)->freeDrawCtx(); }
static void tqt_draw_text(TermWin *tw, int x, int y, wchar_t *text, int len,
                          unsigned long attr, int lattr, truecolour tc)
{
    GET_TW_INST(tw)->drawText(x, y, text, len, attr, lattr, tc);
}
static void tqt_draw_cursor(TermWin *tw, int x, int y, wchar_t *text, int len,
                            unsigned long attr, int lattr, truecolour tc)
{
    GET_TW_INST(tw)->drawCursor(x, y, text, len, attr, lattr, tc);
}
static void tqt_draw_trust_sigil(TermWin *tw, int x, int y) { GET_TW_INST(tw)->drawTrustSigil(x, y); }
static int tqt_char_width(TermWin *tw, int uc) { return GET_TW_INST(tw)->charWidth(uc); }
static void tqt_set_cursor_pos(TermWin *tw, int x, int y) { GET_TW_INST(tw)->setCursorPos(x, y); }
static void tqt_set_raw_mouse_mode(TermWin *tw, bool enable) { GET_TW_INST(tw)->setRawMouseMode(enable); }
static void tqt_set_scrollbar(TermWin *tw, int total, int start, int page)
{
    GET_TW_INST(tw)->setScrollbar(total, start, page);
}
static void tqt_bell(TermWin *tw, int mode) { GET_TW_INST(tw)->bell(mode); }
static void tqt_clip_write(TermWin *tw, int clipboard, wchar_t *text, int *attrs,
                           truecolour *colours, int len, bool must_deselect)
{
    GET_TW_INST(tw)->clipWrite(clipboard, text, attrs, colours, len, must_deselect);
}
static void tqt_clip_request_paste(TermWin *tw, int clipboard)
{
    GET_TW_INST(tw)->clipRequestPaste(clipboard);
}
static void tqt_refresh(TermWin *tw) { GET_TW_INST(tw)->refreshWindow(); }
static void tqt_request_resize(TermWin *tw, int w, int h) { GET_TW_INST(tw)->requestResize(w, h); }
static void tqt_set_title(TermWin *tw, const char *title) { GET_TW_INST(tw)->setWindowTitle(title); }
static void tqt_set_icon_title(TermWin *tw, const char *icontitle) { GET_TW_INST(tw)->setIconTitle(icontitle); }
static void tqt_set_minimised(TermWin *tw, bool minimised)
{
    PuTTYTermWidget *w = GET_TW_INST(tw);
    if (w && w->topLevelWidget()) {
        if (minimised) w->topLevelWidget()->showMinimized();
        else w->topLevelWidget()->showNormal();
    }
}
static bool tqt_is_minimised(TermWin *tw)
{
    PuTTYTermWidget *w = GET_TW_INST(tw);
    return (w && w->topLevelWidget()) ? w->topLevelWidget()->isMinimized() : false;
}
static void tqt_set_maximised(TermWin *tw, bool maximised)
{
    PuTTYTermWidget *w = GET_TW_INST(tw);
    if (w && w->topLevelWidget()) {
        if (maximised) w->topLevelWidget()->showMaximized();
        else w->topLevelWidget()->showNormal();
    }
}
static void tqt_move(TermWin *tw, int x, int y) { (void)tw; (void)x; (void)y; }
static void tqt_set_zorder(TermWin *tw, bool top) { (void)tw; (void)top; }
static bool tqt_palette_get(TermWin *tw, int n, int *r, int *g, int *b)
{
    return GET_TW_INST(tw)->paletteGet(n, r, g, b);
}
static void tqt_palette_set(TermWin *tw, int n, int r, int g, int b)
{
    GET_TW_INST(tw)->paletteSet(n, r, g, b);
}
static void tqt_palette_reset(TermWin *tw) { GET_TW_INST(tw)->paletteReset(); }
static void tqt_get_pos(TermWin *tw, int *x, int *y) { (void)tw; *x = 0; *y = 0; }
static void tqt_get_pixels(TermWin *tw, int *x, int *y)
{
    PuTTYTermWidget *w = GET_TW_INST(tw);
    *x = w->width();
    *y = w->height();
}
static const char *tqt_get_title(TermWin *tw, bool icon)
{
    (void)icon;
    PuTTYTermWidget *w = GET_TW_INST(tw);
    if (!w) return "PuTTY";
    static char titlebuf[256];
    strncpy(titlebuf, w->caption().utf8().data(), sizeof(titlebuf) - 1);
    titlebuf[sizeof(titlebuf) - 1] = '\0';
    return titlebuf;
}
static bool tqt_tw_is_utf8(TermWin *tw) { (void)tw; return true; }


// --------------------------------------------------------------------------
// Seat C VTable trampoline implementations
// --------------------------------------------------------------------------

#define GET_SEAT_INST(s) (((TQtSeat *)(s))->inst)

static size_t tqt_seat_output(Seat *seat, bool is_stderr, const void *data, size_t len)
{
    PuTTYTermWidget *w = GET_SEAT_INST(seat);
    if (len > 0) {
        w->notifyConnectionEstablished();
    }
    return term_data(w->terminal(), is_stderr, data, len);
}
static bool tqt_seat_eof(Seat *seat) { (void)seat; return true; }
static void tqt_seat_notify_remote_exit(Seat *seat)
{
    PuTTYTermWidget *w = GET_SEAT_INST(seat);
    w->notifySessionClosed();
}
static void tqt_seat_connection_fatal(Seat *seat, const char *message)
{
    PuTTYTermWidget *w = GET_SEAT_INST(seat);
    TQMessageBox::critical(w, "PuTTY-TDE Fatal Error", message);
    w->notifySessionClosed();
}
static void tqt_seat_update_specials_menu(Seat *seat) { (void)seat; }
static char *tqt_seat_get_ttymode(Seat *seat, const char *mode)
{
    PuTTYTermWidget *w = GET_SEAT_INST(seat);
    return term_get_ttymode(w->terminal(), mode);
}
static void tqt_seat_set_busy_status(Seat *seat, BusyStatus status)
{
    PuTTYTermWidget *w = GET_SEAT_INST(seat);
    if (status == BUSY_WAITING)
        w->setCursor(TQt::waitCursor);
    else
        w->setCursor(TQt::ibeamCursor);
}
static bool tqt_seat_is_utf8(Seat *seat) { (void)seat; return true; }
static void tqt_seat_echoedit_update(Seat *seat, bool echo, bool edit)
{
    (void)seat; (void)echo; (void)edit;
}
static const char *tqt_seat_get_x_display(Seat *seat)
{
    (void)seat;
    return getenv("DISPLAY");
}
static bool tqt_seat_get_windowid(Seat *seat, long *id_out)
{
    PuTTYTermWidget *w = GET_SEAT_INST(seat);
    if (w && id_out) {
        *id_out = (long)w->winId();
        return true;
    }
    return false;
}
static bool tqt_seat_get_window_pixel_size(Seat *seat, int *width, int *height)
{
    PuTTYTermWidget *w = GET_SEAT_INST(seat);
    *width = w->width();
    *height = w->height();
    return true;
}
static StripCtrlChars *tqt_seat_stripctrl_new(Seat *seat, BinarySink *bs_out, SeatInteractionContext sic)
{
    (void)sic;
    PuTTYTermWidget *w = GET_SEAT_INST(seat);
    return stripctrl_new_term(bs_out, false, 0, w->terminal());
}
static bool tqt_seat_set_trust_status(Seat *seat, bool trusted)
{
    PuTTYTermWidget *w = GET_SEAT_INST(seat);
    term_set_trust_status(w->terminal(), trusted);
    if (!trusted) {
        w->notifyConnectionEstablished();
    }
    return true;
}
static bool tqt_seat_verbose(Seat *seat) { (void)seat; return false; }
static bool tqt_seat_interactive(Seat *seat) { (void)seat; return true; }
static bool tqt_seat_get_cursor_position(Seat *seat, int *x, int *y)
{
    PuTTYTermWidget *w = GET_SEAT_INST(seat);
    if (w->terminal()) {
        term_get_cursor_position(w->terminal(), x, y);
        return true;
    }
    return false;
}

// --------------------------------------------------------------------------
// Entry point for new sessions
// --------------------------------------------------------------------------

extern "C" void tqt_seat_set_username(Seat *seat, const char *user)
{
    if (!seat || !user || !*user) return;
    PuTTYTermWidget *w = GET_SEAT_INST(seat);
    if (!w) return;
    w->setUsername(TQString::fromUtf8(user));
}

void new_session_window(Conf *conf, const char *geometry_string)
{
    prepare_session(conf);
    PuTTYSessionWindow *win = new PuTTYSessionWindow(conf, geometry_string);
    win->show();
}
