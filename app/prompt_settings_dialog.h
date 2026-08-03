#pragma once

#include "intent_prompt_config.h"

#include <QDialog>
#include <QList>

class QGroupBox;
class QLineEdit;
class QPlainTextEdit;
class QVBoxLayout;
class QWidget;

class PromptSettingsDialog final : public QDialog
{
public:
    explicit PromptSettingsDialog(const IntentPromptConfig& config, QWidget* parent = nullptr);
    IntentPromptConfig config() const;

protected:
    void accept() override;

private:
    struct FunctionEditor {
        QString id;
        bool persistent = false;
        QGroupBox* card = nullptr;
        QLineEdit* name = nullptr;
        QLineEdit* description = nullptr;
        QPlainTextEdit* recommendationPrompt = nullptr;
        QPlainTextEdit* actionPrompt = nullptr;
    };

    QWidget* createCollectionPage(bool persistent);
    void addFunctionEditor(const IntentDefinition& definition);
    void removeFunctionEditor(QGroupBox* card);
    void restorePersistentDefaults();

    QVBoxLayout* persistentListLayout_ = nullptr;
    QVBoxLayout* customListLayout_ = nullptr;
    QList<FunctionEditor> editors_;
};
