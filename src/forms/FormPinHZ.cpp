#include "ui_FormFrameSettings.h"
#include <QMessageBox>
#include <QScopedValueRollback>
#include <QUuid>
#include "FormPinHZ.h"
#include <algorithm>
#include "ui_FormPinHZ.h"
#include "AppTheme.h"
#include "AppItemValue.h"
#include "FormTableImport.h"
#include "AppCalcCheck.h"
#include <QToolTip>
#include <QMenu>
#include <QStyledItemDelegate>
#include <functional>

#include <QDateTime>
#include <QFile>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextStream>

namespace {
QString reverseHexBytes(const QString &hex) {
    QByteArray bytes = QByteArray::fromHex(hex.toLatin1());
    std::reverse(bytes.begin(), bytes.end());
    return QString::fromLatin1(bytes.toHex().toUpper());
}

bool canBindLength(const s_itemUnit &entry) {
    return entry.itemType == ItemType::Text && entry.itemZone != ItemZone::Check &&
           entry.dataType != DataType::F32 && entry.dataType != DataType::F64;
}

class HexInputDelegate : public QStyledItemDelegate {
public:
    HexInputDelegate(QObject *parent, std::function<bool()> littleEndian,
                     std::function<void(int, const QString &)> commit)
        : QStyledItemDelegate(parent), m_littleEndian(littleEndian), m_commit(commit) {}

    void setEditorData(QWidget *editor, const QModelIndex &index) const override {
        auto *edit = qobject_cast<QLineEdit *>(editor);
        const QString wireHex = index.data(Qt::EditRole).toString();
        edit->setText(m_littleEndian() ? reverseHexBytes(wireHex) : wireHex);
        edit->setToolTip("按数值顺序输入 HEX，确认后显示按大小端排列的字节。");
        edit->selectAll();
    }

    void setModelData(QWidget *editor, QAbstractItemModel *,
                      const QModelIndex &index) const override {
        const auto *edit = qobject_cast<QLineEdit *>(editor);
        // Commit the numeric input even if it matches the displayed wire bytes.
        if (edit->isModified())
            m_commit(index.row(), edit->text());
    }

private:
    std::function<bool()> m_littleEndian;
    std::function<void(int, const QString &)> m_commit;
};
} // namespace

FormPinHZ::FormPinHZ(QWidget *parent) : QWidget(parent), ui(new Ui::FormPinHZ) {
    ui->setupUi(this);
    ui->table_PinHZ->setItemDelegateForColumn(
        colDataHex, new HexInputDelegate(
                        ui->table_PinHZ, [this] { return m_endian == Endian::LittleEndian; },
                        [this](int row, const QString &text) {
                            applyValueEdit(ui->table_PinHZ->item(row, colDataHex), text);
                        }));
    auto *lengthMenu = new QMenu(ui->button_lengthBinding);
    ui->button_lengthBinding->setMenu(lengthMenu);
    connect(lengthMenu, &QMenu::aboutToShow, this, [this, lengthMenu] {
        lengthMenu->clear();
        auto *unbound =
            lengthMenu->addAction(tr("不绑定"), this, [this] { setLengthBinding(QString()); });
        unbound->setCheckable(true);
        unbound->setChecked(m_lengthItemId.isEmpty());
        for (int row = 0; row < m_itemUnitList.size(); ++row) {
            const auto &entry = m_itemUnitList[row];
            if (!canBindLength(entry))
                continue;
            const QString id = entry.itemId;
            const QString label =
                entry.comment.isEmpty()
                    ? tr("第 %1 行").arg(row + 1)
                    : tr("第 %1 行 · %2").arg(row + 1).arg(entry.comment.simplified().left(40));
            auto *action = lengthMenu->addAction(label, this, [this, id] { setLengthBinding(id); });
            action->setCheckable(true);
            action->setChecked(id == m_lengthItemId);
        }
    });

    // Reserve room for the styled Head/Data/Check/Tail labels, including header padding.
    ui->table_PinHZ->verticalHeader()->setMinimumWidth(100);
    ui->table_PinHZ->verticalHeader()->setTextElideMode(Qt::ElideNone);
    ui->table_PinHZ->horizontalHeader()->setMinimumHeight(36);

    m_autoReplyTimes = 0;
    m_autoReplyDelay = 0;

    ui->table_PinHZ->setColumnWidth(colDataType, 80);
    ui->table_PinHZ->setColumnWidth(colDataHex, 130);
    ui->table_PinHZ->setCurrentCell(0, colDataHex);
    QMetaObject::invokeMethod(ui->button_PinHZ, "clicked", Qt::QueuedConnection);

    QHeaderView *colHeader = ui->table_PinHZ->horizontalHeader();
    colHeader->setSectionResizeMode(colDataType, QHeaderView::Fixed);
    colHeader->setSectionResizeMode(colComment, QHeaderView::Stretch);

    m_itemTypeDlg = new FormItemType(this);
    connect(m_itemTypeDlg, &FormItemType::itemConfDone, this, &FormPinHZ::on_itemConfDone,
            Qt::DirectConnection);
    s_itemUnit itemUnit;
    m_itemTypeDlg->clearItemUnit(itemUnit);
    itemUnit.itemZone = ItemZone::Head;
    itemUnit.valueStr = "EB";
    itemUnit.comment = "Head";
    m_itemUnitList.append(itemUnit);

    m_fillItemDlg = new FormFillItem(this);
    connect(m_fillItemDlg, &FormFillItem::fillConfDone, this, &FormPinHZ::on_fillConfDone,
            Qt::QueuedConnection);

    m_crcConfDlg = new FormCRCConf(this);
    connect(m_crcConfDlg, &FormCRCConf::CRCConfDone, this, &FormPinHZ::on_CRCConfDone,
            Qt::DirectConnection);

    // Timer
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &FormPinHZ::on_timerOut);
    m_timer->setInterval(10);
    m_timer->start();

    m_receivePending.clear();
    rebuildZoneCounts();
    refreshTable();
    QMetaObject::invokeMethod(ui->button_PinHZ, "clicked", Qt::QueuedConnection);
}

FormPinHZ::~FormPinHZ() {
    delete ui;
    delete m_itemTypeDlg;
    delete m_fillItemDlg;
    delete m_crcConfDlg;
    delete m_timer;
}

ItemZone FormPinHZ::rowItemZone(int32_t row) const {
    if (row < 0 || row >= m_itemUnitList.size())
        return ItemZone::Head;
    return m_itemUnitList.at(row).itemZone;
}

int32_t FormPinHZ::firstRowOfZone(ItemZone zone) const {
    for (int32_t row = 0; row < m_itemUnitList.size(); row++) {
        if (m_itemUnitList.at(row).itemZone == zone)
            return row;
    }
    return -1;
}

int32_t FormPinHZ::modelCheckRow() const {
    return firstRowOfZone(ItemZone::Check);
}

void FormPinHZ::rebuildZoneCounts() {
    m_rowCntHead = 0;
    m_rowCntData = 0;
    m_rowCntTail = 0;

    int32_t checkRow = modelCheckRow();
    if (checkRow >= 0) {
        int32_t comboIndex = m_itemUnitList.at(checkRow).comboIndex;
        if (comboIndex < 0 || comboIndex > static_cast<int32_t>(CheckType::CRC))
            comboIndex = static_cast<int32_t>(CheckType::CheckSum8);
        m_itemUnitList[checkRow].comboIndex = comboIndex;
        m_checkType = static_cast<CheckType>(comboIndex);
    } else {
        m_checkType = CheckType::None;
        m_crcParamsPending = false;
    }

    for (int32_t row = 0; row < m_itemUnitList.size(); row++) {
        switch (m_itemUnitList.at(row).itemZone) {
        case ItemZone::Head:
            m_rowCntHead++;
            break;
        case ItemZone::Data:
            m_rowCntData++;
            break;
        case ItemZone::Check:
            break;
        case ItemZone::Tail:
            m_rowCntTail++;
            break;
        default:
            break;
        }
    }
}

int32_t FormPinHZ::dataTypeByteLen(DataType dataType) const {
    switch (dataType) {
    case DataType::U08:
    case DataType::S08:
        return 1;
    case DataType::U16:
    case DataType::S16:
        return 2;
    case DataType::U32:
    case DataType::S32:
    case DataType::F32:
        return 4;
    case DataType::U64:
    case DataType::S64:
    case DataType::F64:
        return 8;
    default:
        return 1;
    }
}

DataType FormPinHZ::checkDataType(CheckType checkType) const {
    switch (checkType) {
    case CheckType::CheckSum8:
        return DataType::U08;
    case CheckType::CheckSum16:
        return DataType::U16;
    case CheckType::CheckSum32:
        return DataType::U32;
    case CheckType::CRC: {
        int32_t width = m_crcConfDlg ? m_crcConfDlg->m_crcConf.width : 8;
        if (width <= 8)
            return DataType::U08;
        if (width <= 16)
            return DataType::U16;
        return DataType::U32;
    }
    default:
        return DataType::U08;
    }
}

int32_t FormPinHZ::checkByteLen(CheckType checkType) const {
    switch (checkType) {
    case CheckType::CheckSum8:
        return 1;
    case CheckType::CheckSum16:
        return 2;
    case CheckType::CheckSum32:
        return 4;
    case CheckType::CRC: {
        int32_t width = m_crcConfDlg ? m_crcConfDlg->m_crcConf.width : 8;
        return (width + 7) / 8;
    }
    default:
        return 0;
    }
}

bool FormPinHZ::crcConfigPending() const {
    return m_crcParamsPending;
}

QString FormPinHZ::bytesToHex(const QByteArray &bytes) const {
    QString hex = "";
    for (int32_t i = 0; i < bytes.size(); i++) {
        hex += QString::asprintf("%02X", static_cast<uint8_t>(bytes.at(i)));
    }
    return hex;
}

QByteArray FormPinHZ::hexStringToBytes(QString hex) const {
    QByteArray bytes;
    hex = hex.simplified();
    hex.remove(' ');
    hex.remove('\t');
    while (hex.startsWith("0x") || hex.startsWith("0X"))
        hex.remove(0, 2);

    if (hex.length() % 2 != 0)
        hex.prepend('0');

    for (int32_t i = 0; i + 1 < hex.length(); i += 2) {
        bool isHex = false;
        uint8_t value = hex.mid(i, 2).toUInt(&isHex, 16);
        bytes.append(isHex ? static_cast<char>(value) : static_cast<char>(0));
    }
    return bytes;
}

