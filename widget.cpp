#include "ui_widget.h"
#include "widget.h"
#include <qmetaobject.h>

#define CONF_PATH "./conf.ini"

#define ZONE_START_HEAD (0)
#define ZONE_END_HEAD   (g_rowCntHead - 1)
#define ZONE_START_DATA (g_rowCntHead)
#define ZONE_END_DATA   (g_rowCntHead + g_rowCntData - 1)
#define ZONE_START_TAIL (g_rowCntHead + g_rowCntData)
#define ZONE_END_TAIL   (g_rowCntHead + g_rowCntData + g_rowCntCheck + g_rowCntTail - 1)

#define PINHZ_CACHE_LEN (81920)

QString g_logColor[] =
{
    FONT_COLOR_BLUE,  /* LOG_LEVEL_DBG */
    FONT_COLOR_BLACK, /* LOG_LEVEL_INF */
    FONT_COLOR_PINK,  /* LOG_LEVEL_WAR */
    FONT_COLOR_RED,   /* LOG_LEVEL_ERR */
    FONT_COLOR_BLACK  /* LOG_LEVEL_COLORFUL */
};

uint32_t g_timerCnt_10ms = 0;

// 拼好帧
int32_t g_rowCntHead = 1;
int32_t g_rowCntData = 0;
int32_t g_rowCntCheck = 0;
int32_t g_rowCntTail = 0;
bool g_isLittleEndian = true;
bool g_isAutoPinHZMode = true;

int32_t g_fillStart = 0;
int32_t g_fillRowCnt = 0;
int32_t g_fillBytes = 0;
uint32_t g_fillBuf[4096] = {0, };

uint8_t g_dataLog_recvMode = 0;
uint8_t g_dataLog_sendMode = 0;
// 0: serial inside 1: net inside
// 2: serial outside 3: net outside
uint8_t g_dataLog_logMode = 0;

bool g_isDebugMode = false;

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    ui->setupUi(this);

    m_autoReplyTimes = 0;
    m_autoReplyDelay = 0;

    m_netUnit.netType = 0;
    m_netUnit.netState = 0;

    // sub dialog
    m_fillItemDlg = new FormFillItem(this);
    connect(m_fillItemDlg, &FormFillItem::fillConfDone,
            this, &Widget::on_fillConfDone, Qt::QueuedConnection);

    m_dataLogDlgSerial = new FormDataLog(this, m_dataLogMode[0]);
    m_dataLogDlgSerial->setWindowTitle("串口数据日志");
    connect(this, &Widget::dataShowSerial, m_dataLogDlgSerial,
            &FormDataLog::on_dataShow, Qt::DirectConnection);
    connect(this, &Widget::updateDataCntSerial, m_dataLogDlgSerial,
            &FormDataLog::on_updateDataCnt, Qt::QueuedConnection);
    m_dataLogDlgNet = new FormDataLog(this, m_dataLogMode[1]);
    m_dataLogDlgNet->setWindowTitle("网口数据日志");
    connect(this, &Widget::dataShowNet, m_dataLogDlgNet,
            &FormDataLog::on_dataShow, Qt::DirectConnection);
    connect(this, &Widget::updateDataCntNet, m_dataLogDlgNet,
            &FormDataLog::on_updateDataCnt, Qt::QueuedConnection);

    if (ui->combo_portMode->currentIndex() == 0)
    {
        ui->tabWidget->insertTab(1, m_dataLogDlgSerial, "串口数据日志");
    }
    else
    {
        ui->tabWidget->insertTab(1, m_dataLogDlgNet, "网口数据日志");
    }

    m_crcConfDlg = new FormCRCConf(this);
    connect(m_crcConfDlg, &FormCRCConf::CRCConfDone,
            this, &Widget::on_CRCConfDone, Qt::DirectConnection);

    // log
    connect(this, &Widget::showLog, this, &Widget::on_showLog, Qt::DirectConnection);

    // serial port
    m_serialPort = new QSerialPort(this);
    connect(m_serialPort, &QSerialPort::readyRead, this, &Widget::on_serialRecv, Qt::DirectConnection);

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

    // Timer
    m_timer_Run = new QTimer(this);
    connect(m_timer_Run, &QTimer::timeout, this, &Widget::on_timerOut_Run);
    m_timer_Run->setInterval(10);
    m_timer_Run->start();

    ui->table_PinHZ->setColumnWidth(colDataType, 70);
    ui->table_PinHZ->setColumnWidth(colDataHex, 130);
    QHeaderView *colHeader = ui->table_PinHZ->horizontalHeader();
    colHeader->setSectionResizeMode(colDataType, QHeaderView::ResizeToContents);
    colHeader->setSectionResizeMode(colComment, QHeaderView::Stretch);
    PinHZComboInit(0, 0);
    ui->table_PinHZ->setCurrentCell(0, colDataHex);
    QMetaObject::invokeMethod(ui->button_PinHZ, "clicked", Qt::QueuedConnection);

    // install event filter, for click app icon 5 times to login debug mode
    ui->label_appIcon->installEventFilter(this);
}

Widget::~Widget()
{
    m_dataLogDlgSerial->close();
    m_dataLogDlgNet->close();
    saveConf();

    if (m_udpSocket->state() == QAbstractSocket::BoundState)
    {
        m_udpSocket->abort();
    }

    if (m_netUnit.netType == NetType_UDP)
    {
    }
    else if (m_netUnit.netType == NetType_TCPC && m_tcpClient->state() != QTcpSocket::UnconnectedState)
    {
        m_tcpClient->disconnectFromHost();
        if (!m_tcpClient->waitForDisconnected(1000))
            m_tcpClient->abort();
    }
    else if (m_netUnit.netType == NetType_TCPS && m_tcpServer->isListening())
    {
        netStop();
    }
    else
    {
        ;
    }

    delete m_timer_Run;
    delete m_fillItemDlg;
    delete m_dataLogDlgSerial;
    delete m_dataLogDlgNet;
    delete m_crcConfDlg;
    delete m_serialPort;
    delete m_udpSocket;
    delete m_tcpClient;
    delete m_tcpServer;
    delete ui;
}

uint8_t sumCheck(uint8_t *buf, uint32_t len)
{
    uint8_t ret = 0x00;
    uint32_t i = 0;
    for (i = 0; i < len; i++)
    {
        ret += buf[i];
    }
    return ret;
}

/* **************************************************
 * @brief 10ms Timer Slot
 * **************************************************/
void Widget::on_timerOut_Run()
{
    g_timerCnt_10ms++;
    static int32_t replyDelayCnt = 0;
    if (m_autoReplyTimes > 0)
    {
        if (replyDelayCnt >= m_autoReplyDelay)
        {
            QMetaObject::invokeMethod(ui->button_PinHZSend, "clicked", Qt::QueuedConnection);
            replyDelayCnt = 0;
            m_autoReplyTimes--;
        }
        replyDelayCnt += 10;
    }
    if (ui->check_autoSend->isChecked())
    {
        if (g_timerCnt_10ms % m_autoSendPeriod == 0)
            QMetaObject::invokeMethod(ui->button_PinHZSend, "clicked", Qt::QueuedConnection);
    }
}

