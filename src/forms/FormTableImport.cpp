#include "FormTableImport.h"
#include "AppItemValue.h"
#include "ui_FormTableImport.h"
#include <QApplication>
#include <QClipboard>
#include <QTextTable>
#include <QHeaderView>
#include <QPushButton>
#include <QSignalBlocker>
#include <QRegularExpression>
#include <QSet>

bool TablePasteEdit::clipboardText(const QMimeData *mime, QString &text, QString &error) {
    text.clear();
    error.clear();
    if (mime->hasHtml()) {
        if (mime->html().size() > 4 * 1024 * 1024) {
            error = "剪贴板 HTML 过大。";
            return false;
        }
        QTextDocument document;
        document.setHtml(mime->html());
        QList<QTextTable *> tables;
        for (auto it = document.rootFrame()->begin(); !it.atEnd(); ++it)
            if (auto *table = qobject_cast<QTextTable *>(it.currentFrame()))
                tables.append(table);
        if (tables.size() > 1) {
            error = "请一次只复制一张表格。";
            return false;
        }
        if (!tables.isEmpty()) {
            auto *table = tables.first();
            if (table->rows() > 1001 || table->columns() > 32) {
                error = "表格超过 1000 行或 32 列。";
                return false;
            }
            QStringList lines;
            for (int row = 0; row < table->rows(); ++row) {
                QStringList cells;
                for (int col = 0; col < table->columns(); ++col) {
                    const auto cell = table->cellAt(row, col);
                    if (cell.rowSpan() != 1 || cell.columnSpan() != 1) {
                        error = "存在合并单元格，请先拆分后复制。";
                        return false;
                    }
                    for (auto it = cell.begin(); !it.atEnd(); ++it)
                        if (it.currentFrame()) {
                            error = "不支持嵌套表格。";
                            return false;
                        }
                    QTextCursor cursor = cell.firstCursorPosition();
                    cursor.setPosition(cell.lastCursorPosition().position(),
                                       QTextCursor::KeepAnchor);
                    QString value = cursor.selectedText();
                    value.replace(QChar(0x2029), ' ');
                    value.replace('\t', ' ');
                    value.replace('\n', ' ');
                    cells.append(value.trimmed());
                }
                lines.append(cells.join('\t'));
            }
            text = lines.join('\n');
            return true;
        }
    }
    text = mime->text();
    if (text.isEmpty()) {
        error = "剪贴板没有可读取的表格文本。";
        return false;
    }
    if (text.size() > 1024 * 1024) {
        error = "输入超过 1 Mi 字符。";
        return false;
    }
    return true;
}
void TablePasteEdit::insertFromMimeData(const QMimeData *mime) {
    QString text, error;
    if (clipboardText(mime, text, error))
        insertPlainText(text);
    else
        emit pasteError(error);
}

bool FormTableImport::parseGrid(QString text, QList<QStringList> &rows, QString &error) {
    rows.clear();
    error.clear();
    if (text.size() > 1024 * 1024) {
        error = "输入超过 1 Mi 字符。";
        return false;
    }
    text.replace("\r\n", "\n");
    text.replace('\r', '\n');
    if (text.startsWith(QChar(0xfeff)))
        text.remove(0, 1);
    const bool tsv = text.contains('\t');
    for (QString line : text.split('\n')) {
        if (line.trimmed().isEmpty())
            continue;
        QStringList cells;
        if (tsv)
            cells = line.split('\t', QString::KeepEmptyParts);
        else {
            line = line.trimmed();
            if (line.startsWith('|'))
                line.remove(0, 1);
            if (line.endsWith('|') && !line.endsWith("\\|"))
                line.chop(1);
            QString cell;
            for (int i = 0; i < line.size(); ++i) {
                if (line[i] == '\\' && i + 1 < line.size() && line[i + 1] == '|') {
                    cell += '|';
                    ++i;
                } else if (line[i] == '|') {
                    cells.append(cell.trimmed());
                    cell.clear();
                } else
                    cell += line[i];
            }
            cells.append(cell.trimmed());
        }
        for (QString &cell : cells)
            cell = cell.trimmed();
        bool separator = !tsv && rows.size() == 1;
        for (const QString &cell : cells)
            separator = separator && QRegularExpression("^:?-{3,}:?$").match(cell).hasMatch();
        if (separator && cells.size() == rows.first().size())
            continue;
        if (cells.size() > 32 || (!rows.isEmpty() && cells.size() != rows.first().size())) {
            error = QString("第 %1 行列数不一致或超过 32 列，请检查分隔符。").arg(rows.size() + 1);
            rows.clear();
            return false;
        }
        rows.append(cells);
        if (rows.size() > 1001) {
            error = "最多导入 1000 行数据。";
            rows.clear();
            return false;
        }
    }
    if (rows.isEmpty()) {
        error = "没有可解析的表格。";
        return false;
    }
    return true;
}

