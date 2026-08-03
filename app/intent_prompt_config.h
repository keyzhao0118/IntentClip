#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

struct IntentDefinition
{
    QString id;
    QString name;
    QString description;
    QString actionPrompt;

    QJsonObject toJson() const;
};

struct IntentPromptConfig
{
    QString defaultFunctionId;
    QList<IntentDefinition> intents;

    static IntentPromptConfig defaults();
    static IntentPromptConfig load(QString* errorMessage = nullptr);
    static QString filePath();

    bool save(QString* errorMessage = nullptr) const;
    const IntentDefinition* findById(const QString& id) const;
};