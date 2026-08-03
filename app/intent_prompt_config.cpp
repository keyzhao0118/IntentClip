#include "intent_prompt_config.h"

#include <QDir>
#include <QFile>
#include <QFileInfoList>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

namespace {

bool isValidId(const QString& id)
{
    static const QRegularExpression pattern(QStringLiteral("^[a-z0-9][a-z0-9_-]{0,63}$"));
    return pattern.match(id).hasMatch();
}

QString legacyDirectoryPath()
{
    const QString overridden = qEnvironmentVariable("INTENTCLIP_INTENTS_DIR");
    if (!overridden.isEmpty()) return QDir::cleanPath(overridden);
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
        .filePath(QStringLiteral("intents"));
}

QString fallbackActionPrompt(const QString& id, const QString& name)
{
    for (const IntentDefinition& definition : IntentPromptConfig::defaults().intents) {
        if (definition.id == id) return definition.actionPrompt;
    }
    return QStringLiteral("根据用户提供的内容执行“%1”功能。直接给出可用结果，不要解释处理过程。").arg(name);
}

bool readDefinition(
    const QJsonObject& object,
    IntentDefinition* definition,
    bool allowFallbackPrompt,
    QString* error)
{
    definition->id = object.value(QStringLiteral("id")).toString().trimmed();
    definition->name = object.value(QStringLiteral("name")).toString().trimmed();
    definition->description = object.value(QStringLiteral("description")).toString().trimmed();
    definition->actionPrompt = object.value(QStringLiteral("action_prompt")).toString().trimmed();
    if (allowFallbackPrompt && definition->actionPrompt.isEmpty())
        definition->actionPrompt = fallbackActionPrompt(definition->id, definition->name);
    if (!isValidId(definition->id) || definition->name.isEmpty()
        || definition->description.isEmpty() || definition->actionPrompt.isEmpty()) {
        *error = QStringLiteral("功能配置缺少合法的 id、名称、描述或功能执行提示词。");
        return false;
    }
    return true;
}

bool validateConfig(const IntentPromptConfig& config, QString* error)
{
    QSet<QString> ids;
    QSet<QString> names;
    for (const IntentDefinition& definition : config.intents) {
        if (!isValidId(definition.id) || definition.name.trimmed().isEmpty()
            || definition.description.trimmed().isEmpty() || definition.actionPrompt.trimmed().isEmpty()) {
            *error = QStringLiteral("功能“%1”的配置不完整。").arg(definition.name);
            return false;
        }
        if (ids.contains(definition.id) || names.contains(definition.name.trimmed())) {
            *error = QStringLiteral("功能 id 或名称不能重复。");
            return false;
        }
        ids.insert(definition.id);
        names.insert(definition.name.trimmed());
    }
    if (config.intents.isEmpty()) {
        if (!config.defaultFunctionId.isEmpty()) {
            *error = QStringLiteral("没有功能时不能设置默认功能。");
            return false;
        }
    } else if (!ids.contains(config.defaultFunctionId)) {
        *error = QStringLiteral("默认功能必须引用一个已配置的功能。");
        return false;
    }
    return true;
}

void removeLegacyFiles(const QString& currentFilePath)
{
    QDir legacyRoot(legacyDirectoryPath());
    if (!legacyRoot.exists()) return;
    const QString currentAbsolutePath = QFileInfo(currentFilePath).absoluteFilePath();
    const auto removeJsonFiles = [&currentAbsolutePath](const QDir& directory) {
        const QFileInfoList files = directory.entryInfoList(
            {QStringLiteral("*.json")}, QDir::Files, QDir::Name);
        for (const QFileInfo& file : files) {
            if (file.absoluteFilePath() != currentAbsolutePath) QFile::remove(file.absoluteFilePath());
        }
    };
    removeJsonFiles(QDir(legacyRoot.filePath(QStringLiteral("persistent"))));
    removeJsonFiles(QDir(legacyRoot.filePath(QStringLiteral("custom"))));
    removeJsonFiles(legacyRoot);
    QFile::remove(legacyRoot.filePath(QStringLiteral(".initialized")));
    legacyRoot.rmdir(QStringLiteral("persistent"));
    legacyRoot.rmdir(QStringLiteral("custom"));
    QDir parent = legacyRoot;
    parent.cdUp();
    parent.rmdir(legacyRoot.dirName());
}

} // namespace

