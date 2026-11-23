#include "FormTimeConvert.h"
#include "ui_FormTimeConvert.h"
#include <QHeaderView>
#include <QLineEdit>
#include <QScopedValueRollback>
#include <cmath>

namespace {
QDateTime epoch(int y, int m, int d) {
    return QDateTime(QDate(y, m, d), QTime(0, 0), Qt::UTC);
}
const QStringList names{"UTC 日期", "UTC 周", "GPS",  "BDS",       "GLONASS",
                        "JD",       "MJD",    "Unix", "偏移（秒）"};
int fieldCount(int row) {
    return row == 4 ? 3 : row == 1 || row == 2 || row == 3 || row == 8 ? 2 : 1;
}
QString number(double value, int digits = 3) {
    return QString::number(value, 'f', digits);
}
QStringList weekFields(double seconds) {
    double week = std::floor(seconds / 604800);
    return {number(week, 0), number(seconds - week * 604800)};
}
bool readNumber(const QString &text, double &v) {
    bool ok;
    v = text.toDouble(&ok);
    return ok && std::isfinite(v);
}
} // namespace
FormTimeConvert::FormTimeConvert(QWidget *parent) : QWidget(parent), ui(new Ui::FormTimeConvert) {
    ui->setupUi(this);
    auto *table = ui->table_time;
    table->setColumnCount(4);
    table->setRowCount(names.size());
    table->setHorizontalHeaderLabels(
        {"时间表示", "日期 / 周 / 周期", "周内秒 / 天", "日内秒 / 单位"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->hide();
    table->setMinimumHeight(380);
    for (int row = 0; row < names.size(); ++row) {
        table->setItem(row, 0, new QTableWidgetItem(names[row]));
        int count = fieldCount(row);
        if (count == 1)
            table->setSpan(row, 1, 1, 3);
        for (int part = 0; part < count; ++part) {
            auto *edit = new QLineEdit(table);
            edit->setObjectName(QString("edit_time_%1_%2").arg(row).arg(part));
            edit->setToolTip(row == 8 ? (part == 0 ? "GPS-UTC（整秒），历史日期请填写当时偏移"
                                                   : "BDS-UTC（整秒），历史日期请填写当时偏移")
                             : row == 1 ? "以顶部 UTC 起算日期为原点，周数为整数，周内秒 [0,604800)"
                             : row == 4 ? (part == 0   ? "四年周期 N4，从 1 开始"
                                           : part == 1 ? "周期内天数 Nt，从 1 开始"
                                                       : "日内秒 [0,86400)")
                             : row == 2 || row == 3 ? (part == 0 ? "完整周数，不自动处理截断周翻转"
                                                                 : "周内秒 [0,604800)")
                                                    : "直接编辑，实时换算其余行");
            table->setCellWidget(row, part + 1, edit);
            connect(edit, &QLineEdit::textChanged, this, [this, row] { timeEdited(row); });
        }
        if (count == 2)
            table->setItem(row, 3, new QTableWidgetItem(row == 8 ? "GPS-UTC / BDS-UTC" : "秒"));
    }
    ui->date_weekEpoch->setMinimumDate(QDate(1, 1, 1));
    ui->date_weekEpoch->setMaximumDate(QDate(9999, 12, 31));
    connect(ui->date_weekEpoch, &QDateEdit::dateChanged, this, [this] { timeEdited(8); });
    {
        QScopedValueRollback<bool> guard(m_updatingTime, true);
        qobject_cast<QLineEdit *>(table->cellWidget(8, 1))->setText("18");
        qobject_cast<QLineEdit *>(table->cellWidget(8, 2))->setText("4");
        qobject_cast<QLineEdit *>(table->cellWidget(0, 1))
            ->setText(QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss.zzz"));
    }
    timeEdited(0);
}
FormTimeConvert::~FormTimeConvert() {
    delete ui;
}
QStringList FormTimeConvert::fields(int row) const {
    QStringList result;
    for (int part = 0; part < fieldCount(row); ++part)
        result.append(qobject_cast<QLineEdit *>(ui->table_time->cellWidget(row, part + 1))->text());
    return result;
}
bool FormTimeConvert::calculate(QDateTime &utc, QString &error) const {
    auto offsets = fields(8);
    bool gpsOk, bdsOk;
    int gpsOffset = offsets[0].toInt(&gpsOk), bdsOffset = offsets[1].toInt(&bdsOk);
    if (!gpsOk || !bdsOk || gpsOffset < -100 || gpsOffset > 100 || bdsOffset < -100 ||
        bdsOffset > 100) {
        error = "偏移必须为 -100 到 100 的整秒。";
        return false;
    }
    const int row = m_timeSourceRow;
    const auto values = m_timeSourceValues;
    error = "输入无效：请检查日期、完整周、周内秒或周期范围；不自动猜测周计数翻转。";
    if (row == 0) {
        utc = QDateTime::fromString(values.value(0), "yyyy-MM-dd HH:mm:ss.zzz");
        if (!utc.isValid())
            utc = QDateTime::fromString(values.value(0), "yyyy-MM-dd HH:mm:ss");
        utc.setTimeSpec(Qt::UTC);
    } else {
        double a = 0, b = 0, c = 0;
        if (!readNumber(values.value(0), a))
            return false;
        if (fieldCount(row) > 1 && !readNumber(values.value(1), b))
            return false;
        if (fieldCount(row) > 2 && !readNumber(values.value(2), c))
            return false;
        if (row >= 1 && row <= 3) {
            if (std::abs(a) > 1000000 || a != std::trunc(a) || b < 0 || b >= 604800)
                return false;
            const auto start = row == 1
                                   ? QDateTime(ui->date_weekEpoch->date(), QTime(0, 0), Qt::UTC)
                               : row == 2 ? epoch(1980, 1, 6)
                                          : epoch(2006, 1, 1);
            const int offset = row == 2 ? gpsOffset : row == 3 ? bdsOffset : 0;
            utc = start.addMSecs(qRound64((a * 604800 + b - offset) * 1000));
        } else if (row == 4) {
            if (a < 1 || a > 2000 || a != std::trunc(a) || b < 1 || b > 1461 ||
                b != std::trunc(b) || c < 0 || c >= 86400)
                return false;
            const auto start = epoch(1996 + int(a - 1) * 4, 1, 1);
            if (b > start.daysTo(start.addYears(4)))
                return false;
            utc = start.addDays(qint64(b - 1)).addMSecs(qRound64((c - 10800) * 1000));
        } else {
            const double seconds = row == 5   ? (a - 2440587.5) * 86400
                                   : row == 6 ? (a - 40587) * 86400
                                              : a;
            if (std::abs(seconds) >= 1e13)
                return false;
            utc = QDateTime::fromMSecsSinceEpoch(qRound64(seconds * 1000), Qt::UTC);
        }
    }
    if (!utc.isValid() || utc.date().year() < 1 || utc.date().year() > 9999)
        return false;
    error.clear();
    return true;
}
void FormTimeConvert::timeEdited(int row) {
    if (m_updatingTime)
        return;
    QScopedValueRollback<bool> guard(m_updatingTime, true);
    if (row != 8) {
        m_timeSourceRow = row;
        m_timeSourceValues = fields(row);
    }
    QDateTime utc;
    QString error;
    calculate(utc, error);
    render(utc, error);
}
void FormTimeConvert::render(const QDateTime &utc, const QString &error) {
    const bool valid = error.isEmpty();
    QList<QStringList> rows;
    if (valid) {
        const auto offsets = fields(8);
        const double gps = epoch(1980, 1, 6).msecsTo(utc) / 1000.0 + offsets[0].toInt();
        const double bds = epoch(2006, 1, 1).msecsTo(utc) / 1000.0 + offsets[1].toInt();
        const double unixSeconds = utc.toMSecsSinceEpoch() / 1000.0;
        const auto gl = utc.addSecs(10800);
        int n4 = int(std::floor((gl.date().year() - 1996) / 4.0)) + 1;
        const auto start = epoch(1996 + (n4 - 1) * 4, 1, 1);
        rows = {
            QStringList{utc.toString("yyyy-MM-dd HH:mm:ss.zzz")},
            weekFields(QDateTime(ui->date_weekEpoch->date(), QTime(0, 0), Qt::UTC).msecsTo(utc) /
                       1000.0),
            weekFields(gps),
            weekFields(bds),
            QStringList{n4 >= 1 ? QString::number(n4) : QString("历元前"),
                        n4 >= 1 ? QString::number(start.daysTo(gl) + 1) : QString(),
                        n4 >= 1 ? number(QTime(0, 0).msecsTo(gl.time()) / 1000.0) : QString()},
            QStringList{number(unixSeconds / 86400 + 2440587.5, 9)},
            QStringList{number(unixSeconds / 86400 + 40587, 9)},
            QStringList{number(unixSeconds)},
            offsets};
    }
    for (int row = 0; row < names.size(); ++row)
        if (row != m_timeSourceRow && row != 8)
            for (int part = 0; part < fieldCount(row); ++part)
                qobject_cast<QLineEdit *>(ui->table_time->cellWidget(row, part + 1))
                    ->setText(valid ? rows[row].value(part) : QString());
    ui->label_timeStatus->setText(error);
    ui->label_timeStatus->setVisible(!error.isEmpty());
}
