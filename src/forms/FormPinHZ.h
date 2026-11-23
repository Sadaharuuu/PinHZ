#ifndef FORMPINHZ_H
#define FORMPINHZ_H

#include "CommonDefine.h"
#include "FormCRCConf.h"
#include "FormFillItem.h"
#include "FormItemType.h"
#include "Hex2Dec.h"
#include <algorithm>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QIcon>
#include <QInputDialog>
#include <QMap>
#include <QScrollBar>
#include <QSerialPortInfo>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>
#include <QtNetwork>
#include <QtSerialPort>
#include <qmetaobject.h>

#define PINHZ_CACHE_LEN (81920)

namespace Ui {
class FormPinHZ;
}

class FormPinHZ : public QWidget {
    Q_OBJECT

public:
    bool beginTemplateSave(const QString &path, QString &error);
    explicit FormPinHZ(QWidget *parent = 0);
    ~FormPinHZ();

    // variable
    QList<s_itemUnit> m_itemUnitList;

    // method
    int32_t PinHZ(QString &hexStr); // get table data buf
    int32_t PinHZ_parse(QString str, QByteArray &data);
    int32_t PinHZ_deal2Table(QByteArray data, bool force = false);
    int32_t length(void);
    int32_t zoneLength(ItemZone zone);
    int32_t rowCnt();
    QString outputText() const;

    // get all original data stream length

    inline int32_t endian() {
        return static_cast<int32_t>(m_endian);
    }

    inline bool isAutoPin() {
        return m_isAutoPin;
    }
    inline bool isAutoDeal() {
        return m_isAutoDeal;
    }
    inline bool isAutoSend() {
        return m_isAutoSend;
    }
    inline bool isAutoReply() {
        return m_isAutoReply;
    }
    inline bool isSaveAsTemp() {
        return m_isSaveAsTemp;
    }
    inline void setEndian(Endian endian) {
        m_endian = endian;
    }
    inline void setAutoPin(bool isAuto) {
        m_isAutoPin = isAuto;
    }
    inline void setAutoDeal(bool isAuto) {
        m_isAutoDeal = isAuto;
    }
    inline void setAutoSend(bool isAuto, int32_t period) {
        m_isAutoSend = isAuto;
        m_autoSendPeriod = period;
    }
    inline void setAutoReply(bool isAuto) {
        m_isAutoReply = isAuto;
    }
    inline void setSaveAsTemp(bool isAuto) {
        m_isSaveAsTemp = isAuto;
    }
    inline void addAutoReplyTimes() {
        m_autoReplyTimes++;
    }
signals:
    // Log
    void showLog(LogLevel level, QString string);
    void PinHZ_send(QByteArray &data, int32_t len);
    void PinHZ_outputChanged(QString dataStr);
public slots:
    void on_PinHZ_recv(QByteArray &data, int32_t len);
private slots:
    void on_button_frameSettings_clicked();
    void on_button_editItem_clicked();
    void on_button_importTable_clicked();
    void on_button_addHead_clicked();
    void on_button_addItem_clicked();
    void on_button_checkSet_clicked();
    void on_button_addTail_clicked();
    void on_button_copyCurRow_clicked();
    void on_button_subCurRow_clicked();
    void on_button_fillCurRow_clicked();
    void on_button_PinHZ_clicked();
    void on_button_PinHZSave_clicked();
    void on_button_PinHZLoad_clicked();
    void on_button_PinHZSend_clicked();
    void on_button_PinHZDeal_clicked();
    void on_combo_PinHZ_dataTypeChanged(int index);
    void on_combo_PinHZ_checkChanged(int index);
    void on_combo_hexOrder_currentIndexChanged(int index);
    void on_check_fieldPinHZ_toggled(bool checked);
    void on_check_saveAsTemp_toggled(bool checked);
    void on_check_autoPinHZ_toggled(bool checked);
    void on_check_autoPinHZDeal_toggled(bool checked);
    void on_check_autoReply_toggled(bool checked);
    void on_check_autoSend_toggled(bool checked);
    void on_table_PinHZ_itemChanged(QTableWidgetItem *item);
    void on_spin_replyTime_valueChanged(int arg1);
    void on_spin_sendPeriod_valueChanged(int arg1);
    void on_combo_PinHZ_customComboChanged(int index);
    void on_fillConfDone(bool isValid);
    void on_itemConfDone(int8_t validCode);
    void on_CRCConfDone(int8_t validCode);
    void on_timerOut();

private:
    Ui::FormPinHZ *ui;
    Hex2Dec m_hex2dec;

