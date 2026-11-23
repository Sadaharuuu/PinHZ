#include "FormNavigation.h"
#include "ui_FormNavigation.h"
#include <QHeaderView>
#include <QRegularExpression>
#include <QScopedValueRollback>
#include <cmath>
namespace {
QString dmsText(double value) {
    const QString sign = std::signbit(value) ? "-" : "";
    value = std::abs(value);
    double degree = std::floor(value), minute = std::floor((value - degree) * 60),
           second = std::round(((value - degree) * 60 - minute) * 60 * 1e8) / 1e8;
    if (second >= 60) {
        second = 0;
        ++minute;
    }
    if (minute >= 60) {
        minute = 0;
        ++degree;
    }
    QString seconds = QString::number(second, 'f', 8);
    seconds.remove(QRegularExpression("\\.?0+$"));
    return QString("%1%2°%3'%4\"")
        .arg(sign)
        .arg(degree, 0, 'f', 0)
        .arg(minute, 0, 'f', 0)
        .arg(seconds);
}
bool readDms(QString text, double &value) {
    const bool negative = text.trimmed().startsWith('-');
    const auto parts = text.split(QRegularExpression("[\\s:°′″'\"]+"), QString::SkipEmptyParts);
    bool ok = false;
    if (parts.size() == 1) {
        value = parts[0].toDouble(&ok);
        return ok && std::isfinite(value);
    }
    if (parts.size() != 3)
        return false;
    double d = parts[0].toDouble(&ok);
    if (!ok || !std::isfinite(d) || d != std::trunc(d))
        return false;
    double m = parts[1].toDouble(&ok);
    if (!ok || m < 0 || m >= 60)
        return false;
    double s = parts[2].toDouble(&ok);
    if (!ok || !std::isfinite(s) || s < 0 || s >= 60)
        return false;
    value = (negative ? -1 : 1) * (std::abs(d) + m / 60 + s / 3600);
    return true;
}
} // namespace

