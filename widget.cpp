#include "ui_widget.h"
#include "widget.h"
#include "AppTheme.h"

#define CONF_PATH "./conf.ini"

#define SOFT_VERSION "V3.00"

QString g_strLogColor[] = {
    FONT_COLOR_BLUE,  /* LOG_LEVEL_DBG */
    FONT_COLOR_BLACK, /* LOG_LEVEL_INF */
    FONT_COLOR_PINK,  /* LOG_LEVEL_WAR */
    FONT_COLOR_RED,   /* LOG_LEVEL_ERR */
    FONT_COLOR_BLACK  /* LOG_LEVEL_COLORFUL */
};

bool g_isDebugMode = false;
Widget::Widget(QWidget *parent) : QWidget(parent), ui(new Ui::Widget) {
    ui->setupUi(this);
    connect(ui->tools, &FormTools::showLog, this, &Widget::on_showLog);
    ui->tabWidget->setTabPosition(QTabWidget::North);
    ui->tabWidget->setDocumentMode(true);
    AppTheme::prepareComboBox(ui->combo_theme);

    this->setWindowTitle("PinHZ_" SOFT_VERSION);
    ui->label_softVersion->setText("Version: " SOFT_VERSION);
    QString compileDate = __DATE__;
    ui->label_compileDate->setText("Compile: " + compileDate + "_" __TIME__);

    m_netUnit.netType = NetType::UDP;
    m_netUnit.isWorking = false;

    // sub dialog
    m_PinHzDlg = new FormPinHZ(this);
    ui->tabWidget->insertTab(0, m_PinHzDlg, "拼好帧");
    connect(m_PinHzDlg, &FormPinHZ::PinHZ_send, this, on_PinHZ_send, Qt::DirectConnection);
    connect(this, PinHZ_recv, m_PinHzDlg, &FormPinHZ::on_PinHZ_recv, Qt::QueuedConnection);
    connect(m_PinHzDlg, &FormPinHZ::PinHZ_outputChanged, this, &Widget::on_PinHZ_frameChanged,
            Qt::DirectConnection);

    m_dataLogDlgSerial = new FormDataLog(this, m_dataLogMode[0]);
    m_dataLogDlgSerial->setWindowTitle("串口数据日志");
    connect(this, &Widget::dataShowSerial, m_dataLogDlgSerial, &FormDataLog::on_dataShow,
            Qt::DirectConnection);
    connect(this, &Widget::updateDataCntSerial, m_dataLogDlgSerial, &FormDataLog::on_updateDataCnt,
            Qt::QueuedConnection);
    connect(m_dataLogDlgSerial, &FormDataLog::dataLog_send, this, &Widget::on_dataLog_send,
            Qt::DirectConnection);
    connect(m_dataLogDlgSerial, &FormDataLog::syncPinHZRequested, this,
            &Widget::on_dataLogSyncRequested, Qt::DirectConnection);
    m_dataLogDlgNet = new FormDataLog(this, m_dataLogMode[1]);
    m_dataLogDlgNet->setWindowTitle("网口数据日志");
    connect(this, &Widget::dataShowNet, m_dataLogDlgNet, &FormDataLog::on_dataShow,
            Qt::DirectConnection);
    connect(this, &Widget::updateDataCntNet, m_dataLogDlgNet, &FormDataLog::on_updateDataCnt,
            Qt::QueuedConnection);
    connect(m_dataLogDlgNet, &FormDataLog::dataLog_send, this, &Widget::on_dataLog_send,
            Qt::DirectConnection);
    connect(m_dataLogDlgNet, &FormDataLog::syncPinHZRequested, this,
            &Widget::on_dataLogSyncRequested, Qt::DirectConnection);
    for (FormDataLog *log : {m_dataLogDlgSerial, m_dataLogDlgNet}) {
        connect(log, &FormDataLog::fileChunkRequested, this, [this, log](const QByteArray &bytes) {
            if (log == m_dataLogDlgSerial) {
                if (!m_serialPort->isOpen()) {
                    log->fileChunkResult(0, "请先打开串口。");
                    return;
                }
                if (m_serialPort->bytesToWrite() > 65536) {
                    log->fileChunkResult(0, "");
                    return;
                }
            } else {
                if (m_netUnit.netType == NetType::TCP_C && m_tcpClient->bytesToWrite() > 65536) {
                    log->fileChunkResult(0, "");
                    return;
                }
                if (m_netUnit.netType == NetType::TCP_S) {
                    if (ui->combo_netClient->currentIndex() <= 0) {
                        log->fileChunkResult(0, "文件发送请选择单个 TCP 客户端。");
                        return;
                    }
                    for (QTcpSocket *socket : m_netUnit.tcpClientList)
                        if (socket->bytesToWrite() > 65536) {
                            log->fileChunkResult(0, "");
                            return;
                        }
                }
            }
            const quint64 before = log->m_sendByte;
            if (log == m_dataLogDlgSerial)
                serialSend(bytes, bytes.size());
            else
                netSend(bytes, bytes.size());
            const quint64 accepted = log->m_sendByte - before;
            log->fileChunkResult(qint64(accepted),
                                 accepted ? QString()
                                          : QString("连接未就绪或写入失败，请检查运行日志。"));
        });
    }

    if (ui->combo_portMode->currentIndex() == 0) {
        ui->tabWidget->insertTab(1, m_dataLogDlgSerial, "串口数据日志");
    } else {
        ui->tabWidget->insertTab(1, m_dataLogDlgNet, "网口数据日志");
    }

    // log
    connect(this, &Widget::showLog, this, &Widget::on_showLog, Qt::QueuedConnection);
    connect(m_PinHzDlg, &FormPinHZ::showLog, this, &Widget::on_showLog, Qt::QueuedConnection);
    connect(m_dataLogDlgNet, &FormDataLog::showLog, this, &Widget::on_showLog,
            Qt::QueuedConnection);
    connect(m_dataLogDlgSerial, &FormDataLog::showLog, this, &Widget::on_showLog,
            Qt::QueuedConnection);

    // serial port
    m_serialPort = new QSerialPort(this);
    connect(m_serialPort, &QSerialPort::readyRead, this, &Widget::on_serialRecv,
            Qt::DirectConnection);

    // net port
    m_udpSocket = new QUdpSocket(this);
    m_tcpClient = new QTcpSocket(this);
    m_tcpServer = new QTcpServer(this);

    connect(m_udpSocket, &QUdpSocket::readyRead, this, &Widget::on_netReadyRead);
    connect(m_tcpClient, &QTcpSocket::connected, this, &Widget::on_netConnected);
    connect(m_tcpClient, &QTcpSocket::disconnected, this, &Widget::on_netDisconnected);
    connect(m_tcpClient, &QTcpSocket::readyRead, this, &Widget::on_netReadyRead);
    connect(m_tcpClient, &QTcpSocket::stateChanged, this, &Widget::on_netStateChanged);
    connect(m_tcpClient,
            static_cast<void (QTcpSocket::*)(QAbstractSocket::SocketError)>(&QTcpSocket::error),
            this, &Widget::on_netSocketErr);

    connect(m_tcpServer, &QTcpServer::newConnection, this, &Widget::on_netNewConnection);

    // load stored config
    loadConf();

    QSettings appearance(CONF_PATH, QSettings::IniFormat);
    bool validTheme = false;
    int theme = appearance.value("Main/Theme", AppTheme::defaultIndex()).toInt(&validTheme);
    if (!validTheme || theme < 0 || theme >= AppTheme::names().size())
        theme = AppTheme::defaultIndex();
    ui->combo_theme->setCurrentIndex(theme);
    AppTheme::apply(theme);
    connect(ui->combo_theme, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [](int index) { AppTheme::apply(index); });

    // install event filter, for click app icon 5 times to login debug mode
    ui->label_appIcon->installEventFilter(this);
}

