#include "FormItemType.h"
#include "ui_FormItemType.h"
#include "AppCalcExpression.h"
#include "AppItemValue.h"
#include <QHeaderView>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSet>
#include <cmath>

FormItemType::FormItemType(QWidget *parent) : QWidget(parent), ui(new Ui::FormItemType) {
    ui->setupUi(this);
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    ui->table_combo->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->table_combo->horizontalHeader()->setMinimumSectionSize(135);
    ui->table_combo->verticalHeader()->setMinimumWidth(76);
    ui->table_combo->verticalHeader()->setTextElideMode(Qt::ElideNone);
    ui->table_combo->verticalHeader()->setDefaultSectionSize(36);
    connect(ui->groupBox_text, &QGroupBox::toggled, this, &FormItemType::on_groupCheck_toggled);
    connect(ui->groupBox_combo, &QGroupBox::toggled, this, &FormItemType::on_groupCheck_toggled);
    for (auto *check : {ui->check_upperLimit, ui->check_lowerLimit, ui->check_regex})
        connect(check, &QCheckBox::toggled, this, &FormItemType::on_textCheck_toggled);
    for (auto *edit :
         {ui->lineEdit_ioFormula, ui->lineEdit_reverseFormula, ui->lineEdit_previewValue,
          ui->lineEdit_upperLimit, ui->lineEdit_lowerLimit, ui->lineEdit_regex})
        connect(edit, &QLineEdit::textChanged, this, &FormItemType::updateFormulaPreview);
    connect(ui->combo_dataType,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &FormItemType::updateComboValues);
    connect(ui->combo_dataType,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                QString hex, error;
                if (AppItemValue::normalizeHex(ui->lineEdit_defaultValue->text(),
                                               static_cast<DataType>(index), m_endian, hex, error,
                                               true))
                    ui->lineEdit_defaultValue->setText(hex);
            });
    clearItemUnit(m_itemUnit);
    on_button_addCombo_clicked();
    updateFormulaPreview();
}
FormItemType::~FormItemType() {
    delete ui;
}
void FormItemType::loadItem(const s_itemUnit &item, bool editing) {
    m_itemUnit = item;
    ui->combo_dataType->setCurrentIndex(int(item.dataType));
    ui->lineEdit_defaultValue->setText(item.valueStr);
    ui->lineEdit_comment->setText(item.comment);
    ui->lineEdit_ioFormula->setText(item.ioFormula);
    ui->lineEdit_reverseFormula->setText(item.reverseFormula);
    ui->lineEdit_upperLimit->setText(QString::number(item.upperLimit, 'g', 17));
    ui->lineEdit_lowerLimit->setText(QString::number(item.lowerLimit, 'g', 17));
    ui->lineEdit_regex->setText(item.regex);
    ui->check_upperLimit->setChecked(item.upperLimitEnabled);
    ui->check_lowerLimit->setChecked(item.lowerLimitEnabled);
    ui->check_regex->setChecked(!item.regex.isEmpty());
    ui->groupBox_text->setChecked(item.itemType == ItemType::Text);
    ui->groupBox_combo->setChecked(item.itemType == ItemType::Combo);
    {
        const QSignalBlocker block(ui->table_combo);
        ui->table_combo->setRowCount(item.keyList.size());
        for (int row = 0; row < item.keyList.size(); ++row) {
            ui->table_combo->setItem(row, 0, new QTableWidgetItem(item.optionList.value(row)));
            ui->table_combo->setItem(row, 1, new QTableWidgetItem(item.keyList[row]));
            ui->table_combo->setItem(row, 2, new QTableWidgetItem());
        }
    }
    ui->spin_addNum->setValue(1);
    ui->spin_addNum->setEnabled(!editing);
    ui->button_addItem->setText(editing ? tr("应用修改") : tr("添加条目"));
    setWindowTitle(editing ? tr("编辑选中条目") : tr("添加条目"));
    ui->label_validation->clear();
    updateComboValues();
    updateFormulaPreview();
}

