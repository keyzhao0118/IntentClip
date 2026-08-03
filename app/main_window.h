#pragma once

#include <QHash>
#include <QList>
#include <QDialog>

#include "intent_prompt_config.h"

class QButtonGroup;
class QCloseEvent;
class QFrame;
class InferenceClient;
class QLabel;
class QGridLayout;
class QLayout;
class QProgressBar;
class QScrollArea;
class QToolButton;
class QTextEdit;
class QVBoxLayout;

class MainWindow final : public QDialog
{
public:
    explicit MainWindow(QWidget* parent = nullptr);
    void showClipboardText(const QString& text);
    void showPanel();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void showConfiguredFunctions(bool executeSelection = true);
    void showFunctionOptions(const QStringList& functionIds,
        const QString& selectedFunctionId = {}, bool executeSelection = true);
    void addFunction();
    void editFunction(const QString& functionId);
    void deleteFunction(const QString& functionId);
    void setDefaultFunction(const QString& functionId);
    bool saveFunctionConfig(const QString& successMessage);
    void refreshFunctionButtonsPreservingState();
    void invalidateCacheForContentChange();
    void selectIntent(const QString& intent);
    void beginFunctionExecution(bool forceRegeneration = false);
    void renderResult(const QString& intent, const QString& body);
    void clearLayout(QLayout* layout);
    void updateExpandedSize();

    IntentPromptConfig promptConfig_;
    InferenceClient* inferenceClient_ = nullptr;
    QTextEdit* contentEdit_ = nullptr;
    QToolButton* contentEditButton_ = nullptr;
    QFrame* intentSection_ = nullptr;
    QFrame* intentOptions_ = nullptr;
    QGridLayout* intentOptionsLayout_ = nullptr;
    QButtonGroup* intentButtonGroup_ = nullptr;
    QFrame* resultSection_ = nullptr;
    QProgressBar* resultProgress_ = nullptr;
    QLabel* resultLoadingLabel_ = nullptr;
    QScrollArea* resultScrollArea_ = nullptr;
    QFrame* resultContent_ = nullptr;
    QVBoxLayout* resultContentLayout_ = nullptr;
    QList<QToolButton*> intentButtons_;
    QHash<QString, QString> resultCache_;
    QString currentIntent_;
};