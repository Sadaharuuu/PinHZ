#include "ui_widget.h"
#include "widget.h"
#include <qmetaobject.h>

#define CONF_PATH "./conf.ini"

uint32_t g_timerCnt_10ms = 0;

// 拼好帧
int32_t g_rowCntHead = 1;
int32_t g_rowCntData = 0;
int32_t g_rowCntCheck = 0;
int32_t g_rowCntTail = 0;
bool g_isLittleEndian = true;
bool g_isAutoPinHZMode = true;

int32_t g_fillStart = 0;
int32_t g_fillLen = 0;
uint8_t g_fillBuf[1024] = {0, };

uint8_t g_dataLog_recvMode = 0;
uint8_t g_dataLog_sendMode = 0;
bool g_dataLog_logMode = 0;

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    ui->setupUi(this);

    m_dataLogMode = 0x11;
    m_autoReplyTimes = 0;
    m_autoReplyDelay = 0;

    // log显示
    connect(this, &Widget::showLog, this, &Widget::on_showLog, Qt::DirectConnection);

    // 初始化串口
    // 限制波特率只能输入数字
    ui->combo_serialBaud->setValidator(new QIntValidator(1, 20000000));
    serialPort = new QSerialPort(this);
    // serialPortRxCount = 0;

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
    PinHZComboInit(0, 0);                                                          // 第一行的comboBox
    ui->table_PinHZ->setCurrentCell(0, colDataHex);                                // 选中第一行
    QMetaObject::invokeMethod(ui->button_PinHZ, "clicked", Qt::QueuedConnection);

    m_fillItemDlg = new FormFillItem(this);
    connect(m_fillItemDlg, &FormFillItem::fillConfDone, this, &Widget::on_fillConfDone, Qt::QueuedConnection);

    m_datalogDlg = new FormDataLog(this, m_dataLogMode);
    connect(this, &Widget::dataShow, m_datalogDlg, &FormDataLog::on_dataShow, Qt::DirectConnection);

    m_crcConfDlg = new FormCRCConf(this);
    connect(m_crcConfDlg, &FormCRCConf::CRCConfDone, this, &Widget::on_CRCConfDone, Qt::DirectConnection);

    connect(this, &Widget::serialSend, this, &Widget::on_serialSend);
    connect(serialPort, &QSerialPort::readyRead, this, &Widget::on_serialRecv, Qt::DirectConnection);
}

Widget::~Widget()
{
    saveConf();
    delete m_timer_Run;
    delete m_fillItemDlg;
    delete m_datalogDlg;
    delete m_crcConfDlg;
    delete serialPort;
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

    m_dataLogMode = 0;
    m_dataLogMode |= m_datalogDlg->m_recvMode;
    m_dataLogMode |= m_datalogDlg->m_sendMode << 2;
    m_dataLogMode |= m_datalogDlg->m_isLog << 4;
    settings.setValue("DataLog/ModeCtrl", m_dataLogMode);
}

void Widget::loadConf()
{
    QSettings settings(CONF_PATH, QSettings::IniFormat);
    QString str = "";
    int32_t index = 0;

    str = settings.value("Serial/COM", "Unknown").toString();

    // 刷新串口号
    ui->combo_serialPort->clear();
    foreach (QSerialPortInfo info, QSerialPortInfo::availablePorts())
    {
        ui->combo_serialPort->addItem(info.portName());
        if (info.portName() == str)
            ui->combo_serialPort->setCurrentText(str);
    }

    index = settings.value("Serial/Baud", 5).toInt();
    ui->combo_serialBaud->setCurrentIndex(index);
    index = settings.value("Serial/DataBit", 0).toInt();
    ui->combo_serialDataBit->setCurrentIndex(index);
    index = settings.value("Serial/StopBit", 0).toInt();
    ui->combo_serialStopBit->setCurrentIndex(index);
    index = settings.value("Serial/ParityBit", 0).toInt();
    ui->combo_serialParityBit->setCurrentIndex(index);

    m_dataLogMode = settings.value("DataLog/ModeCtrl", 0x11).toInt();
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
            &Widget::on_combo_PinHZ_indexChanged);
    ui->table_PinHZ->setCellWidget(row, colDataType, comboBox);
}