Widget::~Widget() {
    m_dataLogDlgSerial->close();
    m_dataLogDlgNet->close();
    m_PinHzDlg->close();
    saveConf();

    if (m_udpSocket->state() == QAbstractSocket::BoundState) {
        m_udpSocket->abort();
    }

    if (m_netUnit.netType == NetType::UDP) {
        ;
    } else if (m_netUnit.netType == NetType::TCP_C &&
               m_tcpClient->state() != QTcpSocket::UnconnectedState) {
        m_tcpClient->disconnectFromHost();
        if (!m_tcpClient->waitForDisconnected(1000))
            m_tcpClient->abort();
    } else if (m_netUnit.netType == NetType::TCP_S && m_tcpServer->isListening()) {
        netStop();
    } else {
        ;
    }

    delete m_PinHzDlg;
    delete m_dataLogDlgSerial;
    delete m_dataLogDlgNet;
    delete m_serialPort;
    delete m_udpSocket;
    delete m_tcpClient;
    delete m_tcpServer;
    delete ui;
}

void Widget::saveConf() {
    QSettings settings(CONF_PATH, QSettings::IniFormat);

    settings.setValue("Main/TabIndex", ui->tabWidget->currentIndex());
    settings.setValue("Main/Theme", ui->combo_theme->currentIndex());

    settings.setValue("Serial/COM", ui->combo_serialPort->currentText());
    settings.setValue("Serial/Baud", ui->combo_serialBaud->currentIndex());
    settings.setValue("Serial/DataBit", ui->combo_serialDataBit->currentIndex());
    settings.setValue("Serial/StopBit", ui->combo_serialStopBit->currentIndex());
    settings.setValue("Serial/ParityBit", ui->combo_serialParityBit->currentIndex());

    settings.setValue("Net/NetType", ui->combo_netType->currentIndex());
    settings.setValue("Net/AddrLocal_UDP", m_netUnit.addrLocal[toInt32(NetType::UDP)]);
    settings.setValue("Net/PortLocal_UDP", m_netUnit.portLocal[toInt32(NetType::UDP)]);
    settings.setValue("Net/AddrTarget_UDP", m_netUnit.addrTarget[toInt32(NetType::UDP)]);
    settings.setValue("Net/PortTarget_UDP", m_netUnit.portTarget[toInt32(NetType::UDP)]);
    settings.setValue("Net/AddrLocal_TCPC", m_netUnit.addrLocal[toInt32(NetType::TCP_C)]);
    settings.setValue("Net/PortLocal_TCPC", m_netUnit.portLocal[toInt32(NetType::TCP_C)]);
    settings.setValue("Net/AddrTarget_TCPC", m_netUnit.addrTarget[toInt32(NetType::TCP_C)]);
    settings.setValue("Net/PortTarget_TCPC", m_netUnit.portTarget[toInt32(NetType::TCP_C)]);
    settings.setValue("Net/AddrLocal_TCPS", m_netUnit.addrLocal[toInt32(NetType::TCP_S)]);
    settings.setValue("Net/PortLocal_TCPS", m_netUnit.portLocal[toInt32(NetType::TCP_S)]);

    settings.setValue("Net/isPortLocalAuto_UDP",
                      m_netUnit.isPortLocalAuto[toInt32(NetType::UDP)] ? 1 : 0);
    settings.setValue("Net/isPortLocalAuto_TCPC",
                      m_netUnit.isPortLocalAuto[toInt32(NetType::TCP_C)] ? 1 : 0);

    settings.setValue("PortMode", ui->combo_portMode->currentIndex());

    m_dataLogMode[0] = 0;
    m_dataLogMode[0] |= m_dataLogDlgSerial->m_recvMode;
    m_dataLogMode[0] |= m_dataLogDlgSerial->m_sendMode << 2;
    m_dataLogMode[0] |= m_dataLogDlgSerial->m_isLog << 4;
    settings.setValue("DataLog/ModeCtrl_Serial", m_dataLogMode[0]);

    m_dataLogMode[1] = 0;
    m_dataLogMode[1] |= m_dataLogDlgNet->m_recvMode;
    m_dataLogMode[1] |= m_dataLogDlgNet->m_sendMode << 2;
    m_dataLogMode[1] |= m_dataLogDlgNet->m_isLog << 4;
    settings.setValue("DataLog/ModeCtrl_Net", m_dataLogMode[1]);
}

