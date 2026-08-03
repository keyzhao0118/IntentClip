#include "inference_client.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QProcess>
#include <QTimer>

InferenceClient::InferenceClient(QObject* parent)
    : QObject(parent)
    , socket_(new QLocalSocket(this))
    , worker_(new QProcess(this))
    , connectTimer_(new QTimer(this))
    , serverName_(QStringLiteral("IntentClipInference-%1").arg(QCoreApplication::applicationPid()))
{
    connectTimer_->setInterval(100);
    connectTimer_->setSingleShot(false);
    connect(connectTimer_, &QTimer::timeout, this, &InferenceClient::ensureConnected);
    connect(socket_, &QLocalSocket::connected, this, [this] {
        connectTimer_->stop();
        connectionAttempts_ = 0;
        sendPendingRequests();
    });
    connect(socket_, &QLocalSocket::readyRead, this, [this] {
        readBuffer_.append(socket_->readAll());
        qsizetype newline = -1;
        while ((newline = readBuffer_.indexOf('\n')) >= 0) {
            const QByteArray line = readBuffer_.left(newline);
            readBuffer_.remove(0, newline + 1);
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject()) continue;

            const QJsonObject response = document.object();
            const quint64 responseId = response.value(QStringLiteral("id")).toVariant().toULongLong();
            const QString type = response.value(QStringLiteral("type")).toString();
            const QString intentId = response.value(QStringLiteral("intent_id")).toString();

            if (type == QStringLiteral("error")) {
                const QString message = response.value(QStringLiteral("message")).toString();
                if (responseId == latestExecution_) emit executionError(intentId, message);
                else emit inferenceError(message);
                continue;
            }
            if (type == QStringLiteral("result") && responseId == latestExecution_) {
                const QString result = response.value(QStringLiteral("result")).toString().trimmed();
                if (!result.isEmpty()) emit resultReady(intentId, result);
                else emit executionError(intentId, tr("模型返回了空结果。"));
            }
        }
    });
    connect(worker_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        emit inferenceError(tr("无法启动本地推理进程：%1").arg(worker_->errorString()));
    });
    connect(worker_, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        if (status == QProcess::CrashExit || exitCode != 0)
            emit inferenceError(tr("本地推理进程已退出（代码 %1）。").arg(exitCode));
    });
}

void InferenceClient::invalidateExecution()
{
    latestExecution_ = ++generation_;
    pendingRequests_.clear();
}
void InferenceClient::executeFunction(
    const QString& text,
    const QString& intentId,
    const QString& actionPrompt)
{
    latestExecution_ = ++generation_;
    QJsonObject request{
        {QStringLiteral("type"), QStringLiteral("execute")},
        {QStringLiteral("id"), static_cast<qint64>(latestExecution_)},
        {QStringLiteral("intent_id"), intentId},
        {QStringLiteral("text"), text.left(4000)},
        {QStringLiteral("action_prompt"), actionPrompt}
    };
    enqueueRequest(request);
}

void InferenceClient::enqueueRequest(const QJsonObject& request)
{
    pendingRequests_.enqueue(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
    if (socket_->state() == QLocalSocket::ConnectedState) {
        sendPendingRequests();
        return;
    }
    if (worker_->state() == QProcess::NotRunning) {
        const QString executable = QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("IntentClipInference.exe"));
        worker_->setProgram(executable);
        worker_->setArguments({
            QStringLiteral("--server"), serverName_,
            QStringLiteral("--model"), modelPath()
        });
        worker_->start();
    }
    connectionAttempts_ = 0;
    connectTimer_->start();
    ensureConnected();
}

void InferenceClient::ensureConnected()
{
    if (socket_->state() == QLocalSocket::ConnectedState
        || socket_->state() == QLocalSocket::ConnectingState) return;
    socket_->abort();
    socket_->connectToServer(serverName_);
    if (++connectionAttempts_ > 300) {
        connectTimer_->stop();
        emit inferenceError(tr("连接本地推理进程超时。"));
    }
}

void InferenceClient::sendPendingRequests()
{
    if (socket_->state() != QLocalSocket::ConnectedState) return;
    while (!pendingRequests_.isEmpty()) socket_->write(pendingRequests_.dequeue());
    socket_->flush();
}

QString InferenceClient::modelPath() const
{
    const QString configured = qEnvironmentVariable("INTENTCLIP_MODEL_PATH");
    if (!configured.isEmpty()) return QDir::cleanPath(configured);

    const QDir appDirectory(QCoreApplication::applicationDirPath());
    const QString deployed = appDirectory.filePath(QStringLiteral("models/Qwen3-0.6B-Q8_0.gguf"));
    if (QFileInfo::exists(deployed)) return deployed;
    return QDir::cleanPath(
        appDirectory.filePath(QStringLiteral("../../../models/Qwen3-0.6B-Q8_0.gguf")));
}