void Widget::updateDataZoneBytes()
{
    QWidget *widget = nullptr;
    QComboBox *comboBox = nullptr;
    int8_t dataLen = 0;
    int32_t dataZoneBytes = 0;

    for (int32_t row = g_rowCntHead; row < g_rowCntHead + g_rowCntData; row++)
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
        dataZoneBytes += dataLen;
    }

    ui->lineEdit_DataLen->setText(QString::number(dataZoneBytes));
}

int8_t Widget::checkRowZone(int32_t row)
{
    int8_t ret = 0;

    if (row < 0 || row > ui->table_PinHZ->rowCount())
        ret = -1; // 行号错误
    else if (row < g_rowCntHead)
        ret = 1; // 帧头域
    else if (row < g_rowCntHead + g_rowCntData)
        ret = 2; // 数据域
    else if (g_rowCntCheck != 0 && row == g_rowCntHead + g_rowCntData)
        ret = 3; // 校验域
    else if (row < g_rowCntHead + g_rowCntData + g_rowCntCheck + g_rowCntTail)
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
    ui->table_PinHZ->setItem(row, colDataHex, item);               // 为单元格设置Item

    // dataDec
    item = new QTableWidgetItem(dataDec, ctDataDec);
    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter); // 文本对齐格式
    ui->table_PinHZ->setItem(row, colDataDec, item);               // 为单元格设置Item

    // Comment
    item = new QTableWidgetItem(comment, ctComment);
    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter); // 文本对齐格式
    ui->table_PinHZ->setItem(row, colComment, item);               // 为单元格设置Item

    // 解除阻断
    ui->table_PinHZ->blockSignals(false);
}

void Widget::on_showLog(LogLevel level, QString string)
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

void Widget::on_serialSend(uint8_t *buf, int32_t len)
{
    if (serialPort->isOpen())
    {
        m_serialMutex.lock();
        serialPort->write((char *)buf, len);
        m_serialMutex.unlock();
        // if (sendLen == len)
        //     emit showLog(LogLevel_INF, "串口发送成功");
        // else if (sendLen == -1)
        //     emit showLog(LogLevel_INF, "串口发送失败");
        // else
        //     emit showLog(LogLevel_INF, "串口发送长度错误");
        if (m_datalogDlg->m_sendMode != 0)
            emit dataShow(buf, len, true);
    }
    else
    {
        emit showLog(LogLevel_DBG, "串口未打开");
    }
}

void Widget::on_serialRecv()
{
    if (serialPort->bytesAvailable())
    {
        QByteArray data = serialPort->readAll(); // Read all data in QByteArray

        if (!data.isEmpty())
        {
            uint8_t *buf = (uint8_t *)(data.data());
            int32_t size = data.size();

            emit dataShow(buf, size, false);

            if (ui->check_autoReply->isChecked())
            {
                m_autoReplyTimes++;
            }
        }
    }
    else
    {
        return;
    }
}

void Widget::on_fillConfDone()
{
    if (!m_fillItemDlg->m_isFillValid)
    {
        emit showLog(LogLevel_ERR, "填充数据输入错误!");
        return;
    }
    int32_t bufIndex = 0, dataType = 0;
    QString str = "";
    QTableWidgetItem *item = nullptr;
    QWidget *widget = nullptr;
    QComboBox *comboBox = nullptr;

    ui->table_PinHZ->blockSignals(true);
    for (int32_t row = g_fillStart; row < g_fillStart + g_fillLen; row++)
    {
        if (row >= 0 && row < ui->table_PinHZ->rowCount())
        {
            widget = ui->table_PinHZ->cellWidget(row, colDataType);
            comboBox = qobject_cast<QComboBox *>(widget);
            dataType = comboBox->currentIndex();

            item = ui->table_PinHZ->item(row, colDataHex);
            str = QString::asprintf("%02X", g_fillBuf[bufIndex++]);
            str = m_hex2dec.StrFix(str, dataType, true, g_isLittleEndian);
            item->setText(str);
            str = m_hex2dec.Hex2DecString(str, dataType, g_isLittleEndian);
            item = ui->table_PinHZ->item(row, colDataDec);
            item->setText(str);
        }
    }
    ui->table_PinHZ->blockSignals(false);

    on_combo_PinHZ_checkChanged(-1);
}