void Widget::saveConf()
{
    QSettings settings(CONF_PATH, QSettings::IniFormat);

    settings.setValue("Serial/COM", ui->combo_serialPort->currentText());
    settings.setValue("Serial/Baud", ui->combo_serialBaud->currentIndex());
    settings.setValue("Serial/DataBit", ui->combo_serialDataBit->currentIndex());
    settings.setValue("Serial/StopBit", ui->combo_serialStopBit->currentIndex());
    settings.setValue("Serial/ParityBit", ui->combo_serialParityBit->currentIndex());

    settings.setValue("Net/NetType", ui->combo_netType->currentIndex());
    settings.setValue("Net/AddrLocal_UDP", m_netUnit.addrLocal[NetType_UDP]);
    settings.setValue("Net/PortLocal_UDP", m_netUnit.portLocal[NetType_UDP]);
    settings.setValue("Net/AddrTarget_UDP", m_netUnit.addrTarget[NetType_UDP]);
    settings.setValue("Net/PortTarget_UDP", m_netUnit.portTarget[NetType_UDP]);
    settings.setValue("Net/AddrLocal_TCPC", m_netUnit.addrLocal[NetType_TCPC]);
    settings.setValue("Net/PortLocal_TCPC", m_netUnit.portLocal[NetType_TCPC]);
    settings.setValue("Net/AddrTarget_TCPC", m_netUnit.addrTarget[NetType_TCPC]);
    settings.setValue("Net/PortTarget_TCPC", m_netUnit.portTarget[NetType_TCPC]);
    settings.setValue("Net/AddrLocal_TCPS", m_netUnit.addrLocal[NetType_TCPS]);
    settings.setValue("Net/PortLocal_TCPS", m_netUnit.portLocal[NetType_TCPS]);

    settings.setValue("Net/isPortLocalAuto_UDP",
                      m_netUnit.isPortLocalAuto[NetType_UDP] ? 1 : 0);
    settings.setValue("Net/isPortLocalAuto_TCPC",
                      m_netUnit.isPortLocalAuto[NetType_TCPC] ? 1 : 0);

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

void Widget::loadConf()
{
    QSettings settings(CONF_PATH, QSettings::IniFormat);
    QString str = "";
    int32_t utmp32 = 0;
    bool isUint = false;

    // serial port info
    // refresh serial port and select
    serialRefresh();
    str = settings.value("Serial/COM", "COM1").toString();
    utmp32 = ui->combo_serialPort->findText(str);
    if (utmp32 >= 0)
        ui->combo_serialPort->setCurrentIndex(utmp32);

    ui->combo_serialBaud->setValidator(new QIntValidator(1, 20000000));
    utmp32 = settings.value("Serial/Baud", 5).toUInt();
    ui->combo_serialBaud->setCurrentIndex(utmp32);
    utmp32 = settings.value("Serial/DataBit", 0).toUInt();
    ui->combo_serialDataBit->setCurrentIndex(utmp32);
    utmp32 = settings.value("Serial/StopBit", 0).toUInt();
    ui->combo_serialStopBit->setCurrentIndex(utmp32);
    utmp32 = settings.value("Serial/ParityBit", 0).toUInt();
    ui->combo_serialParityBit->setCurrentIndex(utmp32);

    // net port info
    utmp32 = settings.value("Net/NetType", 0).toInt();
    if (utmp32 == 0)
        on_combo_netType_currentIndexChanged(utmp32);
    else
        ui->combo_netType->setCurrentIndex(utmp32);
    m_netUnit.netType = utmp32;

    QString localIP = getLocalIP();

    str = settings.value("Net/AddrLocal_UDP", localIP).toString();
    m_netUnit.addrLocal[NetType_UDP] = str;
    utmp32 = settings.value("Net/PortLocal_UDP", 0).toUInt();
    m_netUnit.portLocal[NetType_UDP] = QString::number(utmp32);
    str = settings.value("Net/AddrTarget_UDP", localIP).toString();
    m_netUnit.addrTarget[NetType_UDP] = str;
    utmp32 = settings.value("Net/PortTarget_UDP", 0).toUInt();
    m_netUnit.portTarget[NetType_UDP] = QString::number(utmp32);
    str = settings.value("Net/AddrLocal_TCPC", localIP).toString();
    m_netUnit.addrLocal[NetType_TCPC] = str;
    utmp32 = settings.value("Net/PortLocal_TCPC", 0).toUInt();
    m_netUnit.portLocal[NetType_TCPC] = QString::number(utmp32);
    str = settings.value("Net/AddrTarget_TCPC", localIP).toString();
    m_netUnit.addrTarget[NetType_TCPC] = str;
    utmp32 = settings.value("Net/PortTarget_TCPC", 0).toUInt();
    m_netUnit.portTarget[NetType_TCPC] = QString::number(utmp32);
    str = settings.value("Net/AddrLocal_TCPS", localIP).toString();
    m_netUnit.addrLocal[NetType_TCPS] = str;
    utmp32 = settings.value("Net/PortLocal_TCPS", 0).toUInt();
    m_netUnit.portLocal[NetType_TCPS] = QString::number(utmp32);

    utmp32 = settings.value("Net/isPortLocalAuto_UDP", 0).toUInt();
    m_netUnit.isPortLocalAuto[NetType_UDP] = utmp32 == 1 ? true : false;
    utmp32 = settings.value("Net/isPortLocalAuto_TCPC", 0).toUInt(&isUint);
    m_netUnit.isPortLocalAuto[NetType_TCPC] = utmp32 == 1 ? true : false;

    // refresh address
    netRefresh();

    // query is the stored address in the combo and select
    utmp32 = ui->combo_netAddrLocal->findText(m_netUnit.addrLocal[m_netUnit.netType]);
    if (utmp32 >= 0)
        ui->combo_netAddrLocal->setCurrentIndex(utmp32);
    if (m_netUnit.netType < 2)
    {
        utmp32 = ui->combo_netAddrTarget->findText(m_netUnit.addrTarget[m_netUnit.netType]);
        if (utmp32 >= 0)
            ui->combo_netAddrTarget->setCurrentIndex(utmp32);
    }

    // set the stored port
    ui->lineEdit_netPortLocal->setText(m_netUnit.portLocal[m_netUnit.netType]);
    if (m_netUnit.netType < 2)
    {
        ui->lineEdit_netPortTarget->setText(m_netUnit.portTarget[m_netUnit.netType]);
        if (m_netUnit.isPortLocalAuto[m_netUnit.netType])
        {
            ui->check_netPortLocal->setChecked(true);
            ui->lineEdit_netPortLocal->setText("Auto");
        }
    }

    utmp32 = settings.value("PortMode", 0).toInt();
    if (utmp32 == 0)
        on_combo_portMode_currentIndexChanged(0); // default 0, so must call slot manual
    else
        ui->combo_portMode->setCurrentIndex(1);

    m_dataLogMode[0] = settings.value("DataLog/ModeCtrl_Serial", 0x1A).toInt();
    m_dataLogMode[1] = settings.value("DataLog/ModeCtrl_Net", 0x1A).toInt();
    m_dataLogDlgSerial->setModeCtrl(m_dataLogMode[0]);
    m_dataLogDlgNet->setModeCtrl(m_dataLogMode[0]);
}

int32_t Widget::PinHZDeal(QString str, uint8_t *buf)
{
    QStringList strList;
    int32_t strNum = 0;
    int32_t dealLen = 0;
    uint8_t utmp8 = 0;
    bool isHex = false;

    strList = str.split(" ");
    strNum = strList.size();

    for (int32_t i = 0; i < strNum; i++)
    {
        str = strList[i];
        for (int32_t j = 0; j < str.length(); j += 2)
        {
            utmp8 = str.mid(j, 2).toUInt(&isHex, 16);
            if (!isHex)
            {
                emit showLog(LogLevel_ERR, "异常字符:" + str.mid(j, 2));
            }
            buf[dealLen++] = utmp8;
            if (dealLen == PINHZ_CACHE_LEN - 1)
            {
                emit showLog(LogLevel_WAR, "缓冲区溢出，丢弃溢出部分");
                return dealLen;
            }
        }
    }

    return dealLen;
}

void Widget::PinHZComboInit(int32_t row, uint8_t dataType)
{
    QComboBox *comboBox = new QComboBox();

    comboBox->addItem("uint8");
    comboBox->addItem("int8");
    comboBox->addItem("uint16");
    comboBox->addItem("int16");
    comboBox->addItem("uint32");
    comboBox->addItem("int32");
    comboBox->addItem("uint64");
    comboBox->addItem("int64");
    comboBox->addItem("float");
    comboBox->addItem("double");
    comboBox->setCurrentIndex(dataType);
    connect(comboBox,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            &Widget::on_combo_PinHZ_dataTypeChanged);
    ui->table_PinHZ->setCellWidget(row, colDataType, comboBox);
}

int32_t Widget::getRowsBytes(int32_t rowStart, int32_t rowEnd)
{
    QWidget *widget = nullptr;
    QComboBox *comboBox = nullptr;
    int8_t dataLen = 0;
    int32_t dataBytes = 0;

    for (int32_t row = rowStart; row <= rowEnd; row++)
    {
        widget = ui->table_PinHZ->cellWidget(row, colDataType);
        comboBox = qobject_cast<QComboBox *>(widget);
        switch (comboBox->currentIndex())
        {
        case 0: /* uint8 */ /* fall-through */
        case 1: /* sint8 */ dataLen = 1; break;
        case 2: /* uint16 */ /* fall-through */
        case 3: /* sint16 */ dataLen = 2; break;
        case 8: /* float */ /* fall-through */
        case 4: /* uint32 */ /* fall-through */
        case 5: /* sint32 */ dataLen = 4; break;
        case 9: /* double */ /* fall-through */
        case 6: /* uint64 */ /* fall-through */
        case 7: /* sint64 */ dataLen = 8; break;
        default: break;
        }
        dataBytes += dataLen;
    }

    return dataBytes;
}

void Widget::updateDataZoneBytes()
{
    int32_t dataZoneBytes = 0;

    dataZoneBytes = getRowsBytes(ZONE_START_DATA, ZONE_END_DATA);

    ui->lineEdit_DataLen->setText(QString::number(dataZoneBytes));
}

int8_t Widget::checkRowZone(int32_t row)
{
    int8_t ret = 0;

    if (row < 0 || row > ui->table_PinHZ->rowCount())
        ret = -1; // 行号错误
    else if (row <= ZONE_END_HEAD)
        ret = 1; // 帧头域
    else if (row <= ZONE_END_DATA)
        ret = 2; // 数据域
    else if (g_rowCntCheck > 0 && row == ZONE_START_TAIL)
        ret = 3; // 校验域
    else if (row <= ZONE_END_TAIL)
        ret = 4; // 帧尾域
    else
        ret = 0;

    return ret;
}

void Widget::createItemsARow(int32_t row, QString rowHead, uint8_t dataType, QString dataHex, QString dataDec, QString comment)
{
    // 为一行的单元格创建 Items
    QTableWidgetItem *item;

    // 阻断信号，避免触发itemChanged
    ui->table_PinHZ->blockSignals(true);

    // rowHead
    item = new QTableWidgetItem(rowHead);
    ui->table_PinHZ->setVerticalHeaderItem(row, item);

    // hexType
    PinHZComboInit(row, dataType);

    // dataHex
    item = new QTableWidgetItem(dataHex, ctDataHex);
    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter); // 文本对齐格式
    ui->table_PinHZ->setItem(row, colDataHex, item);             // 为单元格设置Item

    // dataDec
    item = new QTableWidgetItem(dataDec, ctDataDec);
    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter); // 文本对齐格式
    ui->table_PinHZ->setItem(row, colDataDec, item);             // 为单元格设置Item

    // Comment
    item = new QTableWidgetItem(comment, ctComment);
    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter); // 文本对齐格式
    ui->table_PinHZ->setItem(row, colComment, item);             // 为单元格设置Item

    // 解除阻断
    ui->table_PinHZ->blockSignals(false);
}

void Widget::on_showLog(e_logLevel level, QString string)
{
    QString Time = QDateTime().currentDateTime().toString("[yyyy-MM-dd hh:mm:ss]");
    QString log = "";
    QString fontColor = g_logColor[level];

    if (level == LogLevel_COlORFUL)
    {
        fontColor = string.left(fontColor.length());
        // string.remove(0, fontColor.length()); // unnecessary
        level = LogLevel_INF;
    }

    switch (level)
    {
    case LogLevel_DBG: log = Time + "[DBG]<br>" + string; break;
    case LogLevel_INF: log = Time + "<br>" + string; break;
    case LogLevel_WAR: log = Time + "[WAR]<br>" + string; break;
    case LogLevel_ERR: log = Time + "[ERR]<br>" + string; break;
    default: break;
    }

    if (!(level == LogLevel_DBG && !g_isDebugMode))
    {
        ui->txtBs_log->append(fontColor + log);
        QScrollBar *scrollBar = ui->txtBs_log->verticalScrollBar();
        scrollBar->setSliderPosition(scrollBar->maximum());
    }
}

void Widget::serialSend(uint8_t *buf, int32_t len)
{
    if (m_serialPort->isOpen() == false)
    {
        emit showLog(LogLevel_WAR, "串口未打开, 尝试自动打开");
        on_button_serialSwitch_clicked();
        if (m_serialPort->isOpen() == false)
            return;
    }
    int32_t sendLen = 0;

    m_serialMutex.lock();
    sendLen = m_serialPort->write((char *)buf, len);
    m_serialMutex.unlock();

    if (sendLen < 0)
        emit showLog(LogLevel_ERR, "发送失败!");
    else
    {
        m_dataLogDlgSerial->m_sendByte += sendLen;
        m_dataLogDlgSerial->m_sendFrm++;
        updateDataCntSerial();

        if (m_dataLogDlgSerial->m_sendMode != 0)
            emit dataShowSerial(buf, len, true, "# Send ");
    }
}

void Widget::serialRefresh()
{
    ui->combo_serialPort->clear();
    foreach (QSerialPortInfo info, QSerialPortInfo::availablePorts())
    {
        ui->combo_serialPort->addItem(info.portName());
    }
}

