#include "AppItemValue.h"
#include "AppCalcExpression.h"
#include <QRegularExpression>
#include <cmath>
#include <cstring>
#include <limits>

namespace {
bool reject(QString &error, const QString &message) {
    error = message;
    return false;
}
bool numeric(QString text, double &value, QString &error) {
    static const QRegularExpression number(
        "\\A[+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)(?:[eE][+-]?[0-9]+)?\\z");
    bool ok = false;
    value = text.toDouble(&ok);
    return (number.match(text).hasMatch() && ok && std::isfinite(value)) ||
           reject(error, "请输入有限数值，可使用小数或科学计数法");
}
bool rules(const s_itemUnit &item, const QString &text, double value, QString &error) {
    if (item.lowerLimitEnabled && value < item.lowerLimit)
        return reject(error, QString("显示值不能小于 %1").arg(item.lowerLimit, 0, 'g', 17));
    if (item.upperLimitEnabled && value > item.upperLimit)
        return reject(error, QString("显示值不能大于 %1").arg(item.upperLimit, 0, 'g', 17));
    if (!item.regex.isEmpty()) {
        QRegularExpression pattern("\\A(?:" + item.regex + ")\\z");
        if (!pattern.isValid())
            return reject(error, "正则表达式无效：" + pattern.errorString());
        if (!pattern.match(text).hasMatch())
            return reject(error, "显示值不符合正则表达式：" + item.regex);
    }
    return true;
}
bool signedType(DataType type) {
    return type == DataType::S08 || type == DataType::S16 || type == DataType::S32 ||
           type == DataType::S64;
}
quint64 bitsFromHex(const QString &hex, Endian endian) {
    QByteArray bytes = QByteArray::fromHex(hex.toLatin1());
    quint64 bits = 0;
    for (int i = 0; i < bytes.size(); ++i) {
        int offset = endian == Endian::LittleEndian ? i : bytes.size() - i - 1;
        bits |= quint64(quint8(bytes[i])) << (offset * 8);
    }
    return bits;
}
QString hexFromBits(quint64 bits, int size, Endian endian) {
    QByteArray bytes(size, 0);
    for (int i = 0; i < size; ++i) {
        int offset = endian == Endian::LittleEndian ? i : size - i - 1;
        bytes[i] = char((bits >> (offset * 8)) & 0xff);
    }
    return QString::fromLatin1(bytes.toHex().toUpper());
}
bool rawValue(const QString &hex, DataType type, Endian endian, QString &text, double &value,
              QString &error) {
    quint64 bits = bitsFromHex(hex, endian);
    int width = AppItemValue::byteSize(type) * 8;
    if (type == DataType::F32) {
        quint32 word = quint32(bits);
        float real;
        std::memcpy(&real, &word, sizeof(real));
        value = real;
        text = QString::number(value, 'g', std::numeric_limits<float>::max_digits10);
    } else if (type == DataType::F64) {
        std::memcpy(&value, &bits, sizeof(value));
        text = QString::number(value, 'g', std::numeric_limits<double>::max_digits10);
    } else if (signedType(type)) {
        if (width < 64 && (bits & (quint64(1) << (width - 1))))
            bits |= (~quint64(0)) << width;
        qint64 integer;
        std::memcpy(&integer, &bits, sizeof(integer));
        text = QString::number(integer);
        value = double(integer);
    } else {
        text = QString::number(bits);
        value = double(bits);
    }
    return std::isfinite(value) || reject(error, "原始浮点值为 NaN 或无穷大，无法换算");
}
bool encode(const QString &text, double value, DataType type, Endian endian, bool calculated,
            QString &hex, QString &error) {
    quint64 bits = 0;
    const int width = AppItemValue::byteSize(type) * 8;
    if (type == DataType::F32) {
        float real = float(value);
        if (!std::isfinite(real))
            return reject(error, "数值超出 float 范围");
        if (value != 0 && real == 0)
            return reject(error, "数值太小，float 无法表示非零结果");
        quint32 word;
        std::memcpy(&word, &real, sizeof(word));
        bits = word;
    } else if (type == DataType::F64) {
        std::memcpy(&bits, &value, sizeof(bits));
    } else {
        const bool isSigned = signedType(type);
        static const QRegularExpression integer("\\A[+-]?[0-9]+\\z");
        if (!calculated && integer.match(text).hasMatch()) {
            bool ok = false;
            if (isSigned) {
                qint64 number = text.toLongLong(&ok);
                const qint64 min =
                    width == 64 ? std::numeric_limits<qint64>::min() : -(qint64(1) << (width - 1));
                const qint64 max = width == 64 ? std::numeric_limits<qint64>::max()
                                               : (qint64(1) << (width - 1)) - 1;
                if (!ok || number < min || number > max)
                    return reject(error, "数值超出所选有符号整数类型范围");
                bits = quint64(number);
            } else {
                bits = text.toULongLong(&ok);
                const quint64 max =
                    width == 64 ? std::numeric_limits<quint64>::max() : (quint64(1) << width) - 1;
                if (!ok || text.startsWith('-') || bits > max)
                    return reject(error, "数值超出所选无符号整数类型范围");
            }
        } else {
            double rounded = std::round(value);
            // Permit floating point roundoff in inverse functions, never truncate a fraction.
            if (std::fabs(value - rounded) > (calculated ? 1e-9 : 0.0))
                return reject(error,
                              "换算结果不是整数；请选择浮点类型或在公式中使用 round/floor/ceil");
            if (width == 64 && std::fabs(rounded) > 9007199254740991.0)
                return reject(
                    error, "64 位整数公式结果超过双精度的精确整数范围，请使用 HEX 或原始整数输入");
            const double low = isSigned ? -std::ldexp(1.0, width - 1) : 0;
            const double highExclusive = std::ldexp(1.0, isSigned ? width - 1 : width);
            if (rounded < low || rounded >= highExclusive)
                return reject(error, "换算结果超出所选数据类型范围");
            bits = isSigned ? quint64(qint64(rounded)) : quint64(rounded);
        }
    }
    hex = hexFromBits(bits, width / 8, endian);
    return true;
}
} // namespace