QString FormPinHZ::normalizeDisplayHex(const QString &displayHex, int32_t byteLen,
                                       Endian endian) const {
    if (byteLen <= 0)
        return "";

    QString hex = displayHex.simplified();
    hex.remove(' ');
    hex.remove('\t');
    while (hex.startsWith("0x") || hex.startsWith("0X"))
        hex.remove(0, 2);
    hex = hex.toUpper();

    bool isValid = !hex.isEmpty();
    for (int32_t i = 0; isValid && i < hex.length(); i++) {
        QChar ch = hex.at(i);
        if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F')))
            isValid = false;
    }
    if (!isValid)
        hex = "0";

    if (hex.length() % 2 != 0)
        hex.prepend('0');

    int32_t targetLen = byteLen * 2;
    if (hex.length() > targetLen) {
        if (endian == Endian::LittleEndian)
            hex = hex.right(targetLen);
        else
            hex = hex.left(targetLen);
    } else {
        while (hex.length() < targetLen) {
            if (endian == Endian::LittleEndian)
                hex += "00";
            else
                hex.prepend("00");
        }
    }
    return hex;
}

QString FormPinHZ::wireHexToDisplayHex(const QString &wireHex, int32_t byteLen,
                                       Endian endian) const {
    return normalizeDisplayHex(wireHex, byteLen, endian);
}

QString FormPinHZ::displayHexToWireHex(const QString &displayHex, int32_t byteLen,
                                       Endian endian) const {
    return normalizeDisplayHex(displayHex, byteLen, endian);
}

QByteArray FormPinHZ::displayToWireBytes(const QString &displayHex, int32_t byteLen,
                                         Endian endian) const {
    return hexStringToBytes(displayHexToWireHex(displayHex, byteLen, endian));
}

QByteArray FormPinHZ::rowWireBytes(int32_t row) const {
    if (row < 0 || row >= m_itemUnitList.size())
        return QByteArray();

    const s_itemUnit &itemUnit = m_itemUnitList.at(row);
    if (itemUnit.itemZone == ItemZone::Check) {
        CheckType checkType = m_checkType;
        if (checkType == CheckType::None)
            checkType = static_cast<CheckType>(itemUnit.comboIndex);
        int32_t byteLen = checkByteLen(checkType);
        if (byteLen <= 0)
            return QByteArray();
        return hexStringToBytes(normalizeDisplayHex(itemUnit.valueStr, byteLen, m_endian));
    }

    QString key = itemUnit.valueStr;
    if (itemUnit.itemType == ItemType::Text) {
        return hexStringToBytes(
            normalizeDisplayHex(itemUnit.valueStr, dataTypeByteLen(itemUnit.dataType), m_endian));
    }

    if (itemUnit.comboIndex >= 0 && itemUnit.comboIndex < itemUnit.keyList.size())
        key = itemUnit.keyList.at(itemUnit.comboIndex);
    return hexStringToBytes(normalizeDisplayHex(key, dataTypeByteLen(itemUnit.dataType), m_endian));
}

QByteArray FormPinHZ::dataZoneWireBytes() const {
    QByteArray bytes;
    for (int32_t row = 0; row < m_itemUnitList.size(); row++) {
        if (m_itemUnitList.at(row).itemZone == ItemZone::Data)
            bytes.append(rowWireBytes(row));
    }
    return bytes;
}

QByteArray FormPinHZ::calcCheckWireBytes(CheckType checkType, const QByteArray &dataZone) const {
    if (checkType == CheckType::None)
        return QByteArray();

    int32_t dataByteLen = checkByteLen(checkType);
    if (dataByteLen <= 0)
        return QByteArray();
    if (checkType == CheckType::CRC && crcConfigPending())
        return QByteArray();

    uint32_t checkCalc = 0;

    if (checkType != CheckType::CRC) {
        uint64_t sum = 0;
        for (int32_t i = 0; i < dataZone.size(); i++)
            sum += static_cast<uint8_t>(dataZone.at(i));
        checkCalc = static_cast<uint32_t>(sum);
    } else if (m_crcConfDlg) {
        s_crc_conf crcConf = m_crcConfDlg->m_crcConf;
        if (crcConf.width <= 0 || crcConf.width > 32)
            crcConf = m_crcConfDlg->m_crcCalc.GetTemplate(CRC8);
        checkCalc = m_crcConfDlg->m_crcCalc.calcCRC(
            crcConf, reinterpret_cast<const uint8_t *>(dataZone.constData()), dataZone.size());
    } else {
        return QByteArray();
    }

    if (dataByteLen < 4)
        checkCalc &= static_cast<uint32_t>((1ULL << (dataByteLen * 8)) - 1);

    QByteArray wireBytes(dataByteLen, 0);
    for (int32_t i = 0; i < dataByteLen; i++) {
        int32_t byteIndex = m_endian == Endian::LittleEndian ? i : dataByteLen - 1 - i;
        wireBytes[byteIndex] = static_cast<char>((checkCalc >> (8 * i)) & 0xFF);
    }
    return wireBytes;
}

QString FormPinHZ::calcCheckDisplayHex() const {
    int32_t checkRow = modelCheckRow();
    if (checkRow < 0)
        return "";

    CheckType checkType = m_checkType;
    if (checkType == CheckType::None)
        checkType = static_cast<CheckType>(m_itemUnitList.at(checkRow).comboIndex);
    if (checkType == CheckType::None)
        return "";

    QByteArray checkBytes = calcCheckWireBytes(checkType, checkRangeBytes());
    if (checkBytes.isEmpty())
        return "";
    return bytesToHex(checkBytes);
}

QString FormPinHZ::calcCheckWireHex() const {
    return calcCheckDisplayHex();
}

int32_t FormPinHZ::rowOfCellWidget(QWidget *widget) const {
    if (!widget)
        return -1;
    for (int32_t row = 0; row < ui->table_PinHZ->rowCount(); row++) {
        for (int32_t col = 0; col < ui->table_PinHZ->columnCount(); col++) {
            if (ui->table_PinHZ->cellWidget(row, col) == widget)
                return row;
        }
    }
    return -1;
}

void FormPinHZ::invokeAutoPin() {
    if (m_isAutoPin && !crcConfigPending())
        QMetaObject::invokeMethod(ui->button_PinHZ, "clicked", Qt::QueuedConnection);
}

/* **************************************************
 * @brief get PinHZ table data buf and hex string
 * @para:
 *      [input]data: dataBuf
 *      [input]hexStr: hex string
 * @return:
 *      return 0 when PinHZ succ
 * **************************************************/
int32_t FormPinHZ::PinHZ(QString &hexStr) {
    hexStr.clear();
    m_length = 0;
    QString error;
    if (!validateTextRows(error)) {
        emit showLog(LogLevel::ERR, error.toHtmlEscaped());
        return -1;
    }
    for (int32_t row = 0; row < m_itemUnitList.size(); row++) {
        if (m_itemUnitList.at(row).itemZone == ItemZone::Check && m_checkType == CheckType::None)
            continue;

        QByteArray rowBytes = rowWireBytes(row);
        m_length += rowBytes.size();
        if (rowBytes.isEmpty())
            continue;

        QString str = bytesToHex(rowBytes);
        if (!m_isFieldPinHZ) {
            QString spaced = "";
            for (int32_t i = 0; i < rowBytes.size(); i++) {
                if (i != 0)
                    spaced += " ";
                spaced += QString::asprintf("%02X", static_cast<uint8_t>(rowBytes.at(i)));
            }
            str = spaced;
        }
        if (!hexStr.isEmpty())
            hexStr += " ";
        hexStr += str;
    }
    return 0;
}

QString FormPinHZ::outputText() const {
    return ui->plain_PinHZ->toPlainText();
}

// PinHZ parse, try to parse str to data stream
int32_t FormPinHZ::PinHZ_parse(QString str, QByteArray &data) {
    QString error;
    if (!AppCalcCheck::parseHex(str, data, error)) {
        data.clear();
        emit showLog(LogLevel::ERR, error.toHtmlEscaped());
        return -1;
    }
    return data.size();
}

