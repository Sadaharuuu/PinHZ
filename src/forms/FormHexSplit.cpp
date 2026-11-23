#include "FormHexSplit.h"
#include "ui_FormHexSplit.h"
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>

FormHexSplit::FormHexSplit(QWidget *parent) : QDialog(parent), ui(new Ui::FormHexSplit) {
    ui->setupUi(this);
    connect(ui->button_destination, &QPushButton::clicked, this, [this]() {
        const QString suggestion = ui->edit_destination->text().isEmpty()
                                       ? ui->fileRange->sourcePath() + ".split"
                                       : ui->edit_destination->text();
        const QString path = QFileDialog::getSaveFileName(this, tr("保存切分文件"), suggestion,
                                                          tr("所有文件 (*.*)"));
        if (!path.isEmpty())
            ui->edit_destination->setText(path);
    });
    connect(ui->fileRange, &FormFileRange::rangeChanged, this, [this]() {
        if (ui->edit_destination->text().isEmpty() && !ui->fileRange->sourcePath().isEmpty())
            ui->edit_destination->setText(ui->fileRange->sourcePath() + ".split");
        ui->label_status->setText(tr("范围已更新，请点击“开始切分”。"));
    });
    connect(ui->edit_destination, &QLineEdit::textChanged, this,
            [this]() { ui->label_status->setText(tr("保存路径已更新，请点击“开始切分”。")); });
    connect(ui->button_split, &QPushButton::clicked, this, [this]() { split(); });
    connect(ui->button_close, &QPushButton::clicked, this, &QDialog::close);
}

FormHexSplit::~FormHexSplit() {
    delete ui;
}

void FormHexSplit::split() {
    AppFileTools::Range range;
    QString error;
    if (!ui->fileRange->selectedRange(range, error)) {
        ui->label_status->setText(error);
        return;
    }
    const QString destination = ui->edit_destination->text().trimmed();
    if (destination.isEmpty()) {
        ui->label_status->setText(tr("请填写保存路径。"));
        return;
    }
    if (QFileInfo::exists(destination) &&
        QMessageBox::question(
            this, tr("覆盖目标文件"), tr("目标文件已存在，是否覆盖？\n%1").arg(destination),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    QProgressDialog progress(tr("正在切分文件…"), tr("取消"), 0, 1000, this);
    progress.setWindowTitle(tr("码流分割"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.setMinimumDuration(0);
    progress.show();
    const bool ok =
        AppFileTools::splitFile(ui->fileRange->sourcePath(), destination, range, error,
                                [&progress](qint64 done, qint64 total) {
                                    progress.setValue(total ? int(1000.0L * done / total) : 1000);
                                    QApplication::processEvents();
                                    return !progress.wasCanceled();
                                });
    progress.close();
    ui->label_status->setText(ok ? tr("切分完成：范围 %1，写入 %2 字节。\n保存至：%3")
                                       .arg(range.description())
                                       .arg(range.length())
                                       .arg(destination)
                                 : error);
}
