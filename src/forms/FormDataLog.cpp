#include "FormDataLog.h"
#include "AppTheme.h"
#include "ui_FormDataLog.h"
#include <QDateTime>
#include <QScrollBar>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QShortcut>
#include <QTextDocument>
#include <QRegularExpression>

namespace {
const int MaxLogCharacters = 1024 * 1024;
const int MaxLogBlocks = 10000;
QString payloadText(const QByteArray &data, bool ascii) {
    if (!ascii)
        return QString::fromLatin1(data.toHex(' ').toUpper());
    QString result;
    result.reserve(data.size());
    for (unsigned char byte : data) {
        if ((byte < 32 && byte != '\r' && byte != '\n' && byte != '\t') || byte == 127)
            result += "\\x" + QString::number(byte, 16).rightJustified(2, '0').toUpper();
        else
            result += QChar(byte);
    }
    return result;
}
QColor logColor(bool send) {
    static const QRegularExpression color("#[0-9a-fA-F]{6}");
    const QString html =
        AppTheme::adaptHtml(send ? FONT_COLOR_BLUEGREEN "x" : FONT_COLOR_DARK_ORANGE "x");
    return QColor(color.match(html).captured());
}
} // namespace

FormDataLog::FormDataLog(QWidget *parent, uint8_t modeCtrl)
    : QWidget(parent), ui(new Ui::FormDataLog) {
    ui->setupUi(this);
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    ui->txtBs_recvData->document()->setMaximumBlockCount(MaxLogBlocks);
    ui->txtBs_recvData->setOpenExternalLinks(false);
    auto *find = new QShortcut(QKeySequence::Find, this);
    find->setContext(Qt::WidgetWithChildrenShortcut);
    connect(find, &QShortcut::activated, this, [this] {
        ui->edit_logSearch->setFocus();
        ui->edit_logSearch->selectAll();
    });
    connect(ui->edit_logSearch, &QLineEdit::returnPressed, this,
            &FormDataLog::on_button_findNext_clicked);
    setModeCtrl(modeCtrl);
    on_updateDataCnt();
    updateLogStatus();
    m_fileSendTimer.setInterval(20);
    m_fileRepeatTimer.setSingleShot(true);
    connect(&m_fileRepeatTimer, &QTimer::timeout, this, [this] {
        m_fileSendOffset = 0;
        ui->progress_sendFile->setValue(0);
        ui->label_sendFileStatus->setText(tr("正在提交第 %1 轮。").arg(m_fileRounds + 1));
        m_fileSendTimer.start();
    });
    connect(&m_fileSendTimer, &QTimer::timeout, this,
            [this] { emit fileChunkRequested(m_fileSendBytes.mid(m_fileSendOffset, 1024)); });
}
FormDataLog::~FormDataLog() {
    delete ui;
}

void FormDataLog::setModeCtrl(uint8_t modeCtrl) {
    ui->radio_ASCII->setChecked(modeCtrl & 0x01);
    ui->radio_hex->setChecked(!(modeCtrl & 0x01));
    m_recvMode = modeCtrl & 0x01 ? 1 : 2;
    ui->check_isLog->setChecked(modeCtrl & 0x10);
    m_savedSendMode = modeCtrl & 0x04 ? 1 : modeCtrl & 0x08 ? 2 : 0;
    on_check_isLog_toggled(modeCtrl & 0x10);
}