int32_t FormPinHZ::PinHZ_deal2Table(QByteArray data, bool force) {
    m_lastParseSucceeded = false;
    int32_t dataIndex = 0;
    if (data.isEmpty())
        return 0;
    if (crcConfigPending() && !force) {
        emit showLog(LogLevel::WAR, "CRC参数待配置, 无法校验接收码流, 请先配置CRC!");
        return 0;
    }

    struct RowParseResult {
        bool valid = false;
        QString displayHex;
        int32_t comboIndex = -1;
    };

    QByteArray receivedDataZone;
    QList<RowParseResult> parseRows;
    CheckType parseCheckType = CheckType::None;
    QByteArray receivedCheckBytes;
    bool parseAbort = false;

    for (int32_t row = 0; row < m_itemUnitList.size(); row++) {
        const s_itemUnit &itemUnit = m_itemUnitList.at(row);
        RowParseResult parseItem;
        int32_t byteLen = 0;

        if (itemUnit.itemZone == ItemZone::Check) {
            CheckType checkType = m_checkType;
            if (checkType == CheckType::None)
                checkType = static_cast<CheckType>(itemUnit.comboIndex);
            if (checkType == CheckType::None)
                break;

            parseCheckType = checkType;
            byteLen = checkByteLen(checkType);
            if (dataIndex + byteLen > data.size()) {
                emit showLog(LogLevel::WAR,
                             QString("解帧数据不足: 第%1行(校验)需要%2字节, 实际剩余%3字节")
                                 .arg(row + 1)
                                 .arg(byteLen)
                                 .arg(data.size() - dataIndex));
                break;
            }

            receivedCheckBytes = data.mid(dataIndex, byteLen);
            dataIndex += byteLen;
            parseItem.displayHex = bytesToHex(receivedCheckBytes);
            parseItem.valid = true;
            parseRows.append(parseItem);
            continue;
        }

        byteLen = dataTypeByteLen(itemUnit.dataType);
        if (dataIndex + byteLen > data.size()) {
            emit showLog(LogLevel::WAR, QString("解帧数据不足: 第%1行需要%2字节, 实际剩余%3字节")
                                            .arg(row + 1)
                                            .arg(byteLen)
                                            .arg(data.size() - dataIndex));
            break;
        }

        QByteArray bytes = data.mid(dataIndex, byteLen);
        dataIndex += byteLen;
        QString receivedHex = bytesToHex(bytes);

        if (itemUnit.itemZone == ItemZone::Data)
            receivedDataZone.append(bytes);

        if (itemUnit.itemType == ItemType::Text) {
            parseItem.displayHex = normalizeDisplayHex(receivedHex, byteLen, m_endian);
            s_itemUnit candidate = itemUnit;
            candidate.valueStr = parseItem.displayHex;
            QString converted, error;
            if (!AppItemValue::display(candidate, m_endian, converted, error)) {
                emit showLog(
                    LogLevel::ERR,
                    QString("第 %1 行解帧值无效：%2").arg(row + 1).arg(error.toHtmlEscaped()));
                parseAbort = true;
            }
            parseItem.valid = true;
            parseRows.append(parseItem);
            continue;
        }

        int32_t matchIndex = -1;
        for (int32_t i = 0; i < itemUnit.keyList.size(); i++) {
            if (hexStringToBytes(normalizeDisplayHex(itemUnit.keyList.at(i), byteLen, m_endian)) ==
                bytes) {
                matchIndex = i;
                break;
            }
        }
        if (matchIndex >= 0) {
            parseItem.comboIndex = matchIndex;
            parseItem.displayHex =
                normalizeDisplayHex(itemUnit.keyList.at(matchIndex), byteLen, m_endian);
            parseItem.valid = true;
        } else {
            emit showLog(LogLevel::ERR,
                         QString("第%1行没有匹配的下拉项: 码流[%2]").arg(row + 1).arg(receivedHex));
            parseAbort = true;
        }
        parseRows.append(parseItem);
    }

    if (parseAbort && !force) {
        emit showLog(LogLevel::WAR, "存在不合法条目, 已放弃本次解帧写入，保留原值");
        return dataIndex;
    }

    if (parseRows.size() != m_itemUnitList.size() && !force) {
        emit showLog(LogLevel::WAR,
                     QString("解帧未完整消费: 模板%1行, 实际解析%2行, 已读取%3字节, 已放弃本次解析")
                         .arg(m_itemUnitList.size())
                         .arg(parseRows.size())
                         .arg(dataIndex));
        return dataIndex;
    }

    bool checkPass = true;
    if (parseCheckType != CheckType::None) {
        int32_t checkRow = modelCheckRow();
        QString expectedHex = "";
        if (checkRow >= 0 && !receivedCheckBytes.isEmpty()) {
            QByteArray expectedBytes = calcCheckWireBytes(parseCheckType, checkRangeBytes(&data));
            if (!expectedBytes.isEmpty()) {
                expectedHex = bytesToHex(expectedBytes);
                checkPass = receivedCheckBytes == expectedBytes;
            }
        }

        if (expectedHex.isEmpty()) {
            emit showLog(LogLevel::ERR, "校验配置无效, 无法计算期望校验值");
            checkPass = false;
        } else if (checkPass) {
            emit showLog(LogLevel::INF, QString("校验通过: 接收[%1] 期望[%2]")
                                            .arg(bytesToHex(receivedCheckBytes), expectedHex));
        } else {
            emit showLog(LogLevel::WAR, QString("校验失败: 接收[%1] 期望[%2]")
                                            .arg(bytesToHex(receivedCheckBytes), expectedHex));
        }
    }

    if (!checkPass && !force) {
        emit showLog(LogLevel::WAR, "解帧校验失败，保留原值且不写入 CSV。");
        return dataIndex;
    }
    for (int32_t row = 0; row < parseRows.size(); row++) {
        s_itemUnit &itemUnit = m_itemUnitList[row];
        RowParseResult &parseItem = parseRows[row];
        if (itemUnit.itemZone == ItemZone::Check)
            continue;
        if (!parseItem.valid)
            continue;
        if (itemUnit.itemType == ItemType::Text) {
            itemUnit.valueStr = parseItem.displayHex;
            continue;
        }
        if (parseItem.comboIndex >= 0 && parseItem.comboIndex < itemUnit.keyList.size()) {
            itemUnit.comboIndex = parseItem.comboIndex;
            itemUnit.valueStr = parseItem.displayHex;
        }
    }

    m_lastParseSucceeded = !force;
    if (!force)
        saveParsedFrame();
    refreshTable();

    if (dataIndex < data.size()) {
        emit showLog(LogLevel::WAR, QString("解帧数据有多余字节: 模板消耗%1字节, 数据剩余%2字节")
                                        .arg(dataIndex)
                                        .arg(data.size() - dataIndex));
    }

    if (!checkPass) {
        emit showLog(LogLevel::WAR, "解帧数据校验未通过, 请检查模板或协议配置");
    }

    return dataIndex;
}

void FormPinHZ::PinHZ_comboDataTypeInit(int32_t row, DataType dataType) {
    QComboBox *comboBox = new QComboBox();
    AppTheme::prepareComboBox(comboBox);

    comboBox->addItem("uint8");
    comboBox->addItem("int8");
    comboBox->addItem("uint16");
    comboBox->addItem("int16");
    comboBox->addItem("uint32");
    comboBox->addItem("int32");
    comboBox->addItem("uint64");
    comboBox->addItem("int64");
    comboBox->addItem("float");
    comboBox->addItem("double");
    comboBox->setCurrentIndex((int)dataType);
    connect(comboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &FormPinHZ::on_combo_PinHZ_dataTypeChanged);
    ui->table_PinHZ->setCellWidget(row, colDataType, comboBox);
}

int32_t FormPinHZ::getRowsBytes(int32_t rowStart, int32_t rowEnd) {
    int32_t dataBytes = 0;
    for (int32_t row = rowStart; row <= rowEnd && row < m_itemUnitList.size(); row++) {
        if (row < 0)
            continue;
        if (m_itemUnitList.at(row).itemZone == ItemZone::Check)
            dataBytes += checkByteLen(m_checkType);
        else
            dataBytes += dataTypeByteLen(m_itemUnitList.at(row).dataType);
    }
    return dataBytes;
}

void FormPinHZ::updateDataZoneBytes() {
    ui->button_lengthBinding->setText(tr("不绑定"));
    const int length = dataZoneWireBytes().size();
    ui->lineEdit_DataLen->setText(QString::number(length));
    for (int row = 0; row < m_itemUnitList.size(); ++row)
        if (!m_lengthItemId.isEmpty() && m_itemUnitList[row].itemId == m_lengthItemId) {
            if (!canBindLength(m_itemUnitList[row])) {
                m_lengthItemId.clear();
                emit showLog(LogLevel::WAR, tr("绑定字段已不再是普通整数，已解除帧长绑定。"));
                break;
            }
            ui->button_lengthBinding->setText(tr("第 %1 行").arg(row + 1));
            auto raw = m_itemUnitList[row];
            raw.ioFormula.clear();
            raw.reverseFormula.clear();
            raw.regex.clear();
            raw.lowerLimitEnabled = false;
            raw.upperLimitEnabled = false;
            QString hex, error;
            if (AppItemValue::fromDisplay(raw, QString::number(length), m_endian, hex, error)) {
                m_itemUnitList[row].valueStr = hex;
                if (ui->table_PinHZ->item(row, colDataHex))
                    updateTextValueView(row);
            } else
                emit showLog(LogLevel::ERR, tr("帧长绑定失败：%1").arg(error));
        }
}

void FormPinHZ::updateDataCheck() {
    int32_t checkRow = modelCheckRow();
    if (checkRow < 0 || m_checkType == CheckType::None || crcConfigPending()) {
        return;
    }

    QString checkHex = calcCheckWireHex();
    if (checkHex.isEmpty())
        return;

    m_itemUnitList[checkRow].valueStr = checkHex;
    DataType dataType = checkDataType(m_checkType);
    QString decStr = m_hex2dec.hex2DecString(checkHex, dataType, m_endian);

    ui->table_PinHZ->blockSignals(true);
    QTableWidgetItem *hexItem = ui->table_PinHZ->item(checkRow, colDataHex);
    if (hexItem)
        hexItem->setText(checkHex);
    QTableWidgetItem *decItem = ui->table_PinHZ->item(checkRow, colDataDec);
    if (decItem)
        decItem->setText(decStr);
    ui->table_PinHZ->blockSignals(false);
}

ItemZone FormPinHZ::checkRowZone(int32_t row) {
    return rowItemZone(row);
}

void FormPinHZ::createItemsARow(int32_t row, s_itemUnit itemUnit) {
    QTableWidgetItem *item = nullptr;

    ui->table_PinHZ->blockSignals(true);

    // rowHead
    switch (itemUnit.itemZone) {
    case ItemZone::Head:
        item = new QTableWidgetItem("Head");
        break;
    case ItemZone::Data:
        item = new QTableWidgetItem("Data");
        break;
    case ItemZone::Check:
        item = new QTableWidgetItem("Check");
        break;
    case ItemZone::Tail:
        item = new QTableWidgetItem("Tail");
        break;
    default:
        return;
    }
    item->setText(QString::number(row + 1) + "  " + item->text());
    ui->table_PinHZ->setVerticalHeaderItem(row, item);

    if (itemUnit.itemZone == ItemZone::Check) {
        QComboBox *comboBox = new QComboBox();
        AppTheme::prepareComboBox(comboBox);
        comboBox->addItem("CheckSum-8");
        comboBox->addItem("CheckSum-16");
        comboBox->addItem("CheckSum-32");
        comboBox->addItem("CRC");
        comboBox->setCurrentIndex(qBound(0, itemUnit.comboIndex, 3));
        ui->table_PinHZ->setCellWidget(row, colDataType, comboBox);
        connect(comboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                this, &FormPinHZ::on_combo_PinHZ_checkChanged);

        item = new QTableWidgetItem("00", ctDataHex);
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        ui->table_PinHZ->setItem(row, colDataHex, item);

        item = new QTableWidgetItem("0", ctDataDec);
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        ui->table_PinHZ->setItem(row, colDataDec, item);

        item = new QTableWidgetItem("校验", ctComment);
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        ui->table_PinHZ->setItem(row, colComment, item);
    } else {
        PinHZ_comboDataTypeInit(row, itemUnit.dataType);

        if (itemUnit.itemType == ItemType::Text) {
            QString hexStr = itemUnit.valueStr;
            item = new QTableWidgetItem(hexStr, ctDataHex);
            item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            ui->table_PinHZ->setItem(row, colDataHex, item);

            item = new QTableWidgetItem(QString(), ctDataDec);
            item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            ui->table_PinHZ->setItem(row, colDataDec, item);
            updateTextValueView(row);
        } else {
            int32_t comboCount = qMin(itemUnit.optionList.size(), itemUnit.keyList.size());
            int32_t comboIndex = qBound(0, itemUnit.comboIndex, qMax(0, comboCount - 1));
            m_itemUnitList[row].comboIndex = comboIndex;

            QComboBox *optionCombo = new QComboBox();
            AppTheme::prepareComboBox(optionCombo);
            for (int32_t i = 0; i < comboCount; i++)
                optionCombo->addItem(itemUnit.optionList.at(i));
            optionCombo->setCurrentIndex(comboIndex);
            ui->table_PinHZ->setCellWidget(row, colDataDec, optionCombo);
            connect(optionCombo,
                    static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
                    &FormPinHZ::on_combo_PinHZ_customComboChanged);

            QComboBox *keyCombo = new QComboBox();
            AppTheme::prepareComboBox(keyCombo);
            for (int32_t i = 0; i < comboCount; i++)
                keyCombo->addItem(itemUnit.keyList.at(i));
            keyCombo->setCurrentIndex(comboIndex);
            ui->table_PinHZ->setCellWidget(row, colDataHex, keyCombo);
            connect(keyCombo,
                    static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
                    &FormPinHZ::on_combo_PinHZ_customComboChanged);

            if (itemUnit.comboIndex >= 0 && itemUnit.comboIndex < itemUnit.keyList.size())
                m_itemUnitList[row].valueStr =
                    normalizeDisplayHex(itemUnit.keyList.at(itemUnit.comboIndex),
                                        dataTypeByteLen(itemUnit.dataType), m_endian);
        }
    }

    // Comment
    QString comment = itemUnit.comment;
    if (comment.isEmpty() && itemUnit.itemZone != ItemZone::Check)
        comment = itemUnit.itemZone == ItemZone::Head   ? "Head"
                  : itemUnit.itemZone == ItemZone::Tail ? "Tail"
                                                        : "";
    item = new QTableWidgetItem(comment, ctComment);
    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    ui->table_PinHZ->setItem(row, colComment, item);

    ui->table_PinHZ->blockSignals(false);
}

