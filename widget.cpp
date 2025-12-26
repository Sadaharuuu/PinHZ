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
bool g_dataLog_logMode = 0;

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    ui->setupUi(this);

    m_autoReplyTimes = 0;
    m_autoReplyDelay = 0;

    m_netUnit.netType = 0;
    m_netUnit.netState = 0;

    // log显示
    connect(this, &Widget::showLog, this, &Widget::on_showLog, Qt::DirectConnection);

    // 初始化串口
    // 限制波特率只能输入数字
    ui->combo_serialBaud->setValidator(new QIntValidator(1, 20000000));
    m_serialPort = new QSerialPort(this);

    // 提取保存的配置
    loadConf();

    // 定时器
    m_timer_Run = new QTimer(this);
    connect(m_timer_Run, &QTimer::timeout, this, &Widget::on_timerOut_Run);
    m_timer_Run->setInterval(10);
    m_timer_Run->start();

    // 拼好帧
    ui->table_PinHZ->setColumnWidth(colDataType, 70); // 数据类型列设置列宽
    ui->table_PinHZ->setColumnWidth(colDataHex, 130); // 数据Hex列设置列宽
    QHeaderView *colHeader = ui->table_PinHZ->horizontalHeader();
    colHeader->setSectionResizeMode(colDataType, QHeaderView::ResizeToContents); // 数据列保持最小列宽
    colHeader->setSectionResizeMode(colComment, QHeaderView::Stretch);           // 备注列自动拉伸列宽
    PinHZComboInit(0, 0);                                                        // 第一行的comboBox
    ui->table_PinHZ->setCurrentCell(0, colDataHex);                              // 选中第一行
    QMetaObject::invokeMethod(ui->button_PinHZ, "clicked", Qt::QueuedConnection);

    m_fillItemDlg = new FormFillItem(this);
    connect(m_fillItemDlg, &FormFillItem::fillConfDone, this, &Widget::on_fillConfDone, Qt::QueuedConnection);

    m_dataLogDlgSerial = new FormDataLog(this, m_dataLogMode);
    m_dataLogDlgSerial->setWindowTitle("串口数据日志");
    connect(this, &Widget::dataShowSerial, m_dataLogDlgSerial, &FormDataLog::on_dataShow, Qt::DirectConnection);
    connect(this, &Widget::updateDataCntSerial, m_dataLogDlgSerial, &FormDataLog::on_updateDataCnt, Qt::QueuedConnection);
    m_dataLogDlgNet = new FormDataLog(this, m_dataLogMode);
    m_dataLogDlgNet->setWindowTitle("网口数据日志");
    connect(this, &Widget::dataShowNet, m_dataLogDlgNet, &FormDataLog::on_dataShow, Qt::DirectConnection);
    connect(this, &Widget::updateDataCntNet, m_dataLogDlgNet, &FormDataLog::on_updateDataCnt, Qt::QueuedConnection);

    m_crcConfDlg = new FormCRCConf(this);
    connect(m_crcConfDlg, &FormCRCConf::CRCConfDone, this, &Widget::on_CRCConfDone, Qt::DirectConnection);

    connect(m_serialPort, &QSerialPort::readyRead, this, &Widget::on_serialRecv, Qt::DirectConnection);

    // netport
    m_udpSocket = new QUdpSocket(this);
    m_tcpClient = new QTcpSocket(this);
    m_tcpServer = new QTcpServer(this);

    connect(m_udpSocket, &QUdpSocket::readyRead, this, &Widget::on_netReadyRead);

    connect(m_tcpClient, &QTcpSocket::connected, this, &Widget::on_netConnected);
    connect(m_tcpClient, &QTcpSocket::disconnected, this, &Widget::on_netDisconnected);
    connect(m_tcpClient, &QTcpSocket::readyRead, this, &Widget::on_netReadyRead);
    connect(m_tcpClient, &QTcpSocket::stateChanged, this, &Widget::on_netStateChanged);
    connect(m_tcpClient, \
            static_cast<void (QTcpSocket::*)(QAbstractSocket::SocketError)>(&QTcpSocket::error), \
            this, &Widget::on_netSocketErr);

    connect(m_tcpServer, &QTcpServer::newConnection, this, &Widget::on_netNewConnection);
}