void Widget::on_button_serialRefresh_clicked()
{
    ui->combo_serialPort->clear();
    foreach (QSerialPortInfo info, QSerialPortInfo::availablePorts())
    {
        ui->combo_serialPort->addItem(info.portName());
    }
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
        serialPort->setPort(portInfo);

        index = ui->combo_serialBaud->currentText().toInt();
        serialPort->setBaudRate(index);

        index = ui->combo_serialDataBit->currentIndex();
        switch (index)
        {
        case 0: serialPort->setDataBits(QSerialPort::Data8); break;
        case 1: serialPort->setDataBits(QSerialPort::Data7); break;
        case 2: serialPort->setDataBits(QSerialPort::Data6); break;
        case 3: serialPort->setDataBits(QSerialPort::Data5); break;
        default: serialPort->setDataBits(QSerialPort::Data8); break;
        }

        index = ui->combo_serialStopBit->currentIndex();
        switch (index)
        {
        case 0: serialPort->setStopBits(QSerialPort::OneStop); break;
        case 1: serialPort->setStopBits(QSerialPort::OneAndHalfStop); break;
        case 2: serialPort->setStopBits(QSerialPort::TwoStop); break;
        default: serialPort->setStopBits(QSerialPort::OneStop); break;
        }

        index = ui->combo_serialParityBit->currentIndex();
        switch (index)
        {
        case 0: serialPort->setParity(QSerialPort::NoParity); break;
        case 1: serialPort->setParity(QSerialPort::OddParity); break;
        case 2: serialPort->setParity(QSerialPort::EvenParity); break;
        case 3: serialPort->setParity(QSerialPort::MarkParity); break;
        case 4: serialPort->setParity(QSerialPort::SpaceParity); break;
        default: serialPort->setParity(QSerialPort::NoParity); break;
        }

        serialPort->setReadBufferSize(10240);

        if (serialPort->open(QIODevice::ReadWrite))
        {
            serialPort->clear();
            serialPort->clearError();
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
        serialPort->close();
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
    dstPath = QFileDialog::getSaveFileName(this, tr("选择保存位置"), "", ".ico");
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
    int32_t zoneStart = 0;
    int32_t zoneEnd = g_rowCntHead;

    if (curRow >= zoneStart && curRow < zoneEnd)
        curRow++;
    else
        curRow = zoneEnd;

    ui->table_PinHZ->insertRow(curRow);                      // 插入一行，但不会自动为单元格创建item
    createItemsARow(curRow, "Head", 0, "00", "0", "Head"); // 为某一行创建items
    g_rowCntHead++;
}

void Widget::on_button_addItem_clicked()
{
    int32_t curRow = 0;
    int32_t addNum = ui->spinBox_addItem->value();
    int32_t zoneStart = g_rowCntHead;
    int32_t zoneEnd = g_rowCntHead + g_rowCntData;

    for (int32_t i = 0; i < addNum; i++)
    {
        curRow = ui->table_PinHZ->currentRow(); // 当前行号
        if (curRow >= zoneStart && curRow < zoneEnd)
            curRow++;
        else
            curRow = zoneEnd;

        ui->table_PinHZ->insertRow(curRow);                  // 插入一行，但不会自动为单元格创建item
        createItemsARow(curRow, "Data", 0, "00", "0", ""); // 为某一行创建items
        g_rowCntData++;
        zoneEnd++;
        updateDataZoneBytes();
        if (g_rowCntCheck == 1)
            on_combo_PinHZ_checkChanged(-1);
    }
}

void Widget::on_button_checkSet_clicked()
{
    if (g_rowCntCheck == 1)
        return;

    ui->button_checkSet->setEnabled(false);

    int32_t curRow = g_rowCntHead + g_rowCntData;

    // 阻断信号，避免触发itemChanged
    ui->table_PinHZ->blockSignals(true);

    ui->table_PinHZ->insertRow(curRow); // 插入一行，但不会自动为单元格创建item
    // 为一行的单元格创建 Items
    QTableWidgetItem *item;

    // rowHead
    item = new QTableWidgetItem("Check");
    ui->table_PinHZ->setVerticalHeaderItem(curRow, item);

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
    ui->table_PinHZ->setCellWidget(curRow, colDataType, comboBox);

    // dataHex
    item = new QTableWidgetItem("00", ctDataHex);
    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter); // 文本对齐格式
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    ui->table_PinHZ->setItem(curRow, colDataHex, item); // 为单元格设置Item

    // dataDec
    item = new QTableWidgetItem("0", ctDataDec);
    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter); // 文本对齐格式
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    ui->table_PinHZ->setItem(curRow, colDataDec, item); // 为单元格设置Item

    // Comment
    item = new QTableWidgetItem("校验", ctComment);
    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter); // 文本对齐格式
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    ui->table_PinHZ->setItem(curRow, colComment, item); // 为单元格设置Item

    // 解除阻断
    ui->table_PinHZ->blockSignals(false);
    g_rowCntCheck = 1;
}