int32_t FormPinHZ::length(void) {
    return m_length;
}

int32_t FormPinHZ::zoneLength(ItemZone zone) {
    int32_t len = 0;
    for (int32_t row = 0; row < m_itemUnitList.size(); row++) {
        if (m_itemUnitList.at(row).itemZone != zone)
            continue;
        if (zone == ItemZone::Check)
            len += checkByteLen(m_checkType);
        else
            len += dataTypeByteLen(m_itemUnitList.at(row).dataType);
    }
    return len;
}

int32_t FormPinHZ::rowCnt() {
    return m_itemUnitList.size();
}

int32_t FormPinHZ::insertRowForZone(ItemZone zone, int32_t currentRow) const {
    if (currentRow >= 0 && currentRow < m_itemUnitList.size() &&
        m_itemUnitList.at(currentRow).itemZone == zone)
        return currentRow;

    if (zone == ItemZone::Head)
        return m_rowCntHead;
    else if (zone == ItemZone::Data)
        return m_rowCntHead + m_rowCntData;
    else
        return m_itemUnitList.size();
}

void FormPinHZ::updateRowView(int32_t row) {
    if (row < 0 || row >= m_itemUnitList.size())
        return;

    s_itemUnit &itemUnit = m_itemUnitList[row];
    ui->table_PinHZ->blockSignals(true);

    QComboBox *typeCombo = qobject_cast<QComboBox *>(ui->table_PinHZ->cellWidget(row, colDataType));
    if (typeCombo)
        typeCombo->blockSignals(true);

    if (itemUnit.itemZone == ItemZone::Check) {
        if (m_checkType == CheckType::None ||
            static_cast<int32_t>(m_checkType) > static_cast<int32_t>(CheckType::CRC))
            m_checkType = CheckType::CheckSum8;
        itemUnit.comboIndex = static_cast<int32_t>(m_checkType);
        if (typeCombo && typeCombo->count() == 4)
            typeCombo->setCurrentIndex(static_cast<int32_t>(m_checkType));
        if (typeCombo)
            typeCombo->blockSignals(false);
        ui->table_PinHZ->blockSignals(false);
        updateDataCheck();
        return;
    }

    if (typeCombo && typeCombo->count() == 10) {
        typeCombo->setCurrentIndex(qBound(0, static_cast<int32_t>(itemUnit.dataType),
                                          static_cast<int32_t>(DataType::F64)));
    }

    QTableWidgetItem *commentItem = ui->table_PinHZ->item(row, colComment);
    if (commentItem)
        commentItem->setText(itemUnit.comment);

    if (itemUnit.itemType == ItemType::Text) {
        updateTextValueView(row);
    } else {
        int32_t comboCount = qMin(itemUnit.optionList.size(), itemUnit.keyList.size());
        int32_t comboIndex = qBound(0, itemUnit.comboIndex, qMax(0, comboCount - 1));
        itemUnit.comboIndex = comboIndex;
        if (comboIndex >= 0 && comboIndex < itemUnit.keyList.size()) {
            itemUnit.valueStr = normalizeDisplayHex(itemUnit.keyList.at(comboIndex),
                                                    dataTypeByteLen(itemUnit.dataType), m_endian);
        }

        QComboBox *optionCombo =
            qobject_cast<QComboBox *>(ui->table_PinHZ->cellWidget(row, colDataDec));
        QComboBox *keyCombo =
            qobject_cast<QComboBox *>(ui->table_PinHZ->cellWidget(row, colDataHex));
        if (optionCombo && optionCombo->count() == comboCount) {
            optionCombo->blockSignals(true);
            optionCombo->setCurrentIndex(comboIndex);
            optionCombo->blockSignals(false);
        }
        if (keyCombo && keyCombo->count() == comboCount) {
            keyCombo->blockSignals(true);
            keyCombo->setCurrentIndex(comboIndex);
            keyCombo->blockSignals(false);
        }
    }

    if (typeCombo)
        typeCombo->blockSignals(false);
    ui->table_PinHZ->blockSignals(false);
    updateDataZoneBytes();
    updateDataCheck();
}

void FormPinHZ::updateTextValueView(int32_t row) {
    const QSignalBlocker block(ui->table_PinHZ);
    s_itemUnit &entry = m_itemUnitList[row];
    auto *hex = ui->table_PinHZ->item(row, colDataHex);
    auto *value = ui->table_PinHZ->item(row, colDataDec);
    if (!hex || !value)
        return;
    QString display, error, canonical;
    if (AppItemValue::normalizeHex(entry.valueStr, entry.dataType, m_endian, canonical, error))
        entry.valueStr = canonical;
    const bool valid = AppItemValue::display(entry, m_endian, display, error);
    hex->setFlags(hex->flags() | Qt::ItemIsEditable);
    hex->setText(entry.valueStr);
    value->setText(display.isEmpty() ? QString("无效") : display);
    auto flags = value->flags();
    if (AppItemValue::editable(entry))
        flags |= Qt::ItemIsEditable;
    else
        flags &= ~Qt::ItemIsEditable;
    value->setFlags(flags);
    const bool bound = !m_lengthItemId.isEmpty() && entry.itemId == m_lengthItemId;
    if (bound) {
        hex->setFlags(hex->flags() & ~Qt::ItemIsEditable);
        value->setFlags(value->flags() & ~Qt::ItemIsEditable);
    }
    QString hint =
        QString("正向：%1\n反向：%2")
            .arg(entry.ioFormula.isEmpty() ? "x" : entry.ioFormula,
                 entry.reverseFormula.isEmpty()
                     ? (AppItemValue::identity(entry.ioFormula) ? "x" : "未设置；请编辑 HEX")
                     : entry.reverseFormula);
    if (!valid)
        hint += "\n" + error;
    if (bound)
        hint += tr("\n此字段绑定数据域长度，自动维护；可在帧设置中解除绑定。");
    for (auto *cell : {hex, value}) {
        cell->setToolTip(hint);
        cell->setData(Qt::ForegroundRole, valid ? QVariant() : QVariant(QColor("#dc656d")));
    }
}

bool FormPinHZ::validateTextRows(QString &error) const {
    for (int row = 0; row < m_itemUnitList.size(); ++row) {
        const auto &entry = m_itemUnitList.at(row);
        if (entry.itemType != ItemType::Text || entry.itemZone == ItemZone::Check)
            continue;
        QString display;
        if (!AppItemValue::display(entry, m_endian, display, error)) {
            error.prepend(QString("第 %1 行：").arg(row + 1));
            return false;
        }
    }
    return true;
}

void FormPinHZ::updateAfterModelChange() {
    m_receivePending.clear();
    if (!m_lengthItemId.isEmpty() &&
        std::none_of(m_itemUnitList.begin(), m_itemUnitList.end(),
                     [this](const s_itemUnit &entry) { return entry.itemId == m_lengthItemId; })) {
        m_lengthItemId.clear();
        emit showLog(LogLevel::WAR, tr("帧长绑定字段已删除，已解除绑定。"));
    }
    rebuildZoneCounts();
    refreshTable();
}

void FormPinHZ::refreshTable() {
    QSet<QString> ids;
    for (auto &entry : m_itemUnitList) {
        if (entry.itemId.isEmpty() || ids.contains(entry.itemId))
            entry.itemId = QUuid::createUuid().toString();
        ids.insert(entry.itemId);
    }
    int32_t row = 0, rowCnt = m_itemUnitList.size();
    m_isRebuilding = true;
    ui->table_PinHZ->setRowCount(0);
    ui->table_PinHZ->clearContents();
    for (row = 0; row < rowCnt; row++) {
        ui->table_PinHZ->insertRow(row);
        createItemsARow(row, m_itemUnitList[row]);
    }
    updateDataZoneBytes();
    if (m_checkType != CheckType::None)
        updateDataCheck();
    m_isRebuilding = false;
    invokeAutoPin();
}