int AppItemValue::byteSize(DataType type) {
    switch (type) {
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
    }
    return 0;
}
bool AppItemValue::identity(const QString &formula) {
    return formula.trimmed().isEmpty() || formula.trimmed().compare("x", Qt::CaseInsensitive) == 0;
}
bool AppItemValue::editable(const s_itemUnit &item) {
    return identity(item.ioFormula) || !item.reverseFormula.trimmed().isEmpty();
}
bool AppItemValue::normalizeHex(QString text, DataType type, Endian endian, QString &hex,
                                QString &error, bool allowNarrow) {
    error.clear();
    text = text.trimmed();
    if (text.startsWith("0x", Qt::CaseInsensitive))
        text.remove(0, 2);
    text.remove(QRegularExpression("\\s"));
    static const QRegularExpression valid("\\A[0-9a-fA-F]+\\z");
    if (!valid.match(text).hasMatch())
        return reject(error, "HEX 只能包含 0–9、A–F，可带 0x 前缀及空格");
    const int target = byteSize(type) * 2;
    if (!target)
        return reject(error, QString("HEX 超出当前类型的 %1 字节宽度").arg(target / 2));
    if (text.size() % 2)
        text.prepend('0');
    if (text.size() > target) {
        if (!allowNarrow)
            return reject(error, QString("HEX 超出当前类型的 %1 字节宽度").arg(target / 2));
        text = endian == Endian::LittleEndian ? text.left(target) : text.right(target);
    }
    text = endian == Endian::LittleEndian ? text.leftJustified(target, '0')
                                          : text.rightJustified(target, '0');
    hex = text.toUpper();
    return true;
}
bool AppItemValue::validateConfig(const s_itemUnit &item, QString &error) {
    error.clear();
    if ((item.upperLimitEnabled && !std::isfinite(item.upperLimit)) ||
        (item.lowerLimitEnabled && !std::isfinite(item.lowerLimit)))
        return reject(error, "上下限必须是有限数值");
    if (item.upperLimitEnabled && item.lowerLimitEnabled && item.lowerLimit > item.upperLimit)
        return reject(error, "输入下限不能大于上限");
    if (!AppCalcExpression::validate(item.ioFormula, error)) {
        error.prepend("正向公式：");
        return false;
    }
    if (!AppCalcExpression::validate(item.reverseFormula, error)) {
        error.prepend("反向公式：");
        return false;
    }
    if (item.regex.size() > 1024)
        return reject(error, "正则表达式不能超过 1024 个字符");
    QRegularExpression regex("\\A(?:" + item.regex + ")\\z");
    if (!regex.isValid())
        return reject(error, "正则表达式无效：" + regex.errorString());
    return true;
}
bool AppItemValue::display(const s_itemUnit &item, Endian endian, QString &text, QString &error) {
    text.clear();
    if (!validateConfig(item, error))
        return false;
    QString hex;
    if (!normalizeHex(item.valueStr, item.dataType, endian, hex, error))
        return false;
    double raw = 0;
    if (!rawValue(hex, item.dataType, endian, text, raw, error))
        return false;
    double converted = raw;
    if (!identity(item.ioFormula)) {
        if (item.dataType < DataType::F32 && byteSize(item.dataType) == 8 &&
            std::fabs(raw) > 9007199254740991.0)
            return reject(error, "64 位原始整数超过公式的精确整数范围（2^53−1）");
        if (!AppCalcExpression::evaluate(item.ioFormula, raw, converted, error)) {
            text.clear();
            return false;
        }
        text = QString::number(converted, 'g', 15);
    }
    return true;
}
bool AppItemValue::validateInput(const s_itemUnit &item, Endian endian, QString &text,
                                 QString &error) {
    if (!display(item, endian, text, error))
        return false;
    return rules(item, text, text.toDouble(), error);
}
bool AppItemValue::fromDisplay(const s_itemUnit &item, QString text, Endian endian, QString &hex,
                               QString &error) {
    if (!validateConfig(item, error))
        return false;
    if (!editable(item))
        return reject(error, "未设置反向公式，请编辑 HEX 或补充反向公式");
    text = text.trimmed();
    if (text.size() > 128)
        return reject(error, "数值输入过长");
    double value = 0;
    if (!numeric(text, value, error) || !rules(item, text, value, error))
        return false;
    const bool calculated = !identity(item.reverseFormula);
    double raw = value;
    if (calculated && !AppCalcExpression::evaluate(item.reverseFormula, value, raw, error)) {
        error.prepend("反向公式：");
        return false;
    }
    if (!encode(text, raw, item.dataType, endian, calculated, hex, error))
        return false;
    s_itemUnit candidate = item;
    candidate.valueStr = hex;
    QString roundTrip;
    if (!display(candidate, endian, roundTrip, error))
        return false;
    const double displayed = roundTrip.toDouble();
    const double tolerance = item.dataType == DataType::F32 ? 2e-6 : 1e-10;
    if ((!identity(item.ioFormula) || calculated) &&
        std::fabs(displayed - value) > tolerance * qMax(1.0, std::fabs(value)))
        return reject(error, "正反向公式不一致，或当前类型无法表示输入值");
    return true;
}
void AppItemValue::writeRules(QJsonObject &json, const s_itemUnit &item) {
    json["itemId"] = item.itemId;
    json["upperLimit"] = item.upperLimit;
    json["lowerLimit"] = item.lowerLimit;
    json["upperLimitEnabled"] = item.upperLimitEnabled;
    json["lowerLimitEnabled"] = item.lowerLimitEnabled;
    json["regex"] = item.regex;
    json["ioFormula"] = item.ioFormula;
    json["reverseFormula"] = item.reverseFormula;
}
void AppItemValue::readRules(const QJsonObject &json, s_itemUnit &item) {
    item.itemId = json["itemId"].toString();
    item.upperLimit = json["upperLimit"].toDouble(0);
    item.lowerLimit = json["lowerLimit"].toDouble(0);
    // Old templates did not store enable flags; zero defaults must remain unrestricted.
    item.upperLimitEnabled = json["upperLimitEnabled"].toBool(item.upperLimit != 0);
    item.lowerLimitEnabled = json["lowerLimitEnabled"].toBool(item.lowerLimit != 0);
    item.regex = json["regex"].toString();
    item.ioFormula = json["ioFormula"].toString("x");
    item.reverseFormula = json["reverseFormula"].toString();
}
