#pragma once

#include <QJsonObject>
#include <QString>

struct IntentPromptConfig
{
    QString systemPrompt;
    QString userPromptTemplate;
    int minimumIntents = 1;
    int maximumIntents = 5;

    static IntentPromptConfig defaults();
    static IntentPromptConfig load(QString* errorMessage = nullptr);
    static QString filePath();

    bool save(QString* errorMessage = nullptr) const;
    QJsonObject toJson() const;
};