void Widget::loadConf() {
    QSettings settings(CONF_PATH, QSettings::IniFormat);
    QString str = "";
    int32_t stmp32 = 0;
    bool isUint = false;

    // main widget
    stmp32 = settings.value("Main/TabIndex", 0).toInt();
    ui->tabWidget->setCurrentIndex(stmp32);

    // serial port info
    // refresh serial port and select
    serialRefresh();
    str = settings.value("Serial/COM", "COM1").toString();
    stmp32 = ui->combo_serialPort->findText(str);
    if (stmp32 >= 0)
        ui->combo_serialPort->setCurrentIndex(stmp32);

    ui->combo_serialBaud->setValidator(new QIntValidator(1, 20000000));
    stmp32 = settings.value("Serial/Baud", 5).toInt();
    ui->combo_serialBaud->setCurrentIndex(stmp32);
    stmp32 = settings.value("Serial/DataBit", 0).toInt();
    ui->combo_serialDataBit->setCurrentIndex(stmp32);
    stmp32 = settings.value("Serial/StopBit", 0).toInt();
    ui->combo_serialStopBit->setCurrentIndex(stmp32);
    stmp32 = settings.value("Serial/ParityBit", 0).toInt();
    ui->combo_serialParityBit->setCurrentIndex(stmp32);

    // net port info
    stmp32 = settings.value("Net/NetType", 0).toInt();
    if (stmp32 == 0)
        on_combo_netType_currentIndexChanged(stmp32);
    else if (stmp32 >= 0) {
        ui->combo_netType->setCurrentIndex(stmp32);
        m_netUnit.netType = static_cast<NetType>(stmp32);
    }

    QString localIP = getLocalIP();

    str = settings.value("Net/AddrLocal_UDP", localIP).toString();
    m_netUnit.addrLocal[toInt32(NetType::UDP)] = str;
    stmp32 = settings.value("Net/PortLocal_UDP", 0).toUInt();
    m_netUnit.portLocal[toInt32(NetType::UDP)] = QString::number(stmp32);
    str = settings.value("Net/AddrTarget_UDP", localIP).toString();
    m_netUnit.addrTarget[toInt32(NetType::UDP)] = str;
    stmp32 = settings.value("Net/PortTarget_UDP", 0).toUInt();
    m_netUnit.portTarget[toInt32(NetType::UDP)] = QString::number(stmp32);
    str = settings.value("Net/AddrLocal_TCPC", localIP).toString();
    m_netUnit.addrLocal[toInt32(NetType::TCP_C)] = str;
    stmp32 = settings.value("Net/PortLocal_TCPC", 0).toUInt();
    m_netUnit.portLocal[toInt32(NetType::TCP_C)] = QString::number(stmp32);
    str = settings.value("Net/AddrTarget_TCPC", localIP).toString();
    m_netUnit.addrTarget[toInt32(NetType::TCP_C)] = str;
    stmp32 = settings.value("Net/PortTarget_TCPC", 0).toUInt();
    m_netUnit.portTarget[toInt32(NetType::TCP_C)] = QString::number(stmp32);
    str = settings.value("Net/AddrLocal_TCPS", localIP).toString();
    m_netUnit.addrLocal[toInt32(NetType::TCP_S)] = str;
    stmp32 = settings.value("Net/PortLocal_TCPS", 0).toUInt();
    m_netUnit.portLocal[toInt32(NetType::TCP_S)] = QString::number(stmp32);

    stmp32 = settings.value("Net/isPortLocalAuto_UDP", 0).toUInt();
    m_netUnit.isPortLocalAuto[toInt32(NetType::UDP)] = stmp32 == 1 ? true : false;
    stmp32 = settings.value("Net/isPortLocalAuto_TCPC", 0).toUInt(&isUint);
    m_netUnit.isPortLocalAuto[toInt32(NetType::TCP_C)] = stmp32 == 1 ? true : false;

    // refresh address
    netRefresh();

    // query is the stored address in the combo and select
    stmp32 = ui->combo_netAddrLocal->findText(m_netUnit.addrLocal[toInt32(m_netUnit.netType)]);
    if (stmp32 >= 0)
        ui->combo_netAddrLocal->setCurrentIndex(stmp32);
    if (toInt32(m_netUnit.netType) < 2) {
        // udp or tcpc
        stmp32 =
            ui->combo_netAddrTarget->findText(m_netUnit.addrTarget[toInt32(m_netUnit.netType)]);
        if (stmp32 >= 0)
            ui->combo_netAddrTarget->setCurrentIndex(stmp32);
    }

    // set the stored port
    ui->lineEdit_netPortLocal->setText(m_netUnit.portLocal[toInt32(m_netUnit.netType)]);
    if (toInt32(m_netUnit.netType) < 2) {
        // udp or tcpc
        ui->lineEdit_netPortTarget->setText(m_netUnit.portTarget[toInt32(m_netUnit.netType)]);
        if (m_netUnit.isPortLocalAuto[toInt32(m_netUnit.netType)]) {
            ui->check_netPortLocal->setChecked(true);
            ui->lineEdit_netPortLocal->setText("Auto");
        }
    }

    stmp32 = settings.value("PortMode", 0).toInt();
    if (stmp32 == 0)
        on_combo_portMode_currentIndexChanged(0); // default 0, so must call slot manual
    else
        ui->combo_portMode->setCurrentIndex(1);

    m_dataLogMode[0] = settings.value("DataLog/ModeCtrl_Serial", 0x1A).toInt();
    m_dataLogMode[1] = settings.value("DataLog/ModeCtrl_Net", 0x1A).toInt();
    m_dataLogDlgSerial->setModeCtrl(m_dataLogMode[0]);
    m_dataLogDlgNet->setModeCtrl(m_dataLogMode[1]);
}

void Widget::serialSend(QByteArray data, int32_t len) {
    if (m_serialPort->isOpen() == false) {
        emit showLog(LogLevel::WAR, "串口未打开, 尝试自动打开");
        on_button_serialSwitch_clicked();
        if (m_serialPort->isOpen() == false)
            return;
    }
    int32_t sendLen = 0;

    m_serialMutex.lock();
    sendLen = m_serialPort->write(data, len);
    m_serialMutex.unlock();

    if (sendLen < 0)
        emit showLog(LogLevel::ERR, "发送失败!");
    else {
        m_dataLogDlgSerial->m_sendByte += sendLen;
        m_dataLogDlgSerial->m_sendPkg++;
        emit updateDataCntSerial();

        if (m_dataLogDlgSerial->m_sendMode != 0)
            emit dataShowSerial(data, len, true, "# Send ");
    }
}

void Widget::serialRefresh() {
    ui->combo_serialPort->clear();
    foreach (QSerialPortInfo info, QSerialPortInfo::availablePorts()) {
        ui->combo_serialPort->addItem(info.portName());
    }
}

QString Widget::getLocalIP() {
    QString hostName = QHostInfo::localHostName(); // 本地主机名
    QHostInfo hostInfo = QHostInfo::fromName(hostName);
    QString localIP = "";

    QList<QHostAddress> addList = hostInfo.addresses(); //

    if (!addList.isEmpty())
        for (int i = 0; i < addList.count(); i++) {
            QHostAddress aHost = addList.at(i);
            if (QAbstractSocket::IPv4Protocol == aHost.protocol()) {
                localIP = aHost.toString();
                break;
            }
        }
    return localIP;
}

void Widget::netRefresh() {
    // refresh ip address
    QString str = QHostInfo::localHostName();
    QHostInfo hostInfo = QHostInfo::fromName(str);
    QList<QHostAddress> addrList = hostInfo.addresses();
    foreach (QHostAddress hostAddr, addrList) {
        if (QAbstractSocket::IPv4Protocol == hostAddr.protocol()) {
            ui->combo_netAddrLocal->addItem(hostAddr.toString());
            ui->combo_netAddrTarget->addItem(hostAddr.toString());
        }
    }
    ui->combo_netAddrLocal->addItem("127.0.0.1");
    ui->combo_netAddrTarget->addItem("127.0.0.1");
    ui->combo_netAddrLocal->addItem("0.0.0.0");
    ui->combo_netAddrTarget->addItem("0.0.0.0");
}