namespace {
enum PreviewColumn { InitialValueColumn = 4, StatusColumn };
}

FormTableImport::FormTableImport(Endian endian, QWidget *parent)
    : QDialog(parent), ui(new Ui::FormTableImport), m_endian(endian) {
    ui->setupUi(this);
    ui->buttons->button(QDialogButtonBox::Ok)->setText("追加导入");
    ui->buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    ui->buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    connect(ui->buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(ui->buttons, &QDialogButtonBox::accepted, this, [this] {
        validatePreview();
        if (!m_items.isEmpty())
            accept();
    });
    connect(ui->button_parse, &QPushButton::clicked, this, &FormTableImport::parse);
    connect(ui->table_preview, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (item->column() == InitialValueColumn)
            validatePreview();
    });
    connect(ui->button_paste, &QPushButton::clicked, this, [this] { ui->edit_source->paste(); });
    connect(ui->edit_source, &TablePasteEdit::pasteError, ui->label_status, &QLabel::setText);
    connect(ui->edit_source, &QTextEdit::textChanged, this, [this] {
        m_rows.clear();
        m_previewItems.clear();
        m_items.clear();
        ui->table_preview->setRowCount(0);
        ui->buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
        ui->label_status->setText("输入已更新，请重新解析。");
    });
    connect(ui->check_header, &QCheckBox::toggled, this, &FormTableImport::parse);
    for (auto *map : {ui->mapType, ui->mapComment})
        connect(map, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
                [this] { preview(); });
}
FormTableImport::~FormTableImport() {
    delete ui;
}
void FormTableImport::parse() {
    QString error;
    m_previewItems.clear();
    m_items.clear();
    ui->table_preview->setRowCount(0);
    ui->buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    if (!parseGrid(ui->edit_source->toPlainText(), m_rows, error)) {
        ui->label_status->setText(error);
        return;
    }
    const QStringList headers = m_rows.first();
    const QList<QComboBox *> maps{ui->mapType, ui->mapComment};
    const QList<QStringList> names{{"数据类型", "字段类型", "类型", "type", "datatype"},
                                   {"字段说明", "说明", "comment", "描述", "字段名", "名称"}};
    for (int m = 0; m < maps.size(); ++m) {
        const QSignalBlocker block(maps[m]);
        maps[m]->clear();
        maps[m]->addItem("不使用", -1);
        for (int col = 0; col < headers.size(); ++col) {
            maps[m]->addItem(QString("第 %1 列：%2").arg(col + 1).arg(headers[col].left(60)), col);
            QString name = headers[col].toLower();
            name.remove(' ');
            if (ui->check_header->isChecked() && names[m].contains(name))
                maps[m]->setCurrentIndex(col + 1);
        }
    }
    preview();
}
void FormTableImport::preview() {
    const QSignalBlocker block(ui->table_preview);
    m_previewItems.clear();
    m_items.clear();
    ui->buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    if (m_rows.isEmpty())
        return;
    QList<QComboBox *> maps{ui->mapType, ui->mapComment};
    QSet<int> used;
    for (auto *map : maps)
        if (map->currentData().toInt() >= 0) {
            int col = map->currentData().toInt();
            if (used.contains(col)) {
                ui->label_status->setText("同一列不能映射到多个字段。");
                return;
            }
            used.insert(col);
        }
    if (ui->mapType->currentIndex() <= 0) {
        ui->label_status->setText("请选择数据类型列。");
        return;
    }
    ui->table_preview->setColumnCount(StatusColumn + 1);
    ui->table_preview->setHorizontalHeaderLabels(
        {"源行", "区域", "数据类型", "字段说明", "初始值（数值）", "检查结果"});
    ui->table_preview->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    const int start = ui->check_header->isChecked() ? 1 : 0;
    ui->table_preview->setRowCount(m_rows.size() - start);
    for (int row = start; row < m_rows.size(); ++row) {
        auto value = [&](QComboBox *map) {
            int col = map->currentData().toInt();
            return col >= 0 ? m_rows[row].value(col) : QString();
        };
        s_itemUnit entry = {};
        entry.itemType = ItemType::Text;
        entry.itemZone = ItemZone::Data;
        entry.comment = value(ui->mapComment);
        QString error, type = value(ui->mapType).toLower();
        if (ui->check_header->isChecked()) {
            const QStringList unsupported{"公式",    "正向公式",  "反向公式",
                                          "formula", "ioformula", "reverseformula",
                                          "选项",    "combo",     "crc参数"};
            for (int col = 0; col < m_rows.first().size(); ++col) {
                QString heading = m_rows.first()[col].trimmed().toLower();
                heading.remove(' ');
                if (unsupported.contains(heading) && !m_rows[row][col].isEmpty())
                    error =
                        "当前不支持导入公式、combo 或 CRC 参数，请移除这些内容后在条目设置中配置";
            }
        }
        const QStringList types{"uint8", "int8",   "uint16", "int16", "uint32",
                                "int32", "uint64", "int64",  "float", "double"};
        const QStringList aliases{"u08", "s08", "u16", "s16", "u32",
                                  "s32", "u64", "s64", "f32", "f64"};
        if (type.endsWith("_t"))
            type.chop(2);
        type.remove(QRegularExpression("\\s+"));
        const QMap<QString, QString> cTypes{{"unsignedchar", "uint8"},
                                            {"signedchar", "int8"},
                                            {"char", "int8"},
                                            {"unsignedshort", "uint16"},
                                            {"unsignedshortint", "uint16"},
                                            {"short", "int16"},
                                            {"shortint", "int16"},
                                            {"unsignedint", "uint32"},
                                            {"signedint", "int32"},
                                            {"int", "int32"},
                                            {"unsignedlonglong", "uint64"},
                                            {"longlong", "int64"},
                                            {"u8", "uint8"},
                                            {"s8", "int8"},
                                            {"i8", "int8"},
                                            {"i16", "int16"},
                                            {"i32", "int32"},
                                            {"i64", "int64"}};
        type = cTypes.value(type, type);
        if (type == "long" || type == "unsignedlong")
            error = "long 位宽与平台有关，请明确填写 int32/uint32 或 int64/uint64";
        int index = types.indexOf(type);
        if (index < 0)
            index = aliases.indexOf(type);
        if (index < 0)
            error = "未知数据类型；请选择 uint8/int16/float 等普通类型";
        else
            entry.dataType = static_cast<DataType>(index);
        const auto *previous = ui->table_preview->item(row - start, InitialValueColumn);
        const QString initial = previous ? previous->text() : QString("0");
        const QStringList cells{QString::number(row + 1), "Data", type, entry.comment, initial,
                                error};
        for (int col = 0; col < cells.size(); ++col) {
            auto *cell = new QTableWidgetItem(cells[col]);
            if (col != InitialValueColumn || !error.isEmpty())
                cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
            if (col == StatusColumn)
                cell->setData(Qt::UserRole, error);
            ui->table_preview->setItem(row - start, col, cell);
        }
        m_previewItems.append(entry);
    }
    validatePreview();
}

void FormTableImport::validatePreview() {
    const QSignalBlocker block(ui->table_preview);
    m_items.clear();
    bool valid = !m_previewItems.isEmpty() && m_previewItems.size() <= 1000;
    for (int row = 0; row < m_previewItems.size(); ++row) {
        auto entry = m_previewItems[row];
        auto *status = ui->table_preview->item(row, StatusColumn);
        QString error = status->data(Qt::UserRole).toString();
        if (error.isEmpty()) {
            const QString initial = ui->table_preview->item(row, InitialValueColumn)->text();
            if (AppItemValue::fromDisplay(entry, initial, m_endian, entry.valueStr, error))
                m_items.append(entry);
        }
        status->setText(error.isEmpty() ? QString("通过") : error);
        status->setForeground(error.isEmpty() ? QBrush() : QBrush(QColor("#dc656d")));
        valid = valid && error.isEmpty();
    }
    if (!valid) {
        m_items.clear();
        ui->label_status->setText(
            "请检查无效行的数据类型或初始值。");
    } else
        ui->label_status->setText(QString("%1 行可追加。双击初始值可修改。").arg(m_items.size()));
    ui->buttons->button(QDialogButtonBox::Ok)->setEnabled(valid);
}
