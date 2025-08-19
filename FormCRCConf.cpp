#include "FormCRCConf.h"
#include "ui_FormCRCConf.h"
#include "Hex2Dec.h"

FormCRCConf::FormCRCConf(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormCRCConf)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    m_dataType = 0;

    on_button_templateSet_clicked();
}

FormCRCConf::~FormCRCConf()
{
    delete ui;
}

void FormCRCConf::on_button_templateSet_clicked()
{
    e_CRC_Temp tempID = (e_CRC_Temp)ui->combo_template->currentIndex();
    t_crc_conf crcConf = m_crcCalc.GetTemplate(tempID);
    QString str = "";

    ui->spinBox_dataWidth->setValue(crcConf.width);
    str.sprintf("%x", crcConf.poly);
    ui->lineEdit_poly->setText(str);
    str.sprintf("%x", crcConf.init);
    ui->lineEdit_init->setText(str);
    if (crcConf.ref_in)
        ui->radio_refIn->setChecked(true);
    else
        ui->radio_refIn_reverse->setChecked(true);
    if (crcConf.ref_out)
        ui->radio_refOut->setChecked(true);
    else
        ui->radio_refOut_reverse->setChecked(true);
    str.sprintf("%x", crcConf.xor_out);
    ui->lineEdit_xorOut->setText(str);
}

void FormCRCConf::on_button_confDone_clicked()
{
    bool isValid = false;
    int8_t validCode = 0;
    do {
        m_crcConf.width = ui->spinBox_dataWidth->value();

        if (m_crcConf.width <= 8)
            m_dataType = 0; // uint8
        else if (m_crcConf.width <= 16)
            m_dataType = 1; // uint16
        else if (m_crcConf.width <= 32)
            m_dataType = 2; // uint32
        else
            m_dataType = 0; // uint8

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
        m_crcConf.ref_in = ui->radio_refIn->isChecked();
        m_crcConf.ref_out = ui->radio_refOut->isChecked();
        m_crcConf.xor_out = ui->lineEdit_xorOut->text().toUInt(&isValid, 16);
        if (!isValid)
        {
            validCode = -3;
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
    int32_t order, maxOrder = ui->spinBox_maxOrder->value();;
    uint64_t hexPoly = 0;

    if (isMath2Hex)
    {
        QStringList mathList = ui->lineEdit_poly_math->text().split('+');
        foreach (QString mathStr, mathList)
        {
            QString numStr = mathStr.mid(1);
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
        ui->spinBox_maxOrder->setValue(highestOrder);

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
        foreach (order, orderList)
        {
            if (order == 0)
                mathStr += "x0";
            else
                mathStr += QString("x%1").arg(order);
            mathStr += '+';
        }
        mathStr.chop(1); // 移除最后一个+
        ui->lineEdit_poly_math->setText(mathStr);
    }
}
