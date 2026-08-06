#include "copy_gesture_monitor.h"
#include "i18n.h"
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
    tray.showMessage(
        QCoreApplication::translate("App", "IntentClip"),
        QCoreApplication::translate("App",
            "After selecting text, press Ctrl+C twice within 500 ms to open the panel.\n"
            "You can also open it from the system tray at any time."),
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

    installAppTranslations(application);

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
    tray.setToolTip(QCoreApplication::translate("App", "IntentClip"));
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
    QAction* openAction = trayMenu.addAction(
        QCoreApplication::translate("App", "Open Panel"));
    QAction* settingsAction = trayMenu.addAction(
        QCoreApplication::translate("App", "Function Settings"));
    QAction* quitAction = trayMenu.addAction(
        QCoreApplication::translate("App", "Quit"));
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
        tray.showMessage(
            QCoreApplication::translate("App", "IntentClip"),
            QCoreApplication::translate("App",
                "Failed to enable the double-copy listener. "
                "You can still open the panel from the tray."));
    }

    showFirstLaunchTutorial(tray);

    return application.exec();
}