void Widget::on_serialRecv()
{
    int64_t availableBytes = m_serialPort->bytesAvailable();
    if (availableBytes > 0)
    {
        if (availableBytes > PINHZ_CACHE_LEN)
            availableBytes = PINHZ_CACHE_LEN;
        QByteArray data = m_serialPort->read(availableBytes);

        if (!data.isEmpty())
        {
            uint8_t *buf = (uint8_t *)(data.data());
            int32_t recvLen = data.size();

            m_dataLogDlgSerial->m_recvByte += recvLen;
            m_dataLogDlgSerial->m_recvFrm++;
            updateDataCntSerial();

            emit dataShowSerial(buf, recvLen, false, "# Recv ");

            if (ui->check_autoReply->isChecked())
            {
                m_autoReplyTimes++;
            }
        }
    }

    return;
}

void Widget::on_fillConfDone()
{
    if (!m_fillItemDlg->m_isFillValid)
    {
        emit showLog(LogLevel_ERR, "填充数据输入错误!");
        return;
    }
    int32_t bufIndex = 0, dataType = 0, value = 0, dataLen = 0;
    QString str = "";
    QTableWidgetItem *item = nullptr;
    QWidget *widget = nullptr;
    QComboBox *comboBox = nullptr;
    uint8_t *pFillBuf = (uint8_t *)g_fillBuf;

    ui->table_PinHZ->blockSignals(true);
    for (int32_t row = g_fillStart; row < g_fillStart + g_fillRowCnt; row++)
    {
        if (row >= 0 && row < ui->table_PinHZ->rowCount())
        {
            widget = ui->table_PinHZ->cellWidget(row, colDataType);
            comboBox = qobject_cast<QComboBox *>(widget);
            dataType = comboBox->currentIndex();

            item = ui->table_PinHZ->item(row, colDataHex);
            if (m_fillItemDlg->m_fillStatus == 1) // 顺序填充
                str = QString::asprintf("%X", g_fillBuf[bufIndex++]);
            else // 重复填充
            {
                switch (dataType)
                {
                case 0: /* uint8 */ /* fall-through */
                case 1: /* sint8 */ dataLen = 1; break;
                case 2: /* uint16 */ /* fall-through */
                case 3: /* sint16 */ dataLen = 2; break;
                case 4: /* uint32 */ /* fall-through */
                case 5: /* sint32 */ /* fall-through */
                case 8: /* float  */ dataLen = 4; break;
                case 6: /* uint64 */ /* fall-through */
                case 7: /* sint64 */ /* fall-through */
                case 9: /* double */ dataLen = 8; break;
                default: break;
                }
                memcpy(&value, pFillBuf + bufIndex, dataLen);
                bufIndex += dataLen;
                str = QString::asprintf("%X", value);
            }

            str = m_hex2dec.StrFix(str, dataType, true, g_isLittleEndian);
            item->setText(str);
            str = m_hex2dec.Hex2DecString(str, dataType, g_isLittleEndian);
            item = ui->table_PinHZ->item(row, colDataDec);
            item->setText(str);
        }
    }
    ui->table_PinHZ->blockSignals(false);

    on_combo_PinHZ_checkChanged(-1);

    QMetaObject::invokeMethod(ui->button_PinHZ, "clicked", Qt::QueuedConnection);
}

void Widget::on_CRCConfDone(int8_t validCode)
{
    if (g_rowCntCheck == 0)
        return;
    QString str = "";
    if (validCode < 0)
    {
        switch (validCode)
        {
        case -1: str += "CRC poly"; break;
        case -2: str += "CRC init"; break;
        case -3: str += "CRC xorOut"; break;
        default: str += "CRC未知"; break;
        }
        str += "配置错误";
        emit showLog(LogLevel_ERR, str);
        return;
    }
    on_combo_PinHZ_checkChanged(-1);
}

void Widget::on_button_serialRefresh_clicked()
{
    serialRefresh();
    emit showLog(LogLevel_INF, "串口刷新完成");
}

void Widget::on_button_serialSwitch_clicked()
{
    QString curStr;
    curStr = ui->button_serialSwitch->text();

    if (curStr == "打开串口")
    {
        int32_t index;

        QSerialPortInfo portInfo(ui->combo_serialPort->currentText());
        m_serialPort->setPort(portInfo);

        index = ui->combo_serialBaud->currentText().toInt();
        m_serialPort->setBaudRate(index);

        index = ui->combo_serialDataBit->currentIndex();
        switch (index)
        {
        case 0: m_serialPort->setDataBits(QSerialPort::Data8); break;
        case 1: m_serialPort->setDataBits(QSerialPort::Data7); break;
        case 2: m_serialPort->setDataBits(QSerialPort::Data6); break;
        case 3: m_serialPort->setDataBits(QSerialPort::Data5); break;
        default: m_serialPort->setDataBits(QSerialPort::Data8); break;
        }

        index = ui->combo_serialStopBit->currentIndex();
        switch (index)
        {
        case 0: m_serialPort->setStopBits(QSerialPort::OneStop); break;
        case 1: m_serialPort->setStopBits(QSerialPort::OneAndHalfStop); break;
        case 2: m_serialPort->setStopBits(QSerialPort::TwoStop); break;
        default: m_serialPort->setStopBits(QSerialPort::OneStop); break;
        }

        index = ui->combo_serialParityBit->currentIndex();
        switch (index)
        {
        case 0: m_serialPort->setParity(QSerialPort::NoParity); break;
        case 1: m_serialPort->setParity(QSerialPort::OddParity); break;
        case 2: m_serialPort->setParity(QSerialPort::EvenParity); break;
        case 3: m_serialPort->setParity(QSerialPort::MarkParity); break;
        case 4: m_serialPort->setParity(QSerialPort::SpaceParity); break;
        default: m_serialPort->setParity(QSerialPort::NoParity); break;
        }

        m_serialPort->setReadBufferSize(10240);

        if (m_serialPort->open(QIODevice::ReadWrite))
        {
            m_serialPort->clear();
            m_serialPort->clearError();
            ui->button_serialSwitch->setText("关闭串口");
            ui->button_serialRefresh->setEnabled(false);
            ui->combo_serialPort->setEnabled(false);
            ui->combo_serialBaud->setEnabled(false);
            ui->combo_serialDataBit->setEnabled(false);
            ui->combo_serialStopBit->setEnabled(false);
            ui->combo_serialParityBit->setEnabled(false);
            emit showLog(LogLevel_INF, "打开了串口");
        }
        else
        {
            emit showLog(LogLevel_ERR, "打开串口失败, 请检查是否串口被占用或者打开的是com1");
        }
    }
    else
    {
        m_serialPort->close();
        ui->button_serialSwitch->setText("打开串口");
        ui->button_serialRefresh->setEnabled(true);
        ui->combo_serialPort->setEnabled(true);
        ui->combo_serialBaud->setEnabled(true);
        ui->combo_serialDataBit->setEnabled(true);
        ui->combo_serialStopBit->setEnabled(true);
        ui->combo_serialParityBit->setEnabled(true);
        emit showLog(LogLevel_INF, "关闭了串口");
    }
}

/* **************************************************
 * @brief LOG信息清除
 * **************************************************/
void Widget::on_button_clearLog_clicked()
{
    ui->txtBs_log->setText("");
}

void Widget::on_button_picSelect_clicked()
{
    QString srcPath, dstPath;
    // 注意, 直接使用QFileDialog是模态的, 会阻塞主线程
    srcPath = QFileDialog::getOpenFileName(this, tr("选择源文件"), "", "");
    dstPath = QFileDialog::getSaveFileName(this, tr("选择保存位置"), "dst.ico", ".ico");
    QImage img(srcPath);
    if (img.save(dstPath, "ICO"))
    {
        emit showLog(LogLevel_INF, "转换成功");
    }
    else
    {
        emit showLog(LogLevel_ERR, "转换失败");
    }
}

void Widget::on_button_addHead_clicked()
{
    int32_t curRow = ui->table_PinHZ->currentRow();
    int32_t zoneStart = ZONE_START_HEAD;
    int32_t zoneEnd = ZONE_END_HEAD;

    if (curRow >= zoneStart && curRow <= zoneEnd)
        curRow++;
    else
        curRow = zoneEnd + 1;

    ui->table_PinHZ->insertRow(curRow);                    // 插入一行，但不会自动为单元格创建item
    createItemsARow(curRow, "Head", 0, "00", "0", "Head"); // 为某一行创建items
    g_rowCntHead++;
}

void Widget::on_button_addItem_clicked()
{
    int32_t curRow = 0;
    int32_t addNum = ui->spin_addItem->value();
    int32_t zoneStart = ZONE_START_DATA;
    int32_t zoneEnd = ZONE_END_DATA;

    for (int32_t i = 0; i < addNum; i++)
    {
        curRow = ui->table_PinHZ->currentRow(); // 当前行号
        if (curRow >= zoneStart && curRow <= zoneEnd)
            curRow++;
        else
            curRow = zoneEnd + 1;

        ui->table_PinHZ->insertRow(curRow);                // 插入一行，但不会自动为单元格创建item
        createItemsARow(curRow, "Data", 0, "00", "0", ""); // 为某一行创建items
        g_rowCntData++;
        zoneEnd = ZONE_END_DATA;
        updateDataZoneBytes();
        on_combo_PinHZ_checkChanged(-1);
    }
}

void Widget::on_button_checkSet_clicked()
{
    int32_t checkRow = ZONE_END_DATA + 1;

    if (g_rowCntCheck == 1)
    {
        // 校验域已存在
        ui->table_PinHZ->removeRow(checkRow);
        g_rowCntCheck = 0;
        return;
    }

    // 阻断信号，避免触发itemChanged
    ui->table_PinHZ->blockSignals(true);

    ui->table_PinHZ->insertRow(checkRow); // 插入一行，但不会自动为单元格创建item
    // 为一行的单元格创建 Items
    QTableWidgetItem *item;

    // rowHead
    item = new QTableWidgetItem("Check");
    ui->table_PinHZ->setVerticalHeaderItem(checkRow, item);

    // hexType
    QComboBox *comboBox = new QComboBox();

    comboBox->addItem("None");
    comboBox->addItem("CheckSum-8");
    comboBox->addItem("CheckSum-16");
    comboBox->addItem("CheckSum-32");
    comboBox->addItem("CRC");
    connect(comboBox, \
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), \
            this, \
            &Widget::on_combo_PinHZ_checkChanged);
    comboBox->setCurrentIndex(0);
    ui->table_PinHZ->setCellWidget(checkRow, colDataType, comboBox);

    // dataHex
    item = new QTableWidgetItem("00", ctDataHex);
    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter); // 文本对齐格式
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    ui->table_PinHZ->setItem(checkRow, colDataHex, item); // 为单元格设置Item

    // dataDec
    item = new QTableWidgetItem("0", ctDataDec);
    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter); // 文本对齐格式
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    ui->table_PinHZ->setItem(checkRow, colDataDec, item); // 为单元格设置Item

    // Comment
    item = new QTableWidgetItem("校验", ctComment);
    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter); // 文本对齐格式
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    ui->table_PinHZ->setItem(checkRow, colComment, item); // 为单元格设置Item
    g_rowCntCheck = 1;

    // 解除阻断
    ui->table_PinHZ->blockSignals(false);

}