namespace {
const double radiansPerDegree = 3.14159265358979323846 / 180;
QString number(double v) {
    return QString::number(v, 'g', 16);
}
bool readNumber(const QString &text, double &v) {
    bool ok;
    v = text.toDouble(&ok);
    return ok && std::isfinite(v);
}
} // namespace
FormNavigation::FormNavigation(QWidget *parent) : QDialog(parent), ui(new Ui::FormNavigation) {
    ui->setupUi(this);
    auto *table = ui->table_coordinates;
    table->setColumnCount(5);
    table->setRowCount(3);
    table->setHorizontalHeaderLabels(
        {"XYZ (m)", "BLH（角度 / 椭球高）", "BLH（弧度 / 椭球高）", "BLH（海拔高）", "ENU (m)"});
    table->setVerticalHeaderLabels({"X / B / E", "Y / L / N", "Z / H / U"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setFixedHeight(158);
    for (int col = 0; col < 5; ++col)
        for (int row = 0; row < 3; ++row) {
            auto *edit = new QLineEdit(table);
            edit->setObjectName(QString("edit_coord_%1_%2").arg(col).arg(row));
            edit->setToolTip(row < 2 && (col == 1 || col == 3)
                                 ? "经纬度格式由上方下拉框选择；度分秒示例：-0°30'0\""
                                 : "直接编辑此列，实时换算其余列");
            table->setCellWidget(row, col, edit);
            connect(edit, &QLineEdit::textChanged, this, [this, col] { coordinateEdited(col); });
        }
    for (auto *edit : {ui->edit_origin1, ui->edit_origin2, ui->edit_origin3})
        connect(edit, &QLineEdit::textChanged, this, &FormNavigation::originEdited);
    connect(ui->combo_angleFormat,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [this] { angleFormatChanged(); });
    connect(ui->button_useCurrent, &QPushButton::clicked, this, [this] {
        if (!m_hasCoordinates)
            return;
        {
            QScopedValueRollback<bool> guard(m_updatingCoordinates, true);
            bool dms = ui->combo_angleFormat->currentIndex() == 1;
            ui->edit_origin1->setText(dms ? dmsText(m_blh.x) : number(m_blh.x));
            ui->edit_origin2->setText(dms ? dmsText(m_blh.y) : number(m_blh.y));
            ui->edit_origin3->setText(number(m_blh.z));
        }
        // Hold the absolute point fixed when choosing a new local origin.
        m_sourceColumn = 0;
        fillCoordinates(-1);
    });
    connect(ui->edit_angle, &QLineEdit::textChanged, this, [this] { angleEdited(false); });
    connect(ui->edit_radians, &QLineEdit::textChanged, this, [this] { angleEdited(true); });
    {
        QScopedValueRollback<bool> guard(m_updatingCoordinates, true);
        QStringList initial{"-1341903.767", "5340992.644", "3207884.437"};
        for (int row = 0; row < 3; ++row)
            qobject_cast<QLineEdit *>(table->cellWidget(row, 0))->setText(initial[row]);
    }
    coordinateEdited(0);
    ui->button_useCurrent->click();
    ui->edit_angle->setText("0");
}
FormNavigation::~FormNavigation() {
    delete ui;
}
bool FormNavigation::readOrigin(navigation::Vec3 &origin, double rotation[9]) const {
    navigation::Vec3 blh;
    if (!readDms(ui->edit_origin1->text(), blh.x) || !readDms(ui->edit_origin2->text(), blh.y) ||
        !readNumber(ui->edit_origin3->text(), blh.z))
        return false;
    blh.y = std::remainder(blh.y, 360.0);
    return navigation::toEcef(blh, origin) && navigation::localFrame(origin, rotation);
}
void FormNavigation::coordinateEdited(int column) {
    if (m_updatingCoordinates)
        return;
    QScopedValueRollback<bool> guard(m_updatingCoordinates, true);
    m_sourceColumn = column;
    double v[3]{};
    bool valid = true;
    for (int row = 0; row < 3; ++row) {
        QString text =
            qobject_cast<QLineEdit *>(ui->table_coordinates->cellWidget(row, column))->text();
        bool dms =
            (column == 1 || column == 3) && row < 2 && ui->combo_angleFormat->currentIndex() == 1;
        valid = (dms ? readDms(text, v[row]) : readNumber(text, v[row])) && valid;
    }
    navigation::Vec3 xyz{}, blh{v[0], v[1], v[2]};
    if (valid && column == 4) {
        navigation::Vec3 origin;
        double rotation[9];
        valid = readOrigin(origin, rotation);
        if (valid) {
            xyz = navigation::rotate(rotation, blh, true);
            xyz = {xyz.x + origin.x, xyz.y + origin.y, xyz.z + origin.z};
            valid = navigation::toGeodetic(xyz, blh);
        }
    } else if (valid && column == 0) {
        xyz = blh;
        valid = navigation::toGeodetic(xyz, blh);
    } else if (valid) {
        if (column == 2) {
            blh.x /= radiansPerDegree;
            blh.y /= radiansPerDegree;
        }
        blh.y = std::remainder(blh.y, 360.0);
        if (column == 3) {
            double n;
            valid = navigation::geoid(blh.x, blh.y, n);
            if (valid)
                blh.z += n;
        }
        if (valid)
            valid = navigation::toEcef(blh, xyz);
    }
    m_hasCoordinates = valid;
    ui->button_useCurrent->setEnabled(valid);
    if (!valid) {
        for (int col = 0; col < 5; ++col)
            if (col != column)
                for (int row = 0; row < 3; ++row)
                    qobject_cast<QLineEdit *>(ui->table_coordinates->cellWidget(row, col))->clear();
        ui->label_coordinateStatus->setText(
            "输入尚未有效。请检查经纬度、度分秒格式；ENU 输入还需要有效参考点。分秒范围 [0,60)。");
        return;
    }
    m_xyz = xyz;
    m_blh = blh;
    fillCoordinates(column);
}
void FormNavigation::fillCoordinates(int sourceColumn) {
    QScopedValueRollback<bool> guard(m_updatingCoordinates, true);
    navigation::Vec3 origin, enu{};
    double rotation[9], n;
    bool hasOrigin = readOrigin(origin, rotation);
    if (hasOrigin)
        enu = navigation::rotate(rotation,
                                 {m_xyz.x - origin.x, m_xyz.y - origin.y, m_xyz.z - origin.z});
    bool hasGeoid = navigation::geoid(m_blh.x, m_blh.y, n);
    double cart[3]{m_xyz.x, m_xyz.y, m_xyz.z}, geo[3]{m_blh.x, m_blh.y, m_blh.z},
        local[3]{enu.x, enu.y, enu.z};
    bool dms = ui->combo_angleFormat->currentIndex() == 1;
    for (int row = 0; row < 3; ++row) {
        QString angle = row < 2 && dms ? dmsText(geo[row]) : number(geo[row]);
        QStringList values{
            number(cart[row]), angle, number(row < 2 ? geo[row] * radiansPerDegree : geo[row]),
            row < 2    ? angle
            : hasGeoid ? number(m_blh.z - n)
                       : QString(),
            hasOrigin ? QString::number(std::abs(local[row]) < 0.0000005 ? 0.0 : local[row], 'f', 6)
                      : QString()};
        for (int col = 0; col < 5; ++col)
            if (col != sourceColumn)
                qobject_cast<QLineEdit *>(ui->table_coordinates->cellWidget(row, col))
                    ->setText(values[col]);
    }
    QString status = hasGeoid ? QString("N = %1 m").arg(n, 0, 'f', 4) : "高度网格不可用";
    if (!hasOrigin)
        status += " · ENU 参考点无效";
    ui->label_coordinateStatus->setText(status);
    ui->label_coordinateStatus->setToolTip("大地水准面差距 N，海拔 H = h − N。");
}
void FormNavigation::originEdited() {
    if (m_updatingCoordinates)
        return;
    if (m_sourceColumn == 4)
        coordinateEdited(4);
    else if (m_hasCoordinates)
        fillCoordinates(m_sourceColumn);
}
void FormNavigation::angleFormatChanged() {
    {
        QScopedValueRollback<bool> guard(m_updatingCoordinates, true);
        for (auto *edit : {ui->edit_origin1, ui->edit_origin2}) {
            double degrees;
            if (readDms(edit->text(), degrees))
                edit->setText(ui->combo_angleFormat->currentIndex() ? dmsText(degrees)
                                                                    : number(degrees));
        }
        if (m_hasCoordinates)
            fillCoordinates(-1);
    }
    double degrees;
    if (readDms(ui->edit_angle->text(), degrees)) {
        QScopedValueRollback<bool> guard(m_updatingAngle, true);
        ui->edit_angle->setText(ui->combo_angleFormat->currentIndex() ? dmsText(degrees)
                                                                      : number(degrees));
    }
}
void FormNavigation::angleEdited(bool radians) {
    if (m_updatingAngle)
        return;
    QScopedValueRollback<bool> guard(m_updatingAngle, true);
    double degrees;
    bool valid = radians ? readNumber(ui->edit_radians->text(), degrees)
                 : ui->combo_angleFormat->currentIndex()
                     ? readDms(ui->edit_angle->text(), degrees)
                     : readNumber(ui->edit_angle->text(), degrees);
    if (!valid) {
        (radians ? ui->edit_angle : ui->edit_radians)->clear();
        return;
    }
    if (radians) {
        degrees /= radiansPerDegree;
        ui->edit_angle->setText(ui->combo_angleFormat->currentIndex() ? dmsText(degrees)
                                                                      : number(degrees));
    } else
        ui->edit_radians->setText(number(degrees * radiansPerDegree));
}
