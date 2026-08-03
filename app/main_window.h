#pragma once

#include <QHash>
#include <QList>
#include <QMainWindow>

#include "intent_prompt_config.h"

class QButtonGroup;
class QCloseEvent;
class QFrame;
class InferenceClient;
class QLabel;
class QProgressBar;
class QScrollArea;
class QTimer;
class QToolButton;
class QTextEdit;
class QVBoxLayout;

class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(QWidget* parent = nullptr);
    void showClipboardText(const QString& text);
    void showPanel();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void beginIntentRecognition();
    void showIntentOptions(const QStringList& options);
    void selectIntent(const QString& intent);
    void beginFunctionExecution(bool forceRegeneration = false);
    void showResults();
    void renderResult(const QString& intent, const QString& body);
    void clearLayout(QVBoxLayout* layout);
    void updateExpandedSize();

    IntentPromptConfig promptConfig_;
    InferenceClient* inferenceClient_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QTextEdit* contentEdit_ = nullptr;
    QToolButton* contentEditButton_ = nullptr;
    QFrame* intentSection_ = nullptr;
    QProgressBar* intentProgress_ = nullptr;
    QLabel* intentLoadingLabel_ = nullptr;
    QFrame* intentOptions_ = nullptr;
    QVBoxLayout* intentOptionsLayout_ = nullptr;
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
    QTimer* resultTimer_ = nullptr;
};