void Widget::on_button_addTail_clicked()
{
    int32_t curRow = ui->table_PinHZ->currentRow(); // 当前行号
    int32_t zoneStart = ZONE_START_TAIL;
    int32_t zoneEnd = ZONE_END_TAIL;

    if (curRow >= zoneStart && curRow <= zoneEnd)
        curRow++;
    else
        curRow = zoneEnd + 1;

    ui->table_PinHZ->insertRow(curRow);                    // 插入一行，但不会自动为单元格创建item
    createItemsARow(curRow, "Tail", 0, "00", "0", "Tail"); // 为某一行创建items
    g_rowCntTail++;
}

void Widget::on_button_copyCurRow_clicked()
{
    QSet<int32_t> selectedRows;
    for (const QModelIndex &index : ui->table_PinHZ->selectionModel()->selectedIndexes())
    {
        selectedRows.insert(index.row());
    }

    QList<int32_t> rows = selectedRows.values();

    std::sort(rows.begin(), rows.end());

    if (rows.last() - rows.first() != rows.count() - 1)
    {
        emit showLog(LogLevel_ERR, "不支持非连续域复制!");
        return;
    }

    if (checkRowZone(rows.first()) != checkRowZone(rows.last()))
    {
        emit showLog(LogLevel_ERR, "不支持跨域复制!");
        return;
    }

    if (checkRowZone(rows.first()) == 3)
    {
        emit showLog(LogLevel_ERR, "不支持校验域复制!");
        return;
    }

    QStringList colStrList;
    QTableWidgetItem *item = nullptr;
    QWidget *widget = nullptr;
    QComboBox *comboBox = nullptr;
    uint8_t curDataType = 0;
    int32_t pasteRow = rows.last() + 1;
    int8_t rowZone = 0;

    for (int32_t row : rows)
    {
        // 获取当前行参数
        item = ui->table_PinHZ->verticalHeaderItem(row); // Zone
        colStrList << item->text();
        widget = ui->table_PinHZ->cellWidget(row, colDataType); // dataType
        comboBox = qobject_cast<QComboBox *>(widget);
        colStrList << comboBox->currentText();
        item = ui->table_PinHZ->item(row, colDataHex); // DataHex
        colStrList << item->text();
        item = ui->table_PinHZ->item(row, colDataDec); // DataDec
        colStrList << item->text();
        item = ui->table_PinHZ->item(row, colComment); // Comment
        colStrList << item->text();

        if (colStrList[1] == "uint8") curDataType = 0;
        else if (colStrList[1] == "int8") curDataType = 1;
        else if (colStrList[1] == "uint16") curDataType = 2;
        else if (colStrList[1] == "int16") curDataType = 3;
        else if (colStrList[1] == "uint32") curDataType = 4;
        else if (colStrList[1] == "int32") curDataType = 5;
        else if (colStrList[1] == "uint64") curDataType = 6;
        else if (colStrList[1] == "int64") curDataType = 7;
        else if (colStrList[1] == "float") curDataType = 8;
        else if (colStrList[1] == "double") curDataType = 9;
        else { ; }

        ui->table_PinHZ->insertRow(pasteRow); // 插入一行，但不会自动为单元格创建item
        createItemsARow(pasteRow, colStrList[0], curDataType, colStrList[2], colStrList[3], colStrList[4]); // 为某一行创建items

        rowZone = checkRowZone(row);
        switch (rowZone)
        {
        case 1: g_rowCntHead++; break;
        case 2:
            g_rowCntData++;
            if (g_rowCntCheck == 1)
                on_combo_PinHZ_checkChanged(-1);
            break;
        case 4: g_rowCntTail++; break;
        default: break;
        }
        pasteRow++;
        colStrList.clear();
    }

    updateDataZoneBytes();
}

void Widget::on_button_subCurRow_clicked()
{
    if (ui->table_PinHZ->rowCount() <= 1)
    {
        return;
    }

    QSet<int32_t> selectedRows;
    for (const QModelIndex &index : ui->table_PinHZ->selectionModel()->selectedIndexes())
    {
        selectedRows.insert(index.row());
    }

    QList<int32_t> rows = selectedRows.values();

    std::sort(rows.begin(), rows.end(), std::greater<int32_t>());

    for (int32_t row : rows)
    {
        if (row >= 0 && row < ui->table_PinHZ->rowCount())
        {
            if (ui->table_PinHZ->rowCount() <= 1)
                break;
            ui->table_PinHZ->removeRow(row);
            int8_t rowZone = checkRowZone(row);
            switch (rowZone) {
            case 1: g_rowCntHead--; break;
            case 2: g_rowCntData--; break;
            case 3: g_rowCntCheck = 0; break;
            case 4: g_rowCntTail--; break;
            default: break;
            }
        }
    }

    // 删除后选中到下一行
    int32_t curRow = ui->table_PinHZ->currentRow(); // 当前行号
    int32_t nextRow = curRow, curCol = ui->table_PinHZ->currentColumn();
    if (nextRow >= ui->table_PinHZ->rowCount())
        nextRow = ui->table_PinHZ->rowCount() - 1;
    if (nextRow >= 0)
    {
        ui->table_PinHZ->selectRow(nextRow);
        ui->table_PinHZ->setCurrentCell(nextRow, curCol);
    }

    updateDataZoneBytes();
    if (g_rowCntCheck)
        on_combo_PinHZ_checkChanged(-1);
}

void Widget::on_button_fillCurRow_clicked()
{
    QSet<int32_t> selectedRows;

    for (const QModelIndex &index : ui->table_PinHZ->selectionModel()->selectedIndexes())
    {
        selectedRows.insert(index.row());
    }

    QList<int32_t> rows = selectedRows.values();

    std::sort(rows.begin(), rows.end());

    if (rows.last() - rows.first() != rows.count() - 1)
    {
        emit showLog(LogLevel_ERR, "不支持非连续域填充!");
        return;
    }

    if (checkRowZone(rows.first()) != checkRowZone(rows.last()))
    {
        emit showLog(LogLevel_ERR, "不支持跨域填充!");
        return;
    }

    if (checkRowZone(rows.first()) == 3)
    {
        emit showLog(LogLevel_ERR, "不支持校验域填充!");
        return;
    }

    g_fillStart = rows.first();
    g_fillRowCnt = rows.size();
    g_fillBytes = getRowsBytes(rows.first(), rows.last());
    m_fillItemDlg->show();
}

void Widget::on_button_PinHZ_clicked()
{
    QString str = "", PinHZStr = "";
    QTableWidgetItem *cellItem = nullptr;

    ui->plain_PinHZ->clear(); // 文本编辑器清空
    for (int32_t row = 0; row < ui->table_PinHZ->rowCount(); row++) // 逐行处理
    {
        if (g_rowCntCheck == 1 && m_checkType == 0 && row == ZONE_END_DATA + 1)
            continue; // 校验位为None不参与拼好帧
        cellItem = ui->table_PinHZ->item(row, colDataHex); // 获取单元格的item
        str = cellItem->text();                            // 字符串连接
        if (!m_isFieldPinHZ)
        {
            // 按字节拼好帧
            QString formatStr = "";
            for (int32_t i = 0; i < str.length(); i+=2)
            {
                if (i != 0)
                    formatStr += " ";
                formatStr += str.mid(i, 2);
            }
            str = formatStr;
        }
        if (row != 0)
            PinHZStr += " ";
        PinHZStr += str;
    }

    ui->plain_PinHZ->setPlainText(PinHZStr);
}

void Widget::on_button_PinHZSave_clicked()
{
    QString defaultFileName = "PinHZ_";
    QString filePath = "";

    filePath = QFileDialog::getSaveFileName(this, \
                                            tr("请选择或输入你要存储的文件名"), \
                                            defaultFileName, \
                                            tr("逗号分隔文件(*.csv)"));
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        emit showLog(LogLevel_WAR, "存储文件位置非法, 放弃保存!");
        ui->txtBs_log->append(FONT_COLOR_DARK_ORANGE + filePath);
        return; // 开启失败
    }

    QTextStream txt(&file);
    int32_t rowCnt = ui->table_PinHZ->rowCount();
    QTableWidgetItem *item = nullptr;
    QWidget *widget = nullptr;
    QComboBox *comboBox = nullptr;

    txt << "Zone,";
    txt << "Data Type,";
    txt << "Data Hex,";
    txt << "Data Dec,";
    txt << "Comment";
    txt << endl;

    for (int32_t row = 0; row < rowCnt; row++)
    {
        item = ui->table_PinHZ->verticalHeaderItem(row);
        txt << item->text();
        txt << ",";
        widget = ui->table_PinHZ->cellWidget(row, colDataType);
        comboBox = qobject_cast<QComboBox *>(widget);
        txt << comboBox->currentText();
        txt << ",";
        item = ui->table_PinHZ->item(row, colDataHex);
        txt << item->text();
        txt << ",";
        item = ui->table_PinHZ->item(row, colDataDec);
        txt << item->text();
        txt << ",";
        item = ui->table_PinHZ->item(row, colComment);
        txt << item->text();
        txt << endl;
    }
    file.close();

    filePath = FONT_COLOR_BLUE + filePath;
    emit showLog(LogLevel_INF, "frmLayout存储完成, 文件保存于:" + filePath);
}

