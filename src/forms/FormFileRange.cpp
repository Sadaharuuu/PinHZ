#include "FormFileRange.h"
#include "ui_FormFileRange.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QSignalBlocker>
#include <limits>

namespace {
void setText(QLineEdit *edit, const QString &text) {
    const QSignalBlocker blocker(edit);
    edit->setText(text);
}
} // namespace

FormFileRange::FormFileRange(QWidget *parent) : QWidget(parent), ui(new Ui::FormFileRange) {
    ui->setupUi(this);
    connect(ui->button_source, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("选择源文件"), sourcePath(),
                                                          tr("所有文件 (*.*)"));
        if (!path.isEmpty())
            ui->edit_source->setText(path);
    });
    connect(ui->edit_source, &QLineEdit::textChanged, this, [this]() { updateRange(); });
    for (QLineEdit *edit : {ui->edit_start, ui->edit_end}) {
        connect(edit, &QLineEdit::textChanged, this, [this]() {
            m_lengthDriven = false;
            updateRange();
        });
    }
    for (QCheckBox *check : {ui->check_start, ui->check_end}) {
        connect(check, &QCheckBox::toggled, this, [this](bool) {
            m_lengthDriven = false;
            updateRange();
        });
    }
    connect(ui->edit_length, &QLineEdit::textChanged, this, [this]() {
        m_lengthDriven = true;
        // An edited length defines a custom end; the unchecked state always means file tail.
        const QSignalBlocker blocker(ui->check_end);
        ui->check_end->setChecked(true);
        updateRange();
    });
    connect(ui->check_hex, &QCheckBox::toggled, this, [this](bool hex) { changeBase(hex); });
    updateRange();
}

FormFileRange::~FormFileRange() {
    delete ui;
}
QString FormFileRange::sourcePath() const {
    return ui->edit_source->text().trimmed();
}

bool FormFileRange::inputRange(AppFileTools::Range &range, QString &error) const {
    error.clear();
    AppFileTools::Range resolved;
    if (!AppFileTools::parseNumber(ui->edit_start->text(), true, resolved.start)) {
        error = tr("起始地址无效，请输入十六进制非负整数。");
        return false;
    }
    const qint64 maximum = std::numeric_limits<qint64>::max();
    if (m_lengthDriven) {
        qint64 length = 0;
        if (!AppFileTools::parseNumber(ui->edit_length->text(), m_hex, length)) {
            error =
                tr("处理长度无效，请输入%1非负整数。").arg(m_hex ? tr("十六进制") : tr("十进制"));
            return false;
        }
        if (length > maximum - resolved.start) {
            error = tr("起始地址加处理长度超出 64 位整数范围。");
            return false;
        }
        resolved.endExclusive = resolved.start + length;
    } else {
        const QFileInfo file(sourcePath());
        if (!ui->check_end->isChecked() && file.isFile() && file.size() == 0) {
            resolved.endExclusive = 0;
        } else {
            qint64 end = 0;
            if (!AppFileTools::parseNumber(ui->edit_end->text(), true, end)) {
                error = ui->check_end->isChecked()
                            ? tr("结束地址无效，请输入十六进制非负整数。")
                            : tr("尚未选择源文件，可先设置自定义结束地址或处理长度。");
                return false;
            }
            if (end < resolved.start) {
                error = tr("结束地址必须大于等于起始地址（包含结束地址）。");
                return false;
            }
            if (end == maximum) {
                error = tr("结束地址超出可处理的 64 位文件范围。");
                return false;
            }
            resolved.endExclusive = end + 1;
        }
    }
    if (resolved.endExclusive < resolved.start) {
        error = tr("结束地址必须大于等于起始地址，空文件的起始偏移应为 0。");
        return false;
    }
    range = resolved;
    return true;
}

bool FormFileRange::validateFileRange(const AppFileTools::Range &range, QString &error) const {
    const QFileInfo file(sourcePath());
    if (!file.isFile())
        return true; // No file yet: defer file-size validation until selection.
    if (range.start > file.size() || range.endExclusive < range.start ||
        range.endExclusive > file.size()) {
        error = tr("处理范围超出文件大小（%1 字节），请调整起始地址、结束地址或处理长度。")
                    .arg(file.size());
        return false;
    }
    return true;
}

bool FormFileRange::selectedRange(AppFileTools::Range &range, QString &error) const {
    if (sourcePath().isEmpty() || !QFileInfo(sourcePath()).isFile()) {
        error = tr("请选择有效的源文件。");
        return false;
    }
    return inputRange(range, error) && validateFileRange(range, error);
}

void FormFileRange::updateRange() {
    ui->edit_start->setEnabled(ui->check_start->isChecked());
    ui->edit_end->setEnabled(ui->check_end->isChecked());
    ui->label_lengthPrefix->setVisible(m_hex);
    const QFileInfo file(sourcePath());
    const bool hasFile = file.isFile();
    ui->label_fileSize->setText(
        hasFile ? tr("文件大小：%1 字节（0x%2）").arg(file.size()).arg(file.size(), 0, 16)
                : tr("尚未选择有效文件"));
    if (!ui->check_start->isChecked())
        setText(ui->edit_start, "0");
    if (!ui->check_end->isChecked()) {
        setText(ui->edit_end, hasFile && file.size() > 0
                                  ? QString::number(file.size() - 1, 16).toUpper()
                                  : QString());
    }

    AppFileTools::Range range;
    QString error;
    if (inputRange(range, error)) {
        if (m_lengthDriven) {
            setText(ui->edit_end, range.length() == 0
                                      ? QString()
                                      : QString::number(range.endExclusive - 1, 16).toUpper());
        } else {
            setText(ui->edit_length, QString::number(range.length(), m_hex ? 16 : 10).toUpper());
        }
        if (!validateFileRange(range, error)) {
            ui->label_rangePreview->setText(error);
        } else {
            const QString preview =
                range.length() == 0
                    ? tr("空范围，起始偏移 0x%1，长度 0 字节，无结束地址。").arg(range.start, 0, 16)
                    : tr("处理范围 [0x%1, 0x%2]（含结束地址），长度 %3 字节（0x%4）。")
                          .arg(range.start, 0, 16)
                          .arg(range.endExclusive - 1, 0, 16)
                          .arg(range.length())
                          .arg(range.length(), 0, 16);
            ui->label_rangePreview->setText(
                preview + (hasFile ? QString() : tr(" 未选择源文件，暂不校验文件边界。")));
        }
    } else {
        // Do not leave a derived value from an earlier, valid input on screen.
        if (m_lengthDriven)
            setText(ui->edit_end, QString());
        else
            setText(ui->edit_length, QString());
        ui->label_rangePreview->setText(error);
    }
    emit rangeChanged();
}

void FormFileRange::changeBase(bool hex) {
    qint64 length = 0;
    if (AppFileTools::parseNumber(ui->edit_length->text(), m_hex, length))
        setText(ui->edit_length, QString::number(length, hex ? 16 : 10).toUpper());
    m_hex = hex;
    updateRange();
}
