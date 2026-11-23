#include "FormDataCheck.h"
#include "ui_FormDataCheck.h"

FormDataCheck::FormDataCheck(QWidget *parent) : QDialog(parent), ui(new Ui::FormDataCheck) {
    ui->setupUi(this);
    connect(ui->plain_dataCheckInput, &QPlainTextEdit::textChanged, ui->checkPanel,
            &FormCheckPanel::clearResult);
    connect(ui->button_calculateCheck, &QPushButton::clicked, this, [this]() { calculateInput(); });
    connect(ui->button_clear, &QPushButton::clicked, ui->plain_dataCheckInput,
            &QPlainTextEdit::clear);
    connect(ui->button_close, &QPushButton::clicked, this, &QDialog::close);
}

FormDataCheck::~FormDataCheck() {
    delete ui;
}

void FormDataCheck::calculateInput() {
    ui->checkPanel->clearResult();
    QByteArray data;
    QString error;
    if (!AppCalcCheck::parseHex(ui->plain_dataCheckInput->toPlainText(), data, error)) {
        ui->checkPanel->setStatus(error);
        return;
    }
    AppCalcCheck calculator(ui->checkPanel->settings());
    calculator.addData(data);
    const QByteArray result = calculator.result();
    ui->checkPanel->showResult(
        result, tr("计算完成：输入 %1 字节，结果 %2 字节。").arg(data.size()).arg(result.size()));
}
