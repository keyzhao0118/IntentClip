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

bool legacyPersistentId(const QString& id)
{
    static const QSet<QString> ids = {
        QStringLiteral("generate_reply"),
        QStringLiteral("polish_rewrite"),
        QStringLiteral("summarize_points"),
        QStringLiteral("explain_content"),
        QStringLiteral("extract_information")
    };
    return ids.contains(id);
}

QString fallbackActionPrompt(const QString& id, const QString& name)
{
    for (const IntentDefinition& definition : IntentPromptConfig::defaults().intents) {
        if (definition.id == id) return definition.actionPrompt;
    }
    return QStringLiteral("根据用户提供的内容执行“%1”功能。直接给出可用结果，不要解释处理过程。").arg(name);
}

bool readDefinition(const QString& path, IntentDefinition* definition, bool* migrated, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("无法读取 %1：%2").arg(path, file.errorString());
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        *error = QStringLiteral("%1 不是合法 JSON：%2").arg(path, parseError.errorString());
        return false;
    }

    const QJsonObject object = document.object();
    definition->id = object.value(QStringLiteral("id")).toString().trimmed();
    const QString kind = object.value(QStringLiteral("kind")).toString();
    definition->persistent = kind == QStringLiteral("persistent")
        || (kind.isEmpty() && legacyPersistentId(definition->id));
    definition->name = object.value(QStringLiteral("name")).toString().trimmed();
    definition->description = object.value(QStringLiteral("description")).toString().trimmed();
    definition->recommendationPrompt = object.value(QStringLiteral("recommendation_prompt")).toString().trimmed();
    definition->actionPrompt = object.value(QStringLiteral("action_prompt")).toString().trimmed();

    *migrated = object.value(QStringLiteral("version")).toInt(1) < 2
        || kind.isEmpty() || definition->actionPrompt.isEmpty();
    if (definition->actionPrompt.isEmpty())
        definition->actionPrompt = fallbackActionPrompt(definition->id, definition->name);

    if (!isValidId(definition->id) || definition->name.isEmpty()
        || definition->description.isEmpty() || definition->actionPrompt.isEmpty()
        || (!definition->persistent && definition->recommendationPrompt.isEmpty())) {
        *error = QStringLiteral("%1 缺少合法的 id、名称、描述或必要提示词。").arg(path);
        return false;
    }
    return true;
}

} // namespace

QJsonObject IntentDefinition::toJson() const
{
    QJsonObject object{
        {QStringLiteral("version"), 2},
        {QStringLiteral("id"), id},
        {QStringLiteral("kind"), persistent ? QStringLiteral("persistent") : QStringLiteral("custom")},
        {QStringLiteral("name"), name},
        {QStringLiteral("description"), description},
        {QStringLiteral("action_prompt"), actionPrompt}
    };
    if (!persistent)
        object.insert(QStringLiteral("recommendation_prompt"), recommendationPrompt);
    return object;
}

IntentPromptConfig IntentPromptConfig::defaults()
{
    return {{
        {
            QStringLiteral("generate_reply"), true,
            QStringLiteral("生成回复"),
            QStringLiteral("处理邮件、通知、聊天消息。"),
            {},
            QStringLiteral("根据原文起草一份可直接发送的回复。结合上下文选择恰当语气，覆盖需要回应的问题、请求或通知，不要虚构事实。")
        },
        {
            QStringLiteral("polish_rewrite"), true,
            QStringLiteral("润色改写"),
            QStringLiteral("改善公文、邮件、汇报和通知的表达。"),
            {},
            QStringLiteral("在不改变原意和事实的前提下润色原文，改善措辞、语气、结构、专业性和可读性。直接输出完整改写稿。")
        },
        {
            QStringLiteral("summarize_points"), true,
            QStringLiteral("总结要点"),
            QStringLiteral("压缩长邮件、会议记录和材料。"),
            {},
            QStringLiteral("提炼原文的核心结论、关键背景和主要事项，使用简洁的分点结构输出，不遗漏重要限制与数字。")
        },
        {
            QStringLiteral("explain_content"), true,
            QStringLiteral("解释内容"),
            QStringLiteral("理解制度、术语、复杂要求和陌生材料。"),
            {},
            QStringLiteral("用通俗、准确的语言解释原文中的制度、术语和复杂要求；必要时拆解概念并说明实际影响。")
        },
        {
            QStringLiteral("extract_information"), true,
            QStringLiteral("提取信息"),
            QStringLiteral("整理时间、人员、事项、金额、联系方式等。"),
            {},
            QStringLiteral("从原文提取可核实的信息，按时间、地点、人员、事项、金额、联系方式、编号和截止日期等字段结构化整理；没有的字段不要编造。")
        }
    }};
}

