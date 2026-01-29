#ifndef FORMCRCCONF_H
#define FORMCRCCONF_H

#include "AppCalcCRC.h"
#include "CommonDefine.h"
#include <QTextBrowser>
#include <QWidget>

namespace Ui
{
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
    e_dataType m_dataType = DataType_U08;
signals:
    void CRCConfDone(int8_t validCode);
private slots:
    void on_button_templateSet_clicked();
    void on_button_confDone_clicked();
    void on_button_convertPoly_clicked();

    void on_button_generateMeter_clicked();

private:
    Ui::FormCRCConf *ui;

    QWidget *m_CRCMeterWidget = nullptr;
    QTextBrowser *m_CRCMeterBrowser = nullptr;
};

#endif // FORMCRCCONF_H
