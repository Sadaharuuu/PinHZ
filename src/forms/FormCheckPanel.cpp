#include "FormCheckPanel.h"
#include "FormCRCConf.h"
#include "ui_FormCheckPanel.h"
#include <QApplication>
#include <QClipboard>

FormCheckPanel::FormCheckPanel(QWidget *parent) : QWidget(parent), ui(new Ui::FormCheckPanel) {
    ui->setupUi(this);
    m_crcDialog = new FormCRCConf(this);
    AppCalcCRC crc;
    m_crcConf = crc.GetTemplate(CRC16_MODBUS);
    m_crcDialog->setCrcConf(m_crcConf);
    connect(ui->button_crcConf, &QPushButton::clicked, this, [this]() {
        m_crcDialog->setCrcConf(m_crcConf);
        m_crcDialog->show();
        m_crcDialog->raise();
        m_crcDialog->activateWindow();
    });
    connect(m_crcDialog, &FormCRCConf::CRCConfDone, this, [this](int8_t code) {
        const int width = m_crcDialog->m_crcConf.width;
        if (code < 0 || width < 4 || width > 32) {
            clearResult();
            setStatus(tr("CRC 配置无效，位宽请选择 4–32，并检查其他参数。"));
            return;
        }
        m_crcConf = m_crcDialog->m_crcConf;
        updateType();
    });
    connect(ui->combo_dataCheckType,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [this](int) { updateType(); });
    connect(ui->combo_dataCheckEndian,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [this](int) {
                clearResult();
                emit settingsChanged();
            });
    connect(ui->button_copyResult, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(ui->plain_dataCheckResult->toPlainText());
    });
    updateType();
}

FormCheckPanel::~FormCheckPanel() {
    delete ui;
}

AppCalcCheck::Settings FormCheckPanel::settings() const {
    AppCalcCheck::Settings settings;
    settings.type = static_cast<AppCalcCheck::Type>(ui->combo_dataCheckType->currentIndex());
    settings.crc = m_crcConf;
    settings.littleEndian = ui->combo_dataCheckEndian->currentIndex() == 1;
    return settings;
}

void FormCheckPanel::clearResult() {
    ui->plain_dataCheckResult->clear();
    ui->button_copyResult->setEnabled(false);
    setStatus(tr("输入或参数已更新，请点击“计算校验”。"));
}

void FormCheckPanel::setStatus(const QString &text) {
    ui->label_dataCheckStatus->setText(text);
}

void FormCheckPanel::showResult(const QByteArray &result, const QString &status) {
    // File digest strings can be pasted directly into common checksum comparison tools.
    const QByteArray hex =
        AppCalcCheck::isDigest(settings().type) ? result.toHex() : result.toHex(' ');
    ui->plain_dataCheckResult->setPlainText(QString::fromLatin1(hex.toUpper()));
    ui->button_copyResult->setEnabled(!result.isEmpty());
    setStatus(status);
}

void FormCheckPanel::updateType() {
    const auto type = settings().type;
    ui->button_crcConf->setVisible(type == AppCalcCheck::CRC);
    ui->label_crcInfo->setVisible(type == AppCalcCheck::CRC);
    ui->label_crcInfo->setText(tr("CRC-%1  Poly=0x%2  Init=0x%3  RefIn=%4  RefOut=%5  XorOut=0x%6")
                                   .arg(m_crcConf.width)
                                   .arg(m_crcConf.poly, 0, 16)
                                   .arg(m_crcConf.init, 0, 16)
                                   .arg(m_crcConf.ref_in)
                                   .arg(m_crcConf.ref_out)
                                   .arg(m_crcConf.xor_out, 0, 16));
    ui->combo_dataCheckEndian->setEnabled(!AppCalcCheck::isDigest(type) &&
                                          type != AppCalcCheck::Sum8 &&
                                          type != AppCalcCheck::Xor8 && type != AppCalcCheck::Lrc8);
    QString hint;
    if (AppCalcCheck::isDigest(type)) {
        hint = tr("摘要按标准顺序输出，不受大小端选项影响。");
        if (type == AppCalcCheck::Md4 || type == AppCalcCheck::Md5 || type == AppCalcCheck::Sha1)
            hint += tr("用于比对已有文件摘要；安全用途请使用 SHA-256 或 SHA-3。");
        if (type >= AppCalcCheck::Sha3_224 && type <= AppCalcCheck::Sha3_512)
            hint += tr("SHA-3 使用标准 FIPS 202 算法，与 Keccak 摘要不同。");
        if (type == AppCalcCheck::Blake2sp256)
            hint += tr("BLAKE2sp-256 是 WinRAR / RAR5 支持的文件摘要，按所选范围的原始字节计算。");
        else if (type == AppCalcCheck::Blake2s256 || type == AppCalcCheck::Blake2b512)
            hint += tr("BLAKE2s、BLAKE2b 与 BLAKE2sp 是不同算法，比较时请选择相同类型。");
        if (type >= AppCalcCheck::Xxh32 && type <= AppCalcCheck::Xxh3_128)
            hint += tr("xxHash 使用默认种子 0 和标准摘要顺序，适合快速比对文件内容。");
    } else if (type == AppCalcCheck::Adler32)
        hint = tr("Adler-32：初始值 1，模数 65521；用于 zlib 等文件数据校验。");
    else if (type == AppCalcCheck::Xor8)
        hint = tr("XOR-8 / BCC：初始值 0，逐字节异或，输出 1 字节。");
    else if (type == AppCalcCheck::Crc64Ecma)
        hint = tr("CRC-64/ECMA-182：Poly=42F0E1EBA9EA3693，Init=0，XorOut=0，不反转。");
    else if (type == AppCalcCheck::Crc64Xz)
        hint = tr("CRC-64/XZ：ECMA 多项式，Init/XorOut 全 1，输入输出反转，与 ECMA-182 不同。");
    else if (type == AppCalcCheck::Lrc8)
        hint = tr("LRC-8：逐字节相加后取二补码。Modbus ASCII 请输入解码后的字节，不含冒号、CR/LF "
                  "和校验字节。");
    else if (type == AppCalcCheck::Fletcher16)
        hint = tr("Fletcher-16：两个累加器初值 0，按字节累加并模 255，输出 (sum2 << 8) | sum1。");
    else if (type == AppCalcCheck::Internet16)
        hint = tr("Internet Checksum：按网络字节序合成 16 "
                  "位字，回卷进位后取反；奇数长度末尾补零。仅校验输入的字节。");
    else if (type == AppCalcCheck::CRC)
        hint = tr("自定义 CRC 可通过“CRC 配置”选择预设或修改参数。");
    else
        hint = tr("累加和按字节相加并保留低 8/16/32 位；");
    ui->label_algorithmHint->setText(hint);
    clearResult();
    emit settingsChanged();
}