QString IntentPromptConfig::directoryPath()
{
    const QString overridden = qEnvironmentVariable("INTENTCLIP_INTENTS_DIR");
    if (!overridden.isEmpty()) return QDir::cleanPath(overridden);
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
        .filePath(QStringLiteral("intents"));
}

IntentPromptConfig IntentPromptConfig::load(QString* errorMessage)
{
    if (errorMessage) errorMessage->clear();
    QDir directory(directoryPath());
    if (!directory.exists()) {
        IntentPromptConfig initial = defaults();
        initial.save(errorMessage);
        return initial;
    }

    const QDir persistentDirectory(directory.filePath(QStringLiteral("persistent")));
    const QDir customDirectory(directory.filePath(QStringLiteral("custom")));
    QFileInfoList files = persistentDirectory.entryInfoList(
        {QStringLiteral("*.json")}, QDir::Files | QDir::Readable, QDir::Name);
    files.append(customDirectory.entryInfoList(
        {QStringLiteral("*.json")}, QDir::Files | QDir::Readable, QDir::Name));
    // Read legacy flat files last. Nested files win if a previous migration was interrupted.
    files.append(directory.entryInfoList(
        {QStringLiteral("*.json")}, QDir::Files | QDir::Readable, QDir::Name));
    if (files.isEmpty()) {
        if (QFile::exists(directory.filePath(QStringLiteral(".initialized"))))
            return {};
        IntentPromptConfig initial = defaults();
        initial.save(errorMessage);
        return initial;
    }

    IntentPromptConfig config;
    QStringList errors;
    QSet<QString> ids;
    QSet<QString> names;
    bool needsMigration = false;
    for (const QFileInfo& file : files) {
        IntentDefinition definition;
        bool migrated = false;
        QString error;
        if (!readDefinition(file.absoluteFilePath(), &definition, &migrated, &error)) {
            errors.append(error);
            continue;
        }
        const bool legacyFlatFile = file.dir().absolutePath() == directory.absolutePath();
        if ((ids.contains(definition.id) || names.contains(definition.name)) && legacyFlatFile) {
            needsMigration = true;
            continue;
        }
        if (ids.contains(definition.id) || names.contains(definition.name)) {
            errors.append(QStringLiteral("%1 的 id 或名称与其他功能重复。").arg(file.absoluteFilePath()));
            continue;
        }
        ids.insert(definition.id);
        names.insert(definition.name);
        config.intents.append(definition);
        const QString expectedDirectory = definition.persistent
            ? persistentDirectory.absolutePath() : customDirectory.absolutePath();
        needsMigration = needsMigration || migrated || legacyFlatFile
            || file.dir().absolutePath() != expectedDirectory;
    }
    if (!errors.isEmpty() && errorMessage) *errorMessage = errors.join(QStringLiteral("\n"));
    if (config.intents.isEmpty() && errors.isEmpty()) {
        config = defaults();
        config.save(errorMessage);
    } else if (needsMigration && errors.isEmpty()) {
        config.save(errorMessage);
    }
    return config;
}

