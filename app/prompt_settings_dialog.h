#pragma once

#include "intent_prompt_config.h"

#include <QDialog>

class QLineEdit;
class QPlainTextEdit;

class PromptSettingsDialog final : public QDialog
{
public:
    explicit PromptSettingsDialog(const IntentDefinition& definition, QWidget* parent = nullptr);
    IntentDefinition definition() const;

protected:
    void accept() override;

private:
    QString id_;
    QLineEdit* name_ = nullptr;
    QLineEdit* description_ = nullptr;
    QPlainTextEdit* actionPrompt_ = nullptr;
};