void Widget::netSend(QByteArray data, int32_t len) {
    int32_t sendLen = 0;
    QString targetInfo = "";
    QString addrStrTarget = ui->combo_netAddrTarget->currentText();
    QString portStrTarget = ui->lineEdit_netPortTarget->text();
    uint16_t portTarget = 0;

    switch (m_netUnit.netType) {
    case NetType::UDP: {
        if (m_udpSocket->state() != QUdpSocket::BoundState) {
            emit showLog(LogLevel::ERR, "Udp has not bound local port!");
            return;
        }
        bool isUint = false;
        addrStrTarget = ui->combo_netAddrTarget->currentText();
        portStrTarget = ui->lineEdit_netPortTarget->text();
        QHostAddress addrTarget(addrStrTarget);
        portTarget = portStrTarget.toUInt(&isUint);
        if (isUint == false) {
            emit showLog(LogLevel::ERR, "Udp Target Port Err!");
            return;
        }
        if (addrTarget.protocol() != QAbstractSocket::IPv4Protocol) {
            // ipv4 invalid
            emit showLog(LogLevel::ERR, "Udp Target Addr Err");
            return;
        }

        m_netUnit.addrTarget[toInt32(NetType::UDP)] = addrStrTarget;
        m_netUnit.portTarget[toInt32(NetType::UDP)] = portStrTarget;
        targetInfo = addrStrTarget + ":" + portStrTarget + " ";

        m_netMutex.lock();
        sendLen = m_udpSocket->writeDatagram(data.data(), len, addrTarget, portTarget);
        m_netMutex.unlock();
        break;
    }
    case NetType::TCP_C:
        if (m_tcpClient->state() != QTcpSocket::ConnectedState) {
            emit showLog(LogLevel::WAR, "Socket is unconnected, send failed");
            return;
        }
        addrStrTarget = m_tcpClient->peerAddress().toString();
        portStrTarget = QString::number(m_tcpClient->peerPort());
        targetInfo = addrStrTarget + ":" + portStrTarget + " ";
        m_netMutex.lock();
        sendLen = m_tcpClient->write(data.data(), len);
        m_netMutex.unlock();
        break;
    case NetType::TCP_S: {
        bool isSend = false;
        if (ui->combo_netClient->count() < 2) {
            emit showLog(LogLevel::ERR, "No active connection!");
            return;
        }

        if (ui->combo_netClient->currentIndex() > 0) {
            // not send to all
            QStringList strList = ui->combo_netClient->currentText().split(':');
            addrStrTarget = strList.at(0);
            portStrTarget = strList.at(1);
            portTarget = portStrTarget.toUInt();
            targetInfo = addrStrTarget + ":" + portStrTarget + " ";
        }

        foreach (QTcpSocket *socket, m_netUnit.tcpClientList) {
            if ((ui->combo_netClient->currentIndex() == 0) ||
                (socket->peerAddress().toString() == addrStrTarget &&
                 socket->peerPort() == portTarget)) {
                addrStrTarget = socket->peerAddress().toString();
                portStrTarget = QString::number(socket->peerPort());
                targetInfo = addrStrTarget + ":" + portStrTarget + " ";
                if (socket->state() != QTcpSocket::ConnectedState) {
                    emit showLog(LogLevel::ERR, targetInfo + "socket is unconnected, send failed");
                    continue;
                }
                m_netMutex.lock();
                sendLen = socket->write(data.data(), len);
                m_netMutex.unlock();
                isSend = true;

                if (sendLen >= 0) {
                    m_dataLogDlgNet->m_sendByte += sendLen;
                    m_dataLogDlgNet->m_sendPkg++;
                    emit updateDataCntNet();

                    if (m_dataLogDlgNet->m_sendMode != 0)
                        emit dataShowNet(data, len, true, "# Send to " + targetInfo);
                } else
                    emit showLog(LogLevel::ERR, "发送失败!");
            }
        }
        if (!isSend) {
            emit showLog(LogLevel::ERR, "Target client " + targetInfo + "not found, send failed");
        }
        return;
    }
    default:
        return;
    }

    if (sendLen >= 0) {
        m_dataLogDlgNet->m_sendByte += sendLen;
        m_dataLogDlgNet->m_sendPkg++;
        emit updateDataCntNet();

        if (m_dataLogDlgNet->m_sendMode != 0)
            emit dataShowNet(data, len, true, "# Send to " + targetInfo);
    } else
        emit showLog(LogLevel::ERR, "发送失败!");
}

void Widget::netStop() {
    switch (m_netUnit.netType) {
    case NetType::UDP:
        if (m_udpSocket->state() == QUdpSocket::BoundState) {
            // if has bound, try to unbind
            m_udpSocket->abort();
            // check again
            if (m_udpSocket->state() != QUdpSocket::BoundState) {
                emit showLog(LogLevel::INF, "端口解绑成功!");
            } else {
                emit showLog(LogLevel::ERR, "端口解绑异常!!!");
            }
        } else {
            emit showLog(LogLevel::WAR, "端口未绑定!");
        }
        ui->button_netSwitch->setText("绑定端口");
        emit showLog(LogLevel::INF, "Udp bind was stop");
        break;
    case NetType::TCP_C:
        if (m_tcpClient->state() != QTcpSocket::UnconnectedState) {
            // if socket was connected, try to disconnect
            m_tcpClient->disconnectFromHost();
            if (!m_tcpClient->waitForDisconnected(1000))
                m_tcpClient->abort();
        }
        ui->button_netSwitch->setText("开始连接");
        emit showLog(LogLevel::INF, "Tcp Client connection was stop");
        break;
    case NetType::TCP_S:
        if (m_tcpServer->isListening()) {
            ui->combo_netClient->setCurrentIndex(0);
            on_button_netClientClose_clicked();
            m_tcpServer->close();
            emit showLog(LogLevel::INF, "Tcp Server was stop");
        }
        ui->button_netSwitch->setText("开始监听");
        break;
    default:
        break;
    }
    ui->combo_netType->setEnabled(true);
    ui->combo_netAddrLocal->setEnabled(true);
    ui->lineEdit_netPortLocal->setEnabled(true);
    ui->combo_netAddrTarget->setEnabled(true);
    ui->lineEdit_netPortTarget->setEnabled(true);
    m_netUnit.isWorking = false;
}

void Widget::on_showLog(LogLevel level, QString string) {
    QString Time = QDateTime().currentDateTime().toString("[yyyy-MM-dd hh:mm:ss]");
    QString log = "";
    QString fontColor = g_strLogColor[(int32_t)level];

    if (level == LogLevel::COLORFUL) {
        fontColor = string.left(fontColor.length());
        // string.remove(0, fontColor.length()); // unnecessary
        level = LogLevel::INF;
    }

    switch (level) {
    case LogLevel::DBG:
        log = Time + "[DBG]<br>" + string;
        break;
    case LogLevel::INF:
        log = Time + "<br>" + string;
        break;
    case LogLevel::WAR:
        log = Time + "[WAR]<br>" + string;
        break;
    case LogLevel::ERR:
        log = Time + "[ERR]<br>" + string;
        break;
    default:
        break;
    }

    if (!(level == LogLevel::DBG && !g_isDebugMode)) {
        ui->txtBs_log->append(AppTheme::adaptHtml(fontColor + log));
        QScrollBar *scrollBar = ui->txtBs_log->verticalScrollBar();
        scrollBar->setSliderPosition(scrollBar->maximum());
    }
}

void Widget::on_PinHZ_send(QByteArray &data, int32_t len) {
    if (m_portMode == PortMode::Serial)
        serialSend(data, len);
    else
        netSend(data, len);
}