bool IntentPromptConfig::save(QString* errorMessage) const
{
    if (errorMessage) errorMessage->clear();
    QDir directory(directoryPath());
    if (!directory.mkpath(QStringLiteral("persistent"))
        || !directory.mkpath(QStringLiteral("custom"))) {
        if (errorMessage) *errorMessage = QStringLiteral("无法创建功能配置目录：%1").arg(directory.path());
        return false;
    }
    QDir persistentDirectory(directory.filePath(QStringLiteral("persistent")));
    QDir customDirectory(directory.filePath(QStringLiteral("custom")));

    QSaveFile marker(directory.filePath(QStringLiteral(".initialized")));
    if (!marker.open(QIODevice::WriteOnly) || marker.write("3\n") < 0 || !marker.commit()) {
        if (errorMessage) *errorMessage = QStringLiteral("无法写入功能配置初始化标记。");
        return false;
    }

    QSet<QString> ids;
    QSet<QString> names;
    QSet<QString> expectedPersistentFiles;
    QSet<QString> expectedCustomFiles;
    for (const IntentDefinition& definition : intents) {
        if (!isValidId(definition.id) || definition.name.trimmed().isEmpty()
            || definition.description.trimmed().isEmpty() || definition.actionPrompt.trimmed().isEmpty()
            || (!definition.persistent && definition.recommendationPrompt.trimmed().isEmpty())) {
            if (errorMessage) *errorMessage = QStringLiteral("功能“%1”的配置不完整。").arg(definition.name);
            return false;
        }
        if (ids.contains(definition.id) || names.contains(definition.name.trimmed())) {
            if (errorMessage) *errorMessage = QStringLiteral("功能 id 或名称不能重复。");
            return false;
        }
        ids.insert(definition.id);
        names.insert(definition.name.trimmed());
        const QString fileName = definition.id + QStringLiteral(".json");
        QDir& targetDirectory = definition.persistent ? persistentDirectory : customDirectory;
        QSet<QString>& expectedFiles = definition.persistent
            ? expectedPersistentFiles : expectedCustomFiles;
        expectedFiles.insert(fileName);

        QSaveFile file(targetDirectory.filePath(fileName));
        if (!file.open(QIODevice::WriteOnly)
            || file.write(QJsonDocument(definition.toJson()).toJson(QJsonDocument::Indented)) < 0
            || !file.commit()) {
            if (errorMessage) *errorMessage = QStringLiteral("无法保存功能“%1”。").arg(definition.name);
            return false;
        }
    }

    const auto removeStaleFiles = [errorMessage](
        const QDir& targetDirectory, const QSet<QString>& expectedFiles) {
        const QFileInfoList existing = targetDirectory.entryInfoList(
            {QStringLiteral("*.json")}, QDir::Files, QDir::Name);
        for (const QFileInfo& file : existing) {
            if (!expectedFiles.contains(file.fileName()) && !QFile::remove(file.absoluteFilePath())) {
                if (errorMessage) *errorMessage = QStringLiteral("无法删除旧功能配置：%1")
                    .arg(file.absoluteFilePath());
                return false;
            }
        }
        return true;
    };
    if (!removeStaleFiles(persistentDirectory, expectedPersistentFiles)
        || !removeStaleFiles(customDirectory, expectedCustomFiles)) return false;

    // Remove v2 files from the legacy flat layout only after every nested file is saved.
    const QFileInfoList legacyFiles = directory.entryInfoList(
        {QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QFileInfo& file : legacyFiles) {
        if (!QFile::remove(file.absoluteFilePath())) {
            if (errorMessage) *errorMessage = QStringLiteral("无法迁移旧功能配置：%1")
                .arg(file.absoluteFilePath());
            return false;
        }
    }
    return true;
}
QJsonObject IntentPromptConfig::toJson() const
{
    QJsonArray array;
    for (const IntentDefinition& definition : intents) array.append(definition.toJson());
    return {{QStringLiteral("intents"), array}};
}

QList<IntentDefinition> IntentPromptConfig::persistentIntents() const
{
    QList<IntentDefinition> result;
    for (const IntentDefinition& definition : intents) {
        if (definition.persistent) result.append(definition);
    }
    return result;
}

QList<IntentDefinition> IntentPromptConfig::customIntents() const
{
    QList<IntentDefinition> result;
    for (const IntentDefinition& definition : intents) {
        if (!definition.persistent) result.append(definition);
    }
    return result;
}

const IntentDefinition* IntentPromptConfig::findById(const QString& id) const
{
    for (const IntentDefinition& definition : intents) {
        if (definition.id == id) return &definition;
    }
    return nullptr;
}
