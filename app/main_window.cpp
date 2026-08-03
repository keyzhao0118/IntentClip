#include "main_window.h"

#include "inference_client.h"

#include <QButtonGroup>
#include <QCloseEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QProgressBar>
#include <QScrollArea>
#include <QScreen>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif

namespace {
QLabel* makeSectionTitle(const QString& index, const QString& title, QWidget* parent)
{
    auto* label = new QLabel(QStringLiteral("<span class='index'>%1</span>  %2").arg(index, title), parent);
    label->setObjectName(QStringLiteral("sectionTitle"));
    label->setTextFormat(Qt::RichText);
    return label;
}

QFrame* makeSection(QWidget* parent)
{
    auto* section = new QFrame(parent);
    section->setObjectName(QStringLiteral("section"));
    return section;
}
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("IntentClip · 拾意"));
    resize(760, 430);
    setMinimumSize(600, 480);

    auto* centralWidget = new QWidget(this);

    centralWidget->setObjectName(QStringLiteral("centralWidget"));
    auto* layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(32, 28, 32, 36);
    layout->setSpacing(14);

    auto* headerLayout = new QHBoxLayout;
    auto* brandLayout = new QVBoxLayout;
    brandLayout->setSpacing(3);
    auto* titleLabel = new QLabel(tr("IntentClip · 拾意"), centralWidget);
    titleLabel->setObjectName(QStringLiteral("titleLabel"));
    brandLayout->addWidget(titleLabel);
    auto* subtitleLabel = new QLabel(tr("复制两次，让文本变成下一步行动。"), centralWidget);
    subtitleLabel->setObjectName(QStringLiteral("subtitleLabel"));
    brandLayout->addWidget(subtitleLabel);
    headerLayout->addLayout(brandLayout);
    headerLayout->addStretch();
    statusLabel_ = new QLabel(tr("正在监听"), centralWidget);
    statusLabel_->setObjectName(QStringLiteral("statusLabel"));
    statusLabel_->setAlignment(Qt::AlignCenter);
    statusLabel_->setFixedSize(96, 32);
    headerLayout->addWidget(statusLabel_, 0, Qt::AlignVCenter);
    layout->addLayout(headerLayout);

    auto* contentSection = makeSection(centralWidget);
    auto* contentLayout = new QVBoxLayout(contentSection);
    contentLayout->setContentsMargins(20, 18, 20, 20);
    contentLayout->setSpacing(12);
    auto* contentHeader = new QHBoxLayout;
    contentHeader->addWidget(makeSectionTitle(QStringLiteral("01"), tr("Content"), contentSection));
    contentHeader->addStretch();
    contentEditButton_ = new QToolButton(contentSection);
    contentEditButton_->setObjectName(QStringLiteral("sectionAction"));
    contentEditButton_->setText(tr("编辑"));
    contentEditButton_->setToolTip(tr("手动编辑剪贴板内容"));
    contentHeader->addWidget(contentEditButton_);
    contentLayout->addLayout(contentHeader);
    auto* contentHint = new QLabel(tr("来自剪贴板的原始内容"), contentSection);
    contentHint->setObjectName(QStringLiteral("sectionHint"));
    contentLayout->addWidget(contentHint);
    contentEdit_ = new QTextEdit(contentSection);
    contentEdit_->setObjectName(QStringLiteral("contentEdit"));
    contentEdit_->setReadOnly(true);
    contentEdit_->setPlaceholderText(tr("双击 Ctrl+C 后，内容会出现在这里。"));
    contentEdit_->setMinimumHeight(130);
    contentEdit_->setMaximumHeight(220);
    contentLayout->addWidget(contentEdit_);
    layout->addWidget(contentSection);

    intentSection_ = makeSection(centralWidget);
    auto* intentLayout = new QVBoxLayout(intentSection_);
    intentLayout->setContentsMargins(20, 18, 20, 20);
    intentLayout->setSpacing(12);
    auto* intentHeader = new QHBoxLayout;
    intentHeader->addWidget(makeSectionTitle(QStringLiteral("02"), tr("Intent"), intentSection_));
    intentHeader->addStretch();
    auto* refreshIntentButton = new QToolButton(intentSection_);
    refreshIntentButton->setObjectName(QStringLiteral("sectionAction"));
    refreshIntentButton->setText(tr("重新识别"));
    refreshIntentButton->setToolTip(tr("根据当前 Content 重新识别意图"));
    intentHeader->addWidget(refreshIntentButton);
    intentLayout->addLayout(intentHeader);
    intentLoadingLabel_ = new QLabel(tr("正在理解内容并匹配可执行能力…"), intentSection_);
    intentLoadingLabel_->setObjectName(QStringLiteral("loadingLabel"));
    intentLayout->addWidget(intentLoadingLabel_);
    intentProgress_ = new QProgressBar(intentSection_);
    intentProgress_->setRange(0, 0);
    intentProgress_->setTextVisible(false);
    intentProgress_->setFixedHeight(4);
    intentLayout->addWidget(intentProgress_);
    intentOptions_ = new QFrame(intentSection_);
    intentOptions_->setObjectName(QStringLiteral("optionsFrame"));
    intentOptionsLayout_ = new QVBoxLayout(intentOptions_);
    intentOptionsLayout_->setContentsMargins(0, 0, 0, 0);
    intentOptionsLayout_->setSpacing(8);
    intentLayout->addWidget(intentOptions_);
    intentSection_->hide();
    layout->addWidget(intentSection_);

    resultSection_ = makeSection(centralWidget);
    auto* resultLayout = new QVBoxLayout(resultSection_);
    resultLayout->setContentsMargins(20, 18, 20, 20);
    resultLayout->setSpacing(12);
    auto* resultHeader = new QHBoxLayout;
    resultHeader->addWidget(makeSectionTitle(QStringLiteral("03"), tr("Result"), resultSection_));
    resultHeader->addStretch();
    auto* regenerateResultButton = new QToolButton(resultSection_);
    regenerateResultButton->setObjectName(QStringLiteral("sectionAction"));
    regenerateResultButton->setText(tr("重新生成"));
    regenerateResultButton->setToolTip(tr("重新执行当前选中的 AI 功能"));
    resultHeader->addWidget(regenerateResultButton);
    resultLayout->addLayout(resultHeader);
    resultLoadingLabel_ = new QLabel(tr("正在执行所选 AI 功能…"), resultSection_);
    resultLoadingLabel_->setObjectName(QStringLiteral("loadingLabel"));
    resultLayout->addWidget(resultLoadingLabel_);
    resultProgress_ = new QProgressBar(resultSection_);
    resultProgress_->setRange(0, 0);
    resultProgress_->setTextVisible(false);
    resultProgress_->setFixedHeight(4);
    resultLayout->addWidget(resultProgress_);
    resultScrollArea_ = new QScrollArea(resultSection_);
    resultScrollArea_->setObjectName(QStringLiteral("resultScrollArea"));
    resultScrollArea_->setWidgetResizable(true);
    resultScrollArea_->setFrameShape(QFrame::NoFrame);
    resultScrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    resultScrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    resultScrollArea_->setMaximumHeight(280);
    resultContent_ = new QFrame(resultScrollArea_);
    resultContent_->setObjectName(QStringLiteral("resultContent"));
    resultContentLayout_ = new QVBoxLayout(resultContent_);
    resultContentLayout_->setContentsMargins(0, 0, 4, 0);
    resultContentLayout_->setSpacing(10);
    resultScrollArea_->setWidget(resultContent_);
    resultLayout->addWidget(resultScrollArea_);
    resultSection_->hide();
    layout->addWidget(resultSection_);
    layout->addStretch();

    connect(contentEditButton_, &QToolButton::clicked, this, [this] {
        const bool beginEditing = contentEdit_->isReadOnly();
        contentEdit_->setReadOnly(!beginEditing);
        contentEditButton_->setText(beginEditing ? tr("完成") : tr("编辑"));
        if (beginEditing) {
            statusLabel_->setText(tr("编辑中"));
            contentEdit_->setFocus();
            QTextCursor cursor = contentEdit_->textCursor();
            cursor.movePosition(QTextCursor::End);
            contentEdit_->setTextCursor(cursor);
        } else {
            statusLabel_->setText(tr("已编辑"));
        }
    });
    connect(refreshIntentButton, &QToolButton::clicked, this, [this] {
        if (!contentEdit_->toPlainText().trimmed().isEmpty()) beginIntentRecognition();
    });

    intentButtonGroup_ = new QButtonGroup(this);
    intentButtonGroup_->setExclusive(true);
    connect(regenerateResultButton, &QToolButton::clicked, this, [this] {
        if (!currentIntent_.isEmpty()) beginFunctionExecution(true);
    });

    inferenceClient_ = new InferenceClient(this);
    connect(inferenceClient_, &InferenceClient::intentsReady, this, [this](const QStringList& intents) {
        showIntentOptions(intents);
    });
    connect(inferenceClient_, &InferenceClient::inferenceError, this, [this](const QString& message) {
        intentProgress_->hide();
        intentLoadingLabel_->setText(tr("识别失败：%1").arg(message));
        intentLoadingLabel_->show();
        statusLabel_->setText(tr("模型错误"));
        updateExpandedSize();
    });

    resultTimer_ = new QTimer(this);
    resultTimer_->setSingleShot(true);
    resultTimer_->setInterval(1100);
    connect(resultTimer_, &QTimer::timeout, this, [this] { showResults(); });

    setCentralWidget(centralWidget);
    setStyleSheet(QStringLiteral(R"(
        #centralWidget { background-color: #f3f5f8; }
        #titleLabel { color: #172033; font-size: 25px; font-weight: 650; }
        #subtitleLabel, #sectionHint { color: #798394; font-size: 13px; }
        #statusLabel { color: #2457a7; background-color: #e1ecff; border-radius: 16px; font-size: 12px; font-weight: 600; }
        QFrame#section { background-color: white; border: 1px solid #dfe4eb; border-radius: 14px; }
        #sectionTitle { color: #182230; font-size: 17px; font-weight: 650; }
        #contentEdit { color: #182230; background-color: #f8fafc; border: 1px solid #e2e7ee; border-radius: 9px; padding: 12px; font-size: 14px; selection-background-color: #b9cff7; }
        #loadingLabel { color: #52606d; font-size: 13px; }
        QProgressBar { background-color: #e6ebf2; border: none; border-radius: 2px; }
        QProgressBar::chunk { background-color: #3d72d7; border-radius: 2px; }
        QToolButton#sectionAction { color: #2457a7; background-color: #eef4ff; border: 1px solid #cddcf5; border-radius: 7px; padding: 6px 11px; font-size: 12px; font-weight: 600; }
        QToolButton#sectionAction:hover { background-color: #e1ecff; border-color: #9bb8e9; }
        QToolButton#sectionAction:pressed { background-color: #d5e4fb; }
        QToolButton#intentButton { color: #344054; background-color: #f8fafc; border: 1px solid #d9e0e9; border-radius: 9px; padding: 11px 14px; font-size: 14px; text-align: left; }
        QToolButton#intentButton:hover { border-color: #8eace3; background-color: #f2f6fd; }
        QToolButton#intentButton:checked { color: #174ea6; border: 1px solid #6790da; background-color: #eaf1ff; font-weight: 600; }
        QFrame#resultCard { background-color: #f8fafc; border: 1px solid #e2e7ee; border-radius: 9px; }
        QLabel#resultTitle { color: #2457a7; font-size: 13px; font-weight: 650; }
        QLabel#resultBody { color: #344054; font-size: 14px; }
    )"));
}

void MainWindow::showClipboardText(const QString& text)
{
    contentEdit_->setReadOnly(true);
    contentEditButton_->setText(tr("编辑"));
    contentEdit_->setPlainText(text);
    statusLabel_->setText(tr("已拾取"));
    beginIntentRecognition();
    showPanel();
}

void MainWindow::showPanel()
{
    showNormal();
    updateExpandedSize();
#ifdef Q_OS_WIN
    const HWND handle = reinterpret_cast<HWND>(winId());
    const HWND foreground = GetForegroundWindow();
    const DWORD currentThread = GetCurrentThreadId();
    const DWORD foregroundThread = foreground ? GetWindowThreadProcessId(foreground, nullptr) : 0;
    const bool attached = foregroundThread != 0 && foregroundThread != currentThread
        && AttachThreadInput(currentThread, foregroundThread, TRUE);

    SetWindowPos(handle, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetWindowPos(handle, HWND_NOTOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    BringWindowToTop(handle);
    SetForegroundWindow(handle);
    SetActiveWindow(handle);
    SetFocus(handle);

    if (attached) AttachThreadInput(currentThread, foregroundThread, FALSE);
#else
    raise();
    activateWindow();
#endif
}

void MainWindow::beginIntentRecognition()
{
    resultTimer_->stop();
    clearLayout(intentOptionsLayout_);
    intentButtons_.clear();
    resultCache_.clear();
    currentIntent_.clear();
    clearLayout(resultContentLayout_);
    intentOptions_->hide();
    intentLoadingLabel_->setText(tr("正在由 Qwen3-0.6B 识别意图…"));
    intentLoadingLabel_->show();
    intentProgress_->show();
    intentSection_->show();
    resultSection_->hide();
    statusLabel_->setText(tr("识别中"));
    inferenceClient_->recognizeIntents(contentEdit_->toPlainText());
    updateExpandedSize();
}

void MainWindow::showIntentOptions(const QStringList& options)
{
    intentLoadingLabel_->hide();
    intentProgress_->hide();

    for (const QString& option : options) {
        auto* button = new QToolButton(intentOptions_);
        button->setObjectName(QStringLiteral("intentButton"));
        button->setText(QStringLiteral("○  %1").arg(option));
        button->setCheckable(true);
        button->setProperty("intentName", option);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        intentButtonGroup_->addButton(button);
        connect(button, &QToolButton::toggled, this, [this, button](bool checked) {
            const QString intent = button->property("intentName").toString();
            button->setText(QStringLiteral("%1  %2").arg(
                checked ? QStringLiteral("●") : QStringLiteral("○"), intent));
            if (checked) selectIntent(intent);
        });
        intentButtons_.append(button);
        intentOptionsLayout_->addWidget(button);
    }
    intentOptions_->show();
    statusLabel_->setText(tr("请选择"));
    updateExpandedSize();
}

void MainWindow::selectIntent(const QString& intent)
{
    currentIntent_ = intent;
    beginFunctionExecution(false);
}

void MainWindow::beginFunctionExecution(bool forceRegeneration)
{
    if (currentIntent_.isEmpty()) return;
    resultTimer_->stop();
    if (!forceRegeneration && resultCache_.contains(currentIntent_)) {
        renderResult(currentIntent_, resultCache_.value(currentIntent_));
        return;
    }

    clearLayout(resultContentLayout_);
    resultScrollArea_->hide();
    resultLoadingLabel_->show();
    resultProgress_->show();
    resultSection_->show();
    statusLabel_->setText(forceRegeneration ? tr("重新生成中") : tr("执行中"));
    resultTimer_->start();
    updateExpandedSize();
}

void MainWindow::showResults()
{
    if (currentIntent_.isEmpty()) return;
    const QString source = contentEdit_->toPlainText().simplified();
    QString body;
    if (currentIntent_ == tr("总结要点")) {
        body = tr("核心内容：%1%2").arg(source.left(100), source.size() > 100 ? QStringLiteral("…") : QString());
    } else if (currentIntent_ == tr("润色改写")) {
        body = tr("建议改写：%1").arg(source);
    } else if (currentIntent_ == tr("翻译为英文")) {
        body = tr("[模拟翻译] English version of the selected content will appear here.");
    } else if (currentIntent_ == tr("提取行动项")) {
        body = tr("• 确认文本中的目标与责任人\n• 明确下一步行动与完成时间");
    } else {
        body = tr("建议回复：收到，我已了解以上内容，会按重点继续跟进并及时反馈。");
    }
    resultCache_.insert(currentIntent_, body);
    renderResult(currentIntent_, body);
}

void MainWindow::renderResult(const QString& intent, const QString& body)
{
    clearLayout(resultContentLayout_);
    auto* card = new QFrame(resultContent_);
    card->setObjectName(QStringLiteral("resultCard"));
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(14, 12, 14, 14);
    cardLayout->setSpacing(7);
    auto* title = new QLabel(intent, card);
    title->setObjectName(QStringLiteral("resultTitle"));
    cardLayout->addWidget(title);
    auto* result = new QLabel(body, card);
    result->setObjectName(QStringLiteral("resultBody"));
    result->setWordWrap(true);
    result->setTextInteractionFlags(Qt::TextSelectableByMouse);
    cardLayout->addWidget(result);
    resultContentLayout_->addWidget(card);

    resultLoadingLabel_->hide();
    resultProgress_->hide();
    resultContent_->adjustSize();
    const int resultHeight = qBound(90, resultContentLayout_->sizeHint().height(), 280);
    resultScrollArea_->setFixedHeight(resultHeight);
    resultScrollArea_->show();
    resultSection_->show();
    statusLabel_->setText(tr("已完成"));
    updateExpandedSize();
}

void MainWindow::updateExpandedSize()
{
    QTimer::singleShot(0, this, [this] {
        if (!centralWidget()) return;
        centralWidget()->layout()->activate();
        QScreen* targetScreen = screen();
        if (!targetScreen) targetScreen = QGuiApplication::primaryScreen();
        const QRect available = targetScreen
            ? targetScreen->availableGeometry().adjusted(24, 24, -24, -24)
            : QRect(0, 0, 1200, 900);
        const int desiredHeight = centralWidget()->sizeHint().height();
        resize(width(), qBound(minimumHeight(), desiredHeight, available.height()));
        if (frameGeometry().bottom() > available.bottom())
            move(x(), available.bottom() - frameGeometry().height() + 1);
        if (frameGeometry().top() < available.top())
            move(x(), available.top());
    });
}

void MainWindow::clearLayout(QVBoxLayout* layout)
{
    while (QLayoutItem* item = layout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    hide();
    event->ignore();
}
