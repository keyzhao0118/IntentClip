#include "main_window.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("IntentClip · 拾意"));
    resize(720, 460);
    setMinimumSize(560, 360);

    auto* centralWidget = new QWidget(this);
    centralWidget->setObjectName(QStringLiteral("centralWidget"));

    auto* layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(40, 40, 40, 40);
    layout->setSpacing(16);
    layout->addStretch();

    auto* titleLabel = new QLabel(tr("IntentClip · 拾意"), centralWidget);
    titleLabel->setObjectName(QStringLiteral("titleLabel"));
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    auto* subtitleLabel = new QLabel(tr("拾取文本，洞悉心意。"), centralWidget);
    subtitleLabel->setObjectName(QStringLiteral("subtitleLabel"));
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setWordWrap(true);
    layout->addWidget(subtitleLabel);

    auto* statusLabel = new QLabel(tr("Demo ready"), centralWidget);
    statusLabel->setObjectName(QStringLiteral("statusLabel"));
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setFixedSize(112, 36);
    layout->addWidget(statusLabel, 0, Qt::AlignHCenter);

    layout->addStretch();
    setCentralWidget(centralWidget);

    setStyleSheet(QStringLiteral(R"(
        #centralWidget {
            background-color: #f4f6f8;
        }
        #titleLabel {
            color: #182230;
            font-size: 32px;
            font-weight: 600;
        }
        #subtitleLabel {
            color: #52606d;
            font-size: 18px;
        }
        #statusLabel {
            color: #2457a7;
            background-color: #e1ecff;
            border-radius: 18px;
            font-size: 14px;
            font-weight: 500;
        }
    )"));
}
