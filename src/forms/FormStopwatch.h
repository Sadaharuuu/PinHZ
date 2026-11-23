#ifndef FORMSTOPWATCH_H
#define FORMSTOPWATCH_H
#include <QDialog>
#include <QElapsedTimer>
#include <QTimer>
namespace Ui {
class FormStopwatch;
}
class FormStopwatch : public QDialog {
    Q_OBJECT
public:
    explicit FormStopwatch(QWidget *parent = nullptr);
    ~FormStopwatch();
    qint64 elapsed() const;

private:
    Ui::FormStopwatch *ui;
    QElapsedTimer m_elapsedTimer;
    QTimer m_refreshTimer;
    qint64 m_accumulatedMs = 0, m_lastLapMs = 0;
    bool m_running = false;
    void refresh();
};
#endif