void FormDataLog::on_dataShow(QByteArray data, int32_t len, bool isSend, QString dataInfo) {
    const int size = qBound(0, int(len), data.size());
    if (!size)
        return;
    data.truncate(size);
    const bool ascii = isSend ? ui->radio_ASCII_send->isChecked() : ui->radio_ASCII->isChecked();
    const QString header = QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss.zzz] ") +
                           (isSend ? "TX " : "RX ") + dataInfo + (ascii ? " ASCII>" : " HEX>");

    // Recording is independent of display pause, scrolling, and the bounded display cache.
    if (!isSend && m_saveMode != SaveMode::NotSave && m_saveFile.isOpen()) {
        const QByteArray record = m_saveMode == SaveMode::OriginData
                                      ? data
                                      : (header + "\n" + payloadText(data, ascii) + "\n").toUtf8();
        if (m_saveFile.write(record) != record.size() || !m_saveFile.flush()) {
            const QString error = m_saveFile.errorString();
            stopRecording();
            emit showLog(LogLevel::ERR, "接收记录写入失败，已停止记录：" + error.toHtmlEscaped());
        }
    }
    if (isSend && (!m_isLog || m_sendMode == 0))
        return;
    if (ui->check_pauseDisplay->isChecked()) {
        ++m_pausedPackets;
        updateLogStatus();
        return;
    }

    auto *browser = ui->txtBs_recvData;
    const int oldScroll = browser->verticalScrollBar()->value();
    QTextCursor cursor(browser->document());
    cursor.movePosition(QTextCursor::End);
    QTextCharFormat format;
    format.setForeground(browser->palette().color(QPalette::Text));
    if (m_isLogModeChanged) {
        if (!browser->document()->isEmpty())
            cursor.insertBlock();
        m_isLogModeChanged = false;
    }
    if (m_isLog) {
        if (!browser->document()->isEmpty() && !cursor.atBlockStart())
            cursor.insertBlock();
        cursor.insertText(header, format);
        cursor.insertBlock();
    }
    // Limit conversion work for unusually large individual packets; recording above remains
    // complete.
    const int maxBytes = ascii ? MaxLogCharacters / 4 : MaxLogCharacters / 3;
    if (data.size() > maxBytes) {
        data = data.right(maxBytes);
        m_displayTrimmed = true;
    }
    const QString text = payloadText(data, ascii);
    if (!m_isLog && !ascii && !browser->document()->isEmpty() && !cursor.atBlockStart())
        cursor.insertText(" ", format);
    format.setForeground(logColor(isSend));
    cursor.insertText(text, format); // Received text is always literal, never HTML.
    trimDisplay();
    if (ui->check_autoScroll->isChecked())
        browser->verticalScrollBar()->setValue(browser->verticalScrollBar()->maximum());
    else
        browser->verticalScrollBar()->setValue(oldScroll);
    updateLogStatus();
}

