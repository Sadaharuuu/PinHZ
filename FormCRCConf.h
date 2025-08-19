#ifndef FORMCRCCONF_H
#define FORMCRCCONF_H

#include <QWidget>
#include "AppCalcCRC.h"

namespace Ui {
class FormCRCConf;
}

class FormCRCConf : public QWidget
{
    Q_OBJECT

public:
    explicit FormCRCConf(QWidget *parent = 0);
    ~FormCRCConf();

    t_crc_conf m_crcConf;
    AppCalcCRC m_crcCalc;

    // PinHZ
    uint8_t m_dataType;
signals:
    void CRCConfDone(int8_t validCode);
private slots:
    void on_button_templateSet_clicked();
    void on_button_confDone_clicked();
    void on_button_convertPoly_clicked();

private:
    Ui::FormCRCConf *ui;
};

#endif // FORMCRCCONF_H
