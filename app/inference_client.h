#pragma once

#include <QByteArray>
#include <QObject>
#include <QQueue>

class QJsonObject;
class QLocalSocket;
class QProcess;
class QTimer;

class InferenceClient final : public QObject
{
    Q_OBJECT
public:
    explicit InferenceClient(QObject* parent = nullptr);

    void invalidateExecution();
    void executeFunction(
        const QString& text,
        const QString& intentId,
        const QString& actionPrompt);

signals:
    void resultUpdated(const QString& intentId, const QString& partialResult);
    void resultReady(const QString& intentId, const QString& result);
    void executionError(const QString& intentId, const QString& message);
    void inferenceError(const QString& message);

private:
    void startWorker();
    void enqueueRequest(const QJsonObject& request);
    void ensureConnected();
    void sendPendingRequests();
    QString modelPath() const;

    QLocalSocket* socket_ = nullptr;
    QProcess* worker_ = nullptr;
    QTimer* connectTimer_ = nullptr;
    QByteArray readBuffer_;
    QQueue<QByteArray> pendingRequests_;
    QString serverName_;
    quint64 generation_ = 0;
    quint64 latestExecution_ = 0;
    int connectionAttempts_ = 0;
};