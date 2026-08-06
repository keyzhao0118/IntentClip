#include "intent_prompt_config.h"

#include <QCoreApplication>
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
    return QCoreApplication::translate("IntentPromptConfig",
        "Execute the \"%1\" function on the provided content. "
        "Return a usable result directly and do not explain the process.").arg(name);
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
        *error = QCoreApplication::translate("IntentPromptConfig", "Function configuration is missing a valid id, name, description, or execution prompt.");
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
            *error = QCoreApplication::translate("IntentPromptConfig", "Function \"%1\" is incomplete.").arg(definition.name);
            return false;
        }
        if (ids.contains(definition.id) || names.contains(definition.name.trimmed())) {
            *error = QCoreApplication::translate("IntentPromptConfig", "Function ids and names must be unique.");
            return false;
        }
        ids.insert(definition.id);
        names.insert(definition.name.trimmed());
    }
    if (config.intents.isEmpty()) {
        if (!config.defaultFunctionId.isEmpty()) {
            *error = QCoreApplication::translate("IntentPromptConfig", "A default function cannot be set when there are no functions.");
            return false;
        }
    } else if (!ids.contains(config.defaultFunctionId)) {
        *error = QCoreApplication::translate("IntentPromptConfig", "The default function must reference a configured function.");
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
                QCoreApplication::translate("IntentPromptConfig", "Generate Reply"),
                QCoreApplication::translate("IntentPromptConfig",
                    "Handle emails, notifications, and chat messages."),
                QCoreApplication::translate("IntentPromptConfig",
                    "Draft a ready-to-send reply based on the original text. "
                    "Choose an appropriate tone for the context, cover every question, request, "
                    "or notice that needs a response, and do not invent facts.")
            },
            {
                QStringLiteral("polish_rewrite"),
                QCoreApplication::translate("IntentPromptConfig", "Polish & Rewrite"),
                QCoreApplication::translate("IntentPromptConfig",
                    "Improve the wording of documents, emails, reports, and notices."),
                QCoreApplication::translate("IntentPromptConfig",
                    "Polish the original text without changing its meaning or facts. "
                    "Improve wording, tone, structure, professionalism, and readability. "
                    "Output the complete revised text directly.")
            },
            {
                QStringLiteral("summarize_points"),
                QCoreApplication::translate("IntentPromptConfig", "Summarize Key Points"),
                QCoreApplication::translate("IntentPromptConfig",
                    "Condense long emails, meeting notes, and materials."),
                QCoreApplication::translate("IntentPromptConfig",
                    "Extract the core conclusions, key background, and main items from the "
                    "original text. Use concise bullet points and do not omit important "
                    "limitations or numbers.")
            },
            {
                QStringLiteral("explain_content"),
                QCoreApplication::translate("IntentPromptConfig", "Explain Content"),
                QCoreApplication::translate("IntentPromptConfig",
                    "Understand rules, terminology, complex requirements, and unfamiliar material."),
                QCoreApplication::translate("IntentPromptConfig",
                    "Explain the rules, terminology, and complex requirements in the original "
                    "text in plain, accurate language. Break down concepts when needed and "
                    "state their practical impact.")
            },
            {
                QStringLiteral("extract_information"),
                QCoreApplication::translate("IntentPromptConfig", "Extract Information"),
                QCoreApplication::translate("IntentPromptConfig",
                    "Organize dates, people, items, amounts, and contact details."),
                QCoreApplication::translate("IntentPromptConfig",
                    "Extract verifiable information from the original text and structure it "
                    "into fields such as date, place, people, items, amounts, contact details, "
                    "reference numbers, and deadlines. Do not invent missing fields.")
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
            if (errorMessage) *errorMessage = QCoreApplication::translate("IntentPromptConfig", "Could not read the function configuration: %1").arg(registry.errorString());
            return {};
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(registry.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            if (errorMessage) *errorMessage = QCoreApplication::translate("IntentPromptConfig", "The function configuration is not valid JSON: %1").arg(parseError.errorString());
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
            migrationError = QCoreApplication::translate("IntentPromptConfig", "Could not read the legacy function configuration: %1").arg(file.absoluteFilePath());
            break;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(input.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            migrationError = QCoreApplication::translate("IntentPromptConfig", "The legacy function configuration is not valid JSON: %1").arg(file.absoluteFilePath());
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
        if (errorMessage) *errorMessage = QCoreApplication::translate("IntentPromptConfig", "Could not create the function configuration directory: %1").arg(directory.path());
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
        if (errorMessage) *errorMessage = QCoreApplication::translate("IntentPromptConfig", "Could not save the function configuration: %1").arg(file.errorString());
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