#pragma once

#include "intent_prompt_config.h"

#include <QDialog>
#include <QList>

class QFrame;
class QGridLayout;
class QLabel;
class QToolButton;

class FunctionSettingsDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit FunctionSettingsDialog(QWidget* parent = nullptr);

private:
    void refreshFunctionButtons();
    void addFunction();
    void editFunction(const QString& functionId);
    void deleteFunction(const QString& functionId);
    void setDefaultFunction(const QString& functionId);
    void restoreDefaultFunctions();
    bool saveConfig();

    IntentPromptConfig config_;
    QFrame* optionsFrame_ = nullptr;
    QGridLayout* optionsLayout_ = nullptr;
    QLabel* emptyLabel_ = nullptr;
    QList<QToolButton*> functionButtons_;
};