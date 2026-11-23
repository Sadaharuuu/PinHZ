#ifndef FORMDATALOG_H
#define FORMDATALOG_H

#include <QWidget>
#include <QFile>
#include <QTimer>
#include "CommonDefine.h"

namespace Ui {
class FormDataLog;
}

class FormDataLog : public QWidget {
    Q_OBJECT

public:
    explicit FormDataLog(QWidget *parent = 0, uint8_t modeCtrl = 0x11);
    ~FormDataLog();

    enum class SaveMode : int8_t { NotSave = 0, ASCII, OriginData };
    bool m_isLogModeChanged = false;
    bool m_isLog = false;
    SaveMode m_saveMode = SaveMode::NotSave;

    uint8_t m_recvMode = 0;
    uint8_t m_sendMode = 0;
    quint64 m_recvPkg = 0;
    quint64 m_sendPkg = 0;
    quint64 m_recvByte = 0;
    quint64 m_sendByte = 0;

    QString m_savePath = "";
    QFile m_saveFile;

    void setModeCtrl(uint8_t modeCtrl);
    bool exportLog(const QString &path, QString &error) const;
    bool selectSendFile(const QString &path, QString &error);
    bool startFileSend(const QString &path, QString &error);
    void fileChunkResult(qint64 accepted, const QString &error);
signals:
    void showLog(LogLevel level, QString string);
    void dataLog_send(QString dataStr);
    void fileChunkRequested(QByteArray bytes);
    void syncPinHZRequested();
public slots:
    void on_dataShow(QByteArray data, int32_t len, bool isSend, QString dataInfo);
    void on_button_clearLog_clicked();
    void on_updateDataCnt();
    void dataSendSync(QString dataStr);
private slots:
    void on_button_chooseSendFile_clicked();
    void on_button_manualSend_clicked();
    void on_button_cancelFile_clicked();
    void on_check_isLog_toggled(bool checked);
    void on_check_syncPinHZ_toggled(bool checked);
    void on_radio_dontShow_send_toggled(bool checked);
    void on_radio_ASCII_toggled(bool checked);
    void on_radio_hex_toggled(bool checked);
    void on_radio_ASCII_send_toggled(bool checked);
    void on_radio_hex_send_toggled(bool checked);
    void on_button_clearCnt_clicked();
    void on_check_saveAs_toggled(bool checked);
    void on_button_send_clicked();
    void on_button_findNext_clicked();
    void on_button_findPrevious_clicked();
    void on_button_exportLog_clicked();
    void on_check_pauseDisplay_toggled(bool checked);

private:
    QTimer m_fileSendTimer;
    QTimer m_fileRepeatTimer;
    bool m_fileTransferActive = false;
    quint64 m_fileRounds = 0;
    QByteArray m_fileSendBytes;
    int m_fileSendOffset = 0;
    QString m_selectedSendFile;
    QString m_manualSendText;
    bool m_resumeSync = false;
    void updateFileSummary();
    void stopFileSend(const QString &status);
    Ui::FormDataLog *ui;
    int m_savedSendMode = 0;
    quint64 m_pausedPackets = 0;
    bool m_displayTrimmed = false;
    void findText(bool backwards);
    void trimDisplay();
    void updateLogStatus();
    void stopRecording();
};

#endif // FormDataLog_H
