#ifndef WIDGET_H
#define WIDGET_H

#include "CommonDefine.h"
#include "FormDataLog.h"
#include "FormPinHZ.h"
#include "FormHex2ASCII.h"
#include "FormDataCheck.h"
#include "FormFileCheck.h"
#include "FormHexSplit.h"
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
#include <QtNetwork>

namespace Ui {
class Widget;
}

typedef struct {
    // 0: udp 1: tcp c 2: tcp s
    NetType netType;

    // 0: not work 1: start work, disable switch netType
    bool isWorking;

    QString addrLocal[3];
    QString portLocal[3];
    QString addrTarget[3];
    QString portTarget[3];

    // for udp & tcpc
    bool isPortLocalAuto[2];
    // for tcps
    QList<QTcpSocket *> tcpClientList;
} s_netUnit;

class Widget : public QWidget {
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = 0);
    ~Widget();
signals:
    // Log
    void showLog(LogLevel level, QString string);

    void PinHZ_recv(QByteArray &data, int32_t len);
    void updateDataCntSerial();
    void dataShowSerial(QByteArray data, int32_t len, bool isSend, QString dataInfo);
    void dataShowNet(QByteArray data, int32_t len, bool isSend, QString dataInfo);
    void updateDataCntNet();
public slots:
private slots:
    // Log
    void on_showLog(LogLevel level, QString string);

    void on_PinHZ_send(QByteArray &data, int32_t len);
    void on_dataLog_send(QString dataStr);
    void on_PinHZ_frameChanged(QString dataStr);
    void on_dataLogSyncRequested();
    void on_serialRecv();
    void on_netConnected();
    void on_netDisconnected();
    void on_netReadyRead();
    void on_netStateChanged(QAbstractSocket::SocketState socketState);
    void on_netSocketErr(QAbstractSocket::SocketError error);
    void on_netNewConnection();

    void on_button_serialRefresh_clicked();
    void on_button_serialSwitch_clicked();
    void on_button_clearLog_clicked();
    void on_button_picSelect_clicked();
    void on_button_dataLog_clicked();
    void on_button_netRefresh_clicked();
    void on_button_netSwitch_clicked();
    void on_button_netClientClose_clicked();
    void on_combo_portMode_currentIndexChanged(int index);
    void on_combo_netType_currentIndexChanged(int index);
    void on_check_netPortLocal_toggled(bool checked);

private:
    Ui::Widget *ui;

    PortMode m_portMode = PortMode::Serial;
    QSerialPort *m_serialPort = nullptr;
    QMutex m_serialMutex;

    QUdpSocket *m_udpSocket = nullptr;
    QTcpSocket *m_tcpClient = nullptr;
    QTcpServer *m_tcpServer = nullptr;

    QMutex m_netMutex;

    s_netUnit m_netUnit;

    FormPinHZ *m_PinHzDlg = nullptr;

    FormDataLog *m_dataLogDlgSerial = nullptr;
    FormDataLog *m_dataLogDlgNet = nullptr;
    PageMode m_dataLogPageMode = PageMode::Embeded;

    // bit [0:1]: recv:                01 = ASCII     10 = HEX
    // bit [2:3]: send: 00 = dont show 01 = ASCII     10 = HEX
    // bit [4:5]: log:  00 = notLog    01 = isLog
    // default : 0x1A, logMode, all Hex
    uint8_t m_dataLogMode[2] = {0x1A, 0x1A};

    void saveConf();
    void loadConf();
    void netSend(QByteArray data, int32_t len);
    void netStop();
    void netRefresh();
    QString getLocalIP();
    // serial
    void serialSend(QByteArray data, int32_t len);
    void serialRefresh();
    void on_label_appIcon_clicked();
    bool eventFilter(QObject *watched, QEvent *event) override;
};

#endif // WIDGET_H
