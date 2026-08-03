#pragma once

#include "intent_prompt_config.h"

#include <QDialog>

class QPlainTextEdit;
class QSpinBox;

class PromptSettingsDialog final : public QDialog
{
public:
    explicit PromptSettingsDialog(const IntentPromptConfig& config, QWidget* parent = nullptr);
    IntentPromptConfig config() const;

protected:
    void accept() override;

private:
    void applyConfig(const IntentPromptConfig& config);

    QPlainTextEdit* systemPromptEdit_ = nullptr;
    QPlainTextEdit* userPromptEdit_ = nullptr;
    QSpinBox* minimumIntentsSpin_ = nullptr;
    QSpinBox* maximumIntentsSpin_ = nullptr;
};
