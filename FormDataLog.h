#ifndef FORMDATALOG_H
#define FORMDATALOG_H

#include <QWidget>

namespace Ui
{
class FormDataLog;
}

class FormDataLog : public QWidget
{
    Q_OBJECT

public:
    explicit FormDataLog(QWidget *parent = 0, uint8_t modeCtrl = 0x11);
    ~FormDataLog();

    bool m_isLogModeChanged = false;
    bool m_isLog = false;

    uint8_t m_recvMode = 0;
    uint8_t m_sendMode = 0;
    uint32_t m_recvFrm = 0;
    uint32_t m_sendFrm = 0;
    uint32_t m_recvByte = 0;
    uint32_t m_sendByte = 0;

    void setModeCtrl(uint8_t modeCtrl);
public slots:
    void on_dataShow(uint8_t *data, int32_t len, bool isSend, QString dataInfo);
    void on_button_clearLog_clicked();
    void on_updateDataCnt();

private slots:
    void on_check_isLog_toggled(bool checked);
    void on_radio_dontShow_send_toggled(bool checked);
    void on_radio_ASCII_toggled(bool checked);
    void on_radio_hex_toggled(bool checked);
    void on_radio_ASCII_send_toggled(bool checked);
    void on_radio_hex_send_toggled(bool checked);
    void on_button_clearCnt_clicked();
    void on_check_saveAs_toggled(bool checked);

private:
    Ui::FormDataLog *ui;
    void checkEmptyline();
};

#endif // FormDataLog_H