void Widget::on_button_PinHZLoad_clicked()
{
    QString filePath = "";

    filePath = QFileDialog::getOpenFileName(this, tr("请选择或输入你要打开的文件名"),
                                            "", tr("逗号分隔文件(*.csv)"));
    if (filePath.isEmpty())
    {
        emit showLog(LogLevel_WAR, "未正确选择文件, 放弃处理");
        return;
    }
    // need real direct
    on_showLog(LogLevel_INF, "开始读取模板<br>" FONT_COLOR_DARK_ORANGE + filePath + FONT_COLOR_BLACK "<br>请稍后...");
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        emit showLog(LogLevel_ERR, "模板读取失败, 文件开启失败");
        return;
    }

    QTextStream txt(&file);
    QString rowStr = "";
    QStringList colStrList;
    int32_t colNum;
    uint8_t curDataType;

    if (txt.atEnd())
    {
        file.close();
        emit showLog(LogLevel_ERR, "模板读取失败, 没有从第一行获取到正确的信息");
        return;
    }

    rowStr = txt.readLine();
    if (rowStr != "Zone,Data Type,Data Hex,Data Dec,Comment")
    {
        file.close();
        emit showLog(LogLevel_ERR, "模板读取失败, 没有从第一行获取到正确的信息");
        return;
    }

    ui->table_PinHZ->setRowCount(0);
    ui->table_PinHZ->clearContents(); // 只清除工作区，不清除表头

    g_rowCntHead = 0;
    g_rowCntData = 0;
    g_rowCntCheck = 0;
    g_rowCntTail = 0;
    ui->button_checkSet->setEnabled(true);
    for (int32_t row = 0;; row++)
    {
        rowStr = txt.readLine();
        if (rowStr == "")
            break;
        colStrList = rowStr.split(",");
        colNum = colStrList.size();
        if (colNum != 5)
        {
            file.close();
            emit showLog(LogLevel_ERR, QString::asprintf("模板读取失败, %d行错误:", row) + rowStr);
            return;
        }
        if (colStrList[0] == "Check")
        {
            if (colStrList[1] == "None") curDataType = 0;
            else if (colStrList[1] == "CheckSum-8") curDataType = 1;
            else if (colStrList[1] == "CheckSum-16") curDataType = 2;
            else if (colStrList[1] == "CheckSum-32") curDataType = 3;
            else if (colStrList[1] == "CRC") curDataType = 4;
            else
            {
                file.close();
                emit showLog(LogLevel_ERR, QString::asprintf("模板读取失败, %d行数据类型错误:", row) + rowStr);
                return;
            }

            on_button_checkSet_clicked();

            QComboBox *comboBox = nullptr;
            QWidget *widget = nullptr;
            widget = ui->table_PinHZ->cellWidget(row, colDataType);
            comboBox = qobject_cast<QComboBox *>(widget);
            comboBox->setCurrentIndex(curDataType);
        }
        else
        {
            if (colStrList[0] == "Head") g_rowCntHead++;
            else if (colStrList[0] == "Data") g_rowCntData++;
            else if (colStrList[0] == "Tail") g_rowCntTail++;
            else
            {
                file.close();
                emit showLog(LogLevel_ERR, QString::asprintf("模板读取失败, %d行头错误:", row) + rowStr);
            }

            if (colStrList[1] == "uint8") curDataType = 0;
            else if (colStrList[1] == "int8") curDataType = 1;
            else if (colStrList[1] == "uint16") curDataType = 2;
            else if (colStrList[1] == "int16") curDataType = 3;
            else if (colStrList[1] == "uint32") curDataType = 4;
            else if (colStrList[1] == "int32") curDataType = 5;
            else if (colStrList[1] == "uint64") curDataType = 6;
            else if (colStrList[1] == "int64") curDataType = 7;
            else if (colStrList[1] == "float") curDataType = 8;
            else if (colStrList[1] == "double") curDataType = 9;
            else
            {
                file.close();
                emit showLog(LogLevel_ERR, QString::asprintf("模板读取失败, %d行数据类型错误:", row) + rowStr);
                return;
            }
            colStrList[2] = m_hex2dec.StrFix(colStrList[2], curDataType, true, false);
            colStrList[3] = m_hex2dec.StrFix(colStrList[3], curDataType, false, false);

            ui->table_PinHZ->insertRow(row); // 插入一行，但不会自动为单元格创建item
            createItemsARow(row, colStrList[0], curDataType, colStrList[2], colStrList[3], colStrList[4]); // 为某一行创建items
        }
    }
    file.close();
    updateDataZoneBytes();
    on_combo_PinHZ_checkChanged(-1);
    if (g_isAutoPinHZMode)
        QMetaObject::invokeMethod(ui->button_PinHZ, "clicked", Qt::QueuedConnection);
    emit showLog(LogLevel_INF, "模板读取成功");
}

void Widget::on_button_PinHZSend_clicked()
{
    QString str = ui->plain_PinHZ->toPlainText();
    uint8_t txBuf[PINHZ_CACHE_LEN] = {0, };
    int32_t txLen = 0;

    txLen = PinHZDeal(str, txBuf);

    if (ui->button_PinHZSend->text() == "串口发送")
        serialSend(txBuf, txLen);
    else
        netSend(txBuf, txLen);
}

void Widget::on_button_PinHZReverse_clicked()
{
    QString str = ui->plain_PinHZ->toPlainText();
    uint8_t buf[PINHZ_CACHE_LEN] = {0, }, dataLen = 0;
    int32_t bufLen = 0, bufIndex = 0;
    int32_t rowCnt = ui->table_PinHZ->rowCount(), dataType = 0;
    QTableWidgetItem *item = nullptr;
    QWidget *widget = nullptr;
    QComboBox *comboBox = nullptr;

    bufLen = PinHZDeal(str, buf);

    // 阻断信号，避免触发itemChanged
    ui->table_PinHZ->blockSignals(true);

    for (int32_t row = 0; row < rowCnt; row++)
    {
        widget = ui->table_PinHZ->cellWidget(row, colDataType);
        comboBox = qobject_cast<QComboBox *>(widget);
        dataType = comboBox->currentIndex();

        switch (dataType)
        {
        case 0: /* uint8 */ /* fall-through */
        case 1: /* sint8 */ dataLen = 1; break;
        case 2: /* uint16 */ /* fall-through */
        case 3: /* sint16 */ dataLen = 2; break;
        case 4: /* uint32 */ /* fall-through */
        case 5: /* sint32 */ /* fall-through */
        case 8: /* float  */ dataLen = 4; break;
        case 6: /* uint64 */ /* fall-through */
        case 7: /* sint64 */ /* fall-through */
        case 9: /* double */ dataLen = 8; break;
        default: break;
        }
        if (row == ZONE_END_DATA + 1)
        {
            // check
            switch (dataType)
            {
            case 0: dataLen = 0; break; // None
            case 1: dataLen = 1; dataType = 0; break; // checkSum8
            case 2: dataLen = 2; dataType = 1; break; // checkSum16
            case 3: dataLen = 4; dataType = 2; break; // checkSum32
            default: dataLen = 0; dataType = 0; break;
            }
        }
        item = ui->table_PinHZ->item(row, colDataHex);
        str = "";
        for (int32_t i = 0; i < dataLen; i++)
        {
            str += QString::asprintf("%02X", buf[bufIndex++]);
        }
        str = m_hex2dec.StrFix(str, dataType, true, false);
        item->setText(str);
        str = m_hex2dec.Hex2DecString(str, dataType, g_isLittleEndian);
        item = ui->table_PinHZ->item(row, colDataDec);
        item->setText(str);
    }
    ui->table_PinHZ->blockSignals(false);
    if (bufIndex != bufLen)
    {
        str = QString::asprintf("模板不匹配, 模板:%dBytes, 数据:%dBytes", bufIndex, bufLen);
        emit showLog(LogLevel_WAR, str);
        return;
    }
}

void Widget::on_button_dataLog_clicked()
{
    if (g_dataLog_logMode < 2)
    {
        // inside
        if (ui->tabWidget->currentIndex() == 1)
            ui->tabWidget->setCurrentIndex(0);
        ui->tabWidget->removeTab(1);
        QPoint mainPos = this->pos();
        if (g_dataLog_logMode == 0)
        {
            // turn to serial outside
            m_dataLogDlgNet->setParent(this);
            m_dataLogDlgSerial->setWindowFlags(Qt::Window | m_dataLogDlgSerial->windowFlags());
            m_dataLogDlgSerial->move(mainPos + QPoint(210, 70));
            m_dataLogDlgSerial->show();
            g_dataLog_logMode = 2;
        }
        else
        {
            // turn to net outside
            m_dataLogDlgNet->setParent(this);
            m_dataLogDlgNet->setWindowFlags(Qt::Window | m_dataLogDlgNet->windowFlags());
            m_dataLogDlgNet->move(mainPos + QPoint(210, 70));
            m_dataLogDlgNet->show();
            g_dataLog_logMode = 3;
        }
        ui->button_dataLog->setText("数据日志->内嵌");
    }
    else
    {
        // outside
        if (g_dataLog_logMode == 2)
        {
            // turn to serial inside
            m_dataLogDlgSerial->setWindowFlags(Qt::Widget);
            m_dataLogDlgSerial->setParent(ui->tabWidget);
            ui->tabWidget->insertTab(1, m_dataLogDlgSerial, "串口数据日志");
            g_dataLog_logMode = 0;
        }
        else
        {
            // turn to net inside
            m_dataLogDlgNet->setWindowFlags(Qt::Widget);
            m_dataLogDlgNet->setParent(ui->tabWidget);
            ui->tabWidget->insertTab(1, m_dataLogDlgNet, "网口数据日志");
            g_dataLog_logMode = 1;
        }
        ui->button_dataLog->setText("数据日志->外挂");
    }
}

void Widget::on_combo_PinHZ_dataTypeChanged(int index)
{
    QComboBox *comboBox = dynamic_cast<QComboBox *>(this->sender());
    int32_t row = 0;
    for (row = 0; row < ui->table_PinHZ->rowCount(); row++)
    {
        // 遍历找到对应的行
        if (ui->table_PinHZ->cellWidget(row, colDataType) == comboBox)
            break;
    }
    if (row == ui->table_PinHZ->rowCount())
        return;

    // 阻断信号，避免触发itemChanged
    ui->table_PinHZ->blockSignals(true);

    QTableWidgetItem *item = ui->table_PinHZ->item(row, colDataHex);
    QString str = item->text();
    if (str.isEmpty())
        return;
    if (g_isLittleEndian)
        m_hex2dec.HexStrTurnOrder(str);
    QString fixStr = m_hex2dec.StrFix(str, index, true, g_isLittleEndian);
    item->setText(fixStr);
    str = m_hex2dec.Hex2DecString(fixStr,
                                  comboBox->currentIndex(),
                                  g_isLittleEndian);
    item = ui->table_PinHZ->item(row, colDataDec);
    item->setText(str);
    ui->table_PinHZ->blockSignals(false);

    if (checkRowZone(row) == 2)
        updateDataZoneBytes();
    on_combo_PinHZ_checkChanged(-1);
}

