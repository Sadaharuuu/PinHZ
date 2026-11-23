#include "CommonDefine.h"
#include "FormCRCConf.h"
#include "Hex2Dec.h"
#include "ui_FormCRCConf.h"
#include <QRegularExpression>

FormCRCConf::FormCRCConf(QWidget *parent) : QWidget(parent), ui(new Ui::FormCRCConf) {
    ui->setupUi(this);
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);

    setCrcConf(m_crcCalc.GetTemplate(static_cast<e_CRC_Temp>(ui->combo_template->currentIndex())));
}

FormCRCConf::~FormCRCConf() {
    delete ui;
    delete m_CRCMeterWidget;
}

void FormCRCConf::setCrcConf(const s_crc_conf &crcConf) {
    m_crcConf = crcConf;
    QString str = "";

    if (crcConf.width <= 8)
        m_dataType = DataType::U08;
    else if (crcConf.width <= 16)
        m_dataType = DataType::U16;
    else if (crcConf.width <= 32)
        m_dataType = DataType::U32;
    else
        m_dataType = DataType::U08;

    ui->spin_dataWidth->setValue(crcConf.width);
    str.sprintf("%X", crcConf.poly);
    ui->lineEdit_poly->setText(str);
    str.sprintf("%X", crcConf.init);
    ui->lineEdit_init->setText(str);
    ui->radio_refIn_reverse->setChecked(crcConf.ref_in);
    ui->radio_refIn->setChecked(!crcConf.ref_in);
    ui->radio_refOut_reverse->setChecked(crcConf.ref_out);
    ui->radio_refOut->setChecked(!crcConf.ref_out);
    str.sprintf("%X", crcConf.xor_out);
    ui->lineEdit_xorOut->setText(str);
}

