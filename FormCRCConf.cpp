#include "CommonDefine.h"
#include "FormCRCConf.h"
#include "Hex2Dec.h"
#include "ui_FormCRCConf.h"

FormCRCConf::FormCRCConf(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormCRCConf)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);

    on_button_templateSet_clicked();
}

FormCRCConf::~FormCRCConf()
{
    delete ui;
    delete m_CRCMeterWidget;
}

void FormCRCConf::on_button_templateSet_clicked()
{
    e_CRC_Temp tempID = (e_CRC_Temp)ui->combo_template->currentIndex();
    t_crc_conf crcConf = m_crcCalc.GetTemplate(tempID);
    QString str = "";

    ui->spin_dataWidth->setValue(crcConf.width);
    str.sprintf("%X", crcConf.poly);
    ui->lineEdit_poly->setText(str);
    str.sprintf("%X", crcConf.init);

    ui->lineEdit_init->setText(str);
    if (crcConf.ref_in)
        ui->radio_refIn_reverse->setChecked(true);
    else
        ui->radio_refIn->setChecked(true);
    if (crcConf.ref_out)
        ui->radio_refOut_reverse->setChecked(true);
    else
        ui->radio_refOut->setChecked(true);

    str.sprintf("%X", crcConf.xor_out);
    ui->lineEdit_xorOut->setText(str);
}

void FormCRCConf::on_button_confDone_clicked()
{
    bool isValid = false;
    int8_t validCode = 0;
    do {
        m_crcConf.width = ui->spin_dataWidth->value();

        if (m_crcConf.width < 8 || m_crcConf.width > 32)
        {
            validCode = -1;
            break;
        }

        if (m_crcConf.width <= 8)
            m_dataType = DataType_U08; // uint8
        else if (m_crcConf.width <= 16)
            m_dataType = DataType_U16; // uint16
        else if (m_crcConf.width <= 32)
            m_dataType = DataType_U32; // uint32
        else
            m_dataType = DataType_U08; // uint8

        m_crcConf.poly = ui->lineEdit_poly->text().toUInt(&isValid, 16);
        if (!isValid)
        {
            validCode = -2;
            break;
        }
        m_crcConf.init = ui->lineEdit_init->text().toUInt(&isValid, 16);
        if (!isValid)
        {
            validCode = -3;
            break;
        }
        m_crcConf.ref_in = ui->radio_refIn_reverse->isChecked();
        m_crcConf.ref_out = ui->radio_refOut_reverse->isChecked();
        m_crcConf.xor_out = ui->lineEdit_xorOut->text().toUInt(&isValid, 16);
        if (!isValid)
        {
            validCode = -4;
            break;
        }
    } while (0);

    emit CRCConfDone(validCode);
}

