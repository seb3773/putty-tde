#ifndef PUTTY_TQTWIN_H
#define PUTTY_TQTWIN_H

#include <tqwidget.h>
#include <tqscrollbar.h>
#include <tqpixmap.h>
#include <tqfont.h>
#include <tqcolor.h>
#include <tqpopupmenu.h>
#include <tqstringlist.h>
#include <tqmap.h>
#include <tqdatetime.h>
#include <tqpoint.h>

#include "putty_headers.h"

class PuTTYTermWidget;
class TQStatusBar;
class TQLabel;

struct TQtTermWin {
    TermWin tw;
    PuTTYTermWidget *inst;
};

struct TQtSeat {
    Seat seat;
    PuTTYTermWidget *inst;
};

struct TQtLogPolicy {
    LogPolicy lp;
    PuTTYTermWidget *inst;
};

class PuTTYTermWidget : public TQWidget {
    TQ_OBJECT
public:
    PuTTYTermWidget(TQWidget *parent, Conf *conf);
    virtual ~PuTTYTermWidget();

    Terminal *terminal() const { return m_term; }
    Seat *seat() { return &m_tqt_seat.seat; }
    TermWin *termWin() { return &m_tqt_termwin.tw; }
    LogPolicy *logPolicy() { return &m_tqt_logpolicy.lp; }
    void setBackend(Backend *b) { m_backend = b; }
    void setLdisc(void *ldisc) { m_ldisc = ldisc; }
    void setLogContext(LogContext *lc) { m_logctx = lc; }

    Conf *conf() const { return m_conf; }
    void setUsername(const TQString &user);
    void notifySessionClosed() { emit sessionClosed(); }
    void notifyConnectionEstablished();
    void resetConnection();
    bool isConnected() const { return m_connected; }
    void reconfigureFromConf();
    void addEventLog(const char *event);
    const TQStringList &eventLog() const { return m_eventLog; }

    TQtTermWin m_tqt_termwin;
    TQtSeat m_tqt_seat;
    TQtLogPolicy m_tqt_logpolicy;

    bool setupDrawCtx();
    void freeDrawCtx();
    void drawText(int x, int y, wchar_t *text, int len,
                  unsigned long attr, int lattr, truecolour tc);
    void drawCursor(int x, int y, wchar_t *text, int len,
                    unsigned long attr, int lattr, truecolour tc);
    void drawTrustSigil(int x, int y);
    int charWidth(int uc);
    void setCursorPos(int x, int y);
    void setRawMouseMode(bool enable);
    void setScrollbar(int total, int start, int page);
    void bell(int mode);
    void clipWrite(int clipboard, wchar_t *text, int *attrs,
                   truecolour *colours, int len, bool must_deselect);
    void clipRequestPaste(int clipboard);
    void refreshWindow();
    void requestResize(int w, int h);
    void setWindowTitle(const char *title);
    void setIconTitle(const char *icontitle);
    bool paletteGet(int n, int *r, int *g, int *b);
    void paletteSet(int n, int r, int g, int b);
    void paletteReset();

    void updateScrollBarValues(int total, int start, int page);
    void onScrollBarMoved(int value);

    int fontWidth() const { return m_fontWidth; }
    int fontHeight() const { return m_fontHeight; }
    int termCols() const { return m_termCols; }
    int termRows() const { return m_termRows; }
    int windowBorder() const { return m_windowBorder; }
    void changeFontSize(int increment);

