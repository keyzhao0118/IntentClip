#pragma once

#include <QColor>
#include <QPainter>
#include <QPen>
#include <QRectF>
#include <QTimer>
#include <QWidget>

// 圆形加载指示器：由 8 段透明度渐变的圆弧构成，持续旋转。
class CircularSpinner final : public QWidget
{
public:
    explicit CircularSpinner(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(22, 22);
        rotationTimer_.setInterval(80);
        connect(&rotationTimer_, &QTimer::timeout, this, [this] {
            startAngle_ = (startAngle_ + 40) % 360;
            update();
        });
    }

    void startSpinning() { rotationTimer_.start(); }
    void stopSpinning() { rotationTimer_.stop(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF bounds(2.5, 2.5, width() - 5.0, height() - 5.0);
        const int segmentCount = 8;
        const int arcSpan = 40;
        for (int i = 0; i < segmentCount; ++i) {
            const int alpha = 255 - i * 30;
            painter.setPen(QPen(QColor(26, 28, 31, alpha), 2.5,
                Qt::SolidLine, Qt::RoundCap));
            painter.drawArc(bounds, (startAngle_ + i * 45) * 16, arcSpan * 16);
        }
    }

private:
    QTimer rotationTimer_;
    int startAngle_ = 0;
};
