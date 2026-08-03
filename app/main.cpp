#include "main_window.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("IntentClip"));
    QApplication::setOrganizationName(QStringLiteral("IntentClip"));

    MainWindow window;
    window.show();

    return application.exec();
}