void Widget::on_PinHZ_frameChanged(QString dataStr) {
    m_dataLogDlgSerial->dataSendSync(dataStr);
    m_dataLogDlgNet->dataSendSync(dataStr);
}

void Widget::on_dataLogSyncRequested() {
    FormDataLog *dataLog = qobject_cast<FormDataLog *>(sender());
    if (!dataLog)
        return;

    QString dataStr = m_PinHzDlg->outputText();
    dataLog->dataSendSync(dataStr);
}

void Widget::on_dataLog_send(QString dataStr) {
    FormDataLog *dataLog = qobject_cast<FormDataLog *>(sender());
    if (!dataLog)
        return;

    QString str = dataStr.simplified();
    if (str.isEmpty()) {
        emit showLog(LogLevel::WAR, "发送内容为空!");
        return;
    }

    QByteArray data;
    int32_t len = m_PinHzDlg->PinHZ_parse(str, data);
    if (len <= 0) {
        emit showLog(LogLevel::WAR, "发送内容解析为空!");
        return;
    }

    if (dataLog == m_dataLogDlgSerial)
        serialSend(data, len);
    else if (dataLog == m_dataLogDlgNet)
        netSend(data, len);
}

void Widget::on_serialRecv() {
    int64_t availableBytes = m_serialPort->bytesAvailable();
    if (availableBytes > 0) {
        if (availableBytes > PINHZ_CACHE_LEN)
            availableBytes = PINHZ_CACHE_LEN;
        QByteArray data = m_serialPort->read(availableBytes);

        if (!data.isEmpty()) {
            int32_t recvLen = data.size();

            m_dataLogDlgSerial->m_recvByte += recvLen;
            m_dataLogDlgSerial->m_recvPkg++;

            emit updateDataCntSerial();
            emit dataShowSerial(data, recvLen, false, "# Recv ");
            emit PinHZ_recv(data, recvLen);
        }
    }

    return;
}

/* **************************************************
 * @brief LOG信息清除
 * **************************************************/
void Widget::on_button_clearLog_clicked() {
    ui->txtBs_log->setText("");
}

void Widget::on_button_picSelect_clicked() {
    QString srcPath = QFileDialog::getOpenFileName(this, tr("选择源文件"), "",
                                                   tr("Images (*.png *.jpg *.bmp *.jpeg *.gif)"));
    if (srcPath.isEmpty()) {
        emit showLog(LogLevel::ERR, "转换取消：未选择源文件");
        return;
    }
    if (!QFile::exists(srcPath)) {
        emit showLog(LogLevel::ERR, QString("转换失败：源文件不存在:%1").arg(srcPath));
        return;
    }

    QString dstPath =
        QFileDialog::getSaveFileName(this, tr("选择保存位置"), "dst.ico", tr("ICO文件 (*.ico)"));
    if (dstPath.isEmpty()) {
        emit showLog(LogLevel::ERR, "转换取消：未选择保存位置");
        return;
    }

    QImage img(srcPath);
    if (img.isNull()) {
        emit showLog(
            LogLevel::ERR,
            QString("转换失败：源文件无法读取(非有效图片/文件损坏):" FONT_COLOR_DARK_ORANGE "%1")
                .arg(srcPath));
        return;
    }

    if (img.save(dstPath, "ICO"))
        emit showLog(LogLevel::INF,
                     QString("转换成功：保存至:" FONT_COLOR_DARK_ORANGE "%1").arg(dstPath));
    else
        emit showLog(LogLevel::ERR,
                     QString("转换失败：(路径无权限/磁盘满)无法保存至:" FONT_COLOR_DARK_ORANGE "%1")
                         .arg(dstPath));
}

void Widget::on_button_dataLog_clicked() {
    if (m_dataLogPageMode == PageMode::Embeded) {
        // embeded to detached
        if (ui->tabWidget->currentIndex() == 1)
            ui->tabWidget->setCurrentIndex(0);
        ui->tabWidget->removeTab(1);
        QPoint mainPos = this->pos();
        if (m_portMode == PortMode::Serial) {
            m_dataLogDlgNet->setParent(this);
            m_dataLogDlgSerial->setWindowFlags(Qt::Window | m_dataLogDlgSerial->windowFlags());
            m_dataLogDlgSerial->move(mainPos + QPoint(210, 70));
            m_dataLogDlgSerial->show();
            m_dataLogPageMode = PageMode::Detached;
        } else {
            m_dataLogDlgNet->setParent(this);
            m_dataLogDlgNet->setWindowFlags(Qt::Window | m_dataLogDlgNet->windowFlags());
            m_dataLogDlgNet->move(mainPos + QPoint(210, 70));
            m_dataLogDlgNet->show();
            m_dataLogPageMode = PageMode::Detached;
        }
        ui->button_dataLog->setText("嵌入数据日志");
    } else {
        // detached to embeded
        if (m_portMode == PortMode::Serial) {
            m_dataLogDlgSerial->setWindowFlags(Qt::Widget);
            m_dataLogDlgSerial->setParent(ui->tabWidget);
            ui->tabWidget->insertTab(1, m_dataLogDlgSerial, "串口数据日志");
            m_dataLogPageMode = PageMode::Embeded;
        } else {
            m_dataLogDlgNet->setWindowFlags(Qt::Widget);
            m_dataLogDlgNet->setParent(ui->tabWidget);
            ui->tabWidget->insertTab(1, m_dataLogDlgNet, "网口数据日志");
            m_dataLogPageMode = PageMode::Embeded;
        }
        ui->button_dataLog->setText("弹出数据日志");
    }
}

void Widget::on_button_serialRefresh_clicked() {
    serialRefresh();
    emit showLog(LogLevel::INF, "串口刷新完成");
}