void FormCRCConf::on_button_convertPoly_clicked()
{
    bool isMath2Hex = ui->radio_math2hex->isChecked();
    bool isSamplePoly = ui->radio_poly_simple->isChecked();
    bool isValid = false;
    QString binStr = "", hexStr = "", mathStr = "";
    QList<int32_t> orderList;
    int32_t order, maxOrder = ui->spin_maxOrder->value();;
    uint64_t hexPoly = 0;

    if (isMath2Hex)
    {
        QStringList mathList = ui->lineEdit_poly_math->text().split('+');
        foreach (QString mathStrItem, mathList)
        {
            QString numStr = mathStrItem.mid(1);
            order = numStr.toInt(&isValid, 10);
            if (!isValid)
            {
                ui->lineEdit_poly_hex->setText("阶数输入错误!!!");
                return;
            }
            orderList.append(order);
        }
        if (orderList.isEmpty())
        {
            ui->lineEdit_poly_hex->setText("阶数解析错误!!!");
            return;
        }
        std::sort(orderList.rbegin(), orderList.rend());
        int32_t highestOrder = *orderList.constBegin();
        ui->spin_maxOrder->setValue(highestOrder);

        if (isSamplePoly)
            highestOrder--;
        for (int32_t i = highestOrder; i >= 0; i--)
        {
            binStr += orderList.contains(i) ? '1' : '0';
        }
        if (binStr.length() % 4)
        {
            binStr = QString(4 - binStr.length() % 4, '0') + binStr;
        }
        hexPoly = binStr.toULongLong(&isValid, 2);
        hexStr.sprintf("%X", (uint32_t)hexPoly);
        if (hexStr.length() % 2 && isSamplePoly)
            hexStr = "0" + hexStr;
        if (maxOrder == 32 && !isSamplePoly)
            hexStr = "1" + hexStr;
        ui->lineEdit_poly_hex->setText(hexStr);
    }
    else
    {
        hexPoly = ui->lineEdit_poly_hex->text().toULongLong(&isValid, 16);
        if (!isValid || hexPoly == 0)
        {
            ui->lineEdit_poly_math->setText("十六进制表达式输入错误!!!");
            return;
        }
        int32_t highestOrder = 0;
        uint64_t utmp64 = hexPoly;
        while (utmp64 >>= 1)
        {
            highestOrder++;
        }

        if (isSamplePoly)
        {
            if (highestOrder > maxOrder - 1)
            {
                ui->lineEdit_poly_math->setText("最大阶数和表达式不匹配!!!");
                return;
            }
            hexPoly |= 1ULL << maxOrder;
        }
        else
        {
            if (highestOrder != maxOrder)
            {
                ui->lineEdit_poly_math->setText("最大阶数和表达式不匹配!!!");
                return;
            }
        }
        binStr = QString::number(hexPoly, 2);

        for (int32_t i = 0; i < binStr.length(); i++)
        {
            if (binStr.at(i) == '1')
            {
                order = binStr.length() - 1 - i;
                orderList.append(order);
            }
        }
        if (orderList.isEmpty())
        {
            ui->lineEdit_poly_math->setText("十六进制表达式输入0!!!");
            return;
        }
        std::sort(orderList.rbegin(), orderList.rend());
        foreach (int32_t orderItem, orderList)
        {
            if (orderItem == 0)
                mathStr += "1";
            else
                mathStr += QString("x%1").arg(orderItem);
            mathStr += '+';
        }
        mathStr.chop(1); // 移除最后一个+
        ui->lineEdit_poly_math->setText(mathStr);
    }
}