Widget::~Widget()
{
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
 * @brief ms定时器槽函数，主要用于检测程序运行时间
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
    settings.setValue("Net/AddrRemote_UDP", m_netUnit.addrRemote[NetType_UDP]);
    settings.setValue("Net/PortRemote_UDP", m_netUnit.portRemote[NetType_UDP]);
    settings.setValue("Net/AddrLocal_TCPC", m_netUnit.addrLocal[NetType_TCPC]);
    settings.setValue("Net/PortLocal_TCPC", m_netUnit.portLocal[NetType_TCPC]);
    settings.setValue("Net/AddrRemote_TCPC", m_netUnit.addrRemote[NetType_TCPC]);
    settings.setValue("Net/PortRemote_TCPC", m_netUnit.portRemote[NetType_TCPC]);
    settings.setValue("Net/AddrLocal_TCPS", m_netUnit.addrLocal[NetType_TCPS]);
    settings.setValue("Net/PortLocal_TCPS", m_netUnit.portLocal[NetType_TCPS]);

    settings.setValue("PortMode", ui->combo_portMode->currentIndex());

    m_dataLogMode = 0;
    m_dataLogMode |= m_dataLogDlgSerial->m_recvMode;
    m_dataLogMode |= m_dataLogDlgSerial->m_sendMode << 2;
    m_dataLogMode |= m_dataLogDlgSerial->m_isLog << 4;
    settings.setValue("DataLog/ModeCtrl", m_dataLogMode);
}

void Widget::loadConf()
{
    QSettings settings(CONF_PATH, QSettings::IniFormat);
    QString str = "";
    int32_t index = 0;

    str = settings.value("Serial/COM", "Unknown").toString();

    // refresh serial port and select
    serialRefresh();
    index = ui->combo_serialPort->findText(str);
    if (index >= 0)
        ui->combo_serialPort->setCurrentIndex(index);

    index = settings.value("Serial/Baud", 5).toInt();
    ui->combo_serialBaud->setCurrentIndex(index);
    index = settings.value("Serial/DataBit", 0).toInt();
    ui->combo_serialDataBit->setCurrentIndex(index);
    index = settings.value("Serial/StopBit", 0).toInt();
    ui->combo_serialStopBit->setCurrentIndex(index);
    index = settings.value("Serial/ParityBit", 0).toInt();
    ui->combo_serialParityBit->setCurrentIndex(index);

    index = settings.value("Net/NetType", 0).toInt();
    if (index == 0)
        on_combo_netType_currentIndexChanged(index);
    else
        ui->combo_netType->setCurrentIndex(index);
    m_netUnit.netType = index;

    str = getLocalIP();
    m_netUnit.addrLocal[NetType_UDP] = settings.value("Net/AddrLocal_UDP", str).toString();
    m_netUnit.portLocal[NetType_UDP] = settings.value("Net/PortLocal_UDP", 0).toString();
    m_netUnit.addrRemote[NetType_UDP] = settings.value("Net/AddrRemote_UDP", str).toString();
    m_netUnit.portRemote[NetType_UDP] = settings.value("Net/PortRemote_UDP", 0).toString();
    m_netUnit.addrLocal[NetType_TCPC] = settings.value("Net/AddrLocal_TCPC", str).toString();
    m_netUnit.portLocal[NetType_TCPC] = settings.value("Net/PortLocal_TCPC", 0).toString();
    m_netUnit.addrRemote[NetType_TCPC] = settings.value("Net/AddrRemote_TCPC", str).toString();
    m_netUnit.portRemote[NetType_TCPC] = settings.value("Net/PortRemote_TCPC", 0).toString();
    m_netUnit.addrLocal[NetType_TCPS] = settings.value("Net/AddrLocal_TCPS", str).toString();
    m_netUnit.portLocal[NetType_TCPS] = settings.value("Net/PortLocal_TCPS", 0).toString();

    // refresh ip address
    str = QHostInfo::localHostName();
    QHostInfo hostInfo = QHostInfo::fromName(str);
    QList<QHostAddress> addrList = hostInfo.addresses();
    foreach (QHostAddress hostAddr, addrList)
    {
        if (QAbstractSocket::IPv4Protocol == hostAddr.protocol())
        {
            ui->combo_netAddrLocal->addItem(hostAddr.toString());
            ui->combo_netAddrRemote->addItem(hostAddr.toString());
        }
    }
    ui->combo_netAddrLocal->addItem("127.0.0.1");
    ui->combo_netAddrRemote->addItem("127.0.0.1");
    ui->combo_netAddrLocal->addItem("0.0.0.0");
    ui->combo_netAddrRemote->addItem("0.0.0.0");

    // query for the current address in the combo and select
    index = ui->combo_netAddrLocal->findText(m_netUnit.addrLocal[m_netUnit.netType]);
    if (index >= 0)
        ui->combo_netAddrLocal->setCurrentIndex(index);
    if (m_netUnit.netType < 2)
    {
        index = ui->combo_netAddrRemote->findText(m_netUnit.addrRemote[m_netUnit.netType]);
        if (index >= 0)
            ui->combo_netAddrRemote->setCurrentIndex(index);
    }

    // set the stored port
    ui->lineEdit_netPortLocal->setText(m_netUnit.portLocal[m_netUnit.netType]);
    if (m_netUnit.netType < 2)
        ui->lineEdit_netPortRemote->setText(m_netUnit.portRemote[m_netUnit.netType]);

    index = settings.value("PortMode", 0).toInt();
    if (index == 0)
        on_combo_portMode_currentIndexChanged(index);
    else
        ui->combo_portMode->setCurrentIndex(index);

    m_dataLogMode = settings.value("DataLog/ModeCtrl", 0x1A).toInt();
}

