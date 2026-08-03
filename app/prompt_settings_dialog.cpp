#include "prompt_settings_dialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>

PromptSettingsDialog::PromptSettingsDialog(const IntentPromptConfig& config, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("功能设置"));
    resize(820, 680);
    setMinimumSize(680, 520);

    auto* layout = new QVBoxLayout(this);
    auto* pathLabel = new QLabel(
        tr("常驻功能：%1/persistent    自定义意图：%1/custom")
            .arg(IntentPromptConfig::directoryPath()),
        this);
    pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pathLabel->setWordWrap(true);
    layout->addWidget(pathLabel);

    auto* tabs = new QTabWidget(this);
    tabs->setDocumentMode(true);
    tabs->addTab(createCollectionPage(true), tr("常驻功能"));
    tabs->addTab(createCollectionPage(false), tr("自定义意图"));
    layout->addWidget(tabs, 1);

    for (const IntentDefinition& definition : config.persistentIntents()) addFunctionEditor(definition);
    for (const IntentDefinition& definition : config.customIntents()) addFunctionEditor(definition);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &PromptSettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QWidget* PromptSettingsDialog::createCollectionPage(bool persistent)
{
    auto* page = new QWidget(this);
    auto* pageLayout = new QVBoxLayout(page);

    auto* explanation = new QLabel(
        persistent
            ? tr("这些功能会在面板调起后立即显示。功能执行提示词只在点击功能、发起第二轮 AI 请求时使用。")
            : tr("这些功能由第一轮意图识别按需推荐。推荐判断提示词用于识别，功能执行提示词用于点击后的第二轮 AI 请求。"),
        page);
    explanation->setWordWrap(true);
    pageLayout->addWidget(explanation);

    auto* actions = new QHBoxLayout;
    auto* addButton = new QPushButton(
        persistent ? tr("＋ 添加常驻功能") : tr("＋ 添加自定义意图"), page);
    actions->addWidget(addButton);
    if (persistent) {
        auto* restoreButton = new QPushButton(tr("恢复默认五项"), page);
        actions->addWidget(restoreButton);
        connect(restoreButton, &QPushButton::clicked, this, &PromptSettingsDialog::restorePersistentDefaults);
    }
    actions->addStretch();
    pageLayout->addLayout(actions);

    auto* scrollArea = new QScrollArea(page);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget(scrollArea);
    auto* listLayout = new QVBoxLayout(content);
    listLayout->setContentsMargins(4, 4, 8, 4);
    listLayout->setSpacing(12);
    listLayout->addStretch();
    scrollArea->setWidget(content);
    pageLayout->addWidget(scrollArea, 1);

    if (persistent) persistentListLayout_ = listLayout;
    else customListLayout_ = listLayout;

    connect(addButton, &QPushButton::clicked, this, [this, persistent] {
        IntentDefinition definition;
        definition.id = (persistent ? QStringLiteral("persistent_") : QStringLiteral("custom_"))
            + QUuid::createUuid().toString(QUuid::Id128).left(16);
        definition.persistent = persistent;
        definition.name = persistent ? tr("新常驻功能") : tr("新自定义意图");
        definition.description = tr("请填写一句话功能描述。");
        if (!persistent)
            definition.recommendationPrompt = tr("请说明在什么内容和用户需求下应当推荐该意图。");
        definition.actionPrompt = tr("请说明点击功能后，本地模型应如何处理 Content 内容。");
        addFunctionEditor(definition);
    });
    return page;
}

void PromptSettingsDialog::addFunctionEditor(const IntentDefinition& definition)
{
    QVBoxLayout* targetLayout = definition.persistent ? persistentListLayout_ : customListLayout_;
    if (!targetLayout) return;

    auto* card = new QGroupBox(definition.name, this);
    auto* cardLayout = new QVBoxLayout(card);
    auto* header = new QHBoxLayout;
    auto* typeLabel = new QLabel(
        definition.persistent ? tr("立即显示") : tr("按需推荐"), card);
    header->addWidget(typeLabel);
    header->addStretch();
    auto* removeButton = new QPushButton(tr("删除"), card);
    header->addWidget(removeButton);
    cardLayout->addLayout(header);

    auto* form = new QFormLayout;
    FunctionEditor editor;
    editor.id = definition.id;
    editor.persistent = definition.persistent;
    editor.card = card;
    editor.name = new QLineEdit(definition.name, card);
    editor.name->setMaxLength(40);
    editor.description = new QLineEdit(definition.description, card);
    editor.description->setMaxLength(120);
    if (!definition.persistent) {
        editor.recommendationPrompt = new QPlainTextEdit(definition.recommendationPrompt, card);
        editor.recommendationPrompt->setMinimumHeight(90);
        editor.recommendationPrompt->setPlaceholderText(tr("说明哪些内容和需求更应该推荐这个意图。"));
    }
    editor.actionPrompt = new QPlainTextEdit(definition.actionPrompt, card);
    editor.actionPrompt->setMinimumHeight(110);
    editor.actionPrompt->setPlaceholderText(tr("说明点击该功能后，模型应该如何处理 Content 内容。"));

    form->addRow(tr("功能名称"), editor.name);
    form->addRow(tr("一句话描述"), editor.description);
    if (editor.recommendationPrompt)
        form->addRow(tr("推荐判断提示词"), editor.recommendationPrompt);
    form->addRow(tr("功能执行提示词"), editor.actionPrompt);
    cardLayout->addLayout(form);

    connect(editor.name, &QLineEdit::textChanged, card, [card](const QString& name) {
        card->setTitle(name.trimmed().isEmpty() ? QObject::tr("未命名功能") : name.trimmed());
    });
    connect(removeButton, &QPushButton::clicked, this, [this, card] {
        removeFunctionEditor(card);
    });

    editors_.append(editor);
    targetLayout->insertWidget(targetLayout->count() - 1, card);
}

void PromptSettingsDialog::removeFunctionEditor(QGroupBox* card)
{
    for (auto iterator = editors_.begin(); iterator != editors_.end(); ++iterator) {
        if (iterator->card == card) {
            editors_.erase(iterator);
            card->deleteLater();
            return;
        }
    }
}

void PromptSettingsDialog::restorePersistentDefaults()
{
    QList<QGroupBox*> cards;
    for (const FunctionEditor& editor : editors_) {
        if (editor.persistent) cards.append(editor.card);
    }
    for (QGroupBox* card : cards) removeFunctionEditor(card);
    for (const IntentDefinition& definition : IntentPromptConfig::defaults().persistentIntents())
        addFunctionEditor(definition);
}

IntentPromptConfig PromptSettingsDialog::config() const
{
    IntentPromptConfig value;
    for (const FunctionEditor& editor : editors_) {
        value.intents.append({
            editor.id,
            editor.persistent,
            editor.name->text().trimmed(),
            editor.description->text().trimmed(),
            editor.recommendationPrompt
                ? editor.recommendationPrompt->toPlainText().trimmed() : QString(),
            editor.actionPrompt->toPlainText().trimmed()
        });
    }
    return value;
}

void PromptSettingsDialog::accept()
{
    const IntentPromptConfig value = config();
    QString error;
    if (!value.save(&error)) {
        QMessageBox::warning(this, tr("配置无效"), error);
        return;
    }
    QDialog::accept();
}