void Widget::on_button_addTail_clicked()
{
    int32_t curRow = ui->table_PinHZ->currentRow(); // 当前行号
    int32_t zoneStart = g_rowCntHead + g_rowCntData + g_rowCntCheck;
    int32_t zoneEnd = zoneStart + g_rowCntTail;

    if (curRow >= zoneStart && curRow < zoneEnd)
        curRow++;
    else
        curRow = zoneEnd;

    ui->table_PinHZ->insertRow(curRow);                      // 插入一行，但不会自动为单元格创建item
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
    if (g_rowCntHead + g_rowCntData + g_rowCntCheck + g_rowCntTail <= 1)
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
            ui->table_PinHZ->removeRow(row);
            int8_t rowZone = checkRowZone(row);
            switch (rowZone) {
            case 1: g_rowCntHead--; break;
            case 2: g_rowCntData--; break;
            case 3:
                g_rowCntCheck = 0;
                ui->button_checkSet->setEnabled(true);
                break;
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
    g_fillLen = rows.size();
    m_fillItemDlg->show();
}

void Widget::on_button_PinHZ_clicked()
{
    QString str = "", PinHZStr = "";
    QTableWidgetItem *cellItem = nullptr;

    ui->plainEdit_PinHZ->clear(); // 文本编辑器清空

    for (int32_t row = 0; row < ui->table_PinHZ->rowCount(); row++) // 逐行处理
    {
        if (g_rowCntCheck == 1 && m_checkType == 0 && row == g_rowCntHead + g_rowCntData)
            continue; // 校验位为None不参与拼好帧
        cellItem = ui->table_PinHZ->item(row, colDataHex); // 获取单元格的item
        str = cellItem->text();                              // 字符串连接
        if (!ui->check_fieldPinHZ->isChecked())
        {
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

    ui->plainEdit_PinHZ->setPlainText(PinHZStr);
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
    emit showLog(LogLevel_INF, QString::asprintf("开始读取模板, 请稍后..."));

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
            if (curDataType == 0)
            {
                colStrList[2] = "00";
                colStrList[3] = "0";
            }
            else
            {
                colStrList[2] = m_hex2dec.StrFix(colStrList[2], curDataType - 1, true, false);
                colStrList[3] = m_hex2dec.StrFix(colStrList[3], curDataType - 1, false, false);
            }

            // 阻断信号，避免触发itemChanged
            ui->table_PinHZ->blockSignals(true);

            ui->table_PinHZ->insertRow(row); // 插入一行，但不会自动为单元格创建item
            g_rowCntCheck = 1;
            ui->button_checkSet->setEnabled(false);

            QTableWidgetItem *item;

            // rowHead
            item = new QTableWidgetItem("Check");
            ui->table_PinHZ->setVerticalHeaderItem(row, item);

            // hexType
            QComboBox *comboBox = new QComboBox();
            comboBox->addItem("None");
            comboBox->addItem("CheckSum-8");
            comboBox->addItem("CheckSum-16");
            comboBox->addItem("CheckSum-32");
            comboBox->addItem("CRC");
            comboBox->setCurrentIndex(curDataType);
            connect(comboBox, \
                    static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), \
                    this, \
                    &Widget::on_combo_PinHZ_checkChanged);
            ui->table_PinHZ->setCellWidget(row, colDataType, comboBox);

            // dataHex
            item = new QTableWidgetItem(colStrList[2], ctDataHex);
            item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter); // 文本对齐格式
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            ui->table_PinHZ->setItem(row, colDataHex, item); // 为单元格设置Item

            // dataDec
            item = new QTableWidgetItem(colStrList[3], ctDataDec);
            item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter); // 文本对齐格式
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            ui->table_PinHZ->setItem(row, colDataDec, item); // 为单元格设置Item

            // Comment
            item = new QTableWidgetItem("校验", ctComment);
            item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter); // 文本对齐格式
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            ui->table_PinHZ->setItem(row, colComment, item); // 为单元格设置Item

            ui->table_PinHZ->blockSignals(false);
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
            if (colStrList[0] == "Data")
            {
                updateDataZoneBytes();
                if (g_rowCntCheck == 1)
                    on_combo_PinHZ_checkChanged(-1);
            }
        }
    }
    file.close();
    updateDataZoneBytes();
    if (g_isAutoPinHZMode)
        QMetaObject::invokeMethod(ui->button_PinHZ, "clicked", Qt::QueuedConnection);
    emit showLog(LogLevel_INF, QString::asprintf("模板读取成功"));
}

