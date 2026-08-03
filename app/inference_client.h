#pragma once

#include <QObject>
#include <QByteArray>

class QLocalSocket;
class QProcess;
class QTimer;

class InferenceClient final : public QObject
{
    Q_OBJECT
public:
    explicit InferenceClient(QObject* parent = nullptr);
    void recognizeIntents(const QString& text);

signals:
    void intentsReady(const QStringList& intents);
    void inferenceError(const QString& message);

private:
    void ensureConnected();
    void sendPendingRequest();
    QString modelPath() const;

    QLocalSocket* socket_ = nullptr;
    QProcess* worker_ = nullptr;
    QTimer* connectTimer_ = nullptr;
    QByteArray readBuffer_;
    QByteArray pendingRequest_;
    QString serverName_;
    quint64 generation_ = 0;
    int connectionAttempts_ = 0;
};