int32_t Widget::PinHZDeal(QString str, uint8_t *buf)
{
    QStringList strList;
    int32_t strNum = 0;
    int32_t dealLen = 0;
    bool isHex = false;

    strList = str.split(" ");
    strNum = strList.size();

    for (int32_t i = 0; i < strNum; i++)
    {
        str = strList[i];
        for (int32_t j = 0; j < str.length(); j += 2)
        {
            buf[dealLen++] = str.mid(j, 2).toUInt(&isHex, 16);
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
    comboBox->addItem("uint16");
    comboBox->addItem("uint32");
    comboBox->addItem("uint64");
    comboBox->addItem("int8");
    comboBox->addItem("int16");
    comboBox->addItem("int32");
    comboBox->addItem("int64");
    comboBox->addItem("float");
    comboBox->addItem("double");
    comboBox->setCurrentIndex(dataType);
    connect(comboBox, \
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), \
            this, \
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
        case 4: /* sint8 */ dataLen = 1; break;
        case 1: /* uint16 */ /* fall-through */
        case 5: /* sint16 */ dataLen = 2; break;
        case 8: /* float  */ /* fall-through */
        case 2: /* uint32 */ /* fall-through */
        case 6: /* sint32 */ dataLen = 4; break;
        case 9: /* double */ /* fall-through */
        case 3: /* uint64 */ /* fall-through */
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

    switch (level)
    {
    case LogLevel_DBG:
        ui->txtBs_log->append(FONT_COLOR_BLUE + Time + "[DBG]<br>" + string);
        break;
    case LogLevel_INF:
        ui->txtBs_log->append(FONT_COLOR_BLACK + Time + "<br>" + string);
        break;
    case LogLevel_WAR:
        ui->txtBs_log->append(FONT_COLOR_DARK_ORANGE + Time + "[WAR]<br>" + string);
        break;
    case LogLevel_ERR:
        ui->txtBs_log->append(FONT_COLOR_RED + Time + "[ERR]<br>" + string);
        break;
    default:
        break;
    }
    QScrollBar *scrollBar = ui->txtBs_log->verticalScrollBar();
    scrollBar->setSliderPosition(scrollBar->maximum());
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
                str.asprintf("%X", g_fillBuf[bufIndex++]);
            else // 重复填充
            {
                switch (dataType)
                {
                case 0: /* uint8  */ /* fall-through */
                case 4: /* sint8  */ dataLen = 1; break;
                case 1: /* uint16 */ /* fall-through */
                case 5: /* sint16 */ dataLen = 2; break;
                case 8: /* float  */ /* fall-through */
                case 2: /* uint32 */ /* fall-through */
                case 6: /* sint32 */ dataLen = 4; break;
                case 9: /* double */ /* fall-through */
                case 3: /* uint64 */ /* fall-through */
                case 7: /* sint64 */ dataLen = 8; break;
                default: break;
                }
                memcpy(&value, pFillBuf + bufIndex, dataLen);
                bufIndex += dataLen;
                str.asprintf("%X", value);
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
        int index;

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

    ui->table_PinHZ->insertRow(curRow);                      // 插入一行，但不会自动为单元格创建item
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

        ui->table_PinHZ->insertRow(curRow);                  // 插入一行，但不会自动为单元格创建item
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
        else if (colStrList[1] == "uint16") curDataType = 1;
        else if (colStrList[1] == "uint32") curDataType = 2;
        else if (colStrList[1] == "uint64") curDataType = 3;
        else if (colStrList[1] == "int8") curDataType = 4;
        else if (colStrList[1] == "int16") curDataType = 5;
        else if (colStrList[1] == "int32") curDataType = 6;
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
        str = cellItem->text();                              // 字符串连接
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

    filePath = QFileDialog::getOpenFileName(this, tr("请选择或输入你要打开的文件名"), \
                                            "", tr("逗号分隔文件(*.csv)"));
    if (filePath.isEmpty())
    {
        emit showLog(LogLevel_WAR, "未正确选择文件, 放弃处理");
        return;
    }
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
            else if (colStrList[1] == "uint16") curDataType = 1;
            else if (colStrList[1] == "uint32") curDataType = 2;
            else if (colStrList[1] == "uint64") curDataType = 3;
            else if (colStrList[1] == "int8") curDataType = 4;
            else if (colStrList[1] == "int16") curDataType = 5;
            else if (colStrList[1] == "int32") curDataType = 6;
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
        case 0: /* uint8  */ /* fall-through */
        case 4: /* sint8  */ dataLen = 1; break;
        case 1: /* uint16 */ /* fall-through */
        case 5: /* sint16 */ dataLen = 2; break;
        case 8: /* float  */ /* fall-through */
        case 2: /* uint32 */ /* fall-through */
        case 6: /* sint32 */ dataLen = 4; break;
        case 9: /* double */ /* fall-through */
        case 3: /* uint64 */ /* fall-through */
        case 7: /* sint64 */ dataLen = 8; break;
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
        // 模板不匹配
        str = QString::asprintf("模板不匹配, 模板:%dBytes, 数据:%dBytes", bufIndex, bufLen);
        emit showLog(LogLevel_WAR, str);
        return;
    }
}

void Widget::on_button_dataLog_clicked()
{
    if (ui->combo_portMode->currentIndex() == 0)
        m_dataLogDlgSerial->show();
    else
        m_dataLogDlgNet->show();
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
    str = m_hex2dec.Dec2HexString(checkStr, \
                                  dataType, \
                                  g_isLittleEndian);
    item = ui->table_PinHZ->item(dataZoneEnd + 1, colDataHex);
    item->setText(str);
    // 校验行的Dec item
    str = m_hex2dec.Hex2DecString(str, \
                                  dataType, \
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
        case 4: /* int8   */ byteStrLen = 1 * 2; break;
        case 1: /* uint16 */ /* fall-through */
        case 5: /* int16  */ byteStrLen = 2 * 2; break;
        case 2: /* uint32 */ /* fall-through */
        case 6: /* int32  */ /* fall-through */
        case 8: /* float  */ byteStrLen = 4 * 2; break;
        case 3: /* uint64 */ /* fall-through */
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
        // 大部分人输入习惯是大端模式，类似输入1根据数据类型默认为01或0001等，字符串默认大端
        fixStr = m_hex2dec.StrFix(str, comboBox->currentIndex(), true, false);
        fixStr = m_hex2dec.StrFix(fixStr, comboBox->currentIndex(), true, g_isLittleEndian);
        item->setText(fixStr);
        str = m_hex2dec.Hex2DecString(fixStr, \
                                      comboBox->currentIndex(), \
                                      g_isLittleEndian);
        item = ui->table_PinHZ->item(row, colDataDec);
        item->setText(str);
        break;
    }
    case colDataDec:
    {
        fixStr = m_hex2dec.StrFix(str, comboBox->currentIndex(), false, g_isLittleEndian);
        item->setText(fixStr);
        str = m_hex2dec.Dec2HexString(fixStr, \
                                      comboBox->currentIndex(), \
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

    bool isUint = false;
    QString addrStr = ui->combo_netAddrLocal->currentText();
    QString portStr = ui->lineEdit_netPortLocal->text();
    if (portStr == "Auto")
        portStr = "0";
    QHostAddress addr(addrStr);
    uint16_t port = portStr.toUInt(&isUint);

    if (isUint == false)
    {
        emit showLog(LogLevel_ERR, "Local Port");
        return;
    }

    if (addr.protocol() != QAbstractSocket::IPv4Protocol\
            && addrStr != "0.0.0.0")
    {
        // ipv4 invalid or all
        emit showLog(LogLevel_ERR, "Local Addr Err");
        return;
    }

    if (m_netUnit.netState == 0)
    {
        // try to connect
        switch (m_netUnit.netType)
        {
        case NetType_UDP:
            if (m_udpSocket->state() == QUdpSocket::BoundState)
            {
                // if has bound, abort
                netStop();
            }
            if (m_udpSocket->bind(addr, port))
            {
                m_netUnit.addrLocal[NetType_UDP] = addrStr;
                m_netUnit.portLocal[NetType_UDP] = portStr;
                on_netConnected();
                if (port == 0)
                    emit showLog(LogLevel_INF, "端口号为0, 已自动获取");
                emit showLog(LogLevel_INF, QString::asprintf("本地端口成功绑定至%s:%d", \
                                                             m_udpSocket->localAddress().toString().toUtf8().data(), \
                                                             m_udpSocket->localPort()));
            }
            else
            {
                emit showLog(LogLevel_INF, "UDP Port bind failed!");
            }
            break;
        case NetType_TCPC:
            m_netUnit.addrLocal[NetType_TCPC] = addrStr;

            if (ui->lineEdit_netPortLocal->text() != "Auto")
                m_netUnit.portLocal[NetType_TCPC] = ui->lineEdit_netPortLocal->text();

            m_netUnit.addrRemote[NetType_TCPC] = ui->combo_netAddrRemote->currentText();
            m_netUnit.portRemote[NetType_TCPC] = ui->lineEdit_netPortRemote->text();

            if (m_tcpClient->state() != QTcpSocket::UnconnectedState)
            {
                // 如果当前socket已连接，则先断开
                m_tcpClient->disconnectFromHost();
                if (!m_tcpClient->waitForDisconnected(1000))
                    netStop();
            }
            if (ui->lineEdit_netPortLocal->text() != "Auto" && false)
            {
                // 如果需要手动配置本地端口，先暂时强制不使用
                if (!m_tcpClient->bind(QHostAddress(m_netUnit.addrLocal[NetType_TCPC]), m_netUnit.portLocal[NetType_TCPC].toUInt()))
                {
                    // 本地端口绑定失败
                    emit showLog(LogLevel_ERR, QString("本地端口绑定失败<br>IP:" + m_netUnit.addrLocal[NetType_TCPC]
                                                        + "<br>端口:" + m_netUnit.portLocal[NetType_TCPC] + "<br>原因:"
                                                        + m_tcpClient->errorString()));
                    return;
                }
            }

            m_tcpClient->connectToHost(QHostAddress(m_netUnit.addrRemote[NetType_TCPC]),
                                       m_netUnit.portRemote[NetType_TCPC].toUInt());
            break;
        case NetType_TCPS:
            if (m_tcpServer->isListening())
            {
                netStop();
                ui->button_netSwitch->setText("开始监听");
            }
            else
            {
                if (m_tcpServer->listen(QHostAddress(m_netUnit.addrLocal[NetType_TCPS]),
                                        m_netUnit.portLocal[NetType_TCPS].toUInt()))
                {
                    emit showLog(LogLevel_INF, "启动监听成功");
                    ui->button_netSwitch->setText("停止监听");
                    ui->combo_netType->setEnabled(false);
                    ui->combo_netAddrLocal->setEnabled(false);
                    ui->lineEdit_netPortLocal->setEnabled(false);
                }
                else
                    emit showLog(LogLevel_INF, "启动监听失败, 原因:" + m_tcpServer->errorString());
            }
            break;
        default: break;
        }
    }
    else
    {
        // try to disconnect
        switch (m_netUnit.netType)
        {
        case NetType_UDP:
            if (m_udpSocket->state() == QUdpSocket::BoundState)
            {
                // if has bound, abort
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
            break;
        case NetType_TCPC:
            if (m_tcpClient->state() != QTcpSocket::UnconnectedState)
            {
                // 如果当前socket已连接，则先断开
                m_tcpClient->disconnectFromHost();
                if (!m_tcpClient->waitForDisconnected(1000))
                    m_tcpClient->abort();
            }
            break;
        case NetType_TCPS:
            netStop();
            break;
        default:
            break;
        }
        ui->combo_netType->setEnabled(true);
        ui->combo_netAddrLocal->setEnabled(true);
        ui->lineEdit_netPortLocal->setEnabled(true);
        ui->combo_netAddrRemote->setEnabled(true);
        ui->lineEdit_netPortRemote->setEnabled(true);
        m_netUnit.netState = 0;
        emit showLog(LogLevel_INF, "网络连接已断开");
    }
}

void Widget::netStop()
{
    switch (m_netUnit.netType)
    {
    case NetType_UDP:
        m_udpSocket->abort();
        emit showLog(LogLevel_INF, "udp bind was stop");
    case NetType_TCPC:
        m_tcpClient->abort();
        emit showLog(LogLevel_INF, "Tcp Client connection was stop");
        break;
    case NetType_TCPS:
        if (m_tcpServer->isListening())
        {
            while (!m_netUnit.tcpClientList.isEmpty())
            {
                QTcpSocket *socket = m_netUnit.tcpClientList.takeFirst();
                if (socket->state() == QTcpSocket::ConnectedState)
                {
                    socket->disconnectFromHost();
                    if (!socket->waitForDisconnected(1000))
                        socket->abort();
                }
                socket->deleteLater();
            }
            m_tcpServer->close();
            emit showLog(LogLevel_INF, "Tcp Server was stop");
        }
        break;
    default:
        break;
    }
    m_netUnit.netState = 0;
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
        ui->combo_netAddrRemote->setEnabled(false);
        ui->lineEdit_netPortRemote->setEnabled(false);
        succMsg = QString("成功连接到服务端!")
                + "<br>客户端:" + m_netUnit.addrLocal[NetType_TCPC] + ":" + m_netUnit.portLocal[NetType_TCPC]
                + "<br>服务端:" + m_netUnit.addrRemote[NetType_TCPC] + ":" + m_netUnit.portRemote[NetType_TCPC];
        break;
    default:
        break;
    }

    ui->combo_netType->setEnabled(false);
    ui->combo_netAddrLocal->setEnabled(false);
    ui->lineEdit_netPortLocal->setEnabled(false);
    m_netUnit.netState = 1;

    emit showLog(LogLevel_INF, succMsg);
}

void Widget::on_netDisconnected()
{
    switch (m_netUnit.netType)
    {
    case NetType_TCPC:
        netStop();
        emit showLog(LogLevel_INF, "net has disconnected");
        break;
    case NetType_TCPS:
        break;
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
                emit showLog(LogLevel_ERR, QString::asprintf("recv failed, errCode:%d, reason:%s", \
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

        if (!socket || !m_netUnit.tcpClientList.contains(socket))
            return;

        data = socket->readAll(); // Read all data in QByteArray
        dataInfo += "from" + socket->peerAddress().toString() \
                + ':' + QString::number(socket->peerPort()) + " ";
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
        msg = "scoket:UnconnectedState";
        break;
    case QAbstractSocket::HostLookupState:
        msg = "scoket:HostLookupState";
        break;
    case QAbstractSocket::ConnectingState:
        msg = "scoket:ConnectingState";
        break;
    case QAbstractSocket::ConnectedState:
        msg = "scoket:ConnectedState";
        break;
    case QAbstractSocket::BoundState:
        msg = "scoket:BoundState";
        break;
    case QAbstractSocket::ClosingState:
        msg = "scoket:ClosingState";
        break;
    case QAbstractSocket::ListeningState:
        msg = "scoket:ListeningState";
    }

    emit showLog(LogLevel_DBG, msg);
}

void Widget::on_netSocketErr(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    emit showLog(LogLevel_ERR, "socket err: " + m_tcpClient->errorString());
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
//        connect(socket, &QTcpSocket::error, this, &Widget::on_netSocketErr);
        m_netUnit.tcpClientList.append(socket);
        ui->combo_netClient->addItem(addr);
        emit showLog(LogLevel_INF, "new client online: " + addr);
    }
}

void Widget::netSend(uint8_t *buf, int32_t len)
{
    int32_t sendLen = 0;

    switch (m_netUnit.netType)
    {
    case NetType_UDP:
    {
        if (m_udpSocket->state() != QUdpSocket::BoundState)
        {
            emit showLog(LogLevel_ERR, "UDP has not bound local port!");
            return;
        }
        bool isUint = false;
        QString addrStr = ui->combo_netAddrRemote->currentText();
        QString portStr = ui->lineEdit_netPortRemote->text();
        QHostAddress addr(addrStr);
        uint16_t port = portStr.toUInt(&isUint);
        if (isUint == false)
        {
            emit showLog(LogLevel_ERR, "UDP Remote Port Err!");
            return;
        }
        if (addr.protocol() != QAbstractSocket::IPv4Protocol\
                && addrStr != "0.0.0.0")
        {
            // ipv4 invalid or all
            emit showLog(LogLevel_ERR, "UDP Remote Addr Err");
            return;
        }

        m_netUnit.addrRemote[NetType_UDP] = addrStr;
        m_netUnit.portRemote[NetType_UDP] = portStr;

        m_netMutex.lock();
        sendLen = m_udpSocket->writeDatagram((char *)buf, len, addr, port);
        m_netMutex.unlock();
        break;
    }
    case NetType_TCPC:
        if (m_tcpClient->state() != QTcpSocket::ConnectedState)
        {
            emit showLog(LogLevel_WAR, "网口未打开, 发送失败");
            return;
        }
        m_netMutex.lock();
        sendLen = m_tcpClient->write((char *)buf, len);
        m_netMutex.unlock();
        break;
    case NetType_TCPS:
    {
        QString sendAddr = "";
        uint16_t sendPort = 0;
        bool isSend = false;
        if (ui->combo_netClient->count() < 2)
        {
            emit showLog(LogLevel_INF, "无客户端已连接, 发送失败");
            return;
        }

        if (ui->combo_netClient->currentIndex() > 0)
        {
            QStringList strList = ui->combo_netClient->currentText().split(':');
            sendAddr = strList.at(0);
            sendPort = strList.at(1).toUInt();
        }

        foreach (QTcpSocket *socket, m_netUnit.tcpClientList)
        {
            if (ui->combo_netClient->currentIndex() == 0 \
                    || (socket->peerAddress().toString() == sendAddr \
                    && socket->peerPort() == sendPort))
            {
                if (socket->state() != QTcpSocket::ConnectedState)
                {
                    emit showLog(LogLevel_ERR, "网口未打开, 发送失败");
                    return;
                }
                m_netMutex.lock();
                sendLen = socket->write((char *)buf, len);
                m_netMutex.unlock();
                isSend = true;
            }
        }
        if (!isSend)
        {
            emit showLog(LogLevel_ERR, "未找到客户端" + sendAddr + ":" + QString::number(sendPort) + "发送失败");
        }
        break;
    }
    default: return;
    }

    if (sendLen < 0)
        emit showLog(LogLevel_ERR, "发送失败!");
    else
    {
        m_dataLogDlgNet->m_sendByte += sendLen;
        m_dataLogDlgNet->m_sendFrm++;
        emit updateDataCntNet();

        if (m_dataLogDlgNet->m_sendMode != 0)
            emit dataShowNet(buf, len, true, "# Recv ");
    }
}

void Widget::on_combo_netType_currentIndexChanged(int index)
{
    // save current config
    m_netUnit.addrLocal[m_netUnit.netType] = ui->combo_netAddrLocal->currentText();
    m_netUnit.portLocal[m_netUnit.netType] = ui->lineEdit_netPortLocal->text();
    if (m_netUnit.netType < 2)
    {
        m_netUnit.isLocalPortAuto[m_netUnit.netType] = ui->check_netPortLocal->isChecked();
        m_netUnit.addrRemote[m_netUnit.netType] = ui->combo_netAddrRemote->currentText();
        m_netUnit.portRemote[m_netUnit.netType] = ui->lineEdit_netPortRemote->text();
    }
    m_netUnit.netType = index;

    // load new net type config
    int32_t textIndex = 0;
    textIndex = ui->combo_netAddrLocal->findText(m_netUnit.addrLocal[m_netUnit.netType]);
    textIndex = textIndex < 0 ? 0 : textIndex;
    ui->combo_netAddrLocal->setCurrentIndex(textIndex);
    ui->lineEdit_netPortLocal->setText(m_netUnit.portLocal[m_netUnit.netType]);

    // deal widget show
    switch (m_netUnit.netType)
    {
    case NetType_UDP: ui->button_netSwitch->setText("绑定端口"); break;
    case NetType_TCPC: ui->button_netSwitch->setText("连接端口"); break;
    case NetType_TCPS: ui->button_netSwitch->setText("监听端口"); break;
    default: break;
    }

    if (m_netUnit.netType < 2)
    {
        // UDP or TCP Client
        // show widget of UDP/TCPC
        ui->label_netAddrRemote->show();
        ui->combo_netAddrRemote->show();
        ui->label_netPortRemote->show();
        ui->lineEdit_netPortRemote->show();
        ui->check_netPortLocal->show();

        // hide widget of TCPS
        ui->combo_netClient->hide();
        ui->button_netClientClose->hide();
        textIndex = ui->combo_netAddrRemote->findText(m_netUnit.addrRemote[m_netUnit.netType]);
        textIndex = textIndex < 0 ? 0 : textIndex;
        ui->combo_netAddrRemote->setCurrentIndex(textIndex);

        ui->lineEdit_netPortRemote->setText(m_netUnit.portRemote[m_netUnit.netType]);
        ui->check_netPortLocal->setChecked(m_netUnit.isLocalPortAuto[m_netUnit.netType]);
    }
    else
    {
        // TCP Server
        // hide widget of UDP/TCPC
        ui->label_netAddrRemote->hide();
        ui->combo_netAddrRemote->hide();
        ui->label_netPortRemote->hide();
        ui->lineEdit_netPortRemote->hide();
        ui->check_netPortLocal->hide();

        // show widget of TCPS
        ui->combo_netClient->show();
        ui->button_netClientClose->show();

        // set local port inpu edit to write able
        if (ui->lineEdit_netPortLocal->isReadOnly())
            ui->lineEdit_netPortLocal->setReadOnly(false);
    }
}

void Widget::on_check_netPortLocal_toggled(bool checked)
{
    if (checked)
    {
        m_netUnit.portLocal[m_netUnit.netType] = ui->lineEdit_netPortLocal->text();
        ui->lineEdit_netPortLocal->setText("Auto");
    }
    else
    {
        ui->lineEdit_netPortLocal->setText(m_netUnit.portLocal[m_netUnit.netType]);
    }

    ui->lineEdit_netPortLocal->setReadOnly(checked);
    m_netUnit.isLocalPortAuto[m_netUnit.netType] = checked;
}

void Widget::on_button_netRefresh_clicked()
{

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

}
