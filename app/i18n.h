#pragma once

#include <QString>

class QCoreApplication;

// 应用翻译目录：可执行文件旁的 translations/。
QString appTranslationsDirectory();

// 当前应用实际使用的翻译后缀，如 "zh_CN"、"en"；未找到时返回空串。
QString appTranslationLocale();

// 安装 Qt 自带翻译与应用翻译；必须在创建任何界面之前调用。
void installAppTranslations(QCoreApplication& application);
