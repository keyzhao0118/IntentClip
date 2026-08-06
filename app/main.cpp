#include "copy_gesture_monitor.h"
#include "main_window.h"
#include "rounded_menu.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QIcon>
#include <QLockFile>
#include <QMenu>
#include <QSettings>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
QIcon createTrayIcon()
{
    return QIcon(QStringLiteral(":/resources/tray-icon.svg"));
}

void showFirstLaunchTutorial(QSystemTrayIcon& tray)
{
    QSettings settings;
    if (settings.value(QStringLiteral("tutorial_shown"), false).toBool()) return;
    settings.setValue(QStringLiteral("tutorial_shown"), true);

    tray.showMessage(
        QStringLiteral("IntentClip · 拾意"),
        QStringLiteral(
            "选中文本后，在 500ms 内连续按两次 Ctrl+C 即可唤起面板。\n"
            "也可以从系统托盘随时打开。"),
        QSystemTrayIcon::Information,
        5000);
}
}

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("IntentClip"));
    QApplication::setOrganizationName(QStringLiteral("IntentClip"));
    QApplication::setQuitOnLastWindowClosed(false);

    QSettings::setDefaultFormat(QSettings::IniFormat);

    const QString lockPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("IntentClip-single-instance.lock"));
    QLockFile instanceLock(lockPath);
    instanceLock.setStaleLockTime(0);
    if (!instanceLock.tryLock(0)) return 0;

    MainWindow window;
    const QIcon icon = createTrayIcon();
    QApplication::setWindowIcon(icon);

    QSystemTrayIcon tray(icon);
    tray.setToolTip(QStringLiteral("IntentClip · 拾意"));
    RoundedMenu trayMenu;
    trayMenu.setObjectName(QStringLiteral("trayMenu"));
    trayMenu.setStyleSheet(QStringLiteral(R"(
        QMenu#trayMenu {
            color: #1a1c1f;
            background-color: #ffffff;
            border: 1px solid #e4e4e4;
            border-radius: 9px;
            padding: 6px;
            font-family: "Segoe UI", "Microsoft YaHei UI";
            font-size: 13px;
        }
        QMenu#trayMenu::item {
            min-width: 112px;
            padding: 9px 22px 9px 12px;
            border-radius: 6px;
        }
        QMenu#trayMenu::item:selected {
            color: #1a1c1f;
            background-color: #f0f1f2;
        }
    )"));
    QAction* openAction = trayMenu.addAction(QStringLiteral("打开面板"));
    QAction* settingsAction = trayMenu.addAction(QStringLiteral("功能设置"));
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
        window.showPanel();
    });
    QObject::connect(settingsAction, &QAction::triggered, &window, [&window] {
        window.openFunctionSettings();
    });
    QObject::connect(quitAction, &QAction::triggered, &application, &QApplication::quit);
    QObject::connect(&tray, &QSystemTrayIcon::activated, &window,
        [&window](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger
                || reason == QSystemTrayIcon::DoubleClick) {
                window.showPanel();
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
        const QString text = QApplication::clipboard()->text().trimmed();
        if (text.isEmpty() && clipboardPollsRemaining > 0) return;
        clipboardTimer.stop();
        if (!text.isEmpty()) window.showClipboardText(text);
    });

    tray.show();
    if (!monitor.start()) {
        tray.showMessage(QStringLiteral("IntentClip"), QStringLiteral("无法启用双复制监听，可从托盘手动打开面板。"));
    }

    showFirstLaunchTutorial(tray);

    return application.exec();
}