#include "function_settings_dialog.h"

#include "prompt_settings_dialog.h"
#include "rounded_menu.h"

#include <QCoreApplication>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLayoutItem>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

namespace {
QString functionButtonText(const QString& name, bool isDefault)
{
    return isDefault
        ? QCoreApplication::translate("FunctionSettingsDialog", "%1 · Default").arg(name)
        : name;
}

QIcon createPlusIcon()
{
    QPixmap pixmap(18, 18);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(QStringLiteral("#75777a")), 2, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(9, 2, 9, 16);
    painter.drawLine(2, 9, 16, 9);
    return QIcon(pixmap);
}
}

FunctionSettingsDialog::FunctionSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Function Settings"));
    setObjectName(QStringLiteral("functionSettingsDialog"));
    setFixedSize(700, 540);

    QString loadError;
    config_ = IntentPromptConfig::load(&loadError);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 28, 30, 26);
    layout->setSpacing(16);

    auto* explanation = new QLabel(
        loadError.isEmpty()
            ? tr("Manage the function matrix: one card per function. Right-click to edit, set as default, or delete.")
            : tr("Configuration error: %1").arg(loadError),
        this);
    explanation->setObjectName(QStringLiteral("dialogExplanation"));
    explanation->setWordWrap(true);
    layout->addWidget(explanation);

    auto* header = new QHBoxLayout;
    auto* title = new QLabel(tr("Function Matrix"), this);
    title->setObjectName(QStringLiteral("sectionTitle"));
    header->addWidget(title);
    header->addStretch();
    layout->addLayout(header);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    optionsFrame_ = new QFrame(scrollArea);
    optionsFrame_->setObjectName(QStringLiteral("optionsFrame"));
    optionsLayout_ = new QGridLayout(optionsFrame_);
    optionsLayout_->setContentsMargins(0, 0, 6, 0);
    optionsLayout_->setHorizontalSpacing(12);
    optionsLayout_->setVerticalSpacing(12);
    for (int column = 0; column < 3; ++column) optionsLayout_->setColumnStretch(column, 1);
    scrollArea->setWidget(optionsFrame_);
    layout->addWidget(scrollArea, 1);

    emptyLabel_ = new QLabel(tr("No functions configured yet. Click \"Add Function\" to create one."), this);
    emptyLabel_->setObjectName(QStringLiteral("sectionHint"));
    emptyLabel_->setAlignment(Qt::AlignCenter);
    layout->addWidget(emptyLabel_);

    auto* bottom = new QHBoxLayout;
    bottom->addStretch();
    auto* restoreButton = new QPushButton(tr("Restore Defaults"), this);
    bottom->addWidget(restoreButton);
    layout->addLayout(bottom);

    connect(restoreButton, &QPushButton::clicked, this, &FunctionSettingsDialog::restoreDefaultFunctions);

    refreshFunctionButtons();

    setStyleSheet(QStringLiteral(R"(
        QDialog#functionSettingsDialog {
            color: #1a1c1f;
            background-color: #f9f9f9;
            font-family: "Segoe UI", "Microsoft YaHei UI";
            font-size: 13px;
        }
        QLabel { color: #1a1c1f; font-weight: 600; }
        QLabel#dialogExplanation {
            color: #5f6062; background-color: #f7f7f7;
            border: 1px solid #e4e4e4; border-radius: 10px;
            padding: 12px 14px; font-weight: 400;
        }
        QLabel#sectionHint { color: #6d6f72; font-size: 12px; font-weight: 400; margin: 24px 0; }
        QLabel#sectionTitle { color: #1a1c1f; font-size: 17px; font-weight: 700; }
        QFrame#optionsFrame { background: transparent; border: none; }
        QScrollArea { background: transparent; border: none; }
        QToolButton#functionSettingsButton {
            color: #5f6062; background-color: #f7f7f7;
            border: 1px solid #e4e4e4; border-radius: 10px;
            padding: 7px 12px; font-size: 13px; font-weight: 550;
            min-height: 40px;
            text-align: center;
        }
        QToolButton#functionSettingsButton:hover {
            color: #1a1c1f; border-color: #e4e4e4; background-color: #f0f1f2;
        }
        QToolButton#functionSettingsButton:pressed { background-color: #e6e7e8; }
        QToolButton#addFunctionButton {
            color: #7a7d80; background-color: #ffffff;
            border: 1px solid #d6d8da; border-radius: 10px;
            padding: 7px 12px; font-size: 13px; font-weight: 600;
            min-height: 40px;
        }
        QToolButton#addFunctionButton:hover {
            color: #1a1c1f; background-color: #f0f1f2; border-color: #9b9ea1;
        }
        QToolButton#addFunctionButton:pressed { background-color: #e6e7e8; }
        QPushButton {
            min-width: 88px; padding: 9px 18px;
            color: #1a1c1f; background-color: #ffffff;
            border: 1px solid #e4e4e4; border-radius: 8px;
            font-weight: 600;
        }
        QPushButton:hover { background-color: #f0f1f2; border-color: #c9cbcb; }
        QPushButton:pressed { background-color: #e6e7e8; }
        QPushButton:default {
            color: #ffffff; background-color: #1a1c1f;
            border-color: #1a1c1f;
        }
        QPushButton:default:hover { background-color: #2e3134; border-color: #2e3134; }
        QScrollBar:vertical { background: transparent; width: 8px; }
        QScrollBar::handle:vertical { background: #c9cbcb; border-radius: 4px; min-height: 28px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QMenu {
            color: #1a1c1f; background: #ffffff; border: 1px solid #e4e4e4;
            border-radius: 9px; padding: 6px;
        }
        QMenu::item { padding: 8px 22px 8px 12px; border-radius: 6px; }
        QMenu::item:selected { color: #1a1c1f; background: #f0f1f2; }
        QMenu::separator { height: 1px; background: #e4e4e4; margin: 5px 8px; }
    )"));
}

void FunctionSettingsDialog::refreshFunctionButtons()
{
    while (QLayoutItem* item = optionsLayout_->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    functionButtons_.clear();

    for (const IntentDefinition& definition : config_.intents) {
        auto* button = new QToolButton(optionsFrame_);
        button->setObjectName(QStringLiteral("functionSettingsButton"));
        button->setContextMenuPolicy(Qt::CustomContextMenu);
        button->setProperty("intentId", definition.id);
        button->setProperty("intentName", definition.name);
        const bool isDefault = definition.id == config_.defaultFunctionId;
        button->setProperty("isDefault", isDefault);
        button->setText(functionButtonText(definition.name, isDefault));
        button->setToolTip(definition.description + tr("\nRight-click to edit, set as default, or delete"));
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setFixedSize(200, 52);
        connect(button, &QToolButton::customContextMenuRequested, this,
            [this, button](const QPoint& position) {
                const QString functionId = button->property("intentId").toString();
                const IntentDefinition* current = config_.findById(functionId);
                if (!current) return;
                RoundedMenu menu(this);
                QAction* editAction = menu.addAction(tr("Edit Function"));
                QAction* defaultAction = menu.addAction(
                    current->id == config_.defaultFunctionId
                        ? tr("Current Default Function") : tr("Set as Default Function"));
                defaultAction->setEnabled(!(current->id == config_.defaultFunctionId));
                menu.addSeparator();
                QAction* deleteAction = menu.addAction(tr("Delete Function"));
                QAction* selected = menu.exec(button->mapToGlobal(position));
                if (selected == editAction) editFunction(functionId);
                else if (selected == defaultAction) setDefaultFunction(functionId);
                else if (selected == deleteAction) deleteFunction(functionId);
            });
        const int position = static_cast<int>(functionButtons_.size());
        functionButtons_.append(button);
        optionsLayout_->addWidget(button, position / 3, position % 3, Qt::AlignCenter);
    }

    auto* addButton = new QToolButton(optionsFrame_);
    addButton->setObjectName(QStringLiteral("addFunctionButton"));
    addButton->setText(tr("Add Function"));
    addButton->setToolTip(tr("Add a new AI function"));
    addButton->setIcon(createPlusIcon());
    addButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    addButton->setFixedSize(200, 52);
    connect(addButton, &QToolButton::clicked, this, &FunctionSettingsDialog::addFunction);
    const int addPosition = static_cast<int>(functionButtons_.size());
    optionsLayout_->addWidget(addButton, addPosition / 3, addPosition % 3, Qt::AlignCenter);

    emptyLabel_->setVisible(functionButtons_.isEmpty());
    optionsFrame_->setVisible(true);
    optionsFrame_->adjustSize();
}

bool FunctionSettingsDialog::saveConfig()
{
    QString error;
    if (!config_.save(&error)) {
        QMessageBox::warning(this, tr("Invalid Configuration"), error);
        return false;
    }
    return true;
}

void FunctionSettingsDialog::addFunction()
{
    IntentDefinition definition{
        QStringLiteral("function_") + QUuid::createUuid().toString(QUuid::Id128).left(16),
        tr("New Function"),
        tr("Write a one-sentence description of the function."),
        tr("Explain what the local model should do with the Content when this function runs.")
    };
    PromptSettingsDialog dialog(definition, this);
    if (dialog.exec() != QDialog::Accepted) return;

    const IntentPromptConfig previous = config_;
    config_.intents.append(dialog.definition());
    if (config_.defaultFunctionId.isEmpty())
        config_.defaultFunctionId = config_.intents.constLast().id;
    if (!saveConfig()) {
        config_ = previous;
        return;
    }
    refreshFunctionButtons();
}

void FunctionSettingsDialog::editFunction(const QString& functionId)
{
    for (IntentDefinition& definition : config_.intents) {
        if (definition.id != functionId) continue;
        PromptSettingsDialog dialog(definition, this);
        if (dialog.exec() != QDialog::Accepted) return;
        definition = dialog.definition();
        if (!saveConfig()) return;
        refreshFunctionButtons();
        return;
    }
}

void FunctionSettingsDialog::deleteFunction(const QString& functionId)
{
    const IntentDefinition* target = config_.findById(functionId);
    if (!target) return;
    if (QMessageBox::question(this, tr("Delete Function"),
            tr("Delete \"%1\"?").arg(target->name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;

    const bool deletedDefault = config_.defaultFunctionId == functionId;
    for (auto iterator = config_.intents.begin(); iterator != config_.intents.end(); ++iterator) {
        if (iterator->id != functionId) continue;
        config_.intents.erase(iterator);
        break;
    }
    if (deletedDefault)
        config_.defaultFunctionId = config_.intents.isEmpty()
            ? QString() : config_.intents.first().id;
    if (!saveConfig()) return;
    refreshFunctionButtons();
}

void FunctionSettingsDialog::setDefaultFunction(const QString& functionId)
{
    if (!config_.findById(functionId) || config_.defaultFunctionId == functionId) return;
    config_.defaultFunctionId = functionId;
    if (!saveConfig()) return;
    refreshFunctionButtons();
}

void FunctionSettingsDialog::restoreDefaultFunctions()
{
    if (QMessageBox::question(this, tr("Restore Default Functions"),
            tr("This will delete your custom functions and restore the 5 built-in functions. Continue?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    config_ = IntentPromptConfig::defaults();
    if (!saveConfig()) return;
    refreshFunctionButtons();
}