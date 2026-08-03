#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

struct IntentDefinition
{
    QString id;
    bool persistent = false;
    QString name;
    QString description;
    QString recommendationPrompt;
    QString actionPrompt;

    QJsonObject toJson() const;
};

struct IntentPromptConfig
{
    QList<IntentDefinition> intents;

    static IntentPromptConfig defaults();
    static IntentPromptConfig load(QString* errorMessage = nullptr);
    static QString directoryPath();

    bool save(QString* errorMessage = nullptr) const;
    QJsonObject toJson() const;
    QList<IntentDefinition> persistentIntents() const;
    QList<IntentDefinition> customIntents() const;
    const IntentDefinition* findById(const QString& id) const;
};
