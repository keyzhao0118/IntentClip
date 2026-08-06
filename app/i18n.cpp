#include "i18n.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QTranslator>

namespace {

QString matchLocale(const QString& baseName)
{
    const QString directory = appTranslationsDirectory();
    for (const QString& localeName : QLocale::system().uiLanguages()) {
        const QString locale = QLocale(localeName).name();
        const QString language = locale.section(QLatin1Char('_'), 0, 0);
        const QString fullPath = directory + QLatin1Char('/')
            + baseName + QLatin1Char('_') + locale + QStringLiteral(".qm");
        if (QFileInfo::exists(fullPath)) return locale;
        const QString languagePath = directory + QLatin1Char('/')
            + baseName + QLatin1Char('_') + language + QStringLiteral(".qm");
        if (QFileInfo::exists(languagePath)) return language;
    }
    return {};
}

void installCatalog(QCoreApplication& application, const QString& baseName)
{
    const QString locale = matchLocale(baseName);
    if (locale.isEmpty()) return;
    auto* translator = new QTranslator(&application);
    if (translator->load(
            baseName + QLatin1Char('_') + locale,
            appTranslationsDirectory())) {
        application.installTranslator(translator);
    } else {
        delete translator;
    }
}

} // namespace

QString appTranslationsDirectory()
{
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("translations"));
}

QString appTranslationLocale()
{
    return matchLocale(QStringLiteral("IntentClip"));
}

void installAppTranslations(QCoreApplication& application)
{
    // 先安装 Qt 自带翻译（标准按钮、消息框等），再安装应用翻译，
    // 后安装的翻译优先，因此应用翻译可以覆盖 Qt 默认文案。
    // windeployqt/安装脚本生成的是合并后的 qt_<locale>.qm，
    // 部分环境也可能是 qtbase_<locale>.qm，两种都尝试。
    installCatalog(application, QStringLiteral("qt"));
    installCatalog(application, QStringLiteral("qtbase"));
    installCatalog(application, QStringLiteral("IntentClip"));
}