void Widget::on_button_serialSwitch_clicked() {
    QString curStr;
    curStr = ui->button_serialSwitch->text();

    if (curStr == "打开串口") {
        int32_t index;

        QSerialPortInfo portInfo(ui->combo_serialPort->currentText());
        m_serialPort->setPort(portInfo);

        index = ui->combo_serialBaud->currentText().toInt();
        m_serialPort->setBaudRate(index);

        index = ui->combo_serialDataBit->currentIndex();
        switch (index) {
        case 0:
            m_serialPort->setDataBits(QSerialPort::Data8);
            break;
        case 1:
            m_serialPort->setDataBits(QSerialPort::Data7);
            break;
        case 2:
            m_serialPort->setDataBits(QSerialPort::Data6);
            break;
        case 3:
            m_serialPort->setDataBits(QSerialPort::Data5);
            break;
        default:
            m_serialPort->setDataBits(QSerialPort::Data8);
            break;
        }

        index = ui->combo_serialStopBit->currentIndex();
        switch (index) {
        case 0:
            m_serialPort->setStopBits(QSerialPort::OneStop);
            break;
        case 1:
            m_serialPort->setStopBits(QSerialPort::OneAndHalfStop);
            break;
        case 2:
            m_serialPort->setStopBits(QSerialPort::TwoStop);
            break;
        default:
            m_serialPort->setStopBits(QSerialPort::OneStop);
            break;
        }

        index = ui->combo_serialParityBit->currentIndex();
        switch (index) {
        case 0:
            m_serialPort->setParity(QSerialPort::NoParity);
            break;
        case 1:
            m_serialPort->setParity(QSerialPort::OddParity);
            break;
        case 2:
            m_serialPort->setParity(QSerialPort::EvenParity);
            break;
        case 3:
            m_serialPort->setParity(QSerialPort::MarkParity);
            break;
        case 4:
            m_serialPort->setParity(QSerialPort::SpaceParity);
            break;
        default:
            m_serialPort->setParity(QSerialPort::NoParity);
            break;
        }

        m_serialPort->setReadBufferSize(10240);

        if (m_serialPort->open(QIODevice::ReadWrite)) {
            m_serialPort->clear();
            m_serialPort->clearError();
            ui->button_serialSwitch->setText("关闭串口");
            ui->button_serialRefresh->setEnabled(false);
            ui->combo_serialPort->setEnabled(false);
            ui->combo_serialBaud->setEnabled(false);
            ui->combo_serialDataBit->setEnabled(false);
            ui->combo_serialStopBit->setEnabled(false);
            ui->combo_serialParityBit->setEnabled(false);
            emit showLog(LogLevel::INF, "打开了串口");
        } else {
            emit showLog(LogLevel::ERR, "打开串口失败, 请检查是否串口被占用或者打开的是com1");
        }
    } else {
        m_serialPort->close();
        ui->button_serialSwitch->setText("打开串口");
        ui->button_serialRefresh->setEnabled(true);
        ui->combo_serialPort->setEnabled(true);
        ui->combo_serialBaud->setEnabled(true);
        ui->combo_serialDataBit->setEnabled(true);
        ui->combo_serialStopBit->setEnabled(true);
        ui->combo_serialParityBit->setEnabled(true);
        emit showLog(LogLevel::INF, "关闭了串口");
    }
}

void Widget::on_button_netSwitch_clicked() {
    m_netUnit.netType = static_cast<NetType>(ui->combo_netType->currentIndex());

    if (m_netUnit.isWorking == false) {
        // try to connect
        bool isUint = false;
        QString addrStrLocal = ui->combo_netAddrLocal->currentText();
        QString portStrLocal = ui->lineEdit_netPortLocal->text();
        if (portStrLocal == "Auto")
            portStrLocal = "0";
        QHostAddress addrLocal(addrStrLocal);
        uint16_t portLocal = portStrLocal.toUInt(&isUint);

        if (isUint == false) {
            emit showLog(LogLevel::ERR, "Local port invalid");
            return;
        }

        if (addrLocal.protocol() != QAbstractSocket::IPv4Protocol) {
            // ipv4 invalid
            emit showLog(LogLevel::ERR, "Local addr invalid");
            return;
        }

        switch (m_netUnit.netType) {
        case NetType::UDP:
            if (m_udpSocket->state() == QUdpSocket::BoundState) {
                // if has bound, abort
                netStop();
            }
            if (m_udpSocket->bind(addrLocal, portLocal)) {
                if (portLocal == 0) {
                    portLocal = m_udpSocket->localPort();
                    portStrLocal = QString(portLocal);
                }
                m_netUnit.addrLocal[toInt32(NetType::UDP)] = addrStrLocal;
                m_netUnit.portLocal[toInt32(NetType::UDP)] = portStrLocal;
                on_netConnected();
            } else
                emit showLog(LogLevel::INF, "Udp Port bind failed!");
            break;
        case NetType::TCP_C: {
            QString addrStrTarget = ui->combo_netAddrTarget->currentText();
            QString portStrTarget = ui->lineEdit_netPortTarget->text();
            QHostAddress addrTarget(addrStrTarget);
            uint16_t portTarget = portStrTarget.toUInt(&isUint);

            if (isUint == false) {
                emit showLog(LogLevel::ERR, "Target port invalid");
                return;
            }

            if (addrTarget.protocol() != QAbstractSocket::IPv4Protocol) {
                // ipv4 invalid
                emit showLog(LogLevel::ERR, "Target addr invalid");
                return;
            }

            if (m_tcpClient->state() != QTcpSocket::UnconnectedState) {
                m_tcpClient->disconnectFromHost();
                if (!m_tcpClient->waitForDisconnected(1000))
                    netStop();
            }

            if (portLocal != 0) {
                if (!m_tcpClient->bind(addrLocal, portLocal)) {
                    emit showLog(LogLevel::ERR,
                                 QString("本地端口手动绑定失败<br>IP:" + addrStrLocal + ":" +
                                         portStrLocal + "<br>原因:" + m_tcpClient->errorString()));
                    return;
                }
            }

            m_tcpClient->connectToHost(addrTarget, portTarget);
            break;
        }
        case NetType::TCP_S:
            if (m_tcpServer->isListening()) {
                // if port has listen, stop
                netStop();
                ui->button_netSwitch->setText("开始监听");
            } else {
                if (m_tcpServer->listen(addrLocal, portLocal)) {
                    m_netUnit.addrLocal[toInt32(NetType::TCP_S)] = addrStrLocal;
                    m_netUnit.portLocal[toInt32(NetType::TCP_S)] = portStrLocal;
                    emit showLog(LogLevel::INF, "启动监听成功");
                    ui->button_netSwitch->setText("停止监听");
                    ui->combo_netType->setEnabled(false);
                    ui->combo_netAddrLocal->setEnabled(false);
                    ui->lineEdit_netPortLocal->setEnabled(false);
                    m_netUnit.isWorking = true;
                } else
                    emit showLog(LogLevel::INF, "启动监听失败, 原因:" + m_tcpServer->errorString());
            }
            break;
        default:
            break;
        }
    } else {
        // try to disconnect
        netStop();
        emit showLog(LogLevel::INF, "网络已断开");
    }
}

void Widget::on_button_netRefresh_clicked() {
    ui->combo_netAddrLocal->clear();
    ui->combo_netAddrTarget->clear();
    netRefresh();
    emit showLog(LogLevel::INF, "网口刷新完成");
}

void Widget::on_button_netClientClose_clicked() {
    int32_t connIndex = ui->combo_netClient->currentIndex();

    if (connIndex == 0) {
        // close all connection
        // delete list item on disconnected slot
        foreach (QTcpSocket *socket, m_netUnit.tcpClientList) {
            if (socket->state() != QTcpSocket::UnconnectedState) {
                // if socket was connected, try to disconnect
                socket->disconnectFromHost();
                if (!socket->waitForDisconnected(1000))
                    socket->abort();
            }
        }
    } else {
        // close select connection
        // delete list item on disconnected slot
        QTcpSocket *socket = m_netUnit.tcpClientList.at(connIndex - 1);
        if (socket->state() != QTcpSocket::UnconnectedState) {
            // if socket was connected, try to disconnect
            socket->disconnectFromHost();
            if (!socket->waitForDisconnected(1000))
                socket->abort();
        }
    }
}

