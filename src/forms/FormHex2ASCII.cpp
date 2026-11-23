#include "AppCalcCheck.h"
#include <QHeaderView>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSaveFile>
#include <QFileInfo>
#include "FormHex2ASCII.h"
#include "FormPinHZ.h"
#include "ui_FormHex2ASCII.h"
#include <QFileDialog>

FormHex2ASCII::FormHex2ASCII(QWidget *parent) : QWidget(parent), ui(new Ui::FormHex2ASCII) {
    ui->setupUi(this);

    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    for (auto *table : {ui->table_ascii, ui->table_asciiRight}) {
        table->setColumnCount(3);
        table->setRowCount(64);
        table->setHorizontalHeaderLabels({"十进制", "HEX", "字符 / 控制符"});
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->verticalHeader()->hide();
    }
    auto *leftScroll = ui->table_ascii->verticalScrollBar();
    auto *rightScroll = ui->table_asciiRight->verticalScrollBar();
    connect(leftScroll, &QScrollBar::valueChanged, rightScroll, &QScrollBar::setValue);
    connect(rightScroll, &QScrollBar::valueChanged, leftScroll, &QScrollBar::setValue);

    const QStringList controls = {"NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
                                  "BS",  "HT",  "LF",  "VT",  "FF",  "CR",  "SO",  "SI",
                                  "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
                                  "CAN", "EM",  "SUB", "ESC", "FS",  "GS",  "RS",  "US"};
    for (int i = 0; i < 128; ++i) {
        const QString symbol = i < 32 ? controls[i] : i == 127 ? "DEL" :
                               i == 32 ? "SPACE" : QString(QChar(i));
        const QStringList cells = {QString::number(i),
                                   QString::number(i, 16).rightJustified(2, '0').toUpper(),
                                   symbol};
        auto *table = i < 64 ? ui->table_ascii : ui->table_asciiRight;
        for (int col = 0; col < cells.size(); ++col) {
            auto *cell = new QTableWidgetItem(cells[col]);
            cell->setTextAlignment(Qt::AlignCenter);
            table->setItem(i % 64, col, cell);
        }
    }

    ui->label_validation->hide();
    connect(ui->plain_hex, &QPlainTextEdit::textChanged, this, [this] { updateText(true); });
    connect(ui->plain_ascii, &QPlainTextEdit::textChanged, this, [this] { updateText(false); });
}

void FormHex2ASCII::updateText(bool fromHex) {
    auto *source = fromHex ? ui->plain_hex : ui->plain_ascii;
    auto *target = fromHex ? ui->plain_ascii : ui->plain_hex;
    const QString text = source->toPlainText();
    QString result, error;
    if (fromHex && !text.trimmed().isEmpty()) {
        QByteArray bytes;
        if (AppCalcCheck::parseHex(text, bytes, error)) {
            for (unsigned char byte : bytes) {
                if (byte > 127) {
                    error = tr("HEX 包含超出 ASCII 范围的字节（00–7F）。");
                    break;
                }
            }
            if (error.isEmpty())
                result = QString::fromLatin1(bytes);
        }
    } else if (!fromHex) {
        for (QChar character : text) {
            if (character.unicode() > 127) {
                error = tr("输入了非 ASCII 字符。");
                break;
            }
        }
        if (error.isEmpty())
            result = QString::fromLatin1(text.toLatin1().toHex(' ').toUpper());
    }
    const QSignalBlocker block(target);
    target->setPlainText(result);
    ui->label_validation->setText(error);
    ui->label_validation->setVisible(!error.isEmpty());
}

FormHex2ASCII::~FormHex2ASCII() {
    delete ui;
}

namespace {
bool convertFile(const QString &src, const QString &dst, bool toText) {
    if (QFileInfo(src).canonicalFilePath() == QFileInfo(dst).canonicalFilePath() &&
        QFileInfo(dst).exists())
        return false;
    QFile input(src);
    QSaveFile output(dst);
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly))
        return false;
    int high = -1;
    bool first = true;
    while (!input.atEnd()) {
        QByteArray block = input.read(1024 * 1024), converted;
        if (input.error() != QFile::NoError)
            return false;
        if (toText) {
            converted = block.toHex(' ').toUpper();
            if (!first)
                converted.prepend(' ');
            first = false;
        } else
            for (unsigned char c : block) {
                if (c == ' ' || c == '\r' || c == '\n' || c == '\t')
                    continue;
                int nibble = c >= '0' && c <= '9'   ? c - '0'
                             : c >= 'a' && c <= 'f' ? c - 'a' + 10
                             : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                                    : -1;
                if (nibble < 0)
                    return false;
                if (high < 0)
                    high = nibble;
                else {
                    converted.append(char(high * 16 + nibble));
                    high = -1;
                }
            }
        if (output.write(converted) != converted.size())
            return false;
    }
    return high < 0 && output.commit();
}
} // namespace
bool FormHex2ASCII::convertHex2ASCII(QString src, QString dst) {
    return convertFile(src, dst, true);
}
bool FormHex2ASCII::convertASCII2Hex(QString src, QString dst) {
    return convertFile(src, dst, false);
}

void FormHex2ASCII::on_button_confirm_clicked() {
    ConvertMode mode = (ConvertMode)ui->combo_convertMode->currentIndex();
    QString srcPath = "", dstPath = "";

    srcPath = QFileDialog::getOpenFileName(this, tr("选择源文件"), "", tr("源文件 (*.*)"));
    if (srcPath.isEmpty()) {
        emit showLog(LogLevel::ERR, "转换取消：未选择源文件");
        return;
    }
    if (!QFile::exists(srcPath)) {
        emit showLog(LogLevel::ERR, QString("转换失败：源文件不存在:%1").arg(srcPath));
        return;
    }

    dstPath = QFileDialog::getSaveFileName(this, tr("选择保存位置"), "dst.*", tr("目标文件 (*.*)"));
    if (dstPath.isEmpty()) {
        emit showLog(LogLevel::ERR, "转换取消：未选择保存位置");
        return;
    }

    if (mode == ConvertMode::hex2ASCII) {
        if (convertHex2ASCII(srcPath, dstPath)) {
            emit showLog(LogLevel::INF, "Hex2ASCII转换成功\n" + srcPath + "\n" + dstPath);
        } else {
            emit showLog(LogLevel::ERR, "Hex2ASCII转换失败\n" + srcPath + "\n" + dstPath);
        }
    } else if (mode == ConvertMode::ASCII2Hex) {
        if (convertASCII2Hex(srcPath, dstPath)) {
            emit showLog(LogLevel::INF, "ASCII2HEX转换成功\n" + srcPath + "\n" + dstPath);
        } else {
            emit showLog(LogLevel::ERR, "ASCII2HEX转换失败\n" + srcPath + "\n" + dstPath);
        }
    }

    emit hex2ASCIIDone(0xFF);
    close();
}
