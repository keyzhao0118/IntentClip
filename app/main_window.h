#pragma once

#include <QMainWindow>

class QCloseEvent;
class QLabel;
class QTextEdit;

class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(QWidget* parent = nullptr);
    void showClipboardText(const QString& text);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    QLabel* statusLabel_ = nullptr;
    QTextEdit* contentEdit_ = nullptr;
};
