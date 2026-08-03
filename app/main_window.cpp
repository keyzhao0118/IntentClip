#include "main_window.h"

#include <QCloseEvent>
#include <QLabel>
#include <QTextEdit>
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

    auto* titleLabel = new QLabel(tr("IntentClip · 拾意"), centralWidget);
    titleLabel->setObjectName(QStringLiteral("titleLabel"));
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    auto* subtitleLabel = new QLabel(tr("快速按两次 Ctrl+C，即可拾取选中文本。"), centralWidget);
    subtitleLabel->setObjectName(QStringLiteral("subtitleLabel"));
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setWordWrap(true);
    layout->addWidget(subtitleLabel);

    statusLabel_ = new QLabel(tr("正在监听"), centralWidget);
    statusLabel_->setObjectName(QStringLiteral("statusLabel"));
    statusLabel_->setAlignment(Qt::AlignCenter);
    statusLabel_->setFixedSize(112, 36);
    layout->addWidget(statusLabel_, 0, Qt::AlignHCenter);

    contentEdit_ = new QTextEdit(centralWidget);
    contentEdit_->setObjectName(QStringLiteral("contentEdit"));
    contentEdit_->setReadOnly(true);
    contentEdit_->setPlaceholderText(tr("双击 Ctrl+C 后，剪贴板文本会显示在这里。"));
    contentEdit_->setMinimumHeight(180);
    layout->addWidget(contentEdit_);
    setCentralWidget(centralWidget);

    setStyleSheet(QStringLiteral(R"(
        #centralWidget { background-color: #f4f6f8; }
        #titleLabel { color: #182230; font-size: 32px; font-weight: 600; }
        #subtitleLabel { color: #52606d; font-size: 18px; }
        #statusLabel { color: #2457a7; background-color: #e1ecff; border-radius: 18px; font-size: 14px; font-weight: 500; }
        #contentEdit { color: #182230; background-color: white; border: 1px solid #d8dee6; border-radius: 10px; padding: 14px; font-size: 15px; }
    )"));
}

void MainWindow::showClipboardText(const QString& text)
{
    contentEdit_->setPlainText(text);
    statusLabel_->setText(tr("已拾取"));
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    hide();
    event->ignore();
}
