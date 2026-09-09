#ifndef PUTTY_TQTCOMM_H
#define PUTTY_TQTCOMM_H

#include <tqobject.h>
#include <tqsocketnotifier.h>
#include <tqtimer.h>

class TQtEventBridge : public TQObject {
    TQ_OBJECT
public:
    static TQtEventBridge *instance();

public slots:
    void socketRead(int fd);
    void socketWrite(int fd);
    void socketException(int fd);
    void timerTriggered();
    void armTimer(long ticks);
    void processToplevelCallbacks();
    void scheduleToplevelCallbacks();

private:
    TQtEventBridge();
    virtual ~TQtEventBridge();

    static TQtEventBridge *s_instance;
    TQTimer *m_timer;
    bool m_callbacksPending;
};

#ifdef __cplusplus
extern "C" {
#endif

void tqtcomm_setup(void);

#ifdef __cplusplus
}
#endif

#endif /* PUTTY_TQTCOMM_H */