void Widget::on_combo_portMode_currentIndexChanged(int index) {
    if (index == 0) {
        // serial mode
        ui->groupBox_serialPort->show();
        ui->groupBox_net->hide();
        m_portMode = PortMode::Serial;
    } else {
        // net mode
        ui->groupBox_serialPort->hide();
        ui->groupBox_net->show();
        m_portMode = PortMode::Net;
    }

    if (m_dataLogPageMode == PageMode::Embeded) {
        int32_t currIndex = ui->tabWidget->currentIndex();
        ui->tabWidget->removeTab(1);
        if (index == 0) {
            m_dataLogDlgSerial->setWindowFlags(Qt::Widget);
            ui->tabWidget->insertTab(1, m_dataLogDlgSerial, "串口数据日志");
        } else {
            m_dataLogDlgNet->setWindowFlags(Qt::Widget);
            ui->tabWidget->insertTab(1, m_dataLogDlgNet, "网口数据日志");
        }
        ui->tabWidget->setCurrentIndex(currIndex);
    } else {
        if (index == 0) {
            m_dataLogDlgNet->close();
            m_dataLogDlgNet->setParent(this);
            m_dataLogDlgSerial->setWindowFlags(Qt::Window | m_dataLogDlgSerial->windowFlags());
            QPoint mainPos = this->pos();
            m_dataLogDlgSerial->move(mainPos + QPoint(210, 70));
            m_dataLogDlgSerial->show();
        } else {
            m_dataLogDlgSerial->close();
            m_dataLogDlgNet->setParent(this);
            m_dataLogDlgNet->setWindowFlags(Qt::Window | m_dataLogDlgNet->windowFlags());
            QPoint mainPos = this->pos();
            m_dataLogDlgNet->move(mainPos + QPoint(210, 70));
            m_dataLogDlgNet->show();
        }
    }
}

void Widget::on_combo_netType_currentIndexChanged(int index) {
    // save current config
    m_netUnit.addrLocal[toInt32(m_netUnit.netType)] = ui->combo_netAddrLocal->currentText();
    if (m_netUnit.netType != NetType::TCP_S) {
        // UDP or TCP Client
        // save target info and is local port auto
        m_netUnit.addrTarget[toInt32(m_netUnit.netType)] = ui->combo_netAddrTarget->currentText();
        m_netUnit.portTarget[toInt32(m_netUnit.netType)] = ui->lineEdit_netPortTarget->text();
        m_netUnit.isPortLocalAuto[toInt32(m_netUnit.netType)] = ui->check_netPortLocal->isChecked();
        if (!ui->check_netPortLocal->isChecked())
            m_netUnit.portLocal[toInt32(m_netUnit.netType)] = ui->lineEdit_netPortLocal->text();
    } else
        m_netUnit.portLocal[toInt32(m_netUnit.netType)] = ui->lineEdit_netPortLocal->text();

    // turn to selected net type
    m_netUnit.netType = static_cast<NetType>(index);

    // load selected net type config
    int32_t addrIndex = 0;
    addrIndex = ui->combo_netAddrLocal->findText(m_netUnit.addrLocal[toInt32(m_netUnit.netType)]);
    addrIndex = addrIndex < 0 ? 0 : addrIndex;
    ui->combo_netAddrLocal->setCurrentIndex(addrIndex);
    ui->lineEdit_netPortLocal->setText(m_netUnit.portLocal[toInt32(m_netUnit.netType)]);

    // deal widget show
    if (m_netUnit.netType != NetType::TCP_S) {
        // UDP or TCP Client
        // show widget of UDP/TCPC
        ui->label_netAddrTarget->show();
        ui->combo_netAddrTarget->show();
        ui->label_netPortTarget->show();
        ui->lineEdit_netPortTarget->show();
        ui->check_netPortLocal->show();

        // hide widget of TCPS
        ui->combo_netClient->hide();
        ui->button_netClientClose->hide();

        addrIndex =
            ui->combo_netAddrTarget->findText(m_netUnit.addrTarget[toInt32(m_netUnit.netType)]);
        addrIndex = addrIndex < 0 ? 0 : addrIndex;
        ui->combo_netAddrTarget->setCurrentIndex(addrIndex);
        ui->lineEdit_netPortTarget->setText(m_netUnit.portTarget[toInt32(m_netUnit.netType)]);
        ui->check_netPortLocal->setChecked(m_netUnit.isPortLocalAuto[toInt32(m_netUnit.netType)]);
        if (ui->check_netPortLocal->isChecked())
            ui->lineEdit_netPortLocal->setText("Auto");
    } else {
        // TCP Server
        // hide widget of UDP/TCPC
        ui->label_netAddrTarget->hide();
        ui->combo_netAddrTarget->hide();
        ui->label_netPortTarget->hide();
        ui->lineEdit_netPortTarget->hide();
        ui->check_netPortLocal->hide();

        // show widget of TCPS
        ui->combo_netClient->show();
        ui->button_netClientClose->show();

        // set local port input lineEdit to write able
        if (ui->lineEdit_netPortLocal->isReadOnly())
            ui->lineEdit_netPortLocal->setReadOnly(false);
    }

    switch (m_netUnit.netType) {
    case NetType::UDP:
        ui->button_netSwitch->setText("绑定端口");
        break;
    case NetType::TCP_C:
        ui->button_netSwitch->setText("连接端口");
        break;
    case NetType::TCP_S:
        ui->button_netSwitch->setText("监听端口");
        break;
    default:
        break;
    }
}

void Widget::on_netConnected() {
    QString succMsg = "";
    switch (m_netUnit.netType) {
    case NetType::UDP:
        ui->button_netSwitch->setText("取消绑定");
        succMsg = QString("端口绑定成功!") +
                  "<br>绑定端口端:" + m_netUnit.addrLocal[toInt32(NetType::UDP)] + ":" +
                  m_netUnit.portLocal[toInt32(NetType::UDP)];
        break;
    case NetType::TCP_C:
        ui->button_netSwitch->setText("断开连接");
        ui->combo_netAddrTarget->setEnabled(false);
        ui->lineEdit_netPortTarget->setEnabled(false);

        m_netUnit.addrLocal[toInt32(NetType::TCP_C)] = m_tcpClient->localAddress().toString();
        m_netUnit.portLocal[toInt32(NetType::TCP_C)] = QString::number(m_tcpClient->localPort());
        m_netUnit.addrTarget[toInt32(NetType::TCP_C)] = m_tcpClient->peerAddress().toString();
        m_netUnit.portTarget[toInt32(NetType::TCP_C)] = QString::number(m_tcpClient->peerPort());

        succMsg = QString("成功连接到服务端!") +
                  "<br>客户端:" + m_netUnit.addrLocal[toInt32(NetType::TCP_C)] + ":" +
                  m_netUnit.portLocal[toInt32(NetType::TCP_C)] +
                  "<br>服务端:" + m_netUnit.addrTarget[toInt32(NetType::TCP_C)] + ":" +
                  m_netUnit.portTarget[toInt32(NetType::TCP_C)];
        break;
    default:
        break;
    }

    ui->combo_netType->setEnabled(false);
    ui->combo_netAddrLocal->setEnabled(false);
    ui->lineEdit_netPortLocal->setEnabled(false);
    m_netUnit.isWorking = true;

    emit showLog(LogLevel::COLORFUL, FONT_COLOR_DARK_ORANGE + succMsg);
}

