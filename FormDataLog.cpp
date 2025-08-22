#include "FormDataLog.h"
#include "ui_FormDataLog.h"
#include "widget.h"

FormDataLog::FormDataLog(QWidget *parent, int8_t modeCtrl) :
    QWidget(parent),
    ui(new Ui::FormDataLog)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);

    if (modeCtrl & 0x01)
    {
        ui->radio_ASCII->setChecked(true);
        m_recvMode = 1;
    }
    else
    {
        ui->radio_hex->setChecked(true);
        m_recvMode = 2;
    }

    ui->check_isLog->setChecked(modeCtrl & 0x10);
    if (modeCtrl & 0x10)
    {
        m_isLog = true;
        if (modeCtrl & 0x04)
        {
            ui->radio_ASCII_send->setChecked(true);
            m_sendMode = 1;
        }
        else if (modeCtrl & 0x08)
        {
            ui->radio_hex_send->setChecked(true);
            m_sendMode = 2;
        }
        else
        {
            ui->radio_show_send->setChecked(true);
            m_sendMode = 0;
        }
    }
}

FormDataLog::~FormDataLog()
{
    delete ui;
}

void FormDataLog::checkEmptyline()
{
    QTextCursor cursor = ui->txtBs_recvData->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    QString lastLine = cursor.selectedText().trimmed();
    if (!lastLine.isEmpty())
    {
        cursor.movePosition(QTextCursor::End);
        cursor.insertText("\n");
        ui->txtBs_recvData->setTextCursor(cursor);
    }
}

void FormDataLog::on_dataShow(uint8_t *data, int32_t len, bool isSend)
{
    QString str = "", dataColor = isSend ? FONT_COLOR_BLUEGREEN : FONT_COLOR_DARK_ORANGE;
    bool isASCII = isSend ? ui->radio_ASCII_send->isChecked() ? true : false \
                    : ui->radio_ASCII->isChecked() ? true : false;
    if (m_isLogModeChanged)
    {
        if (!ui->check_isLog->isChecked())
            ui->txtBs_recvData->append(FONT_COLOR_BLUEVIOLET "Pure Data:<br>");
        m_isLogModeChanged = false;
    }

    for (int32_t i = 0; i < len; i++)
    {
        if (isASCII)
            str += QString::asprintf("%c", data[i]);
        else
            str += QString::asprintf("%02X ", data[i]);
    }

    if (m_isLog)
    {
        // log mode
        QString logInfo = QDateTime().currentDateTime().toString("[yyyy-MM-dd hh:mm:ss.zzz]");

        logInfo += isSend ? "# Send " : "# Recv ";
        logInfo += isASCII ? "ASCII>" : "HEX>";

        ui->txtBs_recvData->append(FONT_COLOR_BLACK + logInfo);
        ui->txtBs_recvData->append(dataColor + str);
    }
    else
    {
        // text mode donot show send
        QTextCursor cursor = ui->txtBs_recvData->textCursor();
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(str);
        ui->txtBs_recvData->setTextCursor(cursor);
    }

    if (ui->check_autoScroll->isChecked())
    {
        QScrollBar *scrollBar = ui->txtBs_recvData->verticalScrollBar();
        scrollBar->setSliderPosition(scrollBar->maximum());
    }
}

void FormDataLog::on_button_clear_clicked()
{
    ui->txtBs_recvData->clear();
    m_isLogModeChanged = true;
}

void FormDataLog::on_check_isLog_toggled(bool checked)
{
    static int8_t saveSendMode = 0;
    if (checked)
    {
        // 日志模式
        ui->radio_show_send->setEnabled(true);
        ui->radio_ASCII_send->setEnabled(true);
        ui->radio_hex_send->setEnabled(true);
        switch (saveSendMode)
        {
        case 0: ui->radio_show_send->setChecked(true); break;
        case 1: ui->radio_ASCII_send->setChecked(true); break;
        case 2: ui->radio_hex_send->setChecked(true); break;
        default: break;
        }
    }
    else
    {
        saveSendMode = ui->radio_show_send->isChecked() ? 0 \
                        : ui->radio_ASCII_send->isChecked() ? 1 \
                        : ui->radio_hex_send->isChecked() ? 2 : 0;
        ui->radio_show_send->setChecked(true);
        ui->radio_show_send->setEnabled(false);
        ui->radio_ASCII_send->setEnabled(false);
        ui->radio_hex_send->setEnabled(false);
    }
    m_isLogModeChanged = true;
    m_isLog = checked;
}

void FormDataLog::on_radio_ASCII_toggled(bool checked)
{
    m_recvMode = checked ? 1 : m_recvMode;
}

void FormDataLog::on_radio_hex_toggled(bool checked)
{
    m_recvMode = checked ? 2 : m_recvMode;
}

void FormDataLog::on_radio_show_send_toggled(bool checked)
{
    m_sendMode = checked ? 0 : m_sendMode;
}

void FormDataLog::on_radio_ASCII_send_toggled(bool checked)
{
    m_sendMode = checked ? 1 : m_sendMode;
}

void FormDataLog::on_radio_hex_send_toggled(bool checked)
{
    m_sendMode = checked ? 2 : m_sendMode;
}