    QTimer *m_timer = nullptr;
    uint32_t m_timerCnt_10ms = 0;

    int32_t m_rowCntHead = 1;
    int32_t m_rowCntData = 0;
    int32_t m_rowCntTail = 0;

    Endian m_endian = Endian::LittleEndian;

    bool m_isAutoPin = true;
    bool m_isAutoDeal = false;
    bool m_isAutoSend = false;
    bool m_isAutoReply = false;
    // auto sync to send box of dataLog
    bool m_isAutoSync = true;
    // show original data by filed like EB90 01020304 either EB 90 01 02 03 04
    bool m_isFieldPinHZ = true;

    bool m_isSaveAsTemp = false;
    int m_editRow = -1;
    int m_checkStartRow = 0, m_checkEndRow = 0;
    QString m_lengthItemId;
    QByteArray checkRangeBytes(const QByteArray *received = nullptr) const;
    QString m_saveAsTempPath = "";
    QStringList m_saveColumns;
    QByteArray m_receivePending;
    QByteArray m_receiveSignature;
    bool m_lastParseSucceeded = false;
    bool m_forceQuestionOpen = false;
    void saveParsedFrame();

    int32_t m_autoSendPeriod = 1;   // default: 10ms
    int32_t m_autoReplyDelay = 100; // default: 1000ms
    int32_t m_autoReplyTimes = 0;

    FormItemType *m_itemTypeDlg = nullptr;
    FormCRCConf *m_crcConfDlg = nullptr;
    FormFillItem *m_fillItemDlg = nullptr;
    CheckType m_checkType = CheckType::None;
    QByteArray m_dataCache;
    bool m_isRebuilding = false;
    bool m_crcParamsPending = false;

    int32_t m_length = 0;

    int32_t getRowsBytes(int32_t rowStart, int32_t rowEnd);
    void refreshTable();
    void rebuildZoneCounts();
    void updateRowView(int32_t row);
    void updateTextValueView(int32_t row);
    void applyValueEdit(QTableWidgetItem *item, const QString &text);
    void setLengthBinding(const QString &itemId);
    bool validateTextRows(QString &error) const;
    void updateAfterModelChange();

    ItemZone checkRowZone(int32_t row);
    void updateDataZoneBytes();
    void updateDataCheck();
    void createItemsARow(int32_t row, s_itemUnit itemUnit);
    void PinHZ_comboDataTypeInit(int32_t row, DataType dataType);
    int32_t modelCheckRow() const;
    int32_t firstRowOfZone(ItemZone zone) const;
    ItemZone rowItemZone(int32_t row) const;
    int32_t dataTypeByteLen(DataType dataType) const;
    DataType checkDataType(CheckType checkType) const;
    int32_t checkByteLen(CheckType checkType) const;
    QString bytesToHex(const QByteArray &bytes) const;
    QByteArray hexStringToBytes(QString hex) const;
    QString wireHexToDisplayHex(const QString &wireHex, int32_t byteLen, Endian endian) const;
    QString displayHexToWireHex(const QString &displayHex, int32_t byteLen, Endian endian) const;
    QString normalizeDisplayHex(const QString &displayHex, int32_t byteLen, Endian endian) const;
    QByteArray displayToWireBytes(const QString &displayHex, int32_t byteLen, Endian endian) const;
    QByteArray rowWireBytes(int32_t row) const;
    QByteArray dataZoneWireBytes() const;
    QByteArray calcCheckWireBytes(CheckType checkType, const QByteArray &dataZone) const;
    QString calcCheckWireHex() const;
    QString calcCheckDisplayHex() const;
    int32_t insertRowForZone(ItemZone zone, int32_t currentRow) const;
    int32_t rowOfCellWidget(QWidget *widget) const;
    void invokeAutoPin();
    bool crcConfigPending() const;
};

#endif // FORMPINHZ_H
