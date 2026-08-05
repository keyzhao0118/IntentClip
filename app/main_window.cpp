#include "main_window.h"
#include "circular_spinner.h"

#include "function_settings_dialog.h"
#include "inference_client.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QFrame>
#include <QApplication>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QScrollArea>
#include <QScreen>
#include <QSignalBlocker>
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

constexpr int kContentViewportHeight = 191;
constexpr int kResultViewportHeight = 180;
namespace {
QLabel* makeSectionTitle(const QString& title, QWidget* parent)
{
    auto* label = new QLabel(title, parent);
    label->setObjectName(QStringLiteral("sectionTitle"));
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
    : QDialog(parent)
{
    setWindowTitle(tr("IntentClip · 拾意"));
    setObjectName(QStringLiteral("mainDialog"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setWindowFlag(Qt::MSWindowsFixedSizeDialogHint, true);
    resize(700, 500);
    // 纯内容区（结果区隐藏）的自然高度约 320px，最小高度不应把它撑出底部留白；
    // 结果区显示时 sizeHint 约 600px+，不受此下限影响。
    setMinimumSize(600, 300);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(24, 22, 24, 24);
    rootLayout->setSpacing(0);

    auto* contentWidget = new QWidget(this);
    contentWidget->setObjectName(QStringLiteral("contentWidget"));
    auto* layout = new QVBoxLayout(contentWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    rootLayout->addWidget(contentWidget);

    auto* contentSection = makeSection(contentWidget);
    auto* contentLayout = new QVBoxLayout(contentSection);
    contentLayout->setContentsMargins(22, 20, 22, 22);
    contentLayout->setSpacing(10);
    auto* contentHeader = new QHBoxLayout;
    contentHeader->addWidget(makeSectionTitle(tr("Content"), contentSection));
    contentHeader->addStretch();
    contentLayout->addLayout(contentHeader);
    contentEdit_ = new QTextEdit(contentSection);
    contentEdit_->setObjectName(QStringLiteral("contentEdit"));
    contentEdit_->setReadOnly(true);
    contentEdit_->setPlaceholderText(tr("双击 Ctrl+C 后，内容会出现在这里。"));
    contentEdit_->setFixedHeight(kContentViewportHeight);
    contentEdit_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    contentEdit_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    contentLayout->addWidget(contentEdit_);
    layout->addWidget(contentSection);

    resultSection_ = makeSection(contentWidget);
    auto* resultLayout = new QVBoxLayout(resultSection_);
    resultLayout->setContentsMargins(22, 20, 22, 22);
    resultLayout->setSpacing(10);
    auto* resultHeader = new QHBoxLayout;
    functionSelector_ = new QComboBox(resultSection_);
    functionSelector_->setObjectName(QStringLiteral("functionSelector"));
    functionSelector_->setToolTip(tr("选择要执行的 AI 功能；切换后立即按当前 Content 重新生成"));
    functionSelector_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    resultHeader->addWidget(functionSelector_);
    resultHeader->addStretch();
    auto* regenerateResultButton = new QToolButton(resultSection_);
    regenerateResultButton->setObjectName(QStringLiteral("sectionAction"));
    regenerateResultButton->setText(tr("重新生成"));
    regenerateResultButton->setToolTip(tr("重新执行当前选中的 AI 功能"));
    resultHeader->addWidget(regenerateResultButton);
    resultLayout->addLayout(resultHeader);
    resultLoadingLabel_ = new QLabel(tr("正在执行所选 AI 功能…"), resultSection_);
    resultLoadingContainer_ = new QFrame(resultSection_);
    resultLoadingContainer_->setFixedHeight(kResultViewportHeight);
    auto* resultLoadingLayout = new QVBoxLayout(resultLoadingContainer_);
    resultLoadingLayout->setContentsMargins(14, 12, 14, 14);
    resultLoadingLayout->setSpacing(12);
    resultLoadingLayout->addStretch();
    resultLoadingLabel_->setObjectName(QStringLiteral("loadingLabel"));
    resultLoadingLabel_->setAlignment(Qt::AlignCenter);
    resultLoadingLabel_->setWordWrap(true);
    resultLoadingLayout->addWidget(resultLoadingLabel_);
    resultSpinner_ = new CircularSpinner(resultLoadingContainer_);
    resultLoadingLayout->addWidget(resultSpinner_, 0, Qt::AlignHCenter);
    resultLoadingLayout->addStretch();
    resultLayout->addWidget(resultLoadingContainer_);
    resultScrollArea_ = new QScrollArea(resultSection_);
    resultScrollArea_->setObjectName(QStringLiteral("resultScrollArea"));
    resultScrollArea_->setWidgetResizable(true);
    resultScrollArea_->setFrameShape(QFrame::NoFrame);
    resultScrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    resultScrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    resultScrollArea_->setFixedHeight(kResultViewportHeight);
    resultContent_ = new QFrame(resultScrollArea_);
    resultContent_->setObjectName(QStringLiteral("resultContent"));
    resultContentLayout_ = new QVBoxLayout(resultContent_);
    resultContentLayout_->setContentsMargins(0, 0, 4, 0);
    resultContentLayout_->setSpacing(10);
    resultScrollArea_->setWidget(resultContent_);
    resultLayout->addWidget(resultScrollArea_);
    layout->addWidget(resultSection_);
    layout->addStretch();

    promptConfig_ = IntentPromptConfig::load();
    QApplication::instance()->installEventFilter(this);
    contentEdit_->installEventFilter(this);
    contentEdit_->viewport()->installEventFilter(this);
    connect(regenerateResultButton, &QToolButton::clicked, this, [this] {
        if (!currentIntent_.isEmpty()) beginFunctionExecution(true);
    });
    connect(functionSelector_, &QComboBox::currentIndexChanged, this,
        [this](int index) {
            switchFunction(functionSelector_->itemData(index).toString());
        });

    inferenceClient_ = new InferenceClient(this);
    connect(contentEdit_, &QTextEdit::textChanged,
        this, &MainWindow::invalidateCacheForContentChange);
    connect(inferenceClient_, &InferenceClient::resultUpdated, this,
        [this](const QString& intentId, const QString& partialResult) {
            if (intentId != currentIntent_ || partialResult.isEmpty()) return;
            if (streamingIntent_ != intentId || !resultBodyLabel_) {
                streamingIntent_ = intentId;
                renderResult(partialResult);
            } else {
                resultBodyLabel_->setText(partialResult);
                resultContent_->adjustSize();
                resultScrollArea_->show();
                resultSection_->show();
                updateExpandedSize();
            }
        });
    connect(inferenceClient_, &InferenceClient::resultReady, this,
        [this](const QString& intentId, const QString& result) {
            resultCache_.insert(intentId, result);
            if (intentId != currentIntent_) return;
            streamingIntent_.clear();
            renderResult(result);
        });
    connect(inferenceClient_, &InferenceClient::executionError, this,
        [this](const QString& intentId, const QString& message) {
            if (intentId != currentIntent_) return;
            showResultStatus(tr("执行失败：%1").arg(message), false);
            updateExpandedSize();
        });
    connect(inferenceClient_, &InferenceClient::inferenceError, this, [this](const QString& message) {
        showResultStatus(tr("本地模型错误：%1").arg(message), false);
        updateExpandedSize();
    });

    refreshFunctionSelection(false);

    setStyleSheet(QStringLiteral(R"(
        * { font-family: "Segoe UI", "Microsoft YaHei UI"; }
        QDialog#mainDialog { background-color: #f9f9f9; }
        #contentWidget { background: transparent; border: none; }
        QFrame#section {
            background-color: #ffffff;
            border: 1px solid #e4e4e4;
            border-radius: 13px;
        }
        #sectionTitle { color: #1a1c1f; font-size: 16px; font-weight: 700; }
        #contentEdit {
            color: #1a1c1f; background-color: transparent;
            border: 1px solid #e4e4e4; border-radius: 10px;
            padding: 12px 14px; font-size: 14px;
            selection-color: #1a1c1f; selection-background-color: #e8e9ea;
        }
        #contentEdit:focus { background-color: transparent; border: 1px solid #75777a; }
        #loadingLabel { color: #5f6062; font-size: 13px; }
        QToolButton#sectionAction {
            color: #1a1c1f; background-color: #f0f1f2;
            border: 1px solid #e4e4e4; border-radius: 8px;
            padding: 6px 12px; font-size: 12px; font-weight: 650;
        }
        QToolButton#sectionAction:hover { color: #1a1c1f; background-color: #e6e7e8; border-color: #e4e4e4; }
        QToolButton#sectionAction:pressed { background-color: #dcddde; }
        QComboBox#functionSelector {
            color: #1a1c1f; background-color: #f0f1f2;
            border: 1px solid #e4e4e4; border-radius: 8px;
            padding: 5px 12px; font-size: 14px; font-weight: 700;
            min-width: 180px;
        }
        QComboBox#functionSelector:hover { background-color: #e6e7e8; border-color: #e4e4e4; }
        QComboBox#functionSelector::drop-down { border: none; width: 26px; }
        QComboBox#functionSelector::down-arrow {
            image: url(:/resources/down-arrow.png);
            width: 12px;
            height: 7px;
            margin-right: 7px;
        }
        QComboBox#functionSelector QAbstractItemView {
            color: #1a1c1f; background: #ffffff;
            border: 1px solid #e4e4e4; border-radius: 8px;
            selection-background-color: #f0f1f2; padding: 4px;
            outline: none;
        }
        QFrame#resultCard { background-color: #f7f7f7; border: 1px solid #e4e4e4; border-radius: 10px; }
        QLabel#resultBody { color: #1a1c1f; font-size: 14px; }
        QScrollArea#resultScrollArea { background: transparent; border: none; }
        QFrame#resultContent { background: transparent; border: none; }
        QScrollArea#resultScrollArea > QWidget#qt_scrollarea_viewport { background: transparent; }
        QScrollBar:vertical { background: transparent; width: 8px; margin: 2px 0; }
        QScrollBar::handle:vertical { background: #c9cbcb; border-radius: 4px; min-height: 28px; }
        QScrollBar::handle:vertical:hover { background: #b0b2b4; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QMenu { color: #1a1c1f; background: #ffffff; border: 1px solid #e4e4e4; border-radius: 9px; padding: 6px; }
        QMenu::item { padding: 8px 22px 8px 12px; border-radius: 6px; }
        QMenu::item:selected { color: #1a1c1f; background: #f0f1f2; }
        QMenu::separator { height: 1px; background: #e4e4e4; margin: 5px 8px; }
    )"));
}

void MainWindow::showClipboardText(const QString& text)
{
    invalidateCacheForContentChange();
    contentEdit_->setReadOnly(true);
    const QSignalBlocker blocker(contentEdit_);
    contentEdit_->setPlainText(text);
    currentIntent_.clear();
    refreshFunctionSelection(true);
    showPanel();
}

void MainWindow::showPanel()
{
    if (functionSelector_->count() == 0) refreshFunctionSelection(false);
    if (contentEdit_->toPlainText().trimmed().isEmpty()) {
        resultSection_->hide();
        updateExpandedSize();
    }
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

void MainWindow::openFunctionSettings()
{
    FunctionSettingsDialog dialog(this);
    dialog.exec();
    refreshFunctionSelection(false);
}

void MainWindow::invalidateCacheForContentChange()
{
    const bool hadResultState = !resultCache_.isEmpty() || resultSection_->isVisible();
    resultCache_.clear();
    if (inferenceClient_) inferenceClient_->invalidateExecution();
    if (!hadResultState) return;

    clearLayout(resultContentLayout_);
    showResultStatus(tr("Content 已变化，请重新生成当前功能的结果。"), false);
    updateExpandedSize();
}

void MainWindow::refreshFunctionSelection(bool executeSelection)
{
    QString configError;
    promptConfig_ = IntentPromptConfig::load(&configError);
    const QString previousSelection = currentIntent_;
    if (!configError.isEmpty()) {
        functionSelector_->clear();
        currentIntent_.clear();
        showResultStatus(tr("配置错误：%1").arg(configError), false);
        updateExpandedSize();
        return;
    }

    QString selected = previousSelection;
    if (!promptConfig_.findById(selected)) {
        selected = promptConfig_.findById(promptConfig_.defaultFunctionId)
            ? promptConfig_.defaultFunctionId
            : (promptConfig_.intents.isEmpty() ? QString() : promptConfig_.intents.first().id);
    }

    const QSignalBlocker blocker(functionSelector_);
    functionSelector_->clear();
    for (const IntentDefinition& definition : promptConfig_.intents)
        functionSelector_->addItem(definition.name, definition.id);
    const int index = functionSelector_->findData(selected);
    functionSelector_->setCurrentIndex(index >= 0 ? index : 0);
    currentIntent_ = functionSelector_->currentIndex() >= 0
        ? functionSelector_->currentData().toString() : QString();

    if (currentIntent_.isEmpty()) {
        clearLayout(resultContentLayout_);
        showResultStatus(tr("尚未配置功能，请在托盘“功能设置”中添加功能。"), false);
        updateExpandedSize();
        return;
    }

    if (executeSelection) {
        beginFunctionExecution(false);
        return;
    }
    if (previousSelection != currentIntent_ && resultSection_->isVisible()) {
        if (resultCache_.contains(currentIntent_)) {
            renderResult(resultCache_.value(currentIntent_));
        } else {
            clearLayout(resultContentLayout_);
            showResultStatus(tr("功能已切换，请重新生成。"), false);
            updateExpandedSize();
        }
    }
}

void MainWindow::switchFunction(const QString& functionId)
{
    if (functionId.isEmpty() || functionId == currentIntent_) return;
    currentIntent_ = functionId;
    beginFunctionExecution(false);
}

void MainWindow::beginFunctionExecution(bool forceRegeneration)
{
    if (currentIntent_.isEmpty()) return;
    if (contentEdit_->toPlainText().trimmed().isEmpty()) {
        contentEdit_->setReadOnly(false);
        contentEdit_->setFocus();
        resultSection_->hide();
        updateExpandedSize();
        return;
    }
    if (!forceRegeneration && resultCache_.contains(currentIntent_)) {
        renderResult(resultCache_.value(currentIntent_));
        return;
    }

    const IntentDefinition* definition = promptConfig_.findById(currentIntent_);
    if (!definition || definition->actionPrompt.trimmed().isEmpty()) {
        showResultStatus(tr("功能执行提示词为空。"), false);
        updateExpandedSize();
        return;
    }

    streamingIntent_ = currentIntent_;
    resultBodyLabel_.clear();
    clearLayout(resultContentLayout_);
    showResultStatus(tr("正在执行“%1”…").arg(definition->name), true);
    inferenceClient_->executeFunction(
        contentEdit_->toPlainText(), currentIntent_, definition->actionPrompt);
    updateExpandedSize();
}

void MainWindow::renderResult(const QString& body)
{
    clearLayout(resultContentLayout_);
    auto* card = new QFrame(resultContent_);
    card->setObjectName(QStringLiteral("resultCard"));
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(14, 12, 14, 14);
    cardLayout->setSpacing(7);
    auto* resultLabel = new QLabel(body, card);
    resultLabel->setObjectName(QStringLiteral("resultBody"));
    resultLabel->setWordWrap(true);
    resultLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    cardLayout->addWidget(resultLabel);
    resultBodyLabel_ = resultLabel;
    resultContentLayout_->addWidget(card);

    resultSpinner_->stopSpinning();
    resultLoadingContainer_->hide();
    resultContent_->adjustSize();
    resultScrollArea_->show();
    resultSection_->show();
    updateExpandedSize();
}

bool MainWindow::isInsideContentEdit(QWidget* widget) const
{
    for (QWidget* current = widget; current; current = current->parentWidget()) {
        if (current == contentEdit_) return true;
    }
    return false;
}

void MainWindow::finishContentEdit()
{
    if (contentEdit_->isReadOnly()) return;
    contentEdit_->setReadOnly(true);
    if (contentEdit_->toPlainText().trimmed().isEmpty()) return;
    beginFunctionExecution(false);
}

void MainWindow::enterContentEditMode()
{
    if (!contentEdit_->isReadOnly()) return;
    contentEdit_->setReadOnly(false);
    contentEdit_->setFocus();
    QTextCursor cursor = contentEdit_->textCursor();
    cursor.movePosition(QTextCursor::End);
    contentEdit_->setTextCursor(cursor);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonDblClick
        && (watched == contentEdit_ || watched == contentEdit_->viewport())) {
        enterContentEditMode();
    } else if (event->type() == QEvent::MouseButtonPress
        && !contentEdit_->isReadOnly()) {
        if (auto* clicked = qobject_cast<QWidget*>(watched))
            if (!isInsideContentEdit(clicked)) finishContentEdit();
    } else if (event->type() == QEvent::FocusOut && watched == contentEdit_
        && !contentEdit_->isReadOnly()) {
        QTimer::singleShot(0, this, [this] {
            if (contentEdit_->isReadOnly()) return;
            finishContentEdit();
        });
    }
    return QDialog::eventFilter(watched, event);
}

void MainWindow::showResultStatus(const QString& message, bool loading)
{
    resultLoadingLabel_->setText(message);
    resultLoadingLabel_->show();
    if (loading) resultSpinner_->startSpinning();
    else resultSpinner_->stopSpinning();
    resultScrollArea_->hide();
    resultLoadingContainer_->show();
    resultSection_->show();
}

void MainWindow::updateExpandedSize()
{
    QTimer::singleShot(0, this, [this] {
        if (!layout()) return;
        layout()->activate();
        QScreen* targetScreen = screen();
        if (!targetScreen) targetScreen = QGuiApplication::primaryScreen();
        const QRect available = targetScreen
            ? targetScreen->availableGeometry().adjusted(24, 24, -24, -24)
            : QRect(0, 0, 1200, 900);
        const int desiredHeight = sizeHint().height();
        resize(width(), qBound(minimumHeight(), desiredHeight, available.height()));
        if (frameGeometry().bottom() > available.bottom())
            move(x(), available.bottom() - frameGeometry().height() + 1);
        if (frameGeometry().top() < available.top())
            move(x(), available.top());
    });
}

void MainWindow::clearLayout(QLayout* layout)
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
