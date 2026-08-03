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

    promptConfig_ = IntentPromptConfig::load();
    connect(addFunctionButton, &QToolButton::clicked, this, &MainWindow::addFunction);
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
    intentButtonGroup_ = new QButtonGroup(this);
    intentButtonGroup_->setExclusive(true);
    connect(regenerateResultButton, &QToolButton::clicked, this, [this] {
        if (!currentIntent_.isEmpty()) beginFunctionExecution(true);
    });

    inferenceClient_ = new InferenceClient(this);
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
            statusLabel_->setText(tr("执行失败"));
            updateExpandedSize();
        });
    connect(inferenceClient_, &InferenceClient::inferenceError, this, [this](const QString& message) {
        resultProgress_->hide();
        resultLoadingLabel_->setText(tr("本地模型错误：%1").arg(message));
        resultLoadingLabel_->show();
        resultSection_->show();
        statusLabel_->setText(tr("模型错误"));
        updateExpandedSize();
    });



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
        QToolButton#intentButton { color: #344054; background-color: #f8fafc; border: 1px solid #d9e0e9; border-radius: 9px; padding: 10px 14px; font-size: 13px; text-align: left; }
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

bool MainWindow::saveFunctionConfig(const QString& successMessage)
{
    QString error;
    if (!promptConfig_.save(&error)) {
        QMessageBox::warning(this, tr("配置无效"), error);
        return false;
    }
    statusLabel_->setText(successMessage);
    showConfiguredFunctions();
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
    promptConfig_.intents.append(dialog.definition());
    if (promptConfig_.defaultFunctionId.isEmpty())
        promptConfig_.defaultFunctionId = promptConfig_.intents.constLast().id;
    saveFunctionConfig(tr("功能已添加"));
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
    if (!promptConfig_.findById(functionId)) return;
    promptConfig_.defaultFunctionId = functionId;
    saveFunctionConfig(tr("默认功能已更新"));
}
void MainWindow::showConfiguredFunctions()
{
    clearLayout(intentOptionsLayout_);
    intentButtons_.clear();
    resultCache_.clear();
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
        statusLabel_->setText(tr("配置错误"));
        updateExpandedSize();
        return;
    }

    QStringList functionIds;
    for (const IntentDefinition& definition : promptConfig_.intents)
        functionIds.append(definition.id);
    showFunctionOptions(functionIds);
}

void MainWindow::showFunctionOptions(const QStringList& functionIds)
{
    QToolButton* defaultButton = nullptr;
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
        if ((definition->id == promptConfig_.defaultFunctionId)) defaultButton = button;
    }
    intentOptions_->show();
    if (intentButtons_.isEmpty()) {
        auto* emptyLabel = new QLabel(tr("尚未配置功能，请点击“添加功能”。"), intentOptions_);
        intentOptionsLayout_->addWidget(emptyLabel, 0, 0, 1, 5);
        statusLabel_->setText(tr("无可用功能"));
        updateExpandedSize();
        return;
    }

    (defaultButton ? defaultButton : intentButtons_.first())->setChecked(true);
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
        statusLabel_->setText(tr("配置错误"));
        updateExpandedSize();
        return;
    }

    clearLayout(resultContentLayout_);
    resultScrollArea_->hide();
    resultLoadingLabel_->setText(tr("正在执行“%1”…").arg(definition->name));
    resultLoadingLabel_->show();
    resultProgress_->show();
    resultSection_->show();
    statusLabel_->setText(forceRegeneration ? tr("重新生成中") : tr("执行中"));
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
