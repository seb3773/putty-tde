#include "tqtcomm.h"

#include "putty_headers.h"

struct uxsel_id {
    int fd;
    TQSocketNotifier *sn_read;
    TQSocketNotifier *sn_write;
    TQSocketNotifier *sn_except;
};

TQtEventBridge *TQtEventBridge::s_instance = 0;

TQtEventBridge *TQtEventBridge::instance()
{
    if (!s_instance) {
        s_instance = new TQtEventBridge();
    }
    return s_instance;
}

TQtEventBridge::TQtEventBridge()
    : TQObject(0, "TQtEventBridge"),
      m_timer(0),
      m_callbacksPending(false)
{
    m_timer = new TQTimer(this);
    connect(m_timer, TQ_SIGNAL(timeout()), this, TQ_SLOT(timerTriggered()));
}

TQtEventBridge::~TQtEventBridge()
{
}

void TQtEventBridge::socketRead(int fd)
{
    select_result(fd, SELECT_R);
}

void TQtEventBridge::socketWrite(int fd)
{
    select_result(fd, SELECT_W);
}

void TQtEventBridge::socketException(int fd)
{
    select_result(fd, SELECT_X);
}

void TQtEventBridge::armTimer(long ticks)
{
    if (m_timer->isActive())
        m_timer->stop();
    m_timer->start(ticks, true);
}

void TQtEventBridge::timerTriggered()
{
    unsigned long now = GETTICKCOUNT();
    unsigned long next = 0;

    if (run_timers(now, &next)) {
        long ticks = (long)(next - now);
        if (ticks <= 0)
            ticks = 1;
        armTimer(ticks);
    }
}

void TQtEventBridge::scheduleToplevelCallbacks()
{
    if (!m_callbacksPending) {
        m_callbacksPending = true;
        TQTimer::singleShot(0, this, TQ_SLOT(processToplevelCallbacks()));
    }
}

void TQtEventBridge::processToplevelCallbacks()
{
    m_callbacksPending = false;
    run_toplevel_callbacks();

    if (toplevel_callback_pending()) {
        scheduleToplevelCallbacks();
    }
}

static void notify_toplevel_callback(void *)
{
    TQtEventBridge::instance()->scheduleToplevelCallbacks();
}

extern "C" {

uxsel_id *uxsel_input_add(int fd, int rwx)
{
    uxsel_id *id = new uxsel_id;
    id->fd = fd;
    id->sn_read = 0;
    id->sn_write = 0;
    id->sn_except = 0;

    TQtEventBridge *bridge = TQtEventBridge::instance();

    if (rwx & SELECT_R) {
        id->sn_read = new TQSocketNotifier(fd, TQSocketNotifier::Read, bridge);
        bridge->connect(id->sn_read, TQ_SIGNAL(activated(int)), bridge, TQ_SLOT(socketRead(int)));
    }
    if (rwx & SELECT_W) {
        id->sn_write = new TQSocketNotifier(fd, TQSocketNotifier::Write, bridge);
        bridge->connect(id->sn_write, TQ_SIGNAL(activated(int)), bridge, TQ_SLOT(socketWrite(int)));
    }
    if (rwx & SELECT_X) {
        id->sn_except = new TQSocketNotifier(fd, TQSocketNotifier::Exception, bridge);
        bridge->connect(id->sn_except, TQ_SIGNAL(activated(int)), bridge, TQ_SLOT(socketException(int)));
    }

    return id;
}

void uxsel_input_remove(uxsel_id *id)
{
    if (!id) return;

    delete id->sn_read;
    delete id->sn_write;
    delete id->sn_except;
    delete id;
}

void timer_change_notify(unsigned long next)
{
    long ticks = (long)(next - GETTICKCOUNT());
    if (ticks <= 0)
        ticks = 1;

    TQtEventBridge::instance()->armTimer(ticks);
}

void tqtcomm_setup(void)
{
    uxsel_init();
    request_callback_notifications(notify_toplevel_callback, NULL);
}

} // extern "C"