void Widget::on_combo_PinHZ_checkChanged(int index)
{
    if (g_rowCntCheck == 0)
    {
        if (index != -1)
            emit showLog(LogLevel_DBG, QString::asprintf("校验字段不存在, 且触发了校验:%d", index));
        if (g_isAutoPinHZMode)
            QMetaObject::invokeMethod(ui->button_PinHZ, "clicked", Qt::QueuedConnection);
        return;
    }
    int32_t dataZoneStart = ZONE_START_DATA, dataZoneEnd = ZONE_END_DATA;
    int32_t dataType = 0;
    QTableWidgetItem *item = nullptr;
    uint8_t checkBuf[2048] = {0, };
    uint32_t checkCalc = 0;
    int32_t bufLen = 0;
    QString str = "", checkStr = "";
    QTableWidgetItem *cellItem = nullptr;
    bool isAppCalled = false;

    m_checkType = index;
    if (m_checkType == -1)
    {
        // app called
        if (g_rowCntCheck == 0)
            return;
        QWidget *widget = ui->table_PinHZ->cellWidget(dataZoneEnd + 1, colDataType);
        QComboBox *comboBox = qobject_cast<QComboBox *>(widget);

        m_checkType = comboBox->currentIndex();
        isAppCalled = true;
    }

    if (m_checkType != 0)
    {
        for (int32_t row = dataZoneStart; row <= dataZoneEnd; row++) // 获取数据域码流
        {
            cellItem = ui->table_PinHZ->item(row, colDataHex); // 获取单元格的item
            str = cellItem->text();
            if (str.length() % 2 != 0)
                str = "0" + str;
            for (int32_t i = 0; i < str.length(); i += 2)
            {
                bool isHex = false;
                int32_t byte = str.mid(i, 2).toInt(&isHex, 16);
                if (isHex)
                {
                    checkBuf[bufLen++] = byte;
                }
                else
                {
                    emit showLog(LogLevel_ERR, "帧数据错误");
                }
            }
        }

        if (m_checkType <= 3)
        {
            // CheckSum
            for (int32_t i = 0; i < bufLen; i++)
            {
                checkCalc += checkBuf[i];
            }

            checkStr = QString::number(checkCalc);
            dataType = m_checkType - 1;
            if (m_crcConfDlg->isVisible())
            {
                m_crcConfDlg->close();
            }
        }
        else if (m_checkType == 4)
        {
            // CRC
            if (isAppCalled)
            {
                checkCalc = m_crcConfDlg->m_crcCalc.calcCRC(m_crcConfDlg->m_crcConf, checkBuf, bufLen);
                checkStr = QString::number(checkCalc);
                dataType = m_crcConfDlg->m_dataType;
            }
            else
            {
                m_crcConfDlg->show();
                return;
            }
        }
        else
        {
            // Err
            checkStr = "00";
        }
    }
    else
    {
        // None
        checkStr = "00";
    }

    // 阻断信号，避免触发itemChanged
    ui->table_PinHZ->blockSignals(true);

    // 校验行的Hex item
    str = m_hex2dec.Dec2HexString(checkStr,
                                  dataType,
                                  g_isLittleEndian);
    item = ui->table_PinHZ->item(dataZoneEnd + 1, colDataHex);
    item->setText(str);
    // 校验行的Dec item
    str = m_hex2dec.Hex2DecString(str,
                                  dataType,
                                  g_isLittleEndian);
    item = ui->table_PinHZ->item(dataZoneEnd + 1, colDataDec);
    item->setText(str);

    ui->table_PinHZ->blockSignals(false);

    if (g_isAutoPinHZMode)
        QMetaObject::invokeMethod(ui->button_PinHZ, "clicked", Qt::QueuedConnection);
}

void Widget::on_combo_hexOrder_currentIndexChanged(int index)
{
    int32_t loopTimes = ui->table_PinHZ->rowCount();
    int32_t byteStrLen = 0;
    QTableWidgetItem *item = nullptr;
    QWidget *widget = nullptr;
    QComboBox *comboBox = nullptr;
    QString str = "", fixStr = "";

    g_isLittleEndian = index == 0 ? true : false;

    // 阻断信号，避免触发itemChanged
    ui->table_PinHZ->blockSignals(true);
    for (int32_t row = 0; row < loopTimes; row++)
    {
        widget = ui->table_PinHZ->cellWidget(row, colDataType);
        comboBox = qobject_cast<QComboBox *>(widget);
        item = ui->table_PinHZ->item(row, colDataHex);

        byteStrLen = comboBox->currentIndex();
        switch (byteStrLen)
        {
        case 0: /* uint8  */ /* fall-through */
        case 1: /* int8   */ byteStrLen = 1 * 2; break;
        case 2: /* uint16 */ /* fall-through */
        case 3: /* int16  */ byteStrLen = 2 * 2; break;
        case 4: /* uint32 */ /* fall-through */
        case 5: /* int32  */ /* fall-through */
        case 8: /* float  */ byteStrLen = 4 * 2; break;
        case 6: /* uint64 */ /* fall-through */
        case 7: /* int64  */ /* fall-through */
        case 9: /* double */ byteStrLen = 8 * 2; break;
        default: break;
        }

        str = item->text();
        for (int32_t i = byteStrLen; i >= 0; i -= 2)
        {
            fixStr += str.mid(i, 2);
        }
        item->setText(fixStr);
        fixStr = "";
    }
    ui->table_PinHZ->blockSignals(false);

    on_combo_PinHZ_checkChanged(-1);
}

void Widget::on_table_PinHZ_itemChanged(QTableWidgetItem *item)
{
    int32_t cellCol = item->column(); // 获取单元格的类型
    int32_t row = item->row();
    QWidget *widget = ui->table_PinHZ->cellWidget(row, colDataType);
    QComboBox *comboBox = qobject_cast<QComboBox *>(widget);
    QString str = item->text();

    if (str.isEmpty())
        str = "0";
    QString fixStr = "";

    // 阻断信号，避免触发itemChanged
    ui->table_PinHZ->blockSignals(true);
    switch (cellCol)
    {
    case colDataHex:
    {
        // 大部分人输入和阅读习惯是大端模式，
        // 比如输入0001(u16)等字符串的时候，默认就是大端的1，而不转为小端的0100
        // 目前有个问题是，由于是默认先处理成大端顺序了的，所以可能会出现以下情况
        // 比如用户在小端和uint16的情况下，输入EB90，得到了90EB，然后发现实际上就想要EB90，
        // 所以又输入了90EB，同时使用的是大写字母输入，那这个item就和输入前一模一样，也就无法触发当前槽函数
        // 所以先修改成别的内容，再输入90EB，或者用小写输入90eb，都可以进行正常流程
        // 原生tabwidget不支持editdone，目前最合理的修改的逻辑是重写tableWidget类，手动控制事件
        fixStr = m_hex2dec.StrFix(str, comboBox->currentIndex(), true, false);
        fixStr = m_hex2dec.StrFix(fixStr, comboBox->currentIndex(), true, g_isLittleEndian);
        item->setText(fixStr);
        str = m_hex2dec.Hex2DecString(fixStr,
                                        comboBox->currentIndex(),
                                        g_isLittleEndian);
        item = ui->table_PinHZ->item(row, colDataDec);
        item->setText(str);
        break;
    }
    case colDataDec:
    {
        fixStr = m_hex2dec.StrFix(str,
                                    comboBox->currentIndex(), false,
                                    g_isLittleEndian);
        item->setText(fixStr);
        str = m_hex2dec.Dec2HexString(fixStr,
                                        comboBox->currentIndex(),
                                        g_isLittleEndian);
        item = ui->table_PinHZ->item(row, colDataHex);
        item->setText(str);
    }
    default:
        break;
    }
    ui->table_PinHZ->blockSignals(false);

    on_combo_PinHZ_checkChanged(-1);
}

void Widget::on_check_autoPinHZ_stateChanged(int state)
{
    if (state == Qt::Checked)
    {
        g_isAutoPinHZMode = true;
        on_combo_PinHZ_checkChanged(-1);
    }
    else
    {
        g_isAutoPinHZMode = false;
    }
}

void Widget::on_spin_replyTime_valueChanged(int arg1)
{
    m_autoReplyDelay = arg1;
}

void Widget::on_check_fieldPinHZ_toggled(bool checked)
{
    m_isFieldPinHZ = checked;
    QMetaObject::invokeMethod(ui->button_PinHZ, "clicked", Qt::QueuedConnection);
}

void Widget::on_spin_sendPeriod_valueChanged(int arg1)
{
    m_autoSendPeriod = arg1 / 10;
}

void Widget::on_combo_portMode_currentIndexChanged(int index)
{
    if (index == 0)
    {
        // 串口模式
        ui->groupBox_serialPort->show();
        ui->groupBox_net->hide();
        ui->button_PinHZSend->setText("串口发送");
    }
    else
    {
        // 网口模式
        ui->groupBox_serialPort->hide();
        ui->groupBox_net->show();
        ui->button_PinHZSend->setText("网口发送");
    }

    if (g_dataLog_logMode < 2)
    {
        // inside
        int32_t currIndex = ui->tabWidget->currentIndex();
        ui->tabWidget->removeTab(1);
        if (index == 0)
        {
            // turn to serial inside
            m_dataLogDlgSerial->setWindowFlags(Qt::Widget);
            ui->tabWidget->insertTab(1, m_dataLogDlgSerial, "串口数据日志");
            g_dataLog_logMode = 0;
        }
        else
        {
            // turn to net inside
            m_dataLogDlgNet->setWindowFlags(Qt::Widget);
            ui->tabWidget->insertTab(1, m_dataLogDlgNet, "网口数据日志");
            g_dataLog_logMode = 1;
        }
        ui->tabWidget->setCurrentIndex(currIndex);
    }
    else
    {
        // outside
        if (index == 0)
        {
            // turn to serial outside
            m_dataLogDlgNet->close();
            m_dataLogDlgNet->setParent(this);
            m_dataLogDlgSerial->setWindowFlags(Qt::Window | m_dataLogDlgSerial->windowFlags());
            QPoint mainPos = this->pos();
            m_dataLogDlgSerial->move(mainPos + QPoint(210, 70));
            m_dataLogDlgSerial->show();
            g_dataLog_logMode = 2;
        }
        else
        {
            // turn to net outside
            m_dataLogDlgSerial->close();
            m_dataLogDlgNet->setParent(this);
            m_dataLogDlgNet->setWindowFlags(Qt::Window | m_dataLogDlgNet->windowFlags());
            QPoint mainPos = this->pos();
            m_dataLogDlgNet->move(mainPos + QPoint(210, 70));
            m_dataLogDlgNet->show();
            g_dataLog_logMode = 3;
        }
    }
}

QString Widget::getLocalIP()
{
    QString hostName = QHostInfo::localHostName(); // 本地主机名
    QHostInfo hostInfo = QHostInfo::fromName(hostName);
    QString localIP = "";

    QList<QHostAddress> addList = hostInfo.addresses(); //

    if (!addList.isEmpty())
        for (int i = 0; i < addList.count(); i++)
        {
            QHostAddress aHost = addList.at(i);
            if (QAbstractSocket::IPv4Protocol == aHost.protocol())
            {
                localIP = aHost.toString();
                break;
            }
        }
    return localIP;
}

