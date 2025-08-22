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
    explicit FormDataLog(QWidget *parent = 0, int8_t modeCtrl = 0x11);
    ~FormDataLog();

    bool m_isLogModeChanged = false;
    uint8_t m_recvMode = 0;
    uint8_t m_sendMode = 0;
    bool m_isLog = false;
public slots:
    void on_dataShow(uint8_t *data, int32_t len, bool isSend);
    void on_button_clear_clicked();

private slots:
    void on_check_isLog_toggled(bool checked);
    void on_radio_show_send_toggled(bool checked);
    void on_radio_ASCII_toggled(bool checked);
    void on_radio_hex_toggled(bool checked);

    void on_radio_ASCII_send_toggled(bool checked);

    void on_radio_hex_send_toggled(bool checked);

private:
    Ui::FormDataLog *ui;
    void checkEmptyline();
};

#endif // FormDataLog_H
