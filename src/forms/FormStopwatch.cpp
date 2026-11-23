#include "FormStopwatch.h"
#include "ui_FormStopwatch.h"
#include <QApplication>
#include <QClipboard>
namespace {
QString duration(qint64 ms) {
    return QString("%1:%2:%3.%4")
        .arg(ms / 3600000, 2, 10, QChar('0'))
        .arg(ms / 60000 % 60, 2, 10, QChar('0'))
        .arg(ms / 1000 % 60, 2, 10, QChar('0'))
        .arg(ms % 1000, 3, 10, QChar('0'));
}
} // namespace
FormStopwatch::FormStopwatch(QWidget *parent) : QDialog(parent), ui(new Ui::FormStopwatch) {
    ui->setupUi(this);
    connect(&m_refreshTimer, &QTimer::timeout, this, &FormStopwatch::refresh);
    m_refreshTimer.setInterval(30);
    connect(ui->button_start, &QPushButton::clicked, this, [this] {
        if (m_running) {
            m_accumulatedMs += m_elapsedTimer.elapsed();
            m_running = false;
            m_refreshTimer.stop();
        } else {
            m_elapsedTimer.start();
            m_running = true;
            m_refreshTimer.start();
        }
        refresh();
    });
    connect(ui->button_reset, &QPushButton::clicked, this, [this] {
        m_running = false;
        m_refreshTimer.stop();
        m_accumulatedMs = 0;
        m_lastLapMs = 0;
        ui->list_laps->clear();
        refresh();
    });
    connect(ui->button_lap, &QPushButton::clicked, this, [this] {
        const qint64 now = elapsed();
        ui->list_laps->addItem(QString("%1    累计 %2    分段 %3")
                                   .arg(ui->list_laps->count() + 1)
                                   .arg(duration(now), duration(now - m_lastLapMs)));
        m_lastLapMs = now;
    });
    connect(ui->button_copy, &QPushButton::clicked, this, [this] {
        QString text = duration(elapsed());
        for (int i = 0; i < ui->list_laps->count(); ++i)
            text += '\n' + ui->list_laps->item(i)->text();
        QApplication::clipboard()->setText(text);
    });
    refresh();
}
FormStopwatch::~FormStopwatch() {
    delete ui;
}
qint64 FormStopwatch::elapsed() const {
    return m_accumulatedMs + (m_running ? m_elapsedTimer.elapsed() : 0);
}
void FormStopwatch::refresh() {
    const qint64 milliseconds = elapsed();
    ui->label_elapsed->setText(duration(milliseconds));
    ui->dial_stopwatch->setElapsed(milliseconds);
    ui->button_start->setText(m_running ? "暂停" : m_accumulatedMs ? "继续" : "开始");
    ui->button_lap->setEnabled(m_running);
}
