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
    setObjectName(QStringLiteral("promptSettingsDialog"));
    resize(660, 460);
    setMinimumSize(540, 380);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 26, 28, 24);
    layout->setSpacing(18);
    auto* explanation = new QLabel(
        tr("修改功能在 Intent 区显示的名称、说明，以及点击后发送给本地模型的执行提示词。"), this);
    explanation->setObjectName(QStringLiteral("dialogExplanation"));
    explanation->setWordWrap(true);
    layout->addWidget(explanation);

    auto* form = new QFormLayout;
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(14);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
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

    setStyleSheet(QStringLiteral(R"(
        QDialog#promptSettingsDialog {
            color: #1a1c1f;
            background-color: #f9f9f9;
            font-family: "Segoe UI", "Microsoft YaHei UI";
            font-size: 13px;
        }
        QLabel { color: #1a1c1f; font-weight: 600; }
        QLabel#dialogExplanation {
            color: #5f6062; background-color: #f7f7f7;
            border: 1px solid #e4e4e4; border-radius: 10px;
            padding: 12px 14px; font-weight: 400;
        }
        QLineEdit, QPlainTextEdit {
            color: #1a1c1f; background-color: #ffffff;
            border: 1px solid #e4e4e4; border-radius: 9px;
            padding: 9px 11px; selection-background-color: #e8e9ea;
        }
        QLineEdit { min-height: 20px; }
        QLineEdit:focus, QPlainTextEdit:focus { border: 1px solid #75777a; }
        QPushButton {
            min-width: 76px; padding: 8px 15px;
            color: #1a1c1f; background-color: #ffffff;
            border: 1px solid #e4e4e4; border-radius: 8px;
            font-weight: 600;
        }
        QPushButton:hover { background-color: #f0f1f2; border-color: #c9cbcb; }
        QPushButton:default {
            color: #ffffff; background-color: #1a1c1f;
            border-color: #1a1c1f;
        }
        QPushButton:default:hover { background-color: #2e3134; border-color: #2e3134; }
        QScrollBar:vertical { background: transparent; width: 8px; }
        QScrollBar::handle:vertical { background: #c9cbcb; border-radius: 4px; min-height: 28px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )"));
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