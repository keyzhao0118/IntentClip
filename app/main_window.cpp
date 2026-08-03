#include "main_window.h"

#include "inference_client.h"
#include "prompt_settings_dialog.h"

#include <QButtonGroup>
#include <QCloseEvent>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QMenu>
#include <QMessageBox>
#include <QProgressBar>
#include <QScrollArea>
#include <QScreen>
#include <QSignalBlocker>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif

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
    resize(760, 500);
    setMinimumSize(600, 460);

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

    intentSection_ = makeSection(contentWidget);
    auto* intentLayout = new QVBoxLayout(intentSection_);
    intentLayout->setContentsMargins(22, 20, 22, 22);
    intentLayout->setSpacing(10);
    auto* intentHeader = new QHBoxLayout;
    intentHeader->addWidget(makeSectionTitle(tr("Intent"), intentSection_));
    intentHeader->addStretch();
    auto* addFunctionButton = new QToolButton(intentSection_);
    addFunctionButton->setObjectName(QStringLiteral("sectionAction"));
    addFunctionButton->setText(tr("＋ 添加功能"));
    addFunctionButton->setToolTip(tr("新增一个始终显示在 Intent 区的功能"));
    intentHeader->addWidget(addFunctionButton);
    intentLayout->addLayout(intentHeader);
    auto* intentHint = new QLabel(tr("选择要执行的功能；面板打开时自动执行第一项。"), intentSection_);
    intentHint->setObjectName(QStringLiteral("sectionHint"));
    intentLayout->addWidget(intentHint);
    intentOptions_ = new QFrame(intentSection_);
    intentOptions_->setObjectName(QStringLiteral("optionsFrame"));
    intentOptionsLayout_ = new QGridLayout(intentOptions_);
    intentOptionsLayout_->setContentsMargins(0, 0, 0, 0);
    intentOptionsLayout_->setHorizontalSpacing(8);
    intentOptionsLayout_->setVerticalSpacing(8);
    for (int column = 0; column < 5; ++column) intentOptionsLayout_->setColumnStretch(column, 1);
    intentLayout->addWidget(intentOptions_);
    intentSection_->hide();
    layout->addWidget(intentSection_);

    resultSection_ = makeSection(contentWidget);
    auto* resultLayout = new QVBoxLayout(resultSection_);
    resultLayout->setContentsMargins(22, 20, 22, 22);
    resultLayout->setSpacing(10);
    auto* resultHeader = new QHBoxLayout;
    resultHeader->addWidget(makeSectionTitle(tr("Result"), resultSection_));
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

    promptConfig_ = IntentPromptConfig::load();
    connect(addFunctionButton, &QToolButton::clicked, this, &MainWindow::addFunction);
    connect(contentEditButton_, &QToolButton::clicked, this, [this] {
        const bool beginEditing = contentEdit_->isReadOnly();
        contentEdit_->setReadOnly(!beginEditing);
        contentEditButton_->setText(beginEditing ? tr("完成") : tr("编辑"));
        if (beginEditing) {
            contentEdit_->setFocus();
            QTextCursor cursor = contentEdit_->textCursor();
            cursor.movePosition(QTextCursor::End);
            contentEdit_->setTextCursor(cursor);
        }
    });
    intentButtonGroup_ = new QButtonGroup(this);
    intentButtonGroup_->setExclusive(true);
    connect(regenerateResultButton, &QToolButton::clicked, this, [this] {
        if (!currentIntent_.isEmpty()) beginFunctionExecution(true);
    });

    inferenceClient_ = new InferenceClient(this);
    connect(contentEdit_, &QTextEdit::textChanged,
        this, &MainWindow::invalidateCacheForContentChange);
    connect(inferenceClient_, &InferenceClient::resultReady, this,
        [this](const QString& intentId, const QString& result) {
            resultCache_.insert(intentId, result);
            if (intentId != currentIntent_) return;
            const IntentDefinition* definition = promptConfig_.findById(intentId);
            renderResult(definition ? definition->name : intentId, result);
        });
    connect(inferenceClient_, &InferenceClient::executionError, this,
        [this](const QString& intentId, const QString& message) {
            if (intentId != currentIntent_) return;
            resultProgress_->hide();
            resultLoadingLabel_->setText(tr("执行失败：%1").arg(message));
            resultLoadingLabel_->show();
            resultSection_->show();
            updateExpandedSize();
        });
    connect(inferenceClient_, &InferenceClient::inferenceError, this, [this](const QString& message) {
        resultProgress_->hide();
        resultLoadingLabel_->setText(tr("本地模型错误：%1").arg(message));
        resultLoadingLabel_->show();
        resultSection_->show();
        updateExpandedSize();
    });



    setStyleSheet(QStringLiteral(R"(
        * { font-family: "Segoe UI", "Microsoft YaHei UI"; }
        QDialog#mainDialog { background-color: #f6f7fb; }
        #contentWidget { background: transparent; border: none; }
        #sectionHint { color: #8a94a6; font-size: 12px; }
        QFrame#section {
            background-color: #ffffff;
            border: 1px solid #e4e7ec;
            border-radius: 13px;
        }
        #sectionTitle { color: #151b2b; font-size: 16px; font-weight: 700; }
        #contentEdit {
            color: #202939; background-color: #f8fafc;
            border: 1px solid #e1e6ed; border-radius: 10px;
            padding: 13px 14px; font-size: 14px;
            selection-color: #172033; selection-background-color: #cfd9ff;
        }
        #contentEdit:focus { background-color: #ffffff; border: 1px solid #7892ea; }
        #loadingLabel { color: #586174; font-size: 13px; }
        QProgressBar { background-color: #e8ebf2; border: none; border-radius: 2px; }
        QProgressBar::chunk { background-color: #526fd4; border-radius: 2px; }
        QToolButton#sectionAction {
            color: #3c56b5; background-color: #f2f4ff;
            border: 1px solid #dce2fb; border-radius: 8px;
            padding: 6px 12px; font-size: 12px; font-weight: 650;
        }
        QToolButton#sectionAction:hover { color: #2e46a4; background-color: #e8ecff; border-color: #c8d1f7; }
        QToolButton#sectionAction:pressed { background-color: #dfe5ff; }
        QToolButton#intentButton {
            color: #465064; background-color: #f8f9fc;
            border: 1px solid #e0e4eb; border-radius: 10px;
            padding: 11px 14px; font-size: 13px; font-weight: 550;
            text-align: center;
        }
        QToolButton#intentButton:hover { color: #334aa5; border-color: #aebced; background-color: #f3f5ff; }
        QToolButton#intentButton:checked { color: #ffffff; border-color: #526fd4; background-color: #526fd4; font-weight: 700; }
        QFrame#resultCard { background-color: #f8fafc; border: 1px solid #e2e6ed; border-radius: 10px; }
        QLabel#resultTitle { color: #3e58ba; font-size: 13px; font-weight: 700; }
        QLabel#resultBody { color: #344054; font-size: 14px; }
        QScrollArea#resultScrollArea { background: transparent; border: none; }
        QScrollBar:vertical { background: transparent; width: 8px; margin: 2px 0; }
        QScrollBar::handle:vertical { background: #c9ced8; border-radius: 4px; min-height: 28px; }
        QScrollBar::handle:vertical:hover { background: #aeb5c2; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QMenu { color: #273043; background: #ffffff; border: 1px solid #dfe3ea; padding: 6px; }
        QMenu::item { padding: 8px 22px 8px 12px; border-radius: 6px; }
        QMenu::item:selected { color: #334aa5; background: #edf1ff; }
        QMenu::separator { height: 1px; background: #eaecf0; margin: 5px 8px; }
    )"));
}

void MainWindow::showClipboardText(const QString& text)
{
    invalidateCacheForContentChange();
    contentEdit_->setReadOnly(true);
    contentEditButton_->setText(tr("编辑"));
    const QSignalBlocker blocker(contentEdit_);
    contentEdit_->setPlainText(text);
    showConfiguredFunctions();
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

void MainWindow::invalidateCacheForContentChange()
{
    resultCache_.clear();
    if (inferenceClient_) inferenceClient_->invalidateExecution();
    if (!intentSection_->isVisible()) return;

    clearLayout(resultContentLayout_);
    resultScrollArea_->hide();
    resultProgress_->hide();
    resultLoadingLabel_->setText(tr("Content 已变化，请重新生成当前功能的结果。"));
    resultLoadingLabel_->show();
    resultSection_->show();
    updateExpandedSize();
}

void MainWindow::refreshFunctionButtonsPreservingState()
{
    const QString previousSelection = currentIntent_;
    QString selectedFunctionId = previousSelection;
    if (!promptConfig_.findById(selectedFunctionId)) {
        selectedFunctionId = promptConfig_.findById(promptConfig_.defaultFunctionId)
            ? promptConfig_.defaultFunctionId
            : (promptConfig_.intents.isEmpty() ? QString() : promptConfig_.intents.first().id);
        currentIntent_ = selectedFunctionId;
    }

    clearLayout(intentOptionsLayout_);
    intentButtons_.clear();
    QStringList functionIds;
    for (const IntentDefinition& definition : promptConfig_.intents)
        functionIds.append(definition.id);
    const bool executeFirstFunction = previousSelection.isEmpty()
        && !selectedFunctionId.isEmpty();
    showFunctionOptions(functionIds, selectedFunctionId, executeFirstFunction);

    if (selectedFunctionId != previousSelection) {
        if (resultCache_.contains(selectedFunctionId)) {
            const IntentDefinition* definition = promptConfig_.findById(selectedFunctionId);
            renderResult(definition ? definition->name : selectedFunctionId,
                resultCache_.value(selectedFunctionId));
        } else {
            clearLayout(resultContentLayout_);
            resultScrollArea_->hide();
            resultProgress_->hide();
            resultLoadingLabel_->hide();
            resultSection_->hide();
        }
    }
}
bool MainWindow::saveFunctionConfig(const QString& successMessage)
{
    QString error;
    if (!promptConfig_.save(&error)) {
        QMessageBox::warning(this, tr("配置无效"), error);
        return false;
    }
    refreshFunctionButtonsPreservingState();
    return true;
}

void MainWindow::addFunction()
{
    IntentDefinition definition{
        QStringLiteral("function_") + QUuid::createUuid().toString(QUuid::Id128).left(16),
        tr("新功能"),
        tr("请填写一句话功能描述。"),
        tr("请说明点击功能后，本地模型应如何处理 Content 内容。")
    };
    PromptSettingsDialog dialog(definition, this);
    if (dialog.exec() != QDialog::Accepted) return;
    const IntentPromptConfig previousConfig = promptConfig_;
    promptConfig_.intents.append(dialog.definition());
    if (promptConfig_.defaultFunctionId.isEmpty())
        promptConfig_.defaultFunctionId = promptConfig_.intents.constLast().id;
    QString error;
    if (!promptConfig_.save(&error)) {
        promptConfig_ = previousConfig;
        QMessageBox::warning(this, tr("配置无效"), error);
        return;
    }

    refreshFunctionButtonsPreservingState();
}

void MainWindow::editFunction(const QString& functionId)
{
    for (IntentDefinition& definition : promptConfig_.intents) {
        if (definition.id != functionId) continue;
        PromptSettingsDialog dialog(definition, this);
        if (dialog.exec() != QDialog::Accepted) return;
        definition = dialog.definition();
        saveFunctionConfig(tr("功能已更新"));
        return;
    }
}

void MainWindow::deleteFunction(const QString& functionId)
{
    const IntentDefinition* target = promptConfig_.findById(functionId);
    if (!target) return;
    if (QMessageBox::question(this, tr("删除功能"),
            tr("确定删除“%1”吗？此操作会删除对应的本地配置文件。").arg(target->name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;

    const bool deletedDefault = promptConfig_.defaultFunctionId == functionId;
    for (auto iterator = promptConfig_.intents.begin(); iterator != promptConfig_.intents.end(); ++iterator) {
        if (iterator->id != functionId) continue;
        promptConfig_.intents.erase(iterator);
        break;
    }
    if (deletedDefault)
        promptConfig_.defaultFunctionId = promptConfig_.intents.isEmpty()
            ? QString() : promptConfig_.intents.first().id;
    saveFunctionConfig(tr("功能已删除"));
}

void MainWindow::setDefaultFunction(const QString& functionId)
{
    if (!promptConfig_.findById(functionId)
        || promptConfig_.defaultFunctionId == functionId) return;

    const QString previousDefaultId = promptConfig_.defaultFunctionId;
    promptConfig_.defaultFunctionId = functionId;
    QString error;
    if (!promptConfig_.save(&error)) {
        promptConfig_.defaultFunctionId = previousDefaultId;
        QMessageBox::warning(this, tr("配置无效"), error);
        return;
    }

    for (QToolButton* button : intentButtons_) {
        const bool isDefault = button->property("intentId").toString() == functionId;
        button->setProperty("isDefault", isDefault);
        const QString marker = button->isChecked()
            ? QStringLiteral("●")
            : (isDefault ? QStringLiteral("★") : QStringLiteral("○"));
        button->setText(QStringLiteral("%1  %2").arg(
            marker, button->property("intentName").toString()));
    }
}
void MainWindow::showConfiguredFunctions()
{
    clearLayout(intentOptionsLayout_);
    intentButtons_.clear();
    currentIntent_.clear();
    clearLayout(resultContentLayout_);
    intentOptions_->hide();
    intentSection_->show();
    resultSection_->hide();

    QString configError;
    promptConfig_ = IntentPromptConfig::load(&configError);
    if (!configError.isEmpty()) {
        auto* errorLabel = new QLabel(tr("配置错误：%1").arg(configError), intentOptions_);
        errorLabel->setWordWrap(true);
        intentOptionsLayout_->addWidget(errorLabel, 0, 0, 1, 5);
        intentOptions_->show();
        updateExpandedSize();
        return;
    }

    QStringList functionIds;
    for (const IntentDefinition& definition : promptConfig_.intents)
        functionIds.append(definition.id);
    showFunctionOptions(functionIds);
}

void MainWindow::showFunctionOptions(const QStringList& functionIds,
    const QString& selectedFunctionId, bool executeSelection)
{
    QToolButton* defaultButton = nullptr;
    QToolButton* selectedButton = nullptr;
    for (const QString& functionId : functionIds) {
        const IntentDefinition* definition = promptConfig_.findById(functionId);
        if (!definition) continue;

        auto* button = new QToolButton(intentOptions_);
        button->setObjectName(QStringLiteral("intentButton"));
        button->setCheckable(true);
        button->setContextMenuPolicy(Qt::CustomContextMenu);
        button->setProperty("intentId", definition->id);
        button->setProperty("intentName", definition->name);
        button->setProperty("isDefault", (definition->id == promptConfig_.defaultFunctionId));
        button->setToolTip(definition->description + tr("\n右键可编辑、设为默认或删除"));
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setText(QStringLiteral("%1  %2").arg(
            (definition->id == promptConfig_.defaultFunctionId) ? QStringLiteral("★") : QStringLiteral("○"), definition->name));
        intentButtonGroup_->addButton(button);
        connect(button, &QToolButton::toggled, this, [this, button](bool checked) {
            const QString name = button->property("intentName").toString();
            const bool isDefault = button->property("isDefault").toBool();
            button->setText(QStringLiteral("%1  %2").arg(
                checked ? QStringLiteral("●")
                        : (isDefault ? QStringLiteral("★") : QStringLiteral("○")),
                name));
            if (checked) selectIntent(button->property("intentId").toString());
        });
        connect(button, &QToolButton::customContextMenuRequested, this,
            [this, button](const QPoint& position) {
                const QString functionId = button->property("intentId").toString();
                const IntentDefinition* current = promptConfig_.findById(functionId);
                if (!current) return;
                QMenu menu(this);
                QAction* editAction = menu.addAction(tr("编辑功能"));
                QAction* defaultAction = menu.addAction(
                    (current->id == promptConfig_.defaultFunctionId) ? tr("当前默认功能") : tr("设为默认功能"));
                defaultAction->setEnabled(!(current->id == promptConfig_.defaultFunctionId));
                menu.addSeparator();
                QAction* deleteAction = menu.addAction(tr("删除功能"));
                QAction* selected = menu.exec(button->mapToGlobal(position));
                if (selected == editAction) editFunction(functionId);
                else if (selected == defaultAction) setDefaultFunction(functionId);
                else if (selected == deleteAction) deleteFunction(functionId);
            });
        const int position = static_cast<int>(intentButtons_.size());
        intentButtons_.append(button);
        intentOptionsLayout_->addWidget(button, position / 5, position % 5);
        if (definition->id == promptConfig_.defaultFunctionId) defaultButton = button;
        if (definition->id == selectedFunctionId) selectedButton = button;
    }
    intentOptions_->show();
    if (intentButtons_.isEmpty()) {
        auto* emptyLabel = new QLabel(tr("尚未配置功能，请点击“添加功能”。"), intentOptions_);
        intentOptionsLayout_->addWidget(emptyLabel, 0, 0, 1, 5);
        updateExpandedSize();
        return;
    }

    QToolButton* targetButton = selectedButton
        ? selectedButton : (defaultButton ? defaultButton : intentButtons_.first());
    if (executeSelection) {
        targetButton->setChecked(true);
    } else {
        const QSignalBlocker blocker(targetButton);
        targetButton->setChecked(true);
        targetButton->setText(QStringLiteral("●  %1")
            .arg(targetButton->property("intentName").toString()));
    }
    updateExpandedSize();
}
void MainWindow::selectIntent(const QString& intentId)
{
    currentIntent_ = intentId;
    beginFunctionExecution(false);
}
void MainWindow::beginFunctionExecution(bool forceRegeneration)
{
    if (currentIntent_.isEmpty()) return;
    if (!forceRegeneration && resultCache_.contains(currentIntent_)) {
        const IntentDefinition* definition = promptConfig_.findById(currentIntent_);
        renderResult(definition ? definition->name : currentIntent_, resultCache_.value(currentIntent_));
        return;
    }

    const IntentDefinition* definition = promptConfig_.findById(currentIntent_);
    if (!definition || definition->actionPrompt.trimmed().isEmpty()) {
        resultLoadingLabel_->setText(tr("功能执行提示词为空。"));
        resultLoadingLabel_->show();
        resultProgress_->hide();
        resultSection_->show();
        updateExpandedSize();
        return;
    }

    clearLayout(resultContentLayout_);
    resultScrollArea_->hide();
    resultLoadingLabel_->setText(tr("正在执行“%1”…").arg(definition->name));
    resultLoadingLabel_->show();
    resultProgress_->show();
    resultSection_->show();
    inferenceClient_->executeFunction(
        contentEdit_->toPlainText(), currentIntent_, definition->actionPrompt);
    updateExpandedSize();
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
    updateExpandedSize();
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
