#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QThread>

#include <llama.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

class LlamaIntentEngine final
{
public:
    explicit LlamaIntentEngine(const QString& modelPath)
    {
        llama_backend_init();
        llama_model_params modelParams = llama_model_default_params();
        modelParams.n_gpu_layers = 0;
        const QByteArray path = QFile::encodeName(modelPath);
        model_ = llama_model_load_from_file(path.constData(), modelParams);
        if (!model_) throw std::runtime_error("无法加载 GGUF 模型：" + modelPath.toStdString());

        llama_context_params contextParams = llama_context_default_params();
        contextParams.n_ctx = 2048;
        contextParams.n_batch = 2048;
        contextParams.n_ubatch = 512;
        contextParams.no_perf = true;
        context_ = llama_init_from_model(model_, contextParams);
        if (!context_) throw std::runtime_error("无法创建 llama.cpp 上下文");
        const int threads = std::max(1, QThread::idealThreadCount() - 1);
        llama_set_n_threads(context_, threads, threads);
    }

    ~LlamaIntentEngine()
    {
        if (context_) llama_free(context_);
        if (model_) llama_model_free(model_);
        llama_backend_free();
    }

    QStringList classify(const QString& input)
    {
        static const QStringList allowed = {
            QStringLiteral("总结要点"), QStringLiteral("润色改写"), QStringLiteral("翻译为英文"),
            QStringLiteral("提取行动项"), QStringLiteral("生成回复")
        };
        const QString prompt = QStringLiteral(
            "<|im_start|>system\n你是本地文本意图分类器。只能从以下功能中选择3到5项并按相关度排序："
            "总结要点、润色改写、翻译为英文、提取行动项、生成回复。"
            "只输出JSON字符串数组，不要解释，不要输出思考过程。<|im_end|>\n"
            "<|im_start|>user\n%1\n/no_think<|im_end|>\n<|im_start|>assistant\n")
            .arg(input.left(1200));
        const QByteArray utf8 = prompt.toUtf8();
        const llama_vocab* vocab = llama_model_get_vocab(model_);
        std::vector<llama_token> tokens(static_cast<size_t>(utf8.size()) + 32);
        int32_t count = llama_tokenize(vocab, utf8.constData(), utf8.size(), tokens.data(), tokens.size(), true, true);
        if (count < 0) {
            tokens.resize(static_cast<size_t>(-count));
            count = llama_tokenize(vocab, utf8.constData(), utf8.size(), tokens.data(), tokens.size(), true, true);
        }
        if (count <= 0 || count >= 1950) throw std::runtime_error("输入无法被模型安全编码");
        tokens.resize(static_cast<size_t>(count));

        llama_memory_clear(llama_get_memory(context_), true);
        if (llama_decode(context_, llama_batch_get_one(tokens.data(), count)) != 0)
            throw std::runtime_error("llama.cpp 处理分类提示失败");

        llama_sampler* sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
        llama_sampler_chain_add(sampler, llama_sampler_init_top_k(20));
        llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.8f, 1));
        llama_sampler_chain_add(sampler, llama_sampler_init_temp(0.2f));
        llama_sampler_chain_add(sampler, llama_sampler_init_dist(42));

        QByteArray output;
        for (int generated = 0; generated < 96; ++generated) {
            const llama_token token = llama_sampler_sample(sampler, context_, -1);
            if (llama_vocab_is_eog(vocab, token)) break;
            char piece[256];
            int32_t pieceLength = llama_token_to_piece(vocab, token, piece, sizeof(piece), 0, true);
            if (pieceLength < 0) {
                std::vector<char> largePiece(static_cast<size_t>(-pieceLength));
                pieceLength = llama_token_to_piece(vocab, token, largePiece.data(), largePiece.size(), 0, true);
                if (pieceLength > 0) output.append(largePiece.data(), pieceLength);
            } else if (pieceLength > 0) {
                output.append(piece, pieceLength);
            }
            llama_token next = token;
            if (llama_decode(context_, llama_batch_get_one(&next, 1)) != 0) break;
        }
        llama_sampler_free(sampler);

        const QString generated = QString::fromUtf8(output);
        struct RankedIntent { qsizetype position; QString name; };
        QList<RankedIntent> ranked;
        for (const QString& intent : allowed) {
            const qsizetype position = generated.indexOf(intent);
            if (position >= 0) ranked.append({position, intent});
        }
        std::sort(ranked.begin(), ranked.end(), [](const RankedIntent& left, const RankedIntent& right) {
            return left.position < right.position;
        });
        QStringList result;
        for (const RankedIntent& intent : ranked) result.append(intent.name);
        for (const QString& fallback : allowed) {
            if (result.size() >= 3) break;
            if (!result.contains(fallback)) result.append(fallback);
        }
        if (result.size() > 5) result = result.mid(0, 5);
        return result;
    }

private:
    llama_model* model_ = nullptr;
    llama_context* context_ = nullptr;
};

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    const QStringList arguments = application.arguments();
    const int serverIndex = arguments.indexOf(QStringLiteral("--server"));
    const int modelIndex = arguments.indexOf(QStringLiteral("--model"));
    if (serverIndex < 0 || serverIndex + 1 >= arguments.size() || modelIndex < 0 || modelIndex + 1 >= arguments.size()) return 2;

    const QString serverName = arguments.at(serverIndex + 1);
    QLocalServer::removeServer(serverName);
    QLocalServer server;
    if (!server.listen(serverName)) return 3;

    std::unique_ptr<LlamaIntentEngine> engine;
    QString loadError;
    try {
        engine = std::make_unique<LlamaIntentEngine>(arguments.at(modelIndex + 1));
    } catch (const std::exception& exception) {
        loadError = QString::fromUtf8(exception.what());
    }

    QObject::connect(&server, &QLocalServer::newConnection, &application, [&] {
        while (QLocalSocket* socket = server.nextPendingConnection()) {
            auto* buffer = new QByteArray;
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket, buffer, &engine, &loadError] {
                buffer->append(socket->readAll());
                qsizetype newline = -1;
                while ((newline = buffer->indexOf('\n')) >= 0) {
                    const QByteArray line = buffer->left(newline);
                    buffer->remove(0, newline + 1);
                    const QJsonObject request = QJsonDocument::fromJson(line).object();
                    QJsonObject response;
                    response.insert(QStringLiteral("id"), request.value(QStringLiteral("id")));
                    if (!engine) {
                        response.insert(QStringLiteral("type"), QStringLiteral("error"));
                        response.insert(QStringLiteral("message"), loadError);
                    } else {
                        try {
                            const QStringList intents = engine->classify(request.value(QStringLiteral("text")).toString());
                            response.insert(QStringLiteral("type"), QStringLiteral("intents"));
                            response.insert(QStringLiteral("intents"), QJsonArray::fromStringList(intents));
                        } catch (const std::exception& exception) {
                            response.insert(QStringLiteral("type"), QStringLiteral("error"));
                            response.insert(QStringLiteral("message"), QString::fromUtf8(exception.what()));
                        }
                    }
                    socket->write(QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n');
                    socket->flush();
                }
            });
            QObject::connect(socket, &QLocalSocket::disconnected, socket, [socket, buffer] {
                delete buffer;
                socket->deleteLater();
            });
        }
    });
    return application.exec();
}
