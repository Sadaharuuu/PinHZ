#ifndef WIDGET_H
#define WIDGET_H

#define FONT_COLOR_BLACK        "<font color=\"#000000\">"
#define FONT_COLOR_WHITE        "<font color=\"#FFFFFF\">"
#define FONT_COLOR_RED          "<font color=\"#FF0000\">"
#define FONT_COLOR_GREEN        "<font color=\"#00FF00\">"
#define FONT_COLOR_BLUE         "<font color=\"#0000FF\">"
#define FONT_COLOR_CYAN         "<font color=\"#00FFFF\">"
#define FONT_COLOR_PINK         "<font color=\"#FF00FF\">"
#define FONT_COLOR_YELLOW       "<font color=\"#FFFF00\">"

#define FONT_COLOR_DARK_ORANGE  "<font color=\"#FF8C00\">"
#define FONT_COLOR_BLUEGREEN    "<font color=\"#00B3B3\">"
#define FONT_COLOR_BLUEVIOLET   "<font color=\"#8A2BE2\">"

enum LogLevel
{
    LogLevel_DBG = 0,
    LogLevel_INF,
    LogLevel_WAR,
    LogLevel_ERR,
};

#include "FormFillItem.h"
#include "FormDataLog.h"
#include "FormCRCConf.h"
#include "Hex2Dec.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QMap>
#include <QSerialPortInfo>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>
#include <QtSerialPort>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QScrollBar>

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
    void on_combo_PinHZ_indexChanged(int index);
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
private:
    Ui::Widget *ui;
    Hex2Dec m_hex2dec;
    // 串口
    QSerialPort *serialPort = nullptr;
    QMutex m_serialMutex;
    // 定时器
    QTimer *m_timer_Run;

    // 数据填充界面
    FormFillItem *m_fillItemDlg;

    // 数据日志界面
    FormDataLog *m_datalogDlg;
    int8_t m_dataLogMode;

    // 自动回令
    int8_t m_autoReplyTimes;
    int32_t m_autoReplyDelay;

    // 校验
    int32_t m_checkType;
    FormCRCConf *m_crcConfDlg;

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
    void updateDataZoneBytes();
    void createItemsARow(int32_t row, QString rowHead, uint8_t dataType, QString dataHex, QString dataDec, QString comment);
    void PinHZComboInit(int32_t row, uint8_t dataType);
    int32_t PinHZDeal(QString str, uint8_t *buf);
    int8_t checkRowZone(int32_t row);
    void saveConf();
    void loadConf();
};

#endif // WIDGET_H