void Widget::on_netDisconnected() {
    switch (m_netUnit.netType) {
    case NetType::TCP_C:
        netStop();
        emit showLog(LogLevel::INF, "Net has disconnected");
        break;
    case NetType::TCP_S: {
        int32_t connIndex = -1;
        QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
        QString socketStr =
            socket->peerAddress().toString() + ":" + QString::number(socket->peerPort());
        for (int32_t i = 0; i < m_netUnit.tcpClientList.size(); i++) {
            if (socket == m_netUnit.tcpClientList.at(i)) {
                connIndex = i;
                break;
            }
        }
        if (connIndex >= 0) {
            m_netUnit.tcpClientList.removeAt(connIndex);
            ui->combo_netClient->removeItem(connIndex);
            ui->combo_netClient->setItemText(
                0, QString::asprintf("All Client(%d)", m_netUnit.tcpClientList.size()));
            emit showLog(LogLevel::INF, "Client offline:<br>" + socketStr);
        } else {
            emit showLog(LogLevel::ERR, "Unexpect client offline:<br>" + socketStr);
        }
        socket->deleteLater();
        break;
    }
    default:
        break;
    }
}

void Widget::on_netReadyRead() {
    QByteArray data;
    QString dataInfo = "# Recv ";
    switch (m_netUnit.netType) {
    case NetType::UDP:
        while (m_udpSocket->hasPendingDatagrams()) {
            int64_t size = m_udpSocket->pendingDatagramSize();
            data = QByteArray(size, 0);
            QHostAddress addr;
            uint16_t port = 0;
            int64_t recvLen = m_udpSocket->readDatagram(data.data(), data.size(), &addr, &port);
            if (recvLen >= 0) {
                dataInfo += "from " + addr.toString() + ':' + QString::number(port) + " ";
            } else {
                // recv failed
                emit showLog(LogLevel::ERR,
                             QString::asprintf("Recv failed, errCode:%d, reason:%s",
                                               m_udpSocket->error(),
                                               m_udpSocket->errorString().toUtf8().data()));
                return;
            }
        }
        break;
    case NetType::TCP_C:
        data = m_tcpClient->readAll(); // Read all data in QByteArray
        break;
    case NetType::TCP_S: {
        QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());

        if (!socket || !m_netUnit.tcpClientList.contains(socket))
            return;

        data = socket->readAll(); // Read all data in QByteArray
        dataInfo += "from " + socket->peerAddress().toString() + ':' +
                    QString::number(socket->peerPort()) + " ";
        break;
    }
    default:
        break;
    }

    if (!data.isEmpty()) {
        int32_t recvLen = data.size();

        m_dataLogDlgNet->m_recvByte += recvLen;
        m_dataLogDlgNet->m_recvPkg++;
        emit updateDataCntNet();

        emit dataShowNet(data, recvLen, false, dataInfo);
    }
}

void Widget::on_netStateChanged(QAbstractSocket::SocketState socketState) {
    QString msg = "";
    switch (socketState) {
    case QAbstractSocket::UnconnectedState:
        msg = "Scoket:UnconnectedState";
        break;
    case QAbstractSocket::HostLookupState:
        msg = "Scoket:HostLookupState";
        break;
    case QAbstractSocket::ConnectingState:
        msg = "Scoket:ConnectingState";
        break;
    case QAbstractSocket::ConnectedState:
        msg = "Scoket:ConnectedState";
        break;
    case QAbstractSocket::BoundState:
        msg = "Scoket:BoundState";
        break;
    case QAbstractSocket::ClosingState:
        msg = "Scoket:ClosingState";
        break;
    case QAbstractSocket::ListeningState:
        msg = "Scoket:ListeningState";
    }

    emit showLog(LogLevel::DBG, msg);
}

void Widget::on_netSocketErr(QAbstractSocket::SocketError error) {
    Q_UNUSED(error);
    emit showLog(LogLevel::ERR, "Socket err: " + m_tcpClient->errorString());
}

void Widget::on_netNewConnection() {
    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket *socket = m_tcpServer->nextPendingConnection();
        if (!socket)
            continue;

        QString addr = socket->peerAddress().toString() + ":" + QString::number(socket->peerPort());

        connect(socket, &QTcpSocket::disconnected, this, &Widget::on_netDisconnected);
        connect(socket, &QTcpSocket::readyRead, this, &Widget::on_netReadyRead);
        // connect(socket, &QTcpSocket::error, this, &Widget::on_netSocketErr);
        m_netUnit.tcpClientList.append(socket);
        ui->combo_netClient->setItemText(
            0, QString::asprintf("All Client(%d)", m_netUnit.tcpClientList.size()));
        ui->combo_netClient->addItem(addr);
        emit showLog(LogLevel::COLORFUL, FONT_COLOR_DARK_ORANGE "New client online: " + addr);
    }
}

void Widget::on_check_netPortLocal_toggled(bool checked) {
    if (checked) {
        // stored port
        m_netUnit.portLocal[toInt32(m_netUnit.netType)] = ui->lineEdit_netPortLocal->text();
        ui->lineEdit_netPortLocal->setText("Auto");
    } else {
        // restore port
        ui->lineEdit_netPortLocal->setText(m_netUnit.portLocal[toInt32(m_netUnit.netType)]);
    }

    ui->lineEdit_netPortLocal->setReadOnly(checked);
    m_netUnit.isPortLocalAuto[toInt32(m_netUnit.netType)] = checked;
}

void Widget::on_label_appIcon_clicked() {
    static int64_t lastClickTime = 0;
    static uint32_t iconClickCnt = 0;
    int64_t currClickTime = QTime::currentTime().msecsSinceStartOfDay();
    int64_t diff = currClickTime - lastClickTime;
    if (diff < 0)
        diff += 86400000;
    if (diff > 3000)
        iconClickCnt = 1;
    else
        iconClickCnt++;
    lastClickTime = currClickTime;
    if (iconClickCnt == 5) {
        if (g_isDebugMode == false) {
            // login creator
            g_isDebugMode = true;
            emit showLog(LogLevel::COLORFUL, FONT_COLOR_BLUEVIOLET "Login debugMode");
        } else {
            // quit creator
            g_isDebugMode = false;
            emit showLog(LogLevel::COLORFUL, FONT_COLOR_BLUEVIOLET "Quit debugMode");
        }

        iconClickCnt = 0;
        lastClickTime = 0;
    }
}

bool Widget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == ui->label_appIcon && event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            on_label_appIcon_clicked();
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}
