#include "FormFileCheck.h"
#include "ui_FormFileCheck.h"
#include <QApplication>
#include <QProgressDialog>

FormFileCheck::FormFileCheck(QWidget *parent) : QDialog(parent), ui(new Ui::FormFileCheck) {
    ui->setupUi(this);
    connect(ui->fileRange, &FormFileRange::rangeChanged, ui->checkPanel,
            &FormCheckPanel::clearResult);
    connect(ui->button_calculateCheck, &QPushButton::clicked, this, [this]() { calculateFile(); });
    connect(ui->button_close, &QPushButton::clicked, this, &QDialog::close);
}

FormFileCheck::~FormFileCheck() {
    delete ui;
}

void FormFileCheck::calculateFile() {
    ui->checkPanel->clearResult();
    AppFileTools::Range range;
    QString error;
    if (!ui->fileRange->selectedRange(range, error)) {
        ui->checkPanel->setStatus(error);
        return;
    }
    QProgressDialog progress(tr("正在计算文件校验…"), tr("取消"), 0, 1000, this);
    progress.setWindowTitle(tr("文件校验"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.setMinimumDuration(0);
    progress.show();
    QByteArray result;
    const bool ok = AppFileTools::checksumFile(
        ui->fileRange->sourcePath(), range, ui->checkPanel->settings(), result, error,
        [&progress](qint64 done, qint64 total) {
            progress.setValue(total ? int(1000.0L * done / total) : 1000);
            QApplication::processEvents();
            return !progress.wasCanceled();
        });
    progress.close();
    if (!ok) {
        ui->checkPanel->setStatus(error);
        return;
    }
    ui->checkPanel->showResult(result, tr("校验完成：范围 %1，处理 %2 字节，结果 %3 字节。")
                                           .arg(range.description())
                                           .arg(range.length())
                                           .arg(result.size()));
}