void Widget::on_button_PinHZSend_clicked()
{
    QString str = ui->plainEdit_PinHZ->toPlainText();
    uint8_t txBuf[2048] = {0, };
    int32_t txLen = 0;

    txLen = PinHZDeal(str, txBuf);

    emit serialSend(txBuf, txLen);
}

void Widget::on_button_PinHZReverse_clicked()
{
    QString str = ui->plainEdit_PinHZ->toPlainText();
    uint8_t buf[1024] = {0, }, dataLen = 0;
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

        switch (dataType) {
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
        if (row == g_rowCntHead + g_rowCntData)
        {
            // check
            switch (dataType) {
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
    m_datalogDlg->show();
}

void Widget::on_combo_PinHZ_indexChanged(int index)
{
    QComboBox *comboBox = dynamic_cast<QComboBox *>(this->sender());
    int32_t row = 0;
    for (row = 0; row < g_rowCntHead + g_rowCntData + g_rowCntTail; row++)
    {
        if (ui->table_PinHZ->cellWidget(row, colDataType) == comboBox)
            break;
    }
    if (row == g_rowCntHead + g_rowCntData + g_rowCntTail)
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

    if (row >= g_rowCntHead && row < g_rowCntHead + g_rowCntData)
        updateDataZoneBytes();
    on_combo_PinHZ_checkChanged(-1);
}

void Widget::on_combo_PinHZ_checkChanged(int index)
{
    int32_t dataZoneStart = g_rowCntHead, dataZoneEnd = g_rowCntHead + g_rowCntData;
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
        QWidget *widget = ui->table_PinHZ->cellWidget(dataZoneEnd, colDataType);
        QComboBox *comboBox = qobject_cast<QComboBox *>(widget);

        m_checkType = comboBox->currentIndex();
        isAppCalled = true;
    }

    if (m_checkType != 0)
    {
        for (int32_t row = dataZoneStart; row < dataZoneEnd; row++) // 获取数据域码流
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
    item = ui->table_PinHZ->item(dataZoneEnd, colDataHex);
    item->setText(str);
    // 校验行的Dec item
    str = m_hex2dec.Hex2DecString(str, \
                                  dataType, \
                                  g_isLittleEndian);
    item = ui->table_PinHZ->item(dataZoneEnd, colDataDec);
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
        return;
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

void Widget::on_spinBox_replyTime_valueChanged(int arg1)
{
    m_autoReplyDelay = arg1;
}

void Widget::on_check_fieldPinHZ_toggled(bool checked)
{
    QMetaObject::invokeMethod(ui->button_PinHZ, "clicked", Qt::QueuedConnection);
}

void Widget::on_CRCConfDone(int8_t validCode)
{
    if (g_rowCntCheck == 0)
        return;
    QString str = "";
    if (validCode < 0)
    {
        switch (validCode) {
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

void Widget::on_spinBox_sendPeriod_valueChanged(int arg1)
{
    m_autoSendPeriod = arg1 / 10;
}
