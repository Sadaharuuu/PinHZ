#ifndef WIDGET_H
#define WIDGET_H

#include "CommonDefine.h"
#include "FormCRCConf.h"
#include "FormDataLog.h"
#include "FormFillItem.h"
#include "Hex2Dec.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QIcon>
#include <QMap>
#include <QScrollBar>
#include <QSerialPortInfo>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>
#include <QtSerialPort>

namespace Ui
{
class Widget;
}

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = 0);
    ~Widget();
signals:
    // log
    void showLog(LogLevel level, QString string);

    // serial
    void serialSend(uint8_t *buf, int32_t len);

    // PinHZ
    void dataShow(uint8_t *data, int32_t len, bool isSend);
    void updateDataCnt();

private slots:
    // Log
    void on_showLog(LogLevel level, QString string);
    // 串口设置及基本功能
    void on_button_serialRefresh_clicked();
    void on_button_serialSwitch_clicked();
    void on_button_clearLog_clicked();
    void on_serialRecv();
    void on_serialSend(uint8_t *buf, int32_t len);
    void on_timerOut_Run();
    void on_button_picSelect_clicked();
    void on_button_subCurRow_clicked();
    void on_button_addHead_clicked();
    void on_button_addItem_clicked();
    void on_button_addTail_clicked();
    void on_button_PinHZ_clicked();
    void on_table_PinHZ_itemChanged(QTableWidgetItem *item);
    void on_combo_PinHZ_dataTypeChanged(int index);
    void on_combo_PinHZ_checkChanged(int index);
    void on_button_checkSet_clicked();
    void on_combo_hexOrder_currentIndexChanged(int index);
    void on_button_PinHZSave_clicked();
    void on_button_PinHZLoad_clicked();
    void on_button_PinHZReverse_clicked();
    void on_check_autoPinHZ_stateChanged(int state);
    void on_button_fillCurRow_clicked();
    void on_fillConfDone();
    void on_button_copyCurRow_clicked();
    void on_button_dataLog_clicked();
    void on_button_PinHZSend_clicked();
    void on_spinBox_replyTime_valueChanged(int arg1);
    void on_check_fieldPinHZ_toggled(bool checked);
    void on_CRCConfDone(int8_t validCode);
    void on_spinBox_sendPeriod_valueChanged(int arg1);

private:
    Ui::Widget *ui;
    Hex2Dec m_hex2dec;
    // 串口
    QSerialPort *m_serialPort = nullptr;
    QMutex m_serialMutex;
    // 定时器
    QTimer *m_timer_Run = nullptr;

    // 数据填充界面
    FormFillItem *m_fillItemDlg = nullptr;

    // 数据日志界面
    FormDataLog *m_datalogDlg = nullptr;
    // bit 0-1: recv:                01 = ASCII     10 = HEX
    // bit 2-3: send: 00 = dont show 01 = ASCII     10 = HEX
    // bit 4-5: log:  00 = notLog    01 = isLog
    // default : 0x1A, logMode, all Hex
    int8_t m_dataLogMode = 0;

    // 是否用字段拼好帧 EB 90 01 02 03 04 or EB90 01020304
    bool m_isFieldPinHZ = true;

    // 自动回令
    int8_t m_autoReplyTimes = 0;
    int32_t m_autoReplyDelay = 0;

    // 循环发送
    int8_t m_autoSendPeriod = 100;

    // 校验
    int32_t m_checkType = 0;
    FormCRCConf *m_crcConfDlg = nullptr;

    // 自定义单元格Type的类型，在创建单元格的Item时使用
    enum CellType
    {
        ctDataType = 1000,
        ctDataHex,
        ctDataDec,
        ctComment
    };
    // 各字段在表格中的列号
    enum FieldColNum
    {
        colDataType = 0,
        colDataHex,
        colDataDec,
        colComment
    };
    int32_t getRowsBytes(int32_t rowStart, int32_t rowEnd);
    void updateDataZoneBytes();
    void createItemsARow(int32_t row, QString rowHead, uint8_t dataType, QString dataHex, QString dataDec, QString comment);
    void PinHZComboInit(int32_t row, uint8_t dataType);
    int32_t PinHZDeal(QString str, uint8_t *buf);
    int8_t checkRowZone(int32_t row);
    void saveConf();
    void loadConf();
};

#endif // WIDGET_H
