#ifndef FORMITEMTYPE_H
#define FORMITEMTYPE_H

#include "CommonDefine.h"
#include <QTableWidgetItem>
#include <QWidget>

namespace Ui {
class FormItemType;
}

class FormItemType : public QWidget {
    Q_OBJECT

public:
    explicit FormItemType(QWidget *parent = 0);
    ~FormItemType();

    s_itemUnit m_itemUnit;
    int32_t m_itemCnt = 0;
    uint8_t m_validCode = 0;

    void clearItemUnit(s_itemUnit &itemType);
    void setEndian(Endian endian);
    void loadItem(const s_itemUnit &item, bool editing);
signals:
    void itemConfDone(int8_t validCode);
private slots:
    void on_button_addItem_clicked();
    void on_button_addCombo_clicked();
    void on_button_subCombo_clicked();
    void on_textCheck_toggled(bool checked);
    void on_groupCheck_toggled(bool checked);
    void on_table_combo_itemChanged(QTableWidgetItem *item);
    void updateFormulaPreview();
    void updateComboValues();

private:
    Ui::FormItemType *ui;
    Endian m_endian = Endian::LittleEndian;
    bool readTextConfig(s_itemUnit &item, QString &error) const;
};

#endif // FORMITEMTYPE_H