    TermWin m_termwin;
    Seat m_seat;
    LogPolicy m_logpolicy;

signals:
    void scrollbarUpdated(int total, int start, int page);
    void windowTitleChanged(const TQString &title);
    void usernameChanged(const TQString &user);
    void connectionEstablished();
    void sessionClosed();
    void showContextMenuRequested(const TQPoint &pos);
    void fontSizeChanged();
    void toggleFullScreenRequested();

protected:
    virtual void paintEvent(TQPaintEvent *e);
    virtual void resizeEvent(TQResizeEvent *e);
    virtual void keyPressEvent(TQKeyEvent *e);
    virtual void mousePressEvent(TQMouseEvent *e);
    virtual void mouseDoubleClickEvent(TQMouseEvent *e);
    virtual void mouseReleaseEvent(TQMouseEvent *e);
    virtual void mouseMoveEvent(TQMouseEvent *e);
    virtual void wheelEvent(TQWheelEvent *e);
    virtual void contextMenuEvent(TQContextMenuEvent *e);
    virtual void focusInEvent(TQFocusEvent *e);
    virtual void focusOutEvent(TQFocusEvent *e);
    virtual void dragEnterEvent(TQDragEnterEvent *e);
    virtual void dropEvent(TQDropEvent *e);

private:
    void initFont();
    void initPalette();
    void ensureBackbuffer(int w, int h);
    void doDrawText(int x, int y, wchar_t *text, int len,
                    unsigned long attr, int lattr, truecolour tc);

    Conf *m_conf;
    Terminal *m_term;
    Backend *m_backend;
    void *m_ldisc;
    LogContext *m_logctx;
    struct unicode_data m_ucsdata;

    TQFont m_font;
    TQFont m_fontBold;
    int m_fontWidth;
    int m_fontHeight;
    int m_fontAscent;
    int m_windowBorder;

    int m_termCols;
    int m_termRows;

    TQPixmap *m_backbuffer;
    TQColor m_palette[262];
    bool m_rawMouse;
    bool m_drawing;
    bool m_connected;
    TQStringList m_eventLog;

    TQTime m_lastClickTime;
    TQPoint m_lastClickPos;
    int m_lastClickAction;
    int m_lastClickButton;
    bool m_cursorHidden;

    friend class PuTTYSessionWindow;
};

class PuTTYSessionWindow : public TQWidget {
    TQ_OBJECT
public:
    enum ConnState {
        CONN_CONNECTING,
        CONN_CONNECTED,
        CONN_CLOSED
    };

    PuTTYSessionWindow(Conf *conf, const char *geometry_string = 0);
    virtual ~PuTTYSessionWindow();

    PuTTYTermWidget *termWidget() const { return m_termWidget; }
    void startSession();
    void applyReconfiguration(Conf *newconf);

public slots:
    void onScrollBarValueChanged(int value);
    void onScrollBarUpdated(int total, int start, int page);
    void onTitleChanged(const TQString &title);
    void onSessionClosed();
    void onShowContextMenu(const TQPoint &pos);
    void menuNewSession();
    void menuRestartSession();
    void menuDuplicateSession();
    void menuSettings();
    void menuEventLog();
    void menuClearScrollback();
    void menuResetTerminal();
    void menuCopyClipboard();
    void menuPasteClipboard();
    void menuCopyAll();
    void menuAbout();
    void onSavedSessionActivated(int id);
    void onSpecialCmdActivated(int id);
    void updateSizeHints();
    void toggleFullScreen();
    void toggleStatusBar();
    void updateStatusText();
    void onConnectionEstablished();

protected:
    virtual void closeEvent(TQCloseEvent *e);

private:
    void updateSavedSessionsMenu();
    void updateSpecialsMenu();

    Conf *m_conf;
    PuTTYTermWidget *m_termWidget;
    TQScrollBar *m_scrollbar;
    TQStatusBar *m_statusBar;
    TQLabel *m_statusHost;
    TQLabel *m_statusHint;
    TQPopupMenu *m_popup;
    TQPopupMenu *m_sessionsMenu;
    TQPopupMenu *m_specialsMenu;
    int m_restartItemId;
    int m_specialsItemId;
    int m_statusBarItemId;
    bool m_showStatusBar;
    TQMap<int, TQString> m_savedSessionMap;
    struct SpecialCmdItem {
        SessionSpecialCode code;
        int arg;
    };
    TQMap<int, SpecialCmdItem> m_specialCmdMap;
    bool m_sessionCounted;
    bool m_sessionActive;
    ConnState m_connState;
};

void show_about_dialog(TQWidget *parent);

#ifdef __cplusplus
extern "C" {
#endif

void new_session_window(Conf *conf, const char *geometry_string);
int get_active_session_count();

#ifdef __cplusplus
}
#endif

#endif /* PUTTY_TQTWIN_H */
