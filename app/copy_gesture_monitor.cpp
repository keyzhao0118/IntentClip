#include "copy_gesture_monitor.h"

#include <QMetaObject>

#ifdef Q_OS_WIN
CopyGestureMonitor* CopyGestureMonitor::instance_ = nullptr;
#endif

CopyGestureMonitor::CopyGestureMonitor(QObject* parent) : QObject(parent) {}
CopyGestureMonitor::~CopyGestureMonitor() { stop(); }

bool CopyGestureMonitor::start()
{
#ifdef Q_OS_WIN
    if (hook_) return true;
    instance_ = this;
    hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardProc, GetModuleHandleW(nullptr), 0);
    if (!hook_) instance_ = nullptr;
    return hook_ != nullptr;
#else
    return false;
#endif
}

void CopyGestureMonitor::stop()
{
#ifdef Q_OS_WIN
    if (hook_) {
        UnhookWindowsHookEx(hook_);
        hook_ = nullptr;
    }
    if (instance_ == this) instance_ = nullptr;
#endif
}

void CopyGestureMonitor::setPaused(bool paused)
{
    paused_ = paused;
#ifdef Q_OS_WIN
    lastCopyTime_ = 0;
    cIsDown_ = false;
#endif
}

#ifdef Q_OS_WIN
LRESULT CALLBACK CopyGestureMonitor::keyboardProc(int code, WPARAM message, LPARAM data)
{
    if (code >= 0 && instance_)
        instance_->handleKeyboardEvent(message, *reinterpret_cast<KBDLLHOOKSTRUCT*>(data));
    return CallNextHookEx(nullptr, code, message, data);
}

void CopyGestureMonitor::handleKeyboardEvent(WPARAM message, const KBDLLHOOKSTRUCT& event)
{
    if (paused_ || event.vkCode != 'C') return;
    if (message == WM_KEYUP || message == WM_SYSKEYUP) {
        cIsDown_ = false;
        return;
    }
    if ((message != WM_KEYDOWN && message != WM_SYSKEYDOWN) || cIsDown_) return;
    cIsDown_ = true;
    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0) {
        lastCopyTime_ = 0;
        return;
    }
    const ULONGLONG now = GetTickCount64();
    if (lastCopyTime_ && now - lastCopyTime_ <= 500) {
        lastCopyTime_ = 0;
        const DWORD clipboardSequence = GetClipboardSequenceNumber();
        QMetaObject::invokeMethod(this, [this, clipboardSequence] {
            emit copyGestureDetected(clipboardSequence);
        }, Qt::QueuedConnection);
    } else {
        lastCopyTime_ = now;
    }
}
#endif
