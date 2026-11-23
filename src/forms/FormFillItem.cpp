#include "FormFillItem.h"
#include "ui_FormFillItem.h"
#include "AppCalcCheck.h"
#include <QSignalBlocker>

FormFillItem::FormFillItem(QWidget *parent) : QWidget(parent), ui(new Ui::FormFillItem) {
    ui->setupUi(this);
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    connect(ui->groupBox_step, &QGroupBox::toggled, this, &FormFillItem::on_groupCheck_clicked);
    connect(ui->groupBox_repeat, &QGroupBox::toggled, this, &FormFillItem::on_groupCheck_clicked);
}
FormFillItem::~FormFillItem() {
    delete ui;
}

void FormFillItem::on_button_fill_clicked() {
    m_fillData.clear();
    m_isFillValid = false;
    QString error;
    QByteArray candidate;
    if (m_fillRowCnt <= 0 || m_fillBytes <= 0)
        error = "请先选择需要填充的条目";
    else if (ui->groupBox_step->isChecked()) {
        m_fillMode = FillMode::Order;
        bool startOk = false, endOk = false;
        const int start = ui->lineEdit_startValue->text().toInt(
            &startOk, ui->check_startValue->isChecked() ? 16 : 10);
        const int end =
            ui->lineEdit_endValue->text().toInt(&endOk, ui->check_endValue->isChecked() ? 16 : 10);
        const int step = ui->spin_step->value();
        if (!startOk || !endOk || start < 0 || start > 255 || end < 0 || end > 255)
            error = "顺序填充的起止值必须在 0–255（HEX：00–FF）之间";
        else if (step <= 0 || (m_fillRowCnt - 1) > qAbs(end - start) / step)
            error = "起止值和步长不足以填满所选条目";
        else {
            const int direction = start <= end ? 1 : -1;
            for (int i = 0; i < m_fillRowCnt; ++i)
                candidate.append(char(start + direction * i * step));
        }
    } else {
        m_fillMode = FillMode::Repeat;
        QByteArray pattern;
        if (AppCalcCheck::parseHex(ui->lineEdit_repeat->text(), pattern, error)) {
            if (pattern.isEmpty())
                error = "重复填充数据不能为空";
            else {
                candidate.reserve(m_fillBytes);
                while (candidate.size() < m_fillBytes)
                    candidate.append(pattern.constData(),
                                     qMin(pattern.size(), m_fillBytes - candidate.size()));
            }
        }
    }
    ui->label_validation->setText(error);
    if (!error.isEmpty()) {
        emit fillConfDone(false);
        return;
    }
    m_fillData = candidate;
    m_isFillValid = true;
    close();
    emit fillConfDone(true);
}
void FormFillItem::on_groupCheck_clicked() {
    if (sender() == ui->groupBox_step) {
        const QSignalBlocker block(ui->groupBox_repeat);
        ui->groupBox_repeat->setChecked(!ui->groupBox_step->isChecked());
    } else {
        const QSignalBlocker block(ui->groupBox_step);
        ui->groupBox_step->setChecked(!ui->groupBox_repeat->isChecked());
    }
    ui->label_validation->clear();
}
