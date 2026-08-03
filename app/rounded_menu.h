#pragma once

#include <QMenu>

class RoundedMenu final : public QMenu
{
public:
    explicit RoundedMenu(QWidget* parent = nullptr)
        : QMenu(parent)
    {
        setWindowFlags(windowFlags()
            | Qt::FramelessWindowHint
            | Qt::NoDropShadowWindowHint);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_OpaquePaintEvent, false);
    }
};