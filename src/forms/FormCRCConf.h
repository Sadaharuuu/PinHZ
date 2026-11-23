#ifndef FORMCRCCONF_H
#define FORMCRCCONF_H

#include "CommonDefine.h"
#include "AppCalcCRC.h"
#include <QTextBrowser>
#include <QWidget>

namespace Ui {
class FormCRCConf;
}

class FormCRCConf : public QWidget {
    Q_OBJECT

public:
    explicit FormCRCConf(QWidget *parent = 0);
    ~FormCRCConf();

    s_crc_conf m_crcConf = {};
    AppCalcCRC m_crcCalc;

    // PinHZ
    DataType m_dataType = DataType::U08;
    void setCrcConf(const s_crc_conf &crcConf);
    static QString tableCode(const s_crc_conf &conf);
signals:
    void CRCConfDone(int8_t validCode);
private slots:
    void on_button_templateSet_clicked();
    void on_button_confDone_clicked();
    void on_button_convertPoly_clicked();
    void on_button_generateMeter_clicked();

private:
    int8_t readConfig(s_crc_conf &conf) const;
    Ui::FormCRCConf *ui;

    QWidget *m_CRCMeterWidget = nullptr;
    QTextBrowser *m_CRCMeterBrowser = nullptr;
};

#endif // FORMCRCCONF_H