void FormDataLog::trimDisplay() {
    auto *document = ui->txtBs_recvData->document();
    if (document->blockCount() >= MaxLogBlocks)
        m_displayTrimmed = true;
    const int excess = document->characterCount() - 1 - MaxLogCharacters;
    if (excess > 0) {
        QTextCursor cursor(document);
        cursor.setPosition(0);
        cursor.setPosition(excess, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        m_displayTrimmed = true;
    }
}
void FormDataLog::updateLogStatus() {
    QString status = ui->check_pauseDisplay->isChecked()
                         ? QString("显示已暂停，已跳过 %1 次显示；接收、计数和文件记录继续。")
                               .arg(m_pausedPackets)
                         : "显示缓存：最多 10,000 行 / 1 Mi 字符；文件记录不受影响。";
    if (m_displayTrimmed)
        status += " 已裁去较早或超长内容。";
    ui->label_logStatus->setText(status);
}
void FormDataLog::on_check_pauseDisplay_toggled(bool checked) {
    if (checked)
        m_pausedPackets = 0;
    else if (m_pausedPackets) {
        QTextCursor cursor(ui->txtBs_recvData->document());
        cursor.movePosition(QTextCursor::End);
        cursor.insertBlock();
        cursor.insertText(QString("[恢复显示：暂停期间跳过 %1 次显示]\n").arg(m_pausedPackets));
        trimDisplay();
    }
    updateLogStatus();
}
void FormDataLog::on_updateDataCnt() {
    ui->lineEdit_pkgCnt->setText(QString("Pkg(R/S): %1/%2").arg(m_recvPkg).arg(m_sendPkg));
    ui->lineEdit_byteCnt->setText(QString("Bytes(R/S): %1/%2").arg(m_recvByte).arg(m_sendByte));
    ui->lineEdit_pkgCnt->setToolTip(ui->lineEdit_pkgCnt->text());
    ui->lineEdit_byteCnt->setToolTip(ui->lineEdit_byteCnt->text());
}
void FormDataLog::dataSendSync(QString dataStr) {
    if (m_selectedSendFile.isEmpty() && ui->check_syncPinHZ->isChecked())
        ui->plain_sendData->setPlainText(dataStr);
}
void FormDataLog::on_button_clearLog_clicked() {
    ui->txtBs_recvData->clear();
    m_isLogModeChanged = true;
    m_displayTrimmed = false;
    updateLogStatus();
}
void FormDataLog::on_button_send_clicked() {
    if (m_selectedSendFile.isEmpty()) {
        emit dataLog_send(ui->plain_sendData->toPlainText());
        return;
    }
    QString error;
    if (!startFileSend(m_selectedSendFile, error))
        ui->label_sendFileStatus->setText(error);
}

bool FormDataLog::startFileSend(const QString &path, QString &error) {
    error.clear();
    if (m_fileTransferActive) {
        error = tr("请先取消当前文件发送。");
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = file.errorString();
        return false;
    }
    if (file.size() > 16 * 1024 * 1024) {
        error = tr("文件发送当前支持最大 16 MiB。");
        return false;
    }
    QByteArray bytes = file.read(16 * 1024 * 1024 + 1);
    if (bytes.size() > 16 * 1024 * 1024) {
        error = tr("文件发送当前支持最大 16 MiB。");
        return false;
    }
    if (file.error() != QFile::NoError) {
        error = file.errorString();
        return false;
    }
    if (bytes.isEmpty()) {
        error = tr("文件为空。");
        return false;
    }
    m_fileSendBytes = bytes;
    m_fileTransferActive = true;
    m_fileRounds = 0;
    ui->check_fileLoop->setEnabled(false);
    ui->spin_fileInterval->setEnabled(false);
    m_fileSendOffset = 0;
    ui->progress_sendFile->setRange(0, bytes.size());
    ui->progress_sendFile->setValue(0);
    ui->button_send->setEnabled(false);
    ui->button_chooseSendFile->setEnabled(false);
    ui->button_manualSend->setEnabled(false);
    ui->button_cancelFile->setEnabled(true);
    ui->label_sendFileStatus->setText(tr("正在分块提交发送，UDP 每块为独立数据报。"));
    m_fileSendTimer.start();
    return true;
}
void FormDataLog::fileChunkResult(qint64 accepted, const QString &error) {
    if (!m_fileTransferActive || !m_fileSendTimer.isActive())
        return;
    if (!error.isEmpty()) {
        stopFileSend(tr("发送停止：%1").arg(error));
        return;
    }
    m_fileSendOffset += int(
        qBound(qint64(0), accepted, qint64(qMin(1024, m_fileSendBytes.size() - m_fileSendOffset))));
    ui->progress_sendFile->setValue(m_fileSendOffset);
    if (m_fileSendOffset == m_fileSendBytes.size()) {
        ++m_fileRounds;
        if (ui->check_fileLoop->isChecked()) {
            m_fileSendTimer.stop();
            ui->label_sendFileStatus->setText(tr("已提交 %1 轮，等待下一轮。").arg(m_fileRounds));
            m_fileRepeatTimer.start(ui->spin_fileInterval->value());
        } else
            stopFileSend(tr("已全部提交到发送缓冲区（不代表对端已接收）。"));
    }
}
void FormDataLog::stopFileSend(const QString &status) {
    m_fileSendTimer.stop();
    m_fileSendBytes.clear();
    m_fileRepeatTimer.stop();
    m_fileTransferActive = false;
    ui->check_fileLoop->setEnabled(true);
    ui->spin_fileInterval->setEnabled(true);
    ui->button_send->setEnabled(true);
    ui->button_cancelFile->setEnabled(false);
    ui->button_chooseSendFile->setEnabled(true);
    ui->button_manualSend->setEnabled(!m_selectedSendFile.isEmpty());
    ui->label_sendFileStatus->setText(status);
}
bool FormDataLog::selectSendFile(const QString &path, QString &error) {
    error.clear();
    if (m_fileTransferActive) {
        error = tr("请先停止文件发送。");
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = file.errorString();
        return false;
    }
    if (file.size() <= 0 || file.size() > 16 * 1024 * 1024) {
        error = tr("请选择非空且不超过 16 MiB 的文件。");
        return false;
    }
    if (m_selectedSendFile.isEmpty()) {
        m_manualSendText = ui->plain_sendData->toPlainText();
        m_resumeSync = ui->check_syncPinHZ->isChecked();
    }
    m_selectedSendFile = QFileInfo(path).absoluteFilePath();
    ui->check_syncPinHZ->setChecked(false);
    ui->check_syncPinHZ->setEnabled(false);
    ui->plain_sendData->setReadOnly(true);
    ui->plain_sendData->setMinimumHeight(200);
    ui->button_manualSend->setEnabled(true);
    ui->button_send->setText(tr("发送文件"));
    ui->progress_sendFile->setValue(0);
    ui->label_sendFileStatus->setText(tr("文件已选择，点击右侧发送。"));
    updateFileSummary();
    return true;
}
void FormDataLog::updateFileSummary() {
    if (m_selectedSendFile.isEmpty())
        return;
    const QFileInfo info(m_selectedSendFile);
    QFile file(m_selectedSendFile);
    QByteArray preview;
    if (file.open(QIODevice::ReadOnly))
        preview = file.read(64);
    const bool ascii = ui->radio_ASCII_send->isChecked();
    ui->plain_sendData->setPlainText(
        tr("文件：%1\n路径：%2\n大小：%3 字节\n预览：%4（前 64 "
           "字节）\n\n%5\n\n按原始字节发送，不添加头尾或换行。")
            .arg(info.fileName(), QDir::toNativeSeparators(info.absoluteFilePath()))
            .arg(info.size())
            .arg(ascii ? "ASCII" : "HEX")
            .arg(payloadText(preview, ascii)));
}
void FormDataLog::on_button_chooseSendFile_clicked() {
    const QString path = QFileDialog::getOpenFileName(this, tr("选择发送文件"));
    if (path.isEmpty())
        return;
    QString error;
    if (!selectSendFile(path, error))
        ui->label_sendFileStatus->setText(error);
}
void FormDataLog::on_button_manualSend_clicked() {
    if (m_fileTransferActive)
        return;
    m_selectedSendFile.clear();
    ui->plain_sendData->setReadOnly(false);
    ui->plain_sendData->setMinimumHeight(96);
    ui->plain_sendData->setPlainText(m_manualSendText);
    ui->button_manualSend->setEnabled(false);
    ui->button_send->setText(tr("发送"));
    ui->check_syncPinHZ->setEnabled(true);
    ui->check_syncPinHZ->setChecked(m_resumeSync);
    ui->progress_sendFile->setValue(0);
    ui->label_sendFileStatus->setText(tr("已返回手动输入。"));
}
void FormDataLog::on_button_cancelFile_clicked() {
    stopFileSend(tr("已取消；已提交的字节无法撤回。"));
}
void FormDataLog::on_check_syncPinHZ_toggled(bool checked) {
    if (checked)
        emit syncPinHZRequested();
}
void FormDataLog::on_button_clearCnt_clicked() {
    m_recvPkg = m_recvByte = m_sendPkg = m_sendByte = 0;
    on_updateDataCnt();
}
void FormDataLog::on_check_isLog_toggled(bool checked) {
    if (!checked && m_isLog)
        m_savedSendMode = m_sendMode;
    const int mode = checked ? m_savedSendMode : 0;
    ui->radio_dontShow_send->setChecked(mode == 0);
    ui->radio_ASCII_send->setChecked(mode == 1);
    ui->radio_hex_send->setChecked(mode == 2);
    ui->radio_dontShow_send->setEnabled(checked);
    ui->radio_ASCII_send->setEnabled(checked);
    ui->radio_hex_send->setEnabled(checked);
    m_sendMode = mode;
    m_isLogModeChanged = true;
    m_isLog = checked;
}
void FormDataLog::on_radio_ASCII_toggled(bool checked) {
    if (checked)
        m_recvMode = 1;
}
void FormDataLog::on_radio_hex_toggled(bool checked) {
    if (checked)
        m_recvMode = 2;
}
void FormDataLog::on_radio_dontShow_send_toggled(bool checked) {
    if (checked)
        m_sendMode = 0;
}
void FormDataLog::on_radio_ASCII_send_toggled(bool checked) {
    if (checked) {
        m_sendMode = 1;
        updateFileSummary();
    }
}
void FormDataLog::on_radio_hex_send_toggled(bool checked) {
    if (checked) {
        m_sendMode = 2;
        updateFileSummary();
    }
}

void FormDataLog::stopRecording() {
    m_saveFile.close();
    m_saveMode = SaveMode::NotSave;
    m_savePath.clear();
    const QSignalBlocker block(ui->check_saveAs);
    ui->check_saveAs->setChecked(false);
}
void FormDataLog::on_check_saveAs_toggled(bool checked) {
    if (!checked) {
        stopRecording();
        return;
    }
    QString filter;
    const QString path = QFileDialog::getSaveFileName(
        this, "保存接收记录", "", "文本日志 UTF-8 (*.txt);;原始字节 (*.bin *.hex);;所有文件 (*.*)",
        &filter);
    if (path.isEmpty()) {
        stopRecording();
        return;
    }
    m_saveFile.setFileName(path);
    if (!m_saveFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString error = m_saveFile.errorString();
        stopRecording();
        emit showLog(LogLevel::ERR, "无法打开接收记录文件：" + error.toHtmlEscaped());
        return;
    }
    m_savePath = path;
    m_saveMode = filter.startsWith("文本日志") ? SaveMode::ASCII : SaveMode::OriginData;
}
bool FormDataLog::exportLog(const QString &path, QString &error) const {
    if (m_saveFile.isOpen() && QFileInfo(path) == QFileInfo(m_saveFile.fileName())) {
        error = "不能覆盖正在写入的接收记录，请选择另一个文件";
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        error = file.errorString();
        return false;
    }
    const QByteArray text = ui->txtBs_recvData->toPlainText().toUtf8();
    if (file.write(text) != text.size() || !file.commit()) {
        error = file.errorString();
        return false;
    }
    error.clear();
    return true;
}
void FormDataLog::on_button_exportLog_clicked() {
    const QString path = QFileDialog::getSaveFileName(this, "导出当前显示日志", "PinHZ_Log.txt",
                                                      "文本日志 UTF-8 (*.txt)");
    if (path.isEmpty())
        return;
    QString error;
    if (!exportLog(path, error))
        emit showLog(LogLevel::ERR, "日志导出失败：" + error.toHtmlEscaped());
    else
        emit showLog(LogLevel::INF, "当前显示日志已导出：" + path.toHtmlEscaped());
}
void FormDataLog::findText(bool backwards) {
    const QString term = ui->edit_logSearch->text();
    if (term.isEmpty()) {
        ui->edit_logSearch->setFocus();
        return;
    }
    auto *browser = ui->txtBs_recvData;
    const QTextCursor original = browser->textCursor();
    const auto flags = backwards ? QTextDocument::FindBackward : QTextDocument::FindFlags();
    bool found = browser->find(term, flags);
    if (!found) {
        QTextCursor cursor(browser->document());
        cursor.movePosition(backwards ? QTextCursor::End : QTextCursor::Start);
        browser->setTextCursor(cursor);
        found = browser->find(term, flags);
    }
    if (!found)
        browser->setTextCursor(original);
    ui->label_logStatus->setText(found ? "已定位匹配项（忽略大小写，首尾循环查找）。"
                                       : "未找到匹配内容。");
}
void FormDataLog::on_button_findNext_clicked() {
    findText(false);
}
void FormDataLog::on_button_findPrevious_clicked() {
    findText(true);
}