void FormPinHZ::on_PinHZ_recv(QByteArray &data, int32_t len) {
    if (data.isEmpty())
        return;

    QByteArray localData = data;
    if (len >= 0 && len < localData.size())
        localData.truncate(len);

    if (m_forceQuestionOpen) {
        m_receivePending += localData;
        return;
    }
    if (isAutoReply() && !isAutoDeal())
        addAutoReplyTimes();

    if (!isAutoDeal()) {
        m_receivePending.clear();
        return;
    }
    if (localData.isEmpty() || crcConfigPending())
        return;
    int frameLength = 0;
    QByteArray header, signature;
    bool prefix = true;
    for (int row = 0; row < m_itemUnitList.size(); ++row) {
        const auto &entry = m_itemUnitList[row];
        const QByteArray bytes = rowWireBytes(row);
        frameLength += bytes.size();
        signature += QByteArray::number(int(entry.itemZone)) + ":" +
                     QByteArray::number(int(entry.dataType)) + ";";
        if (prefix && entry.itemZone == ItemZone::Head)
            header += bytes;
        else
            prefix = false;
        if (entry.itemZone == ItemZone::Head || entry.itemZone == ItemZone::Tail)
            signature += bytes.toHex();
    }
    const auto crc = m_crcConfDlg->m_crcConf;
    signature += QByteArray::number(int(m_endian)) + ":" + QByteArray::number(int(m_checkType)) +
                 ":" + QByteArray::number(crc.width) + ":" + QByteArray::number(crc.poly) + ":" +
                 QByteArray::number(crc.init) + ":" + QByteArray::number(crc.ref_in) +
                 QByteArray::number(crc.ref_out) + ":" + QByteArray::number(crc.xor_out);
    if (frameLength <= 0 || frameLength > PINHZ_CACHE_LEN) {
        m_receivePending.clear();
        return;
    }
    if (signature != m_receiveSignature) {
        m_receivePending.clear();
        m_receiveSignature = signature;
    }
    m_receivePending += localData;
    bool askedForce = false;
    while (!m_receivePending.isEmpty()) {
        if (!header.isEmpty()) {
            int index = m_receivePending.indexOf(header);
            if (index < 0) {
                m_receivePending = m_receivePending.right(header.size() - 1);
                break;
            }
            if (index > 0)
                m_receivePending.remove(0, index);
        }
        if (m_receivePending.size() < frameLength)
            break;
        bool forced = false;
        PinHZ_deal2Table(m_receivePending.left(frameLength));
        if (!m_lastParseSucceeded && !askedForce) {
            askedForce = true;
            QScopedValueRollback<bool> guard(m_forceQuestionOpen, true);
            if (QMessageBox::question(
                    this, tr("自动解帧异常"), tr("是否对本次完整块强行解析？不会保存 CSV。"),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
                forced = true;
                PinHZ_deal2Table(m_receivePending.left(frameLength), true);
            }
        }
        if (m_lastParseSucceeded && isAutoReply())
            addAutoReplyTimes();
        m_receivePending.remove(0, m_lastParseSucceeded || forced || header.isEmpty() ? frameLength
                                                                                      : 1);
    }
}

void FormPinHZ::on_fillConfDone(bool isValid) {
    if (!isValid) {
        emit showLog(LogLevel::ERR, "填充数据输入错误!");
        return;
    }
    int32_t bufIndex = 0, dataLen = 0;
    DataType dataType;
    QString str = "";

    int32_t startRow = qBound(0, m_fillItemDlg->m_fillStart, m_itemUnitList.size());
    int32_t endRow = qMin(m_fillItemDlg->m_fillStart + m_fillItemDlg->m_fillRowCnt,
                          static_cast<int32_t>(m_itemUnitList.size()));
    if (endRow <= startRow) {
        emit showLog(LogLevel::ERR, "填充范围越界, 请重新选择行!");
        return;
    }

    QList<s_itemUnit> candidates = m_itemUnitList;
    for (int32_t row = startRow; row < endRow; row++) {
        dataType = m_itemUnitList[row].dataType;

        if (m_fillItemDlg->m_fillMode == FormFillItem::FillMode::Order) {
            if (bufIndex < m_fillItemDlg->m_fillData.size()) {
                str = bytesToHex(QByteArray(1, m_fillItemDlg->m_fillData.at(bufIndex++)));
            } else {
                emit showLog(LogLevel::ERR, "填充数据不足, 已放弃本次填充!");
                return;
            }
        } else {
            dataLen = dataTypeByteLen(dataType);
            if (bufIndex + dataLen > m_fillItemDlg->m_fillData.size()) {
                emit showLog(LogLevel::ERR, "填充数据不足, 已放弃本次填充!");
                return;
            }
            str = bytesToHex(m_fillItemDlg->m_fillData.mid(bufIndex, dataLen));
            bufIndex += dataLen;
        }

        candidates[row].valueStr = str;
        QString display, error;
        if (!AppItemValue::validateInput(candidates[row], m_endian, display, error)) {
            emit showLog(LogLevel::ERR, QString("第 %1 行填充值无效：%2；本次填充未写入")
                                            .arg(row + 1)
                                            .arg(error.toHtmlEscaped()));
            return;
        }
    }
    m_itemUnitList = candidates;
    for (int row = startRow; row < endRow; ++row)
        updateRowView(row);
    invokeAutoPin();
    emit showLog(LogLevel::INF, "数据填充完成");
}

void FormPinHZ::on_itemConfDone(int8_t validCode) {
    if (validCode != 0) {
        if (validCode & 1 << 0)
            emit showLog(LogLevel::ERR, "上限填写错误!");
        if (validCode & 1 << 1)
            emit showLog(LogLevel::ERR, "下限填写错误!");
        if (validCode & 1 << 2)
            emit showLog(LogLevel::ERR, "正则表达式填写错误!");
        if (validCode & 1 << 3)
            emit showLog(LogLevel::ERR, "默认值(Hex)填写错误!");
        if (validCode & 1 << 4)
            emit showLog(LogLevel::ERR, "显示选项填写错误!");
        if (validCode & 1 << 5)
            emit showLog(LogLevel::ERR, "映射选项填写错误!");
        emit showLog(LogLevel::INF, "条目配置失败!");
    } else {
        emit showLog(LogLevel::INF, "条目配置成功!");
        if (m_editRow >= 0 && m_editRow < m_itemUnitList.size()) {
            auto edited = m_itemTypeDlg->m_itemUnit;
            edited.itemZone = m_itemUnitList[m_editRow].itemZone;
            edited.itemId = m_itemUnitList[m_editRow].itemId;
            m_itemUnitList[m_editRow] = edited;
            const int editedRow = m_editRow;
            m_editRow = -1;
            updateAfterModelChange();
            ui->table_PinHZ->setCurrentCell(editedRow, colDataHex);
            return;
        }
        int32_t curRow = ui->table_PinHZ->currentRow();
        int32_t addNum = m_itemTypeDlg->m_itemCnt;
        s_itemUnit itemUnit = m_itemTypeDlg->m_itemUnit;
        itemUnit.itemZone = ItemZone::Data;
        int32_t insertRow = insertRowForZone(ItemZone::Data, curRow);

        for (int32_t i = 0; i < addNum; i++) {
            m_itemUnitList.insert(insertRow, itemUnit);
            insertRow++;
        }
        updateAfterModelChange();
    }
}

void FormPinHZ::on_CRCConfDone(int8_t validCode) {
    if (m_checkType == CheckType::None)
        return;
    QString str = "";
    if (validCode < 0) {
        switch (validCode) {
        case -1:
            str += "CRC width";
            break;
        case -2:
            str += "CRC poly";
            break;
        case -3:
            str += "CRC init";
            break;
        case -4:
            str += "CRC xorOut";
            break;
        default:
            str += "CRC未知";
            break;
        }
        str += "配置错误";
        emit showLog(LogLevel::ERR, str);
        return;
    }
    m_crcParamsPending = false;
    updateDataCheck();
    if (m_isAutoPin)
        QMetaObject::invokeMethod(ui->button_PinHZ, "clicked", Qt::QueuedConnection);
}

void FormPinHZ::on_button_addHead_clicked() {
    int32_t curRow = ui->table_PinHZ->currentRow();
    int32_t insertRow = insertRowForZone(ItemZone::Head, curRow);
    s_itemUnit itemUnit;
    m_itemTypeDlg->clearItemUnit(itemUnit);
    itemUnit.itemZone = ItemZone::Head;
    itemUnit.comment = "Head";

    m_itemUnitList.insert(insertRow, itemUnit);
    updateAfterModelChange();
}

void FormPinHZ::on_button_addItem_clicked() {
    m_editRow = -1;
    s_itemUnit blank = {};
    m_itemTypeDlg->clearItemUnit(blank);
    m_itemTypeDlg->loadItem(blank, false);
    m_itemTypeDlg->setWindowModality(Qt::NonModal);
    m_itemTypeDlg->setEndian(m_endian);
    m_itemTypeDlg->show();
}
void FormPinHZ::on_button_editItem_clicked() {
    int row = ui->table_PinHZ->currentRow();
    if (row < 0 || row >= m_itemUnitList.size()) {
        emit showLog(LogLevel::WAR, "请先选择条目。");
        return;
    }
    if (m_itemUnitList[row].itemZone == ItemZone::Check) {
        m_crcConfDlg->show();
        return;
    }
    m_editRow = row;
    m_itemTypeDlg->setEndian(m_endian);
    m_itemTypeDlg->loadItem(m_itemUnitList[row], true);
    m_itemTypeDlg->setWindowModality(Qt::WindowModal);
    m_itemTypeDlg->show();
    m_itemTypeDlg->raise();
}

void FormPinHZ::on_button_importTable_clicked() {
    FormTableImport dialog(m_endian, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    auto candidates = m_itemUnitList;
    const auto entries = dialog.items();
    for (const auto &entry : entries) {
        int index = 0;
        while (index < candidates.size() && candidates[index].itemZone <= entry.itemZone)
            ++index;
        candidates.insert(index, entry);
    }
    m_itemUnitList = candidates;
    updateAfterModelChange();
    emit showLog(LogLevel::INF, QString("从表格追加导入 %1 个字段。").arg(entries.size()));
}

void FormPinHZ::on_button_checkSet_clicked() {
    int32_t checkRow = modelCheckRow();

    if (checkRow >= 0) {
        m_itemUnitList.removeAt(checkRow);
        m_checkType = CheckType::None;
    } else {
        int32_t insertRow = m_itemUnitList.size();
        int32_t lastDataRow = -1;
        for (int32_t row = 0; row < m_itemUnitList.size(); row++) {
            if (m_itemUnitList.at(row).itemZone == ItemZone::Data)
                lastDataRow = row;
        }
        if (lastDataRow >= 0) {
            insertRow = lastDataRow + 1;
        } else {
            int32_t tailRow = firstRowOfZone(ItemZone::Tail);
            if (tailRow >= 0)
                insertRow = tailRow;
        }

        s_itemUnit itemUnit;
        m_itemTypeDlg->clearItemUnit(itemUnit);
        itemUnit.itemZone = ItemZone::Check;
        itemUnit.comment = "Check";
        itemUnit.comboIndex = 0;

        m_checkType = CheckType::CheckSum8;
        m_itemUnitList.insert(insertRow, itemUnit);
    }
    updateAfterModelChange();
}

void FormPinHZ::on_button_addTail_clicked() {
    int32_t curRow = ui->table_PinHZ->currentRow();
    int32_t insertRow = insertRowForZone(ItemZone::Tail, curRow);
    s_itemUnit itemUnit;
    m_itemTypeDlg->clearItemUnit(itemUnit);
    itemUnit.itemZone = ItemZone::Tail;
    itemUnit.comment = "Tail";

    m_itemUnitList.insert(insertRow, itemUnit);
    updateAfterModelChange();
}

void FormPinHZ::on_button_copyCurRow_clicked() {
    QSet<int32_t> selectedRows;
    for (const QModelIndex &index : ui->table_PinHZ->selectionModel()->selectedIndexes()) {
        selectedRows.insert(index.row());
    }

    QList<int32_t> rows = selectedRows.values();
    if (rows.isEmpty()) {
        emit showLog(LogLevel::WAR, "请先选中要复制的行!");
        return;
    }
    std::sort(rows.begin(), rows.end());

    if (rows.last() - rows.first() != rows.count() - 1) {
        emit showLog(LogLevel::ERR, "不支持非连续行复制!");
        return;
    }

    if (checkRowZone(rows.first()) != checkRowZone(rows.last())) {
        emit showLog(LogLevel::ERR, "不支持跨域复制!");
        return;
    }

    if (checkRowZone(rows.first()) == ItemZone::Check) {
        emit showLog(LogLevel::ERR, "不支持校验域复制!");
        return;
    }

    bool isValid = false;
    int copyNum = QInputDialog::getInt(this,
                                       "复制选中",   // title
                                       "复制份数: ", //  comment
                                       1,            // default
                                       1, 999,       // limit
                                       1,            // step
                                       &isValid);
    if (!isValid) {
        emit showLog(LogLevel::ERR, "复制份数错误!");
        return;
    }

    int32_t pasteRow = rows.last() + 1;
    s_itemUnit itemUnit;

    for (int32_t copyCnt = 0; copyCnt < copyNum; copyCnt++) {
        for (int32_t row : rows) {
            itemUnit = m_itemUnitList[row];
            itemUnit.itemId.clear();
            m_itemUnitList.insert(pasteRow, itemUnit);
            pasteRow++;
        }
    }

    updateAfterModelChange();
}

void FormPinHZ::on_button_subCurRow_clicked() {
    QSet<int32_t> selectedRows;
    for (const QModelIndex &index : ui->table_PinHZ->selectionModel()->selectedIndexes()) {
        selectedRows.insert(index.row());
    }

    QList<int32_t> rows = selectedRows.values();
    if (rows.isEmpty()) {
        emit showLog(LogLevel::WAR, "请先选中要删除的行!");
        return;
    }

    if (m_itemUnitList.size() - rows.size() < 1) {
        emit showLog(LogLevel::WAR, "模板至少保留一行, 已取消删除!");
        return;
    }

    std::sort(rows.begin(), rows.end(), std::greater<int32_t>());

    for (int32_t row : rows) {
        if (row >= 0 && row < m_itemUnitList.size())
            m_itemUnitList.removeAt(row);
    }

    updateAfterModelChange();
}

void FormPinHZ::on_button_fillCurRow_clicked() {
    QSet<int32_t> selectedRows;

    for (const QModelIndex &index : ui->table_PinHZ->selectionModel()->selectedIndexes()) {
        selectedRows.insert(index.row());
    }

    QList<int32_t> rows = selectedRows.values();

    std::sort(rows.begin(), rows.end());

    if (rows.isEmpty()) {
        emit showLog(LogLevel::WAR, "请先选中要填充的行!");
        return;
    }

    if (rows.last() - rows.first() != rows.count() - 1) {
        emit showLog(LogLevel::ERR, "不支持非连续域填充!");
        return;
    }

    if (checkRowZone(rows.first()) != checkRowZone(rows.last())) {
        emit showLog(LogLevel::ERR, "不支持跨域填充!");
        return;
    }

    if (checkRowZone(rows.first()) == ItemZone::Check) {
        emit showLog(LogLevel::ERR, "不支持校验域填充!");
        return;
    }

    if (checkRowZone(rows.first()) == ItemZone::Data) {
        foreach (int32_t row, rows) {
            if (m_itemUnitList[row].itemType != ItemType::Text) {
                emit showLog(LogLevel::ERR, "不支持下拉选项栏填充!");
                return;
            }
        }
    }

    m_fillItemDlg->m_fillStart = rows.first();
    m_fillItemDlg->m_fillRowCnt = rows.size();
    m_fillItemDlg->m_fillBytes = getRowsBytes(rows.first(), rows.last());
    m_fillItemDlg->show();
}

void FormPinHZ::on_button_PinHZ_clicked() {
    if (crcConfigPending()) {
        emit showLog(LogLevel::WAR, "CRC参数待配置, 请先在CRC配置窗口确认后再重拼帧!");
        if (m_crcConfDlg)
            m_crcConfDlg->show();
        return;
    }

    QString str = "";

    ui->plain_PinHZ->clear();
    PinHZ(str);
    ui->plain_PinHZ->setPlainText(str);
    emit PinHZ_outputChanged(str);
}

void FormPinHZ::on_button_PinHZSave_clicked() {
    QString defaultFileName = "PinHZ_";
    QString filePath = "";

    filePath = QFileDialog::getSaveFileName(this, tr("请选择或输入模板文件名"), defaultFileName,
                                            tr("模板文件(*.json)"));
    if (filePath.isEmpty()) {
        emit showLog(LogLevel::WAR, "未选择存储位置, 放弃保存!");
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit showLog(LogLevel::WAR, QString("模板文件存储位置非法, 放弃保存!") +
                                        FONT_COLOR_DARK_ORANGE + filePath);
        return;
    }

    int32_t rowCnt = m_itemUnitList.size();
    QJsonObject rootObj;
    QJsonObject baseInfo;
    baseInfo["checkStartRow"] = m_checkStartRow;
    baseInfo["checkEndRow"] = m_checkEndRow;
    baseInfo["lengthItemId"] = m_lengthItemId;
    baseInfo["rowCnt"] = rowCnt;
    baseInfo["rowCntHead"] = m_rowCntHead;
    baseInfo["rowCntData"] = m_rowCntData;
    baseInfo["rowCntTail"] = m_rowCntTail;

    int32_t checkType = -1;
    int32_t checkRow = modelCheckRow();
    if (checkRow >= 0) {
        int32_t comboIndex = m_itemUnitList.at(checkRow).comboIndex;
        if (comboIndex >= 0 && comboIndex <= static_cast<int32_t>(CheckType::CRC))
            checkType = comboIndex;
    }
    if (checkType < 0 && m_checkType != CheckType::None)
        checkType = static_cast<int32_t>(m_checkType);
    baseInfo["isChecked"] = checkType >= 0 ? 1 : 0;
    baseInfo["checkType"] = checkType;
    baseInfo["endian"] = static_cast<int32_t>(m_endian);
    if (checkType == static_cast<int32_t>(CheckType::CRC) && m_crcParamsPending) {
        baseInfo["paramsStatus"] = "pending";
        baseInfo["paramsNote"] =
            "CSV 未提供 CRC width/poly/init/refIn/refOut/xorOut, 请用 CRC 配置窗口确认后再使用";
    }
    rootObj["baseInfo"] = baseInfo;

    if (m_crcConfDlg) {
        QJsonObject crcObj;
        if (m_crcParamsPending) {
            crcObj["width"] = 0;
            crcObj["poly"] = 0;
            crcObj["init"] = 0;
            crcObj["refIn"] = 0;
            crcObj["refOut"] = 0;
            crcObj["xorOut"] = 0;
            crcObj["status"] = "pending";
            crcObj["paramsNote"] =
                "CSV 未提供 CRC width/poly/init/refIn/refOut/xorOut, 请用 CRC 配置窗口确认后再使用";
        } else {
            const s_crc_conf &crcConf = m_crcConfDlg->m_crcConf;
            crcObj["width"] = static_cast<int32_t>(crcConf.width);
            crcObj["poly"] = static_cast<int32_t>(crcConf.poly);
            crcObj["init"] = static_cast<int32_t>(crcConf.init);
            crcObj["refIn"] = crcConf.ref_in ? 1 : 0;
            crcObj["refOut"] = crcConf.ref_out ? 1 : 0;
            crcObj["xorOut"] = static_cast<int32_t>(crcConf.xor_out);
            crcObj["status"] = "ok";
        }
        rootObj["crc"] = crcObj;
    }

    QJsonArray rowArr;
    for (int32_t row = 0; row < rowCnt; row++) {
        QJsonObject rowObj;
        const s_itemUnit &itemUnit = m_itemUnitList.at(row);
        rowObj["itemType"] = toInt32(itemUnit.itemType);
        rowObj["itemZone"] = toInt32(itemUnit.itemZone);
        rowObj["dataType"] = toInt32(itemUnit.dataType);
        rowObj["comboIndex"] = itemUnit.comboIndex;
        rowObj["comment"] = itemUnit.comment;
        if (itemUnit.itemType == ItemType::Text) {
            AppItemValue::writeRules(rowObj, itemUnit);
            rowObj["valueStr"] = itemUnit.valueStr;
        } else {
            QJsonArray optionArr;
            int32_t optionCnt = qMin(itemUnit.optionList.size(), itemUnit.keyList.size());
            rowObj["comboCnt"] = itemUnit.comboCnt;
            for (int32_t option = 0; option < optionCnt; option++) {
                QJsonObject optionObj;
                optionObj[QString("option_%1").arg(option)] = itemUnit.optionList.at(option);
                optionObj[QString("key_%1").arg(option)] = itemUnit.keyList.at(option);
                optionArr.append(optionObj);
            }
            rowObj["options"] = optionArr;
        }
        rowArr.append(rowObj);
    }
    rootObj["data"] = rowArr;

    QJsonDocument doc(rootObj);
    QString jsonStr = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << jsonStr;
    file.close();

    emit showLog(LogLevel::INF, "模板文件存储完成, 文件保存于:" FONT_COLOR_DARK_ORANGE + filePath);
}

void FormPinHZ::on_button_PinHZLoad_clicked() {
    QString filePath = "", str = "";

    filePath = QFileDialog::getOpenFileName(this, tr("请选择模板文件"), "", tr("模板文件(*.json)"));
    if (filePath.isEmpty()) {
        emit showLog(LogLevel::WAR, "未正确选择文件, 放弃处理");
        return;
    }

    str = "开始读取模板<br>" FONT_COLOR_DARK_ORANGE + filePath + FONT_COLOR_BLACK "<br>请稍后...";
    QMetaObject::invokeMethod(this, "showLog", Qt::DirectConnection, Q_ARG(LogLevel, LogLevel::INF),
                              Q_ARG(QString, str));
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit showLog(LogLevel::ERR, "模板读取失败, 文件开启失败");
        return;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    QString jsonString = stream.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8(), &error);

    if (error.error != QJsonParseError::NoError) {
        emit showLog(LogLevel::ERR, QString("模板读取失败, json解析失败") + error.errorString());
        return;
    }

    int32_t rowCnt;
    QJsonObject rootObj = doc.object();
    QJsonObject baseInfo = rootObj["baseInfo"].toObject();
    rowCnt = baseInfo["rowCnt"].toInt();
    QJsonArray rowArr = rootObj["data"].toArray();

    if (rowCnt <= 0 || rowArr.isEmpty()) {
        emit showLog(LogLevel::ERR, "模板读取失败, 模板内容为空");
        return;
    }

    int32_t savedCheckType = baseInfo["checkType"].toInt(-1);
    if (savedCheckType < 0 && baseInfo["isChecked"].toInt(0) != 0)
        savedCheckType = static_cast<int32_t>(CheckType::CheckSum8);

    int32_t endianVal = baseInfo["endian"].toInt(static_cast<int32_t>(m_endian));
    const Endian candidateEndian =
        endianVal == static_cast<int>(Endian::BigEndian) ? Endian::BigEndian : Endian::LittleEndian;
    // Validate before touching the existing template, endian selection, or CRC configuration.
    for (int row = 0; row < qMin(rowCnt, rowArr.size()); ++row) {
        const QJsonObject json = rowArr[row].toObject();
        s_itemUnit candidate = {};
        candidate.dataType = static_cast<DataType>(qBound(0, json["dataType"].toInt(), 9));
        if (json["itemZone"].toInt() == toInt32(ItemZone::Check))
            continue;
        QString error, display;
        if (json["itemType"].toInt() == toInt32(ItemType::Text)) {
            AppItemValue::readRules(json, candidate);
            candidate.valueStr = json["valueStr"].toString();
            AppItemValue::display(candidate, candidateEndian, display, error);
        } else {
            const QJsonArray options = json["options"].toArray();
            if (options.isEmpty())
                error = "下拉条目没有选项";
            for (int i = 0; i < options.size() && error.isEmpty(); ++i) {
                const auto option = options[i].toObject();
                if (option[QString("option_%1").arg(i)].toString().trimmed().isEmpty())
                    error = "下拉选项文本为空";
                else
                    AppItemValue::normalizeHex(option[QString("key_%1").arg(i)].toString(),
                                               candidate.dataType, candidateEndian, display, error);
            }
        }
        if (!error.isEmpty()) {
            emit showLog(LogLevel::ERR, QString("模板第 %1 行无效：%2；已保留当前模板")
                                            .arg(row + 1)
                                            .arg(error.toHtmlEscaped()));
            return;
        }
    }
    if (endianVal == static_cast<int32_t>(Endian::BigEndian) ||
        endianVal == static_cast<int32_t>(Endian::LittleEndian))
        m_endian = static_cast<Endian>(endianVal);
    ui->combo_hexOrder->blockSignals(true);
    ui->combo_hexOrder->setCurrentIndex(m_endian == Endian::LittleEndian ? 0 : 1);
    ui->combo_hexOrder->blockSignals(false);

    bool loadCrcPending =
        savedCheckType == static_cast<int32_t>(CheckType::CRC) && !rootObj.contains("crc");
    if (m_crcConfDlg && rootObj.contains("crc")) {
        QJsonObject crcObj = rootObj["crc"].toObject();
        loadCrcPending = crcObj["status"].toString().toLower() == "pending";
        int32_t crcWidth = crcObj["width"].toInt(m_crcConfDlg->m_crcConf.width);
        if (crcWidth <= 0 || crcWidth > 32)
            loadCrcPending = true;

        if (!loadCrcPending) {
            s_crc_conf crcConf = m_crcConfDlg->m_crcConf;
            crcConf.width = static_cast<uint8_t>(crcWidth);
            crcConf.poly = static_cast<uint32_t>(crcObj["poly"].toInt(crcConf.poly));
            crcConf.init = static_cast<uint32_t>(crcObj["init"].toInt(crcConf.init));
            crcConf.ref_in = crcObj["refIn"].toInt(crcConf.ref_in ? 1 : 0) != 0;
            crcConf.ref_out = crcObj["refOut"].toInt(crcConf.ref_out ? 1 : 0) != 0;
            crcConf.xor_out = static_cast<uint32_t>(crcObj["xorOut"].toInt(crcConf.xor_out));
            m_crcConfDlg->setCrcConf(crcConf);
        }
    }

    m_itemUnitList.clear();
    m_checkType = CheckType::None;
    if (rowCnt > rowArr.size())
        rowCnt = rowArr.size();

    for (int32_t row = 0; row < rowCnt; row++) {
        QJsonObject rowObj = rowArr[row].toObject();
        s_itemUnit itemUnit;
        m_itemTypeDlg->clearItemUnit(itemUnit);

        int32_t itemTypeVal = rowObj["itemType"].toInt(toInt32(ItemType::Text));
        itemUnit.itemType =
            itemTypeVal == toInt32(ItemType::Combo) ? ItemType::Combo : ItemType::Text;
        int32_t zoneVal =
            qBound(0, rowObj["itemZone"].toInt(toInt32(ItemZone::Data)), toInt32(ItemZone::Tail));
        itemUnit.itemZone = static_cast<ItemZone>(zoneVal);
        int32_t dataTypeVal =
            qBound(0, rowObj["dataType"].toInt(toInt32(DataType::U08)), toInt32(DataType::F64));
        itemUnit.dataType = static_cast<DataType>(dataTypeVal);
        itemUnit.comment = rowObj["comment"].toString();

        if (itemUnit.itemType == ItemType::Text) {
            AppItemValue::readRules(rowObj, itemUnit);
            itemUnit.valueStr = rowObj["valueStr"].toString();
        } else {
            QJsonArray optionArr = rowObj["options"].toArray();
            int32_t optionCnt = optionArr.size();
            itemUnit.comboCnt = rowObj["comboCnt"].toInt();
            for (int32_t option = 0; option < optionCnt; option++) {
                QJsonObject optionObj = optionArr[option].toObject();
                str = optionObj[QString("option_%1").arg(option)].toString();
                itemUnit.optionList.append(str);
                str = optionObj[QString("key_%1").arg(option)].toString();
                itemUnit.keyList.append(str);
            }
        }

        int32_t optionCnt = qMin(itemUnit.optionList.size(), itemUnit.keyList.size());
        if (itemUnit.itemZone == ItemZone::Check) {
            int32_t comboIndex = rowObj["comboIndex"].toInt(savedCheckType);
            itemUnit.comboIndex = qBound(0, comboIndex, static_cast<int32_t>(CheckType::CRC));
        } else if (itemUnit.itemType == ItemType::Combo) {
            itemUnit.comboIndex = rowObj["comboIndex"].toInt(0);
            itemUnit.comboIndex = qBound(0, itemUnit.comboIndex, qMax(0, optionCnt - 1));
            itemUnit.comboCnt = qMax(itemUnit.comboCnt, optionCnt);
            if (optionCnt > 0 && itemUnit.comboIndex < itemUnit.keyList.size())
                itemUnit.valueStr = itemUnit.keyList.at(itemUnit.comboIndex);
        } else {
            itemUnit.comboIndex =
                qBound(0, rowObj["comboIndex"].toInt(0), static_cast<int32_t>(CheckType::CRC));
        }

        m_itemUnitList.append(itemUnit);
    }

    if (m_itemUnitList.isEmpty()) {
        emit showLog(LogLevel::ERR, "模板读取失败, 未能解析出有效行");
        return;
    }

    int32_t loadedCheckRow = modelCheckRow();
    bool checkRowUsesCrc = loadedCheckRow >= 0 && m_itemUnitList.at(loadedCheckRow).comboIndex ==
                                                      static_cast<int32_t>(CheckType::CRC);
    if (checkRowUsesCrc && !rootObj.contains("crc"))
        loadCrcPending = true;
    if (!checkRowUsesCrc)
        loadCrcPending = false;

    m_crcParamsPending = loadCrcPending;
    m_checkStartRow = baseInfo["checkStartRow"].toInt();
    m_checkEndRow = baseInfo["checkEndRow"].toInt();
    m_lengthItemId = baseInfo["lengthItemId"].toString();
    if ((m_checkStartRow == 0) != (m_checkEndRow == 0) || m_checkStartRow < 0 ||
        m_checkEndRow < m_checkStartRow || m_checkEndRow > m_itemUnitList.size()) {
        m_checkStartRow = m_checkEndRow = 0;
        emit showLog(LogLevel::WAR, tr("模板校验行范围无效，已恢复数据域范围。"));
    }
    updateAfterModelChange();
    invokeAutoPin();
    if (loadCrcPending) {
        emit showLog(LogLevel::WAR,
                     "模板CRC参数待配置, 已暂停自动组帧/发送, 请用CRC配置窗口确认参数后再操作!");
        if (m_crcConfDlg)
            m_crcConfDlg->show();
    }
    emit showLog(LogLevel::INF, "模板读取成功");
}

void FormPinHZ::on_button_PinHZSend_clicked() {
    if (crcConfigPending()) {
        emit showLog(LogLevel::WAR, "CRC参数待配置, 配置完成前禁止发送拼好帧!");
        return;
    }

    QString error;
    if (!validateTextRows(error)) {
        emit showLog(LogLevel::ERR, error.toHtmlEscaped());
        return;
    }
    QString str = ui->plain_PinHZ->toPlainText();
    QByteArray data;
    int32_t len = 0;

    len = PinHZ_parse(str, data);
    if (len <= 0)
        return;
    emit PinHZ_send(data, len);
}

void FormPinHZ::on_button_PinHZDeal_clicked() {
    QString str = ui->plain_PinHZ->toPlainText();
    QByteArray data;
    int32_t dataLen = 0, dataIndex = 0;

    dataLen = PinHZ_parse(str, data);
    if (dataLen <= 0)
        return;
    dataIndex = PinHZ_deal2Table(data);
    if ((!m_lastParseSucceeded || dataIndex != dataLen) &&
        QMessageBox::question(this, tr("解帧异常"),
                              tr("是否强行解析？只按顺序更新能够读取的完整字段；未知 combo "
                                 "保留原值，本次不保存 CSV。"),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) == QMessageBox::Yes)
        dataIndex = PinHZ_deal2Table(data, true);

    if (dataIndex != dataLen) {
        str = QString::asprintf("模板不匹配, 模板:%dBytes, 数据:%dBytes", dataIndex, dataLen);
        emit showLog(LogLevel::WAR, str);
        return;
    }
}

void FormPinHZ::on_combo_PinHZ_dataTypeChanged(int index) {
    QComboBox *comboBox = qobject_cast<QComboBox *>(this->sender());
    int32_t row = rowOfCellWidget(comboBox);
    if (row < 0 || row >= m_itemUnitList.size())
        return;
    if (m_itemUnitList[row].itemZone == ItemZone::Check)
        return;

    DataType newDataType =
        static_cast<DataType>(qBound(0, index, static_cast<int32_t>(DataType::F64)));
    if (m_itemUnitList[row].dataType == newDataType)
        return;

    s_itemUnit candidate = m_itemUnitList[row];
    candidate.dataType = newDataType;
    QString display, error;
    AppItemValue::normalizeHex(candidate.valueStr, newDataType, m_endian, candidate.valueStr, error,
                               true);
    // Type selection changes storage width. Rules remain attached and are shown
    // as validation errors in the row; they must not lock the type selector.
    if (candidate.itemType == ItemType::Combo) {
        for (QString &key : candidate.keyList) {
            if (!AppItemValue::normalizeHex(key, newDataType, m_endian, key, error, true)) {
                const QSignalBlocker block(comboBox);
                comboBox->setCurrentIndex(static_cast<int>(m_itemUnitList[row].dataType));
                emit showLog(LogLevel::ERR, QString("类型修改失败：%1").arg(error.toHtmlEscaped()));
                return;
            }
        }
    }
    m_itemUnitList[row] = candidate;
    updateRowView(row);
    invokeAutoPin();
}

void FormPinHZ::on_combo_PinHZ_checkChanged(int index) {
    int32_t checkIndex = qBound(0, index, static_cast<int32_t>(CheckType::CRC));
    QComboBox *comboBox = qobject_cast<QComboBox *>(this->sender());
    int32_t row = rowOfCellWidget(comboBox);
    if (row >= 0 && row < m_itemUnitList.size())
        m_itemUnitList[row].comboIndex = checkIndex;

    m_checkType = static_cast<CheckType>(checkIndex);
    if (m_checkType == CheckType::CRC) {
        m_crcConfDlg->show();
        return;
    }
    m_crcParamsPending = false;

    if (m_crcConfDlg->isVisible())
        m_crcConfDlg->close();
    updateDataCheck();
    invokeAutoPin();
}

void FormPinHZ::on_combo_PinHZ_customComboChanged(int index) {
    QComboBox *comboBox = qobject_cast<QComboBox *>(this->sender());
    int32_t row = rowOfCellWidget(comboBox);
    if (row < 0 || row >= m_itemUnitList.size())
        return;

    s_itemUnit &itemUnit = m_itemUnitList[row];
    if (itemUnit.itemZone == ItemZone::Check || itemUnit.itemType != ItemType::Combo)
        return;

    int32_t comboCount = qMin(itemUnit.optionList.size(), itemUnit.keyList.size());
    if (comboCount <= 0)
        return;

    int32_t comboIndex = qBound(0, index, comboCount - 1);
    itemUnit.comboIndex = comboIndex;
    if (comboIndex < itemUnit.keyList.size()) {
        itemUnit.valueStr = normalizeDisplayHex(itemUnit.keyList.at(comboIndex),
                                                dataTypeByteLen(itemUnit.dataType), m_endian);
    }

    QComboBox *optionCombo =
        qobject_cast<QComboBox *>(ui->table_PinHZ->cellWidget(row, colDataDec));
    QComboBox *keyCombo = qobject_cast<QComboBox *>(ui->table_PinHZ->cellWidget(row, colDataHex));
    if (optionCombo && optionCombo->count() == comboCount) {
        optionCombo->blockSignals(true);
        optionCombo->setCurrentIndex(comboIndex);
        optionCombo->blockSignals(false);
    }
    if (keyCombo && keyCombo->count() == comboCount) {
        keyCombo->blockSignals(true);
        keyCombo->setCurrentIndex(comboIndex);
        keyCombo->blockSignals(false);
    }

    updateDataZoneBytes();
    updateDataCheck();
    invokeAutoPin();
}

void FormPinHZ::on_combo_hexOrder_currentIndexChanged(int index) {
    const Endian next = index == 0 ? Endian::LittleEndian : Endian::BigEndian;
    if (next != m_endian)
        for (auto &entry : m_itemUnitList) {
            if (entry.itemZone == ItemZone::Check)
                continue;
            auto swap = [this, &entry](const QString &hex) {
                return reverseHexBytes(
                    normalizeDisplayHex(hex, dataTypeByteLen(entry.dataType), m_endian));
            };
            entry.valueStr = swap(entry.valueStr);
            for (auto &key : entry.keyList)
                key = swap(key);
        }
    m_endian = next;
    refreshTable();
}

void FormPinHZ::on_table_PinHZ_itemChanged(QTableWidgetItem *item) {
    if (!item || m_isRebuilding)
        return;
    const int row = item->row(), col = item->column();
    if (row < 0 || row >= m_itemUnitList.size())
        return;
    if (col == colComment) {
        m_itemUnitList[row].comment = item->text();
        return;
    }
    applyValueEdit(item, item->text());
}

void FormPinHZ::applyValueEdit(QTableWidgetItem *item, const QString &text) {
    if (!item || m_isRebuilding)
        return;
    const int row = item->row(), col = item->column();
    if (row < 0 || row >= m_itemUnitList.size() || !(item->flags() & Qt::ItemIsEditable))
        return;
    if (m_itemUnitList[row].itemType != ItemType::Text ||
        m_itemUnitList[row].itemZone == ItemZone::Check)
        return;
    if (col != colDataHex && col != colDataDec)
        return;

    s_itemUnit candidate = m_itemUnitList[row];
    QString hex, display, error;
    bool valid;
    if (col == colDataHex) {
        valid = AppItemValue::normalizeHex(text, candidate.dataType, Endian::BigEndian, hex, error);
        if (valid && m_endian == Endian::LittleEndian)
            hex = reverseHexBytes(hex);
    } else {
        valid = AppItemValue::fromDisplay(candidate, text, m_endian, hex, error);
    }
    if (valid) {
        candidate.valueStr = hex;
        valid = AppItemValue::validateInput(candidate, m_endian, display, error);
    }
    if (valid)
        m_itemUnitList[row] = candidate;
    updateRowView(row);
    if (!valid) {
        const QString message = QString("第 %1 行：%2（已保留原值）").arg(row + 1).arg(error);
        emit showLog(LogLevel::ERR, message.toHtmlEscaped());
        {
            const QSignalBlocker block(ui->table_PinHZ);
            item->setToolTip(message);
        }
        QToolTip::showText(ui->table_PinHZ->viewport()->mapToGlobal(
                               ui->table_PinHZ->visualItemRect(item).bottomLeft()),
                           message, ui->table_PinHZ);
        return;
    }
    invokeAutoPin();
}

void FormPinHZ::on_check_autoPinHZ_toggled(bool checked) {
    if (checked) {
        m_isAutoPin = true;
        invokeAutoPin();
    } else {
        m_isAutoPin = false;
    }
}

void FormPinHZ::on_check_autoPinHZDeal_toggled(bool checked) {
    m_isAutoDeal = checked;
    m_receivePending.clear();
}

void FormPinHZ::on_check_autoReply_toggled(bool checked) {
    m_isAutoReply = checked;
}

void FormPinHZ::on_check_autoSend_toggled(bool checked) {
    m_isAutoSend = checked;
}

void FormPinHZ::on_check_fieldPinHZ_toggled(bool checked) {
    m_isFieldPinHZ = checked;
    QMetaObject::invokeMethod(ui->button_PinHZ, "clicked", Qt::QueuedConnection);
}

namespace {
QString csvField(QString text) {
    text.replace('"', "\"\"");
    return '"' + text + '"';
}
QByteArray csvRecord(const QStringList &values) {
    QStringList escaped;
    for (const auto &value : values)
        escaped.append(csvField(value));
    return (escaped.join(',') + "\r\n").toUtf8();
}
} // namespace

bool FormPinHZ::beginTemplateSave(const QString &path, QString &error) {
    error.clear();
    QStringList columns;
    for (const auto &entry : m_itemUnitList)
        columns.append(entry.comment.isEmpty() ? "-" : entry.comment);
    if (columns.isEmpty()) {
        error = tr("模板没有字段。");
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        error = file.errorString();
        return false;
    }
    const QByteArray record = csvRecord(columns);
    if (file.write(record) != record.size() || !file.commit()) {
        error = file.errorString();
        return false;
    }
    m_saveColumns = columns;
    m_saveAsTempPath = path;
    m_isSaveAsTemp = true;
    const QSignalBlocker block(ui->check_saveAsTemp);
    ui->check_saveAsTemp->setChecked(true);
    return true;
}
void FormPinHZ::saveParsedFrame() {
    if (!m_isSaveAsTemp || m_saveAsTempPath.isEmpty())
        return;
    QStringList columns, values;
    for (const auto &entry : m_itemUnitList) {
        columns.append(entry.comment.isEmpty() ? "-" : entry.comment);
        QString value, error;
        if (entry.itemType == ItemType::Combo)
            value = entry.optionList.value(entry.comboIndex, entry.valueStr);
        else if (entry.itemZone == ItemZone::Check)
            value = entry.valueStr;
        else if (!AppItemValue::display(entry, m_endian, value, error))
            value = entry.valueStr;
        values.append(value);
    }
    QString error;
    if (columns != m_saveColumns)
        error = tr("模板列已变化，请重新选择保存文件。");
    else {
        QFile file(m_saveAsTempPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Append))
            error = file.errorString();
        else {
            const QByteArray record = csvRecord(values);
            if (file.write(record) != record.size() || !file.flush())
                error = file.errorString();
        }
    }
    if (!error.isEmpty()) {
        m_isSaveAsTemp = false;
        m_saveAsTempPath.clear();
        const QSignalBlocker block(ui->check_saveAsTemp);
        ui->check_saveAsTemp->setChecked(false);
        emit showLog(LogLevel::ERR, tr("按模板保存已停止：%1").arg(error.toHtmlEscaped()));
    }
}
void FormPinHZ::on_check_saveAsTemp_toggled(bool checked) {
    if (!checked) {
        m_isSaveAsTemp = false;
        m_saveAsTempPath.clear();
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("保存解帧 CSV"), "PinHZ_SaveAs_",
                                                      tr("CSV 文件 (*.csv)"));
    QString error;
    if (path.isEmpty() || !beginTemplateSave(path, error)) {
        m_isSaveAsTemp = false;
        m_saveAsTempPath.clear();
        const QSignalBlocker block(ui->check_saveAsTemp);
        ui->check_saveAsTemp->setChecked(false);
        if (!error.isEmpty())
            emit showLog(LogLevel::ERR, error.toHtmlEscaped());
    }
}