void Widget::on_button_netSwitch_clicked()
{
    m_netUnit.netType = ui->combo_netType->currentIndex();

    if (m_netUnit.netState == 0)
    {
        // try to connect
        bool isUint = false;
        QString addrStrLocal = ui->combo_netAddrLocal->currentText();
        QString portStrLocal = ui->lineEdit_netPortLocal->text();
        if (portStrLocal == "Auto")
            portStrLocal = "0";
        QHostAddress addrLocal(addrStrLocal);
        uint16_t portLocal = portStrLocal.toUInt(&isUint);

        if (isUint == false)
        {
            emit showLog(LogLevel_ERR, "Local port invalid");
            return;
        }

        if (addrLocal.protocol() != QAbstractSocket::IPv4Protocol)
        {
            // ipv4 invalid
            emit showLog(LogLevel_ERR, "Local addr invalid");
            return;
        }

        switch (m_netUnit.netType)
        {
        case NetType_UDP:
            if (m_udpSocket->state() == QUdpSocket::BoundState)
            {
                // if has bound, abort
                netStop();
            }
            if (m_udpSocket->bind(addrLocal, portLocal))
            {
                if (portLocal == 0)
                {
                    portLocal = m_udpSocket->localPort();
                    portStrLocal = QString(portLocal);
                }
                m_netUnit.addrLocal[NetType_UDP] = addrStrLocal;
                m_netUnit.portLocal[NetType_UDP] = portStrLocal;
                on_netConnected();
            }
            else
                emit showLog(LogLevel_INF, "Udp Port bind failed!");
            break;
        case NetType_TCPC:
        {
            QString addrStrTarget = ui->combo_netAddrTarget->currentText();
            QString portStrTarget = ui->lineEdit_netPortTarget->text();
            QHostAddress addrTarget(addrStrTarget);
            uint16_t portTarget = portStrTarget.toUInt(&isUint);

            if (isUint == false)
            {
                emit showLog(LogLevel_ERR, "Target port invalid");
                return;
            }

            if (addrTarget.protocol() != QAbstractSocket::IPv4Protocol)
            {
                // ipv4 invalid
                emit showLog(LogLevel_ERR, "Target addr invalid");
                return;
            }

            if (m_tcpClient->state() != QTcpSocket::UnconnectedState)
            {
                // 如果当前socket已连接，则先断开
                m_tcpClient->disconnectFromHost();
                if (!m_tcpClient->waitForDisconnected(1000))
                    netStop();
            }

            if (portLocal != 0)
            {
                // 如果需要手动配置本地端口
                if (!m_tcpClient->bind(addrLocal, portLocal))
                {
                    // 本地端口绑定失败
                    emit showLog(
                        LogLevel_ERR,
                        QString("本地端口手动绑定失败<br>IP:"
                                + addrStrLocal + ":" + portStrLocal
                                + "<br>原因:" + m_tcpClient->errorString()));
                    return;
                }
            }

            m_tcpClient->connectToHost(addrTarget, portTarget);
            break;
        }
        case NetType_TCPS:
            if (m_tcpServer->isListening())
            {
                // if port has listen, stop
                netStop();
                ui->button_netSwitch->setText("开始监听");
            }
            else
            {
                if (m_tcpServer->listen(addrLocal, portLocal))
                {
                    m_netUnit.addrLocal[NetType_TCPS] = addrStrLocal;
                    m_netUnit.portLocal[NetType_TCPS] = portStrLocal;
                    emit showLog(LogLevel_INF, "启动监听成功");
                    ui->button_netSwitch->setText("停止监听");
                    ui->combo_netType->setEnabled(false);
                    ui->combo_netAddrLocal->setEnabled(false);
                    ui->lineEdit_netPortLocal->setEnabled(false);
                    m_netUnit.netState = 1;
                }
                else
                    emit showLog(LogLevel_INF, "启动监听失败, 原因:" +
                                                   m_tcpServer->errorString());
            }
            break;
        default: break;
        }
    }
    else
    {
        // try to disconnect
        netStop();
        emit showLog(LogLevel_INF, "网络已断开");
    }
}

