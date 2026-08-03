#include "inference_client.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
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
        sendPendingRequest();
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
            if (response.value(QStringLiteral("id")).toVariant().toULongLong() != generation_) continue;
            if (response.value(QStringLiteral("type")).toString() == QStringLiteral("error")) {
                emit inferenceError(response.value(QStringLiteral("message")).toString());
                continue;
            }
            QStringList intents;
            for (const QJsonValue& value : response.value(QStringLiteral("intents")).toArray()) {
                const QString name = value.isObject()
                    ? value.toObject().value(QStringLiteral("name")).toString()
                    : value.toString();
                if (!name.isEmpty()) intents.append(name);
            }
            if (!intents.isEmpty()) emit intentsReady(intents);
            else emit inferenceError(tr("模型返回的意图列表无效。"));
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

void InferenceClient::recognizeIntents(const QString& text, const QJsonObject& promptConfig)
{
    ++generation_;
    QJsonObject request;
    request.insert(QStringLiteral("type"), QStringLiteral("classify"));
    request.insert(QStringLiteral("id"), static_cast<qint64>(generation_));
    request.insert(QStringLiteral("text"), text.left(4000));
    request.insert(QStringLiteral("prompt_config"), promptConfig);
    pendingRequest_ = QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n';

    if (socket_->state() == QLocalSocket::ConnectedState) {
        sendPendingRequest();
        return;
    }
    if (worker_->state() == QProcess::NotRunning) {
        const QString executable = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("IntentClipInference.exe"));
        worker_->setProgram(executable);
        worker_->setArguments({QStringLiteral("--server"), serverName_, QStringLiteral("--model"), modelPath()});
        worker_->start();
    }
    connectionAttempts_ = 0;
    connectTimer_->start();
    ensureConnected();
}

void InferenceClient::ensureConnected()
{
    if (socket_->state() == QLocalSocket::ConnectedState || socket_->state() == QLocalSocket::ConnectingState) return;
    socket_->abort();
    socket_->connectToServer(serverName_);
    if (++connectionAttempts_ > 150) {
        connectTimer_->stop();
        emit inferenceError(tr("连接本地推理进程超时。"));
    }
}

void InferenceClient::sendPendingRequest()
{
    if (pendingRequest_.isEmpty() || socket_->state() != QLocalSocket::ConnectedState) return;
    socket_->write(pendingRequest_);
    socket_->flush();
    pendingRequest_.clear();
}

QString InferenceClient::modelPath() const
{
    const QString configured = qEnvironmentVariable("INTENTCLIP_MODEL_PATH");
    if (!configured.isEmpty()) return QDir::cleanPath(configured);

    const QDir appDirectory(QCoreApplication::applicationDirPath());
    const QString deployed = appDirectory.filePath(QStringLiteral("models/Qwen3-0.6B-Q8_0.gguf"));
    if (QFileInfo::exists(deployed)) return deployed;
    return QDir::cleanPath(appDirectory.filePath(QStringLiteral("../../../models/Qwen3-0.6B-Q8_0.gguf")));
}