void FormPinHZ::on_spin_replyTime_valueChanged(int arg1) {
    m_autoReplyDelay = arg1;
}

void FormPinHZ::on_spin_sendPeriod_valueChanged(int arg1) {
    m_autoSendPeriod = arg1 / 10;
}

/* **************************************************
 * @brief 10ms Timer Slot
 * **************************************************/
void FormPinHZ::on_timerOut() {
    m_timerCnt_10ms++;
    static int32_t replyDelayCnt = 0;
    if (m_autoReplyTimes > 0 && !crcConfigPending()) {
        if (replyDelayCnt >= m_autoReplyDelay) {
            QMetaObject::invokeMethod(ui->button_PinHZSend, "clicked", Qt::QueuedConnection);
            replyDelayCnt = 0;
            m_autoReplyTimes--;
        }
        replyDelayCnt += 10;
    }
    if (isAutoSend() && !crcConfigPending()) {
        if (m_timerCnt_10ms % m_autoSendPeriod == 0)
            QMetaObject::invokeMethod(ui->button_PinHZSend, "clicked", Qt::QueuedConnection);
    }
}

QByteArray FormPinHZ::checkRangeBytes(const QByteArray *received) const {
    QByteArray bytes;
    int offset = 0;
    for (int row = 0; row < m_itemUnitList.size(); ++row) {
        QByteArray part = rowWireBytes(row);
        if (received)
            part = received->mid(offset, part.size());
        offset += rowWireBytes(row).size();
        bool selected = m_checkStartRow > 0 ? row + 1 >= m_checkStartRow && row + 1 <= m_checkEndRow
                                            : m_itemUnitList[row].itemZone == ItemZone::Data;
        if (selected && m_itemUnitList[row].itemZone != ItemZone::Check)
            bytes += part;
    }
    return bytes;
}
void FormPinHZ::on_button_frameSettings_clicked() {
    QDialog dialog(this);
    Ui::FormFrameSettings settings;
    settings.setupUi(&dialog);
    settings.spin_start->setMaximum(m_itemUnitList.size());
    settings.spin_end->setMaximum(m_itemUnitList.size());
    settings.spin_start->setValue(m_checkStartRow);
    settings.spin_end->setValue(m_checkEndRow);
    connect(settings.buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(settings.buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        const int first = settings.spin_start->value(), last = settings.spin_end->value();
        if ((first == 0) != (last == 0) || first > last) {
            settings.label_status->setText("校验首尾行应同时为 0，或从 1 开始且首行不大于末行。");
            return;
        }
        m_checkStartRow = first;
        m_checkEndRow = last;
        dialog.accept();
    });
    if (dialog.exec() == QDialog::Accepted) {
        refreshTable();
    }
}

void FormPinHZ::setLengthBinding(const QString &itemId) {
    if (itemId == m_lengthItemId)
        return;
    m_lengthItemId = itemId;
    refreshTable();
}