void FormCRCConf::on_button_generateMeter_clicked()
{
    if (m_CRCMeterWidget == nullptr)
    {
        m_CRCMeterWidget = new QWidget(this);
        m_CRCMeterWidget->setWindowTitle("CRC表");
        m_CRCMeterWidget->setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint | Qt::WindowMaximizeButtonHint);
        m_CRCMeterWidget->resize(500, 400);
        m_CRCMeterBrowser = new QTextBrowser(m_CRCMeterWidget);
        m_CRCMeterBrowser->setReadOnly(true);
        m_CRCMeterBrowser->resize(500, 400);
        QVBoxLayout *layout = new QVBoxLayout(m_CRCMeterWidget);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_CRCMeterBrowser);
        m_CRCMeterWidget->setLayout(layout);
    }

    bool isValid = false;
    int8_t validCode = 0;
    QString str = "";
    do {
        m_crcConf.width = ui->spin_dataWidth->value();

        if (m_crcConf.width <= 8)       m_dataType = DataType_U08;
        else if (m_crcConf.width <= 16) m_dataType = DataType_U16;
        else if (m_crcConf.width <= 32) m_dataType = DataType_U32;
        else m_dataType = DataType_U08;

        m_crcConf.poly = ui->lineEdit_poly->text().toUInt(&isValid, 16);
        if (!isValid)
        {
            validCode = -1;
            break;
        }
        m_crcConf.init = ui->lineEdit_init->text().toUInt(&isValid, 16);
        if (!isValid)
        {
            validCode = -2;
            break;
        }
        m_crcConf.ref_in = ui->radio_refIn_reverse->isChecked();
        m_crcConf.ref_out = ui->radio_refOut_reverse->isChecked();
        m_crcConf.xor_out = ui->lineEdit_xorOut->text().toUInt(&isValid, 16);
        if (!isValid)
        {
            validCode = -3;
            break;
        }
    } while (0);

    if (validCode < 0)
    {
        str = FONT_COLOR_RED "<ERR> ";
        switch (validCode)
        {
        case -1: str += "CRC位宽(仅支持8-32位)"; break;
        case -2: str += "CRC poly"; break;
        case -3: str += "CRC init"; break;
        case -4: str += "CRC xorOut"; break;
        default: str += "CRC未知"; break;
        }
        str += "配置错误";
    }
    else
    {
        uint32_t crcCalc = m_crcConf.init & ((1ULL << m_crcConf.width) - 1); // 初始化位init
        QString formatStr = "", dataTypeStr = "";

        switch (m_dataType)
        {
        case DataType_U08: dataTypeStr = "uint8_t "; formatStr = "0x%02X, "; break;
        case DataType_U16: dataTypeStr = "uint16_t "; formatStr = "0x%04X, "; break;
        case DataType_U32: dataTypeStr = "uint32_t "; formatStr = "0x%08X, "; break;
        default: dataTypeStr = "uint8_t ";; formatStr = "0x%02X, "; break;
        }

        str = "// *****CRC分段查表法参考数组*****\n";
        str += QString::asprintf("// * .width = %d\n", m_crcConf.width);
        str += QString::asprintf("// * .poly = 0x%X\n", m_crcConf.poly);
        str += QString::asprintf("// * .init = 0x%X\n", m_crcConf.init);
        str += QString::asprintf("// * .ref_in = %s\n", m_crcConf.ref_in ? "true" : "false");
        str += QString::asprintf("// * .ref_out = %s\n", m_crcConf.ref_out ? "true" : "false");
        str += QString::asprintf("// * .xor_out = 0x%X\n", m_crcConf.xor_out);
        str += "// ****************************\n";

        str += dataTypeStr + "crcTable[256] = {\n";
        uint8_t singleByte = 0;
        for (uint32_t i = 0; i < 256; i++, singleByte++)
        {
            crcCalc = m_crcCalc.calcCRC(m_crcConf, &singleByte, 1);
            str += QString::asprintf(formatStr.toUtf8().constData(), crcCalc);
            if ((i + 1) % 8 == 0)
                str += "\n";
        }
        str += "}\n";

        str += "// *****CRC分段查表法参考代码*****\n";
        str += "// 1.此处定义通用的CRC配置结构体, 最好放入头文件\n";
        str += "typedef struct T_CRC_CONF\n";
        str += "{\n";
        str += "    uint8_t width;    // data width\n";
        str += "    uint32_t poly;    // poly\n";
        str += "    uint32_t init;    // init status\n";
        str += "    bool ref_in;      // input direction\n";
        str += "    bool ref_out;     // output direction\n";
        str += "    uint32_t xor_out; // xor output\n";
        str += "} t_crc_conf;\n";
        str += "// 2.此处定义上方生成的CRC表\n";
        str += dataTypeStr + "g_crcTable[256] = {...}\n";
        str += "// 3.此处为参考的C风格代码\n";
        str += "uint8_t refByte(uint8_t byte)\n";
        str += "{\n";
        str += "    byte = (byte & 0xF0) >> 4 | (byte & 0x0F) << 4;\n";
        str += "    byte = (byte & 0xCC) >> 2 | (byte & 0x33) << 2;\n";
        str += "    byte = (byte & 0xAA) >> 1 | (byte & 0x55) << 1;\n";
        str += "    return byte;\n";
        str += "}\n";
        str += dataTypeStr + "calcCRC(const t_crc_conf crcConf, const uint8_t *data, uint32_t len)\n";
        str += "{\n";
        str += "    if (data == NULL || len == 0) return 0;\n";
        str += "    " + dataTypeStr + "crc = crcConf.init & ((1ULL << crcConf.width) - 1);\n";
        str += "\n";
        str += "    " + dataTypeStr + "currByte = 0;\n";
        str += "    for (uint32_t i = 0; i < len; i++)\n";
        str += "    {\n";
        str += "        currByte = crcConf.ref_in ? refByte(data[i]) : data[i];\n";
        str += "        crc = (crc >> 8) ^ g_crcTable[(crc & 0xFF) ^ currByte];\n";
        str += "    }\n";
        str += "    crc = crcConf.ref_out ? refByte(crc) : crc;\n";
        str += "    crc ^= crcConf.xor_out;\n";
        str += "\n";
        str += "    return crc;\n";
        str += "}\n";
    }

    m_CRCMeterBrowser->setPlainText(str);
    m_CRCMeterWidget->show();
}