void FormCRCConf::on_button_templateSet_clicked() {
    e_CRC_Temp tempID = (e_CRC_Temp)ui->combo_template->currentIndex();
    s_crc_conf crcConf = m_crcCalc.GetTemplate(tempID);
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

int8_t FormCRCConf::readConfig(s_crc_conf &conf) const {
    conf.width = ui->spin_dataWidth->value();
    if (conf.width < 4 || conf.width > 32)
        return -1;
    const uint32_t mask = uint32_t((uint64_t(1) << conf.width) - 1);
    bool valid = false;
    conf.poly = ui->lineEdit_poly->text().toUInt(&valid, 16);
    if (!valid || !conf.poly || conf.poly > mask)
        return -2;
    conf.init = ui->lineEdit_init->text().toUInt(&valid, 16);
    if (!valid || conf.init > mask)
        return -3;
    conf.xor_out = ui->lineEdit_xorOut->text().toUInt(&valid, 16);
    if (!valid || conf.xor_out > mask)
        return -4;
    conf.ref_in = ui->radio_refIn_reverse->isChecked();
    conf.ref_out = ui->radio_refOut_reverse->isChecked();
    return 0;
}

void FormCRCConf::on_button_confDone_clicked() {
    s_crc_conf conf = {};
    const int8_t code = readConfig(conf);
    if (code == 0) {
        setCrcConf(conf);
        ui->label_configStatus->clear();
    } else {
        ui->label_configStatus->setText(
            tr("配置无效：位宽须为 4–32；Poly 非零，且 Poly / Init / XorOut 不得超出位宽。"));
    }
    emit CRCConfDone(code);
    if (code == 0)
        close();
}

void FormCRCConf::on_button_convertPoly_clicked() {
    const bool simple = ui->radio_poly_simple->isChecked();
    if (ui->radio_math2hex->isChecked()) {
        uint64_t poly = 0;
        int highest = 0;
        const auto terms = ui->lineEdit_poly_math->text().toLower().split('+');
        const QRegularExpression power("^x\\^?([0-9]+)$");
        for (const QString &raw : terms) {
            const QString term = raw.trimmed();
            int order = -1;
            if (term == "1")
                order = 0;
            else if (term == "x")
                order = 1;
            else {
                const auto match = power.match(term);
                bool valid = false;
                if (match.hasMatch())
                    order = match.captured(1).toInt(&valid);
                if (!valid)
                    order = -1;
            }
            if (order < 0 || order > 32 || (poly & (uint64_t(1) << order))) {
                ui->lineEdit_poly_hex->setText(
                    tr("项无效或重复，请输入 x16+x12+x5+1（阶数 0–32）。"));
                return;
            }
            poly |= uint64_t(1) << order;
            highest = qMax(highest, order);
        }
        if (highest == 0) {
            ui->lineEdit_poly_hex->setText(tr("最高阶数须为 1–32。"));
            return;
        }
        ui->spin_maxOrder->setValue(highest);
        if (simple)
            poly &= ~(uint64_t(1) << highest);
        QString hex = QString::number(poly, 16).toUpper();
        if (simple && hex.size() % 2)
            hex.prepend('0');
        ui->lineEdit_poly_hex->setText(hex);
    } else {
        bool valid = false;
        uint64_t poly = ui->lineEdit_poly_hex->text().toULongLong(&valid, 16);
        const int highest = ui->spin_maxOrder->value();
        if (!valid || highest < 1 || highest > 32 ||
            (simple ? poly >= (uint64_t(1) << highest)
                    : poly < (uint64_t(1) << highest) || poly >= (uint64_t(1) << (highest + 1)))) {
            ui->lineEdit_poly_math->setText(tr("十六进制多项式与最大阶数不匹配。"));
            return;
        }
        if (simple)
            poly |= uint64_t(1) << highest;
        QStringList terms;
        for (int order = highest; order >= 0; --order)
            if (poly & (uint64_t(1) << order))
                terms.append(order ? QString("x%1").arg(order) : QString("1"));
        ui->lineEdit_poly_math->setText(terms.join('+'));
    }
}

QString FormCRCConf::tableCode(const s_crc_conf &conf) {
    if (conf.width < 4 || conf.width > 32)
        return QString();
    const uint32_t mask = uint32_t((uint64_t(1) << conf.width) - 1);
    if (!conf.poly || conf.poly > mask || conf.init > mask || conf.xor_out > mask)
        return QString();
    // The table contains only the polynomial remainder, never Init / RefOut / XorOut.
    // Sub-byte CRC registers are left-aligned in 8 bits while updating.
    const unsigned shift = conf.width < 8 ? 8 - conf.width : 0;
    const unsigned registerWidth = conf.width + shift;
    const uint32_t registerMask = uint32_t((uint64_t(1) << registerWidth) - 1);
    const QString type = conf.width <= 8 ? "uint8_t" : conf.width <= 16 ? "uint16_t" : "uint32_t";
    const auto hex = [](uint32_t value) { return QString("0x%1u").arg(value, 0, 16); };
    QString code =
        QString(
            "/* Fixed CRC parameters: width=%1 poly=%2 init=%3 refin=%4 refout=%5 xorout=%6\n"
            " * Table and function are a matching pair; regenerate both after changing "
            "parameters.\n"
            " * Check(ASCII 123456789) = %7. Empty input is supported.\n"
            " * Returns a numeric CRC; byte order for transmission is chosen by the caller. */\n")
            .arg(conf.width)
            .arg(hex(conf.poly))
            .arg(hex(conf.init))
            .arg(conf.ref_in ? "true" : "false")
            .arg(conf.ref_out ? "true" : "false")
            .arg(hex(conf.xor_out))
            .arg(
                hex(AppCalcCRC().calcCRC(conf, reinterpret_cast<const uint8_t *>("123456789"), 9)));
    code += "#include <stdint.h>\n#include <stddef.h>\n\n";
    code += "static const " + type + " g_crcTable[256] = {\n";
    for (unsigned i = 0; i < 256; ++i) {
        uint32_t value = i << (registerWidth - 8);
        for (unsigned bit = 0; bit < 8; ++bit)
            value = ((value << 1) ^
                     ((value & (uint32_t(1) << (registerWidth - 1))) ? conf.poly << shift : 0)) &
                    registerMask;
        code += hex(value) + (i == 255 ? "\n" : (i % 8 == 7 ? ",\n" : ", "));
    }
    code += "};\n\n";
    if (conf.ref_in || conf.ref_out) {
        code += "static uint32_t crc_reflect(uint32_t value, unsigned bits)\n{\n"
                "    uint32_t result = 0;\n"
                "    while (bits--) { result = (result << 1) | (value & 1u); value >>= 1; }\n"
                "    return result;\n}\n\n";
    }
    code += type + " calcCRC(const uint8_t *data, size_t len)\n{\n";
    code += "    uint32_t crc = " + hex(conf.init << shift) +
            ";\n"
            "    /* data may be NULL only when len == 0. */\n"
            "    if (data == NULL && len != 0) return 0;\n"
            "    for (size_t i = 0; i < len; ++i) {\n";
    code += conf.ref_in ? "        uint32_t byte = crc_reflect(data[i], 8);\n"
                        : "        uint32_t byte = data[i];\n";
    code +=
        QString(
            "        crc = ((crc << 8) ^ g_crcTable[((crc >> %1) ^ byte) & 0xffu]) & %2;\n    }\n")
            .arg(registerWidth - 8)
            .arg(hex(registerMask));
    if (shift)
        code += QString("    crc >>= %1;\n").arg(shift);
    if (conf.ref_out)
        code += QString("    crc = crc_reflect(crc, %1);\n").arg(conf.width);
    code +=
        "    return (" + type + ")((crc ^ " + hex(conf.xor_out) + ") & " + hex(mask) + ");\n}\n";
    return code;
}

void FormCRCConf::on_button_generateMeter_clicked() {
    if (!m_CRCMeterWidget) {
        m_CRCMeterWidget = new QWidget(this);
        m_CRCMeterWidget->setWindowTitle(tr("CRC 查表代码"));
        m_CRCMeterWidget->setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint |
                                         Qt::WindowMaximizeButtonHint);
        m_CRCMeterWidget->resize(760, 560);
        m_CRCMeterBrowser = new QTextBrowser(m_CRCMeterWidget);
        m_CRCMeterBrowser->setObjectName("crcGeneratedCode");
        m_CRCMeterBrowser->setReadOnly(true);
        QVBoxLayout *layout = new QVBoxLayout(m_CRCMeterWidget);
        layout->addWidget(m_CRCMeterBrowser);
    }
    s_crc_conf conf = {};
    const int8_t code = readConfig(conf);
    if (code < 0) {
        m_CRCMeterBrowser->setPlainText(
            tr("配置错误：位宽须为 4–32；Poly 非零，且 Poly / Init / XorOut 不得超出位宽。"));
    } else {
        m_CRCMeterBrowser->setPlainText(tableCode(conf));
    }
    m_CRCMeterWidget->show();
    m_CRCMeterWidget->raise();
}