QJsonObject IntentDefinition::toJson() const
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("description"), description},
        {QStringLiteral("action_prompt"), actionPrompt}
    };
}

IntentPromptConfig IntentPromptConfig::defaults()
{
    return {
        QStringLiteral("summarize_points"),
        {
            {
                QStringLiteral("generate_reply"),
                QStringLiteral("生成回复"),
                QStringLiteral("处理邮件、通知、聊天消息。"),
                QStringLiteral("根据原文起草一份可直接发送的回复。结合上下文选择恰当语气，覆盖需要回应的问题、请求或通知，不要虚构事实。")
            },
            {
                QStringLiteral("polish_rewrite"),
                QStringLiteral("润色改写"),
                QStringLiteral("改善公文、邮件、汇报和通知的表达。"),
                QStringLiteral("在不改变原意和事实的前提下润色原文，改善措辞、语气、结构、专业性和可读性。直接输出完整改写稿。")
            },
            {
                QStringLiteral("summarize_points"),
                QStringLiteral("总结要点"),
                QStringLiteral("压缩长邮件、会议记录和材料。"),
                QStringLiteral("提炼原文的核心结论、关键背景和主要事项，使用简洁的分点结构输出，不遗漏重要限制与数字。")
            },
            {
                QStringLiteral("explain_content"),
                QStringLiteral("解释内容"),
                QStringLiteral("理解制度、术语、复杂要求和陌生材料。"),
                QStringLiteral("用通俗、准确的语言解释原文中的制度、术语和复杂要求；必要时拆解概念并说明实际影响。")
            },
            {
                QStringLiteral("extract_information"),
                QStringLiteral("提取信息"),
                QStringLiteral("整理时间、人员、事项、金额、联系方式等。"),
                QStringLiteral("从原文提取可核实的信息，按时间、地点、人员、事项、金额、联系方式、编号和截止日期等字段结构化整理；没有的字段不要编造。")
            }
        }
    };
}

QString IntentPromptConfig::filePath()
{
    const QString overridden = qEnvironmentVariable("INTENTCLIP_FUNCTIONS_FILE");
    if (!overridden.isEmpty()) return QDir::cleanPath(overridden);
    const QString directoryOverride = qEnvironmentVariable("INTENTCLIP_FUNCTIONS_DIR");
    if (!directoryOverride.isEmpty())
        return QDir(directoryOverride).filePath(QStringLiteral("functions.json"));
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
        .filePath(QStringLiteral("functions.json"));
}

