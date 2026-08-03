#include "prompt_settings_dialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

PromptSettingsDialog::PromptSettingsDialog(const IntentPromptConfig& config, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("意图识别设置"));
    resize(680, 580);
    setMinimumSize(560, 460);

    auto* layout = new QVBoxLayout(this);
    auto* pathLabel = new QLabel(tr("配置文件：%1").arg(IntentPromptConfig::filePath()), this);
    pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pathLabel->setWordWrap(true);
    layout->addWidget(pathLabel);

    auto* hint = new QLabel(tr("占位符 {intent_definitions} 和 {content} 必须保留。也可在应用退出后直接编辑此 JSON 文件。"), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* form = new QFormLayout;
    systemPromptEdit_ = new QPlainTextEdit(this);
    systemPromptEdit_->setMinimumHeight(220);
    form->addRow(tr("System Prompt"), systemPromptEdit_);
    userPromptEdit_ = new QPlainTextEdit(this);
    userPromptEdit_->setMinimumHeight(100);
    form->addRow(tr("User Template"), userPromptEdit_);

    minimumIntentsSpin_ = new QSpinBox(this);
    minimumIntentsSpin_->setRange(1, 5);
    maximumIntentsSpin_ = new QSpinBox(this);
    maximumIntentsSpin_->setRange(1, 5);
    form->addRow(tr("最少意图数"), minimumIntentsSpin_);
    form->addRow(tr("最多意图数"), maximumIntentsSpin_);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    auto* restoreButton = buttons->addButton(tr("恢复默认"), QDialogButtonBox::ResetRole);
    connect(restoreButton, &QPushButton::clicked, this, [this] { applyConfig(IntentPromptConfig::defaults()); });
    connect(buttons, &QDialogButtonBox::accepted, this, &PromptSettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    applyConfig(config);
}

IntentPromptConfig PromptSettingsDialog::config() const
{
    IntentPromptConfig value;
    value.systemPrompt = systemPromptEdit_->toPlainText().trimmed();
    value.userPromptTemplate = userPromptEdit_->toPlainText().trimmed();
    value.minimumIntents = minimumIntentsSpin_->value();
    value.maximumIntents = maximumIntentsSpin_->value();
    return value;
}

void PromptSettingsDialog::accept()
{
    const IntentPromptConfig value = config();
    if (!value.systemPrompt.contains(QStringLiteral("{intent_definitions}"))
        || !value.userPromptTemplate.contains(QStringLiteral("{content}"))) {
        QMessageBox::warning(this, tr("配置无效"), tr("必须保留 {intent_definitions} 和 {content} 占位符。"));
        return;
    }
    if (value.minimumIntents > value.maximumIntents) {
        QMessageBox::warning(this, tr("配置无效"), tr("最少意图数不能大于最多意图数。"));
        return;
    }
    QString error;
    if (!value.save(&error)) {
        QMessageBox::critical(this, tr("保存失败"), error);
        return;
    }
    QDialog::accept();
}

void PromptSettingsDialog::applyConfig(const IntentPromptConfig& config)
{
    systemPromptEdit_->setPlainText(config.systemPrompt);
    userPromptEdit_->setPlainText(config.userPromptTemplate);
    minimumIntentsSpin_->setValue(config.minimumIntents);
    maximumIntentsSpin_->setValue(config.maximumIntents);
}
