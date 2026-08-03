#pragma once

#include <QObject>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class CopyGestureMonitor final : public QObject
{
    Q_OBJECT
public:
    explicit CopyGestureMonitor(QObject* parent = nullptr);
    ~CopyGestureMonitor() override;
    bool start();
    void stop();
    void setPaused(bool paused);

signals:
    void copyGestureDetected(unsigned long clipboardSequence);

private:
#ifdef Q_OS_WIN
    static LRESULT CALLBACK keyboardProc(int code, WPARAM message, LPARAM data);
    void handleKeyboardEvent(WPARAM message, const KBDLLHOOKSTRUCT& event);
    static CopyGestureMonitor* instance_;
    HHOOK hook_ = nullptr;
    ULONGLONG lastCopyTime_ = 0;
    bool cIsDown_ = false;
    bool ctrlIsDown_ = false;
#endif
    bool paused_ = false;
};
