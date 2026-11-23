#include "FormStopwatchDial.h"
#include <QPainter>
#include <QPalette>

FormStopwatchDial::FormStopwatchDial(QWidget *parent) : QWidget(parent) {
    setMinimumSize(240, 240);
}
QSize FormStopwatchDial::sizeHint() const {
    return QSize(280, 280);
}
void FormStopwatchDial::setElapsed(qint64 milliseconds) {
    m_elapsedMs = milliseconds;
    update();
}
void FormStopwatchDial::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal side = qMin(width(), height()) - 12;
    painter.translate(width() / 2.0, height() / 2.0);
    painter.scale(side / 240.0, side / 240.0);
    const auto colors = palette();
    const QColor text = colors.color(QPalette::WindowText);
    const QColor accent = colors.color(QPalette::Link);
    painter.setPen(QPen(colors.color(QPalette::Mid), 2));
    painter.setBrush(colors.color(QPalette::Base));
    painter.drawEllipse(QPointF(0, 0), 113, 113);
    for (int i = 0; i < 60; ++i) {
        painter.save();
        painter.rotate(i * 6);
        painter.setPen(QPen(i % 5 ? colors.color(QPalette::Mid) : text, i % 5 ? 1 : 2));
        painter.drawLine(QPointF(0, -105), QPointF(0, i % 5 ? -101 : -95));
        painter.restore();
    }
    QFont font = painter.font();
    font.setPixelSize(12);
    painter.setFont(font);
    painter.setPen(text);
    const QList<QPointF> positions{{0, -82}, {82, 0}, {0, 82}, {-82, 0}};
    const QStringList labels{"60", "15", "30", "45"};
    for (int i = 0; i < positions.size(); ++i)
        painter.drawText(QRectF(positions[i].x() - 14, positions[i].y() - 9, 28, 18),
                         Qt::AlignCenter, labels[i]);

    // Independent minute dial, driven by the same monotonic elapsed time.
    painter.save();
    painter.translate(0, -39);
    painter.setPen(QPen(colors.color(QPalette::Mid), 1));
    painter.drawEllipse(QPointF(0, 0), 25, 25);
    for (int i = 0; i < 12; ++i) {
        painter.save();
        painter.rotate(i * 30);
        painter.drawLine(QPointF(0, -23), QPointF(0, -20));
        painter.restore();
    }
    painter.save();
    painter.rotate((m_elapsedMs % 3600000) / 3600000.0 * 360);
    painter.setPen(QPen(text, 2, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(0, 4), QPointF(0, -17));
    painter.restore();
    painter.restore();
    painter.save();
    painter.rotate((m_elapsedMs % 60000) / 60000.0 * 360);
    painter.setPen(QPen(accent, 2, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(0, 15), QPointF(0, -90));
    painter.restore();
    painter.setPen(Qt::NoPen);
    painter.setBrush(accent);
    painter.drawEllipse(QPointF(0, 0), 4, 4);
}
