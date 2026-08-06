#pragma once

#include <QDialog>
#include <QHash>
#include <QList>
#include <QPointer>

#include "intent_prompt_config.h"

class QCloseEvent;
class QEvent;
class QComboBox;
class QFrame;
class InferenceClient;
class QLabel;
class QLayout;
class CircularSpinner;
class QScrollArea;
class QTextEdit;
class QToolButton;
class QVBoxLayout;

class MainWindow final : public QDialog
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    void showClipboardText(const QString& text);
    void showPanel();
    void openFunctionSettings();

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void invalidateCacheForContentChange();
    void refreshFunctionSelection(bool executeSelection = true);
    void switchFunction(const QString& functionId);
    void beginFunctionExecution(bool forceRegeneration = false);
    void renderResult(const QString& body);
    void showResultStatus(const QString& message, bool loading);
    void clearLayout(QLayout* layout);
    void updateExpandedSize();
    void enterContentEditMode();
    void finishContentEdit();
    bool isInsideContentEdit(QWidget* widget) const;
    void saveWindowGeometry();
    void restoreWindowGeometry();

    IntentPromptConfig promptConfig_;
    InferenceClient* inferenceClient_ = nullptr;
    QTextEdit* contentEdit_ = nullptr;
    QFrame* resultSection_ = nullptr;
    QComboBox* functionSelector_ = nullptr;
    CircularSpinner* resultSpinner_ = nullptr;
    QLabel* resultLoadingLabel_ = nullptr;
    QFrame* resultLoadingContainer_ = nullptr;
    QScrollArea* resultScrollArea_ = nullptr;
    QFrame* resultContent_ = nullptr;
    QVBoxLayout* resultContentLayout_ = nullptr;
    QPointer<QLabel> resultBodyLabel_;
    QToolButton* copyResultButton_ = nullptr;
    QString streamingIntent_;
    QHash<QString, QString> resultCache_;
    QString currentIntent_;
};