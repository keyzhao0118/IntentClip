#include "copy_gesture_monitor.h"
#include "main_window.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QSystemTrayIcon>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
QIcon createTrayIcon()
{
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(QStringLiteral("#2457a7")));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(4, 4, 56, 56, 14, 14);
    painter.setPen(QPen(Qt::white, 6, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(22, 18, 22, 46);
    painter.drawLine(22, 18, 41, 18);
    painter.drawLine(22, 46, 41, 46);
    return QIcon(pixmap);
}
}

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("IntentClip"));
    QApplication::setOrganizationName(QStringLiteral("IntentClip"));
    QApplication::setQuitOnLastWindowClosed(false);

    MainWindow window;
    const QIcon icon = createTrayIcon();
    QApplication::setWindowIcon(icon);

    QSystemTrayIcon tray(icon);
    tray.setToolTip(QStringLiteral("IntentClip · 拾意"));
    QMenu trayMenu;
    QAction* openAction = trayMenu.addAction(QStringLiteral("打开面板"));
    QAction* pauseAction = trayMenu.addAction(QStringLiteral("暂停双复制监听"));
    pauseAction->setCheckable(true);
    trayMenu.addSeparator();
    QAction* quitAction = trayMenu.addAction(QStringLiteral("退出"));
    tray.setContextMenu(&trayMenu);

    CopyGestureMonitor monitor;
    QTimer clipboardTimer;
    clipboardTimer.setInterval(25);
    int clipboardPollsRemaining = 0;
#ifdef Q_OS_WIN
    DWORD clipboardSequenceAtGesture = 0;
#endif

    QObject::connect(openAction, &QAction::triggered, &window, [&window] {
        window.showNormal();
        window.raise();
        window.activateWindow();
    });
    QObject::connect(pauseAction, &QAction::toggled, &monitor, &CopyGestureMonitor::setPaused);
    QObject::connect(quitAction, &QAction::triggered, &application, &QApplication::quit);
    QObject::connect(&tray, &QSystemTrayIcon::activated, &window,
        [&window](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::DoubleClick) {
                window.showNormal();
                window.raise();
                window.activateWindow();
            }
        });
    QObject::connect(&monitor, &CopyGestureMonitor::copyGestureDetected, &application, [&](unsigned long sequence) {
#ifdef Q_OS_WIN
        clipboardSequenceAtGesture = sequence;
#else
        Q_UNUSED(sequence);
#endif
        clipboardPollsRemaining = 40;
        clipboardTimer.start();
    });
    QObject::connect(&clipboardTimer, &QTimer::timeout, &application, [&] {
        --clipboardPollsRemaining;
#ifdef Q_OS_WIN
        const bool clipboardUpdated = GetClipboardSequenceNumber() != clipboardSequenceAtGesture;
#else
        const bool clipboardUpdated = true;
#endif
        if (!clipboardUpdated && clipboardPollsRemaining > 0) return;
        clipboardTimer.stop();
        const QString text = QApplication::clipboard()->text().trimmed();
        if (!text.isEmpty()) {
            window.showClipboardText(text);
        } else {
            tray.showMessage(QStringLiteral("IntentClip"), QStringLiteral("剪贴板中没有可读取的文本。"));
        }
    });

    tray.show();
    if (!monitor.start()) {
        pauseAction->setChecked(true);
        pauseAction->setEnabled(false);
        tray.showMessage(QStringLiteral("IntentClip"), QStringLiteral("无法启用双复制监听，可从托盘手动打开面板。"));
    }

    return application.exec();
}
