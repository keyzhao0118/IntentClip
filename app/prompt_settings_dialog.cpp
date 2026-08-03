#include "prompt_settings_dialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QVBoxLayout>

PromptSettingsDialog::PromptSettingsDialog(const IntentDefinition& definition, QWidget* parent)
    : QDialog(parent)
    , id_(definition.id)
{
    setWindowTitle(tr("编辑功能"));
    resize(640, 430);
    setMinimumSize(520, 360);

    auto* layout = new QVBoxLayout(this);
    auto* explanation = new QLabel(
        tr("修改功能在 Intent 区显示的名称、说明，以及点击后发送给本地模型的执行提示词。"), this);
    explanation->setWordWrap(true);
    layout->addWidget(explanation);

    auto* form = new QFormLayout;
    name_ = new QLineEdit(definition.name, this);
    name_->setMaxLength(40);
    description_ = new QLineEdit(definition.description, this);
    description_->setMaxLength(120);
    actionPrompt_ = new QPlainTextEdit(definition.actionPrompt, this);
    actionPrompt_->setMinimumHeight(180);
    actionPrompt_->setPlaceholderText(tr("说明模型应如何处理 Content 内容，并直接输出最终结果。"));
    form->addRow(tr("功能名称"), name_);
    form->addRow(tr("一句话描述"), description_);
    form->addRow(tr("功能执行提示词"), actionPrompt_);
    layout->addLayout(form, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &PromptSettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

IntentDefinition PromptSettingsDialog::definition() const
{
    return {
        id_,
        name_->text().trimmed(),
        description_->text().trimmed(),
        actionPrompt_->toPlainText().trimmed()
    };
}

void PromptSettingsDialog::accept()
{
    const IntentDefinition value = definition();
    if (value.name.isEmpty() || value.description.isEmpty() || value.actionPrompt.isEmpty()) {
        QMessageBox::warning(this, tr("配置无效"), tr("名称、描述和功能执行提示词均不能为空。"));
        return;
    }
    QDialog::accept();
}