IntentPromptConfig IntentPromptConfig::load(QString* errorMessage)
{
    if (errorMessage) errorMessage->clear();
    QFile registry(filePath());
    if (registry.exists()) {
        if (!registry.open(QIODevice::ReadOnly)) {
            if (errorMessage) *errorMessage = QStringLiteral("无法读取功能配置：%1").arg(registry.errorString());
            return {};
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(registry.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            if (errorMessage) *errorMessage = QStringLiteral("功能配置不是合法 JSON：%1").arg(parseError.errorString());
            return {};
        }
        const QJsonObject object = document.object();
        IntentPromptConfig config;
        config.defaultFunctionId = object.value(QStringLiteral("default_function_id")).toString().trimmed();
        QString error;
        for (const QJsonValue& value : object.value(QStringLiteral("functions")).toArray()) {
            IntentDefinition definition;
            if (!readDefinition(value.toObject(), &definition, false, &error)) {
                if (errorMessage) *errorMessage = error;
                return {};
            }
            config.intents.append(definition);
        }
        if (config.intents.isEmpty()) config.defaultFunctionId.clear();
        else if (!config.findById(config.defaultFunctionId)) {
            config.defaultFunctionId = config.intents.first().id;
            config.save(errorMessage);
        }
        if (!validateConfig(config, &error)) {
            if (errorMessage) *errorMessage = error;
            return {};
        }
        return config;
    }

    QDir legacyRoot(legacyDirectoryPath());
    QDir legacyFunctions(legacyRoot.filePath(QStringLiteral("persistent")));
    QFileInfoList files = legacyFunctions.entryInfoList(
        {QStringLiteral("*.json")}, QDir::Files | QDir::Readable, QDir::Name);
    files.append(legacyRoot.entryInfoList(
        {QStringLiteral("*.json")}, QDir::Files | QDir::Readable, QDir::Name));

    IntentPromptConfig config;
    QSet<QString> ids;
    QSet<QString> names;
    QString migrationError;
    for (const QFileInfo& file : files) {
        QFile input(file.absoluteFilePath());
        if (!input.open(QIODevice::ReadOnly)) {
            migrationError = QStringLiteral("无法读取旧功能配置：%1").arg(file.absoluteFilePath());
            break;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(input.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            migrationError = QStringLiteral("旧功能配置不是合法 JSON：%1").arg(file.absoluteFilePath());
            break;
        }
        const QJsonObject object = document.object();
        if (object.value(QStringLiteral("kind")).toString() == QStringLiteral("custom")) continue;
        IntentDefinition definition;
        if (!readDefinition(object, &definition, true, &migrationError)) break;
        if (ids.contains(definition.id) || names.contains(definition.name)) continue;
        ids.insert(definition.id);
        names.insert(definition.name);
        config.intents.append(definition);
        if (object.value(QStringLiteral("is_default")).toBool(false))
            config.defaultFunctionId = definition.id;
    }
    if (!migrationError.isEmpty()) {
        if (errorMessage) *errorMessage = migrationError;
        return {};
    }

    const bool intentionallyEmpty = files.isEmpty()
        && QFile::exists(legacyRoot.filePath(QStringLiteral(".initialized")));
    if (config.intents.isEmpty() && !intentionallyEmpty) config = defaults();
    else if (!config.intents.isEmpty() && !config.findById(config.defaultFunctionId))
        config.defaultFunctionId = config.intents.first().id;
    config.save(errorMessage);
    return config;
}

bool IntentPromptConfig::save(QString* errorMessage) const
{
    if (errorMessage) errorMessage->clear();
    QString error;
    if (!validateConfig(*this, &error)) {
        if (errorMessage) *errorMessage = error;
        return false;
    }

    const QFileInfo target(filePath());
    QDir directory(target.absolutePath());
    if (!directory.mkpath(QStringLiteral("."))) {
        if (errorMessage) *errorMessage = QStringLiteral("无法创建功能配置目录：%1").arg(directory.path());
        return false;
    }

    QJsonArray functions;
    for (const IntentDefinition& definition : intents) functions.append(definition.toJson());
    const QJsonObject object{
        {QStringLiteral("version"), 1},
        {QStringLiteral("default_function_id"), defaultFunctionId},
        {QStringLiteral("functions"), functions}
    };
    QSaveFile file(target.absoluteFilePath());
    if (!file.open(QIODevice::WriteOnly)
        || file.write(QJsonDocument(object).toJson(QJsonDocument::Indented)) < 0
        || !file.commit()) {
        if (errorMessage) *errorMessage = QStringLiteral("无法保存功能配置：%1").arg(file.errorString());
        return false;
    }

    removeLegacyFiles(target.absoluteFilePath());
    return true;
}

const IntentDefinition* IntentPromptConfig::findById(const QString& id) const
{
    for (const IntentDefinition& definition : intents) {
        if (definition.id == id) return &definition;
    }
    return nullptr;
}