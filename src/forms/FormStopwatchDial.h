#ifndef FORMSTOPWATCHDIAL_H
#define FORMSTOPWATCHDIAL_H
#include <QWidget>

class FormStopwatchDial : public QWidget {
public:
    explicit FormStopwatchDial(QWidget *parent = nullptr);
    void setElapsed(qint64 milliseconds);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    qint64 m_elapsedMs = 0;
};
#endif
