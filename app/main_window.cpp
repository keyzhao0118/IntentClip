#include "main_window.h"
#include "circular_spinner.h"

#include "function_settings_dialog.h"
#include "inference_client.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QFrame>
#include <QApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QScrollArea>
#include <QScreen>
#include <QSettings>
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
    setWindowTitle(tr("IntentClip"));
    setObjectName(QStringLiteral("mainDialog"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setWindowFlag(Qt::MSWindowsFixedSizeDialogHint, true);
    setWindowFlag(Qt::WindowStaysOnTopHint, true);
    resize(700, 500);
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
    contentEdit_->setPlaceholderText(tr("After double-pressing Ctrl+C, the copied content will appear here. Double-click here to edit it."));
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
    functionSelector_->setToolTip(tr("Select the AI function to run; switching regenerates immediately using the current Content"));
    functionSelector_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    resultHeader->addWidget(functionSelector_);
    resultHeader->addStretch();
    auto* regenerateResultButton = new QToolButton(resultSection_);
    regenerateResultButton->setObjectName(QStringLiteral("sectionAction"));
    regenerateResultButton->setText(tr("Regenerate"));
    regenerateResultButton->setToolTip(tr("Re-run the currently selected AI function"));
    resultHeader->addWidget(regenerateResultButton);
    copyResultButton_ = new QToolButton(resultSection_);
    copyResultButton_->setObjectName(QStringLiteral("sectionAction"));
    copyResultButton_->setText(tr("Copy Result"));
    copyResultButton_->setToolTip(tr("Copy the AI result to the clipboard"));
    copyResultButton_->setEnabled(false);
    resultHeader->addWidget(copyResultButton_);
    resultLayout->addLayout(resultHeader);
    resultLoadingLabel_ = new QLabel(tr("Running the selected AI function…"), resultSection_);
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
    connect(copyResultButton_, &QToolButton::clicked, this, [this] {
        if (resultBodyLabel_) {
            QApplication::clipboard()->setText(resultBodyLabel_->text());
            copyResultButton_->setText(tr("Copied"));
            QTimer::singleShot(1500, this, [this] {
                copyResultButton_->setText(tr("Copy Result"));
            });
        }
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
            showResultStatus(tr("Execution failed: %1").arg(message), false);
            updateExpandedSize();
        });
    connect(inferenceClient_, &InferenceClient::inferenceError, this, [this](const QString& message) {
        showResultStatus(tr("Local model error: %1").arg(message), false);
        updateExpandedSize();
    });

    refreshFunctionSelection(false);
    restoreWindowGeometry();

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
        QToolButton#sectionAction {
            color: #5f6062; background-color: #f7f7f7;
            border: 1px solid #e4e4e4; border-radius: 8px;
            padding: 6px 14px; font-size: 12px; font-weight: 600;
        }
        QToolButton#sectionAction:hover {
            color: #1a1c1f; background-color: #f0f1f2;
        }
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

    // WindowStaysOnTopHint 已使面板保持置顶，这里仅确保唤起时位于置顶层顶部
    SetWindowPos(handle, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
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
    copyResultButton_->setEnabled(false);
    if (inferenceClient_) inferenceClient_->invalidateExecution();
    if (!hadResultState) return;

    clearLayout(resultContentLayout_);
    showResultStatus(tr("Content has changed; please regenerate the result of the current function."), false);
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
        showResultStatus(tr("Configuration error: %1").arg(configError), false);
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
        showResultStatus(tr("No functions configured. Add one from Function Settings in the tray."), false);
        updateExpandedSize();
        return;
    }

    if (executeSelection) {
        beginFunctionExecution(false);
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
        showResultStatus(tr("The execution prompt of the function is empty."), false);
        updateExpandedSize();
        return;
    }

    streamingIntent_ = currentIntent_;
    copyResultButton_->setEnabled(false);
    resultBodyLabel_.clear();
    clearLayout(resultContentLayout_);
    showResultStatus(tr("Running \"%1\"…").arg(definition->name), true);
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
    copyResultButton_->setEnabled(true);
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
        QScreen* targetScreen = QGuiApplication::primaryScreen();
        const QRect available = targetScreen
            ? targetScreen->availableGeometry().adjusted(24, 24, -24, -24)
            : QRect(0, 0, 1200, 900);
        const int desiredHeight = sizeHint().height();
        resize(width(), qBound(minimumHeight(), desiredHeight, available.height()));
        layout()->activate();
        if (!firstShowPositioned_) {
            // 进程内首次显示：停靠主屏幕右下角
            firstShowPositioned_ = true;
            const int x = qMax(available.left(), available.right() - width() + 1);
            const int y = qMax(available.top(), available.bottom() - height() + 1);
            move(x, y);
        } else {
            // 后续保持用户最新位置，仅在窗口超出可用区域时收回到可视范围
            int newY = y();
            if (frameGeometry().bottom() > available.bottom())
                newY = available.bottom() - frameGeometry().height() + 1;
            if (newY < available.top()) newY = available.top();
            move(x(), newY);
        }
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
    saveWindowGeometry();
    hide();
    event->ignore();
}

void MainWindow::saveWindowGeometry()
{
    QSettings settings;
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
}

void MainWindow::restoreWindowGeometry()
{
    QSettings settings;
    const QByteArray geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    } else {
        resize(700, 500);
        QScreen* targetScreen = QGuiApplication::primaryScreen();
        if (targetScreen) {
            const QRect available = targetScreen->availableGeometry().adjusted(24, 24, -24, -24);
            move(available.right() - width() + 1, available.bottom() - height() + 1);
        }
    }
}