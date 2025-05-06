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
    explicit FormDataLog(QWidget *parent = 0);
    ~FormDataLog();

    bool m_isShowSend;
    bool m_isLogModeChanged;
public slots:
    void on_dataShow(uint8_t *data, int32_t len, bool isSend);
    void on_button_clear_clicked();

private slots:
    void on_check_isLog_toggled(bool checked);
    void on_radio_show_send_toggled(bool checked);

private:
    Ui::FormDataLog *ui;
    void checkEmptyline();
};

#endif // FormDataLog_H