void Widget::netStop()
{
    switch (m_netUnit.netType)
    {
    case NetType_UDP:
        if (m_udpSocket->state() == QUdpSocket::BoundState)
        {
            // if has bound, try to unbind
            m_udpSocket->abort();
            // check again
            if (m_udpSocket->state() != QUdpSocket::BoundState)
            {
                emit showLog(LogLevel_INF, "端口解绑成功!");
            }
            else
            {
                emit showLog(LogLevel_ERR, "端口解绑异常!!!");
            }
        }
        else
        {
            emit showLog(LogLevel_WAR, "端口未绑定!");
        }
        ui->button_netSwitch->setText("绑定端口");
        emit showLog(LogLevel_INF, "Udp bind was stop");
        break;
    case NetType_TCPC:
        if (m_tcpClient->state() != QTcpSocket::UnconnectedState)
        {
            // if socket was connected, try to disconnect
            m_tcpClient->disconnectFromHost();
            if (!m_tcpClient->waitForDisconnected(1000))
                m_tcpClient->abort();
        }
        ui->button_netSwitch->setText("开始连接");
        emit showLog(LogLevel_INF, "Tcp Client connection was stop");
        break;
    case NetType_TCPS:
        if (m_tcpServer->isListening())
        {
            ui->combo_netClient->setCurrentIndex(0);
            on_button_netClientClose_clicked();
            m_tcpServer->close();
            emit showLog(LogLevel_INF, "Tcp Server was stop");
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
    m_netUnit.netState = 0;
}

void Widget::netRefresh()
{
    // refresh ip address
    QString str = QHostInfo::localHostName();
    QHostInfo hostInfo = QHostInfo::fromName(str);
    QList<QHostAddress> addrList = hostInfo.addresses();
    foreach (QHostAddress hostAddr, addrList)
    {
        if (QAbstractSocket::IPv4Protocol == hostAddr.protocol())
        {
            ui->combo_netAddrLocal->addItem(hostAddr.toString());
            ui->combo_netAddrTarget->addItem(hostAddr.toString());
        }
    }
    ui->combo_netAddrLocal->addItem("127.0.0.1");
    ui->combo_netAddrTarget->addItem("127.0.0.1");
    ui->combo_netAddrLocal->addItem("0.0.0.0");
    ui->combo_netAddrTarget->addItem("0.0.0.0");
}

void Widget::on_netConnected()
{
    QString succMsg = "";
    switch (m_netUnit.netType)
    {
    case NetType_UDP:
        ui->button_netSwitch->setText("取消绑定");
        succMsg = QString("端口绑定成功!")
                + "<br>绑定端口端:" + m_netUnit.addrLocal[NetType_UDP] + ":" + m_netUnit.portLocal[NetType_UDP];
        break;
    case NetType_TCPC:
        ui->button_netSwitch->setText("断开连接");
        ui->combo_netAddrTarget->setEnabled(false);
        ui->lineEdit_netPortTarget->setEnabled(false);

        m_netUnit.addrLocal[NetType_TCPC] = m_tcpClient->localAddress().toString();
        m_netUnit.portLocal[NetType_TCPC] = QString::number(m_tcpClient->localPort());
        m_netUnit.addrTarget[NetType_TCPC] = m_tcpClient->peerAddress().toString();
        m_netUnit.portTarget[NetType_TCPC] = QString::number(m_tcpClient->peerPort());

        succMsg = QString("成功连接到服务端!")
                + "<br>客户端:" + m_netUnit.addrLocal[NetType_TCPC] + ":" + m_netUnit.portLocal[NetType_TCPC]
                + "<br>服务端:" + m_netUnit.addrTarget[NetType_TCPC] + ":" + m_netUnit.portTarget[NetType_TCPC];
        break;
    default:
        break;
    }

    ui->combo_netType->setEnabled(false);
    ui->combo_netAddrLocal->setEnabled(false);
    ui->lineEdit_netPortLocal->setEnabled(false);
    m_netUnit.netState = 1;

    emit showLog(LogLevel_COlORFUL, FONT_COLOR_DARK_ORANGE + succMsg);
}

void Widget::on_netDisconnected()
{
    switch (m_netUnit.netType)
    {
    case NetType_TCPC:
        netStop();
        emit showLog(LogLevel_INF, "Net has disconnected");
        break;
    case NetType_TCPS:
    {
        int32_t connIndex = -1;
        QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
        QString socketStr = socket->peerAddress().toString() + ":" + QString::number(socket->peerPort());
        for (int32_t i = 0; i < m_netUnit.tcpClientList.size(); i++)
        {
            if (socket == m_netUnit.tcpClientList.at(i))
            {
                connIndex = i;
                break;
            }
        }
        if (connIndex >= 0)
        {
            m_netUnit.tcpClientList.removeAt(connIndex);
            ui->combo_netClient->removeItem(connIndex);
            ui->combo_netClient->setItemText(0, QString::asprintf("All Client(%d)",
                                                m_netUnit.tcpClientList.size()));
            emit showLog(LogLevel_INF, "Client offline:<br>" + socketStr);
        }
        else
        {
            emit showLog(LogLevel_ERR, "Unexpect client offline:<br>" + socketStr);
        }
        socket->deleteLater();
        break;
    }
    default:
        break;
    }
}

void Widget::on_netReadyRead()
{
    QByteArray data;
    QString dataInfo = "# Recv ";
    switch (m_netUnit.netType)
    {
    case NetType_UDP:
        while(m_udpSocket->hasPendingDatagrams())
        {
            int64_t size = m_udpSocket->pendingDatagramSize();
            data = QByteArray(size, 0);
            QHostAddress addr;
            uint16_t port = 0;
            int64_t recvLen = m_udpSocket->readDatagram(
                        data.data(), data.size(),
                        &addr, &port);
            if (recvLen >= 0)
            {
                dataInfo += "from " + addr.toString() + ':' + QString::number(port) + " ";
            }
            else
            {
                // recv failed
                emit showLog(LogLevel_ERR, QString::asprintf("Recv failed, errCode:%d, reason:%s", \
                                                             m_udpSocket->error(), \
                                                             m_udpSocket->errorString().toUtf8().data()));
                return;
            }
        }
        break;
    case NetType_TCPC:
        data = m_tcpClient->readAll(); // Read all data in QByteArray
        break;
    case NetType_TCPS:
    {
        QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());

        if (!socket || !m_netUnit.tcpClientList.contains(socket)) return;

        data = socket->readAll(); // Read all data in QByteArray
        dataInfo += "from " + socket->peerAddress().toString() + ':' +
                    QString::number(socket->peerPort()) + " ";
        break;
    }
    default:
        break;
    }

    if (!data.isEmpty())
    {
        uint8_t *buf = (uint8_t *)(data.data());
        int32_t recvLen = data.size();

        m_dataLogDlgNet->m_recvByte += recvLen;
        m_dataLogDlgNet->m_recvFrm++;
        emit updateDataCntNet();

        emit dataShowNet(buf, recvLen, false, dataInfo);

        if (ui->check_autoReply->isChecked())
        {
            m_autoReplyTimes++;
        }
    }
}

void Widget::on_netStateChanged(QAbstractSocket::SocketState socketState)
{
    QString msg = "";
    switch (socketState)
    {
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

    emit showLog(LogLevel_DBG, msg);
}

void Widget::on_netSocketErr(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    emit showLog(LogLevel_ERR, "Socket err: " + m_tcpClient->errorString());
}

void Widget::on_netNewConnection()
{
    while (m_tcpServer->hasPendingConnections())
    {
        QTcpSocket *socket = m_tcpServer->nextPendingConnection();
        if (!socket)
            continue;

        QString addr = socket->peerAddress().toString() + ":" + QString::number(socket->peerPort());

        connect(socket, &QTcpSocket::disconnected, this, &Widget::on_netDisconnected);
        connect(socket, &QTcpSocket::readyRead, this, &Widget::on_netReadyRead);
        // connect(socket, &QTcpSocket::error, this, &Widget::on_netSocketErr);
        m_netUnit.tcpClientList.append(socket);
        ui->combo_netClient->setItemText(0, QString::asprintf("All Client(%d)",
                                            m_netUnit.tcpClientList.size()));
        ui->combo_netClient->addItem(addr);
        emit showLog(LogLevel_COlORFUL, FONT_COLOR_DARK_ORANGE "New client online: " + addr);
    }
}

void Widget::netSend(uint8_t *buf, int32_t len)
{
    int32_t sendLen = 0;
    QString targetInfo = "";
    QString addrStrTarget = ui->combo_netAddrTarget->currentText();
    QString portStrTarget = ui->lineEdit_netPortTarget->text();
    uint16_t portTarget = 0;

    switch (m_netUnit.netType)
    {
    case NetType_UDP:
    {
        if (m_udpSocket->state() != QUdpSocket::BoundState)
        {
            emit showLog(LogLevel_ERR, "Udp has not bound local port!");
            return;
        }
        bool isUint = false;
        addrStrTarget = ui->combo_netAddrTarget->currentText();
        portStrTarget = ui->lineEdit_netPortTarget->text();
        QHostAddress addrTarget(addrStrTarget);
        portTarget = portStrTarget.toUInt(&isUint);
        if (isUint == false)
        {
            emit showLog(LogLevel_ERR, "Udp Target Port Err!");
            return;
        }
        if (addrTarget.protocol() != QAbstractSocket::IPv4Protocol)
        {
            // ipv4 invalid
            emit showLog(LogLevel_ERR, "Udp Target Addr Err");
            return;
        }

        m_netUnit.addrTarget[NetType_UDP] = addrStrTarget;
        m_netUnit.portTarget[NetType_UDP] = portStrTarget;
        targetInfo = addrStrTarget + ":" + portStrTarget + " ";

        m_netMutex.lock();
        sendLen = m_udpSocket->writeDatagram((char *)buf, len, addrTarget, portTarget);
        m_netMutex.unlock();
        break;
    }
    case NetType_TCPC:
        if (m_tcpClient->state() != QTcpSocket::ConnectedState)
        {
            emit showLog(LogLevel_WAR, "Socket is unconnected, send failed");
            return;
        }
        addrStrTarget = m_tcpClient->peerAddress().toString();
        portStrTarget = QString::number(m_tcpClient->peerPort());
        targetInfo = addrStrTarget + ":" + portStrTarget + " ";
        m_netMutex.lock();
        sendLen = m_tcpClient->write((char *)buf, len);
        m_netMutex.unlock();
        break;
    case NetType_TCPS:
    {
        bool isSend = false;
        if (ui->combo_netClient->count() < 2)
        {
            emit showLog(LogLevel_ERR, "No active connection!");
            return;
        }

        if (ui->combo_netClient->currentIndex() > 0)
        {
            // not send to all
            QStringList strList = ui->combo_netClient->currentText().split(':');
            addrStrTarget = strList.at(0);
            portStrTarget = strList.at(1);
            portTarget = portStrTarget.toUInt();
            targetInfo = addrStrTarget + ":" + portStrTarget + " ";
        }

        foreach (QTcpSocket *socket, m_netUnit.tcpClientList)
        {
            if ((ui->combo_netClient->currentIndex() == 0)
                    || (socket->peerAddress().toString() == addrStrTarget
                            && socket->peerPort() == portTarget))
            {
                addrStrTarget = socket->peerAddress().toString();
                portStrTarget = QString::number(socket->peerPort());
                targetInfo = addrStrTarget + ":" + portStrTarget + " ";
                if (socket->state() != QTcpSocket::ConnectedState)
                {
                    emit showLog(LogLevel_ERR, targetInfo + "socket is unconnected, send failed");
                    continue;
                }
                m_netMutex.lock();
                sendLen = socket->write((char *)buf, len);
                m_netMutex.unlock();
                isSend = true;

                if (sendLen >= 0)
                {
                    m_dataLogDlgNet->m_sendByte += sendLen;
                    m_dataLogDlgNet->m_sendFrm++;
                    emit updateDataCntNet();

                    if (m_dataLogDlgNet->m_sendMode != 0)
                        emit dataShowNet(buf, len, true, "# Send to " + targetInfo);
                }
                else
                    emit showLog(LogLevel_ERR, "发送失败!");
            }
        }
        if (!isSend)
        {
            emit showLog(LogLevel_ERR, "Target client "
                + targetInfo + "not found, send failed");
        }
        return;
    }
    default: return;
    }

    if (sendLen >= 0)
    {
        m_dataLogDlgNet->m_sendByte += sendLen;
        m_dataLogDlgNet->m_sendFrm++;
        emit updateDataCntNet();

        if (m_dataLogDlgNet->m_sendMode != 0)
            emit dataShowNet(buf, len, true, "# Send to " + targetInfo);
    }
    else
        emit showLog(LogLevel_ERR, "发送失败!");
}

void Widget::on_combo_netType_currentIndexChanged(int index)
{
    // save current config
    m_netUnit.addrLocal[m_netUnit.netType] = ui->combo_netAddrLocal->currentText();
    if (m_netUnit.netType < 2)
    {
        // UDP or TCP Client
        // save target info and is local port auto
        m_netUnit.addrTarget[m_netUnit.netType] = ui->combo_netAddrTarget->currentText();
        m_netUnit.portTarget[m_netUnit.netType] = ui->lineEdit_netPortTarget->text();
        m_netUnit.isPortLocalAuto[m_netUnit.netType] = ui->check_netPortLocal->isChecked();
        if (!ui->check_netPortLocal->isChecked())
            m_netUnit.portLocal[m_netUnit.netType] = ui->lineEdit_netPortLocal->text();
    }
    else
        m_netUnit.portLocal[m_netUnit.netType] = ui->lineEdit_netPortLocal->text();

    // turn to selected net type
    m_netUnit.netType = index;

    // load selected net type config
    int32_t addrIndex = 0;
    addrIndex = ui->combo_netAddrLocal->findText(m_netUnit.addrLocal[m_netUnit.netType]);
    addrIndex = addrIndex < 0 ? 0 : addrIndex;
    ui->combo_netAddrLocal->setCurrentIndex(addrIndex);
    ui->lineEdit_netPortLocal->setText(m_netUnit.portLocal[m_netUnit.netType]);

    // deal widget show
    if (m_netUnit.netType < 2)
    {
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

        addrIndex = ui->combo_netAddrTarget->findText(m_netUnit.addrTarget[m_netUnit.netType]);
        addrIndex = addrIndex < 0 ? 0 : addrIndex;
        ui->combo_netAddrTarget->setCurrentIndex(addrIndex);
        ui->lineEdit_netPortTarget->setText(m_netUnit.portTarget[m_netUnit.netType]);
        ui->check_netPortLocal->setChecked(m_netUnit.isPortLocalAuto[m_netUnit.netType]);
        if (ui->check_netPortLocal->isChecked())
            ui->lineEdit_netPortLocal->setText("Auto");
    }
    else
    {
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

    switch (m_netUnit.netType)
    {
    case NetType_UDP: ui->button_netSwitch->setText("绑定端口"); break;
    case NetType_TCPC: ui->button_netSwitch->setText("连接端口"); break;
    case NetType_TCPS: ui->button_netSwitch->setText("监听端口"); break;
    default: break;
    }
}

void Widget::on_check_netPortLocal_toggled(bool checked)
{
    if (checked)
    {
        // stored port
        m_netUnit.portLocal[m_netUnit.netType] = ui->lineEdit_netPortLocal->text();
        ui->lineEdit_netPortLocal->setText("Auto");
    }
    else
    {
        // restore port
        ui->lineEdit_netPortLocal->setText(m_netUnit.portLocal[m_netUnit.netType]);
    }

    ui->lineEdit_netPortLocal->setReadOnly(checked);
    m_netUnit.isPortLocalAuto[m_netUnit.netType] = checked;
}

void Widget::on_button_netRefresh_clicked()
{
    ui->combo_netAddrLocal->clear();
    ui->combo_netAddrTarget->clear();
    netRefresh();
    emit showLog(LogLevel_INF, "网口刷新完成");
}

void Widget::on_check_saveAsTemp_toggled(bool checked)
{
    if (checked)
    {

    }
    else
    {

    }
}

void Widget::on_button_netClientClose_clicked()
{
    int32_t connIndex = ui->combo_netClient->currentIndex();

    if (connIndex == 0)
    {
        // close all connection
        // delete list item on disconnected slot
        foreach (QTcpSocket *socket, m_netUnit.tcpClientList)
        {
            if (socket->state() != QTcpSocket::UnconnectedState)
            {
                // if socket was connected, try to disconnect
                socket->disconnectFromHost();
                if (!socket->waitForDisconnected(1000))
                    socket->abort();
            }
        }
    }
    else
    {
        // close select connection
        // delete list item on disconnected slot
        QTcpSocket *socket = m_netUnit.tcpClientList.at(connIndex - 1);
        if (socket->state() != QTcpSocket::UnconnectedState)
        {
            // if socket was connected, try to disconnect
            socket->disconnectFromHost();
            if (!socket->waitForDisconnected(1000))
                socket->abort();
        }
    }
}

void Widget::label_appIcon_clicked()
{
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
    if (iconClickCnt == 5)
    {
        if (g_isDebugMode == false)
        {
            // login creator
            g_isDebugMode = true;
            emit showLog(LogLevel_COlORFUL, FONT_COLOR_BLUEVIOLET "Login debugMode");
        }
        else
        {
            // quit creator
            g_isDebugMode = false;
            emit showLog(LogLevel_COlORFUL, FONT_COLOR_BLUEVIOLET "Quit debugMode");
        }

        iconClickCnt = 0;
        lastClickTime = 0;
    }
}

bool Widget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->label_appIcon && event->type() == QEvent::MouseButtonRelease)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton)
        {
            label_appIcon_clicked();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