void FormItemType::clearItemUnit(s_itemUnit &item) {
    m_validCode = 0;
    item = s_itemUnit{};
    item.ioFormula = "x";
    item.valueStr = "00";
    item.dataType = DataType::U08;
    item.itemZone = ItemZone::Head;
    item.itemType = ItemType::Text;
}
void FormItemType::setEndian(Endian endian) {
    m_endian = endian;
    ui->lineEdit_defaultValue->setToolTip(
        endian == Endian::LittleEndian
            ? "按码流顺序输入 HEX；当前为小端，例如 uint16 的 10 输入 0A00"
            : "按码流顺序输入 HEX；当前为大端，例如 uint16 的 10 输入 000A");
    updateComboValues();
}
bool FormItemType::readTextConfig(s_itemUnit &item, QString &error) const {
    item.ioFormula = ui->lineEdit_ioFormula->text().trimmed();
    item.reverseFormula = ui->lineEdit_reverseFormula->text().trimmed();
    item.upperLimitEnabled = ui->check_upperLimit->isChecked();
    item.lowerLimitEnabled = ui->check_lowerLimit->isChecked();
    bool ok = false;
    if (item.upperLimitEnabled) {
        item.upperLimit = ui->lineEdit_upperLimit->text().toDouble(&ok);
        if (!ok || !std::isfinite(item.upperLimit)) {
            error = "上限必须是有限数值";
            return false;
        }
    }
    if (item.lowerLimitEnabled) {
        item.lowerLimit = ui->lineEdit_lowerLimit->text().toDouble(&ok);
        if (!ok || !std::isfinite(item.lowerLimit)) {
            error = "下限必须是有限数值";
            return false;
        }
    }
    if (ui->check_regex->isChecked()) {
        item.regex = ui->lineEdit_regex->text();
        if (item.regex.isEmpty()) {
            error = "启用正则校验后，请填写表达式";
            return false;
        }
    }
    return AppItemValue::validateConfig(item, error);
}
void FormItemType::updateFormulaPreview() {
    ui->label_validation->clear();
    s_itemUnit item = {};
    QString error;
    double result = 0;
    bool ok = false;
    const double raw = ui->lineEdit_previewValue->text().toDouble(&ok);
    if (!readTextConfig(item, error)) {
    } else if (!ok || !std::isfinite(raw))
        error = "试算原始值必须是有限数值";
    else if (AppCalcExpression::evaluate(item.ioFormula, raw, result, error)) {
        QString message = QString("原始 %1 → 显示 %2").arg(raw, 0, 'g', 15).arg(result, 0, 'g', 15);
        if (!item.reverseFormula.isEmpty()) {
            double reversed = 0;
            if (AppCalcExpression::evaluate(item.reverseFormula, result, reversed, error)) {
                message += QString(" → 反算 %1").arg(reversed, 0, 'g', 15);
                if (std::fabs(reversed - raw) > 1e-10 * qMax(1.0, std::fabs(raw)))
                    message += "（此试算点的正反向结果不一致，请检查公式或适用范围）";
            }
        } else if (!AppItemValue::identity(item.ioFormula))
            message += "（未设反向公式，数值列只读）";
        if (error.isEmpty()) {
            ui->label_formulaPreview->setText(message);
            return;
        }
    }
    ui->label_formulaPreview->setText("试算：" + error);
}
void FormItemType::on_button_addItem_clicked() {
    s_itemUnit candidate;
    clearItemUnit(candidate);
    candidate.dataType = static_cast<DataType>(ui->combo_dataType->currentIndex());
    candidate.itemZone = ItemZone::Data;
    candidate.comment = ui->lineEdit_comment->text();
    QString error;
    if (ui->groupBox_text->isChecked()) {
        QString display;
        if (readTextConfig(candidate, error) &&
            AppItemValue::normalizeHex(ui->lineEdit_defaultValue->text(), candidate.dataType,
                                       m_endian, candidate.valueStr, error) &&
            AppItemValue::validateInput(candidate, m_endian, display, error) &&
            !candidate.reverseFormula.isEmpty()) {
            QString roundTrip;
            AppItemValue::fromDisplay(candidate, display, m_endian, roundTrip, error);
        }
    } else {
        candidate.itemType = ItemType::Combo;
        QSet<QString> keys, options;
        for (int row = 0; row < ui->table_combo->rowCount(); ++row) {
            auto *option = ui->table_combo->item(row, colDataType);
            auto *key = ui->table_combo->item(row, colDataHex);
            QString hex;
            if (!option || option->text().trimmed().isEmpty())
                error = "显示选项不能为空";
            else if (!key || !AppItemValue::normalizeHex(key->text(), candidate.dataType, m_endian,
                                                         hex, error)) {
                if (error.isEmpty())
                    error = "请填写 HEX 映射值";
            } else if (keys.contains(hex) || options.contains(option->text().trimmed()))
                error = "显示选项和 HEX 映射值都不能重复";
            else if (!ui->table_combo->item(row, colDataDec)
                          ->data(Qt::UserRole)
                          .toString()
                          .isEmpty())
                error = ui->table_combo->item(row, colDataDec)->data(Qt::UserRole).toString();
            if (!error.isEmpty()) {
                error.prepend(QString("选项 %1：").arg(row + 1));
                break;
            }
            candidate.optionList.append(option->text().trimmed());
            candidate.keyList.append(hex);
            keys.insert(hex);
            options.insert(option->text().trimmed());
        }
        candidate.comboCnt = candidate.keyList.size();
        if (candidate.comboCnt == 0 && error.isEmpty())
            error = "至少添加一个有效选项";
        if (error.isEmpty()) {
            candidate.comboIndex = qMax(0, candidate.keyList.indexOf(m_itemUnit.valueStr));
            candidate.valueStr = candidate.keyList[candidate.comboIndex];
        }
    }
    if (!error.isEmpty()) {
        m_validCode = 1;
        ui->label_validation->setText(error);
        return;
    }
    m_itemUnit = candidate;
    m_validCode = 0;
    m_itemCnt = ui->spin_addNum->value();
    ui->label_validation->clear();
    close();
    emit itemConfDone(0);
}
void FormItemType::on_button_addCombo_clicked() {
    const QSignalBlocker block(ui->table_combo);
    const int row = ui->table_combo->rowCount();
    ui->table_combo->insertRow(row);
    ui->table_combo->setVerticalHeaderItem(row,
                                           new QTableWidgetItem(QString("选项 %1").arg(row + 1)));
    for (int col = 0; col < 3; ++col) {
        auto *item = new QTableWidgetItem(col == 0 ? QString() : "0");
        item->setTextAlignment(Qt::AlignCenter);
        ui->table_combo->setItem(row, col, item);
    }
}
void FormItemType::on_button_subCombo_clicked() {
    const int row = ui->table_combo->rowCount() - 1;
    if (row >= 1)
        ui->table_combo->removeRow(row);
}
void FormItemType::on_textCheck_toggled(bool) {
    ui->lineEdit_upperLimit->setEnabled(ui->check_upperLimit->isChecked());
    ui->lineEdit_lowerLimit->setEnabled(ui->check_lowerLimit->isChecked());
    ui->lineEdit_regex->setEnabled(ui->check_regex->isChecked());
    updateFormulaPreview();
}
void FormItemType::on_groupCheck_toggled(bool checked) {
    if (sender() == ui->groupBox_text) {
        const QSignalBlocker block(ui->groupBox_combo);
        ui->groupBox_combo->setChecked(!checked);
    } else {
        const QSignalBlocker block(ui->groupBox_text);
        ui->groupBox_text->setChecked(!checked);
    }
    ui->label_validation->clear();
}
void FormItemType::updateComboValues() {
    for (int row = 0; row < ui->table_combo->rowCount(); ++row)
        on_table_combo_itemChanged(ui->table_combo->item(row, colDataHex));
}
void FormItemType::on_table_combo_itemChanged(QTableWidgetItem *item) {
    if (!item || item->column() == colDataType)
        return;
    const QSignalBlocker block(ui->table_combo);
    s_itemUnit candidate = {};
    candidate.dataType = static_cast<DataType>(ui->combo_dataType->currentIndex());
    candidate.ioFormula = "x";
    QString error, display;
    if (item->column() == colDataHex) {
        if (AppItemValue::normalizeHex(item->text(), candidate.dataType, m_endian,
                                       candidate.valueStr, error) &&
            AppItemValue::validateInput(candidate, m_endian, display, error)) {
            item->setText(candidate.valueStr);
            ui->table_combo->item(item->row(), colDataDec)->setText(display);
        }
    } else if (AppItemValue::fromDisplay(candidate, item->text(), m_endian, candidate.valueStr,
                                         error)) {
        ui->table_combo->item(item->row(), colDataHex)->setText(candidate.valueStr);
    }
    if (error.isEmpty()) {
        for (int col : {colDataHex, colDataDec}) {
            auto *cell = ui->table_combo->item(item->row(), col);
            cell->setData(Qt::UserRole, QVariant());
            cell->setToolTip(QString());
            cell->setData(Qt::ForegroundRole, QVariant());
        }
        ui->label_validation->clear();
    } else {
        item->setData(Qt::UserRole, error);
        item->setToolTip(error);
        item->setForeground(QColor("#dc656d"));
        ui->label_validation->setText(QString("选项 %1：%2").arg(item->row() + 1).arg(error));
    }
}
