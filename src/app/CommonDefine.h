#ifndef COMMONDEFINE_H
#define COMMONDEFINE_H

#include <stdint.h>
#include <QMetaType>
#include <QString>
#include <QList>
#include <QMessageBox>
#include <type_traits>

#define FONT_COLOR_BLACK "<font color=\"#000000\">"
#define FONT_COLOR_WHITE "<font color=\"#FFFFFF\">"
#define FONT_COLOR_RED "<font color=\"#FF0000\">"
#define FONT_COLOR_GREEN "<font color=\"#00FF00\">"
#define FONT_COLOR_BLUE "<font color=\"#0000FF\">"
#define FONT_COLOR_CYAN "<font color=\"#00FFFF\">"
#define FONT_COLOR_PINK "<font color=\"#FF00FF\">"
#define FONT_COLOR_YELLOW "<font color=\"#FFFF00\">"

#define FONT_COLOR_DARK_ORANGE "<font color=\"#FF8C00\">"
#define FONT_COLOR_BLUEGREEN "<font color=\"#00B3B3\">"
#define FONT_COLOR_BLUEVIOLET "<font color=\"#8A2BE2\">"

template <typename T> inline constexpr int32_t toInt32(T value) {
    return static_cast<int32_t>(value);
}

template <typename T> inline constexpr T toType(int32_t value) {
    return static_cast<T>(value);
}

enum class LogLevel : uint8_t { DBG = 0, INF, WAR, ERR, COLORFUL };
Q_DECLARE_METATYPE(LogLevel)

enum class DataType : uint8_t { U08 = 0, S08, U16, S16, U32, S32, U64, S64, F32, F64 };
Q_DECLARE_METATYPE(DataType)

enum class CheckType : int8_t { CheckSum8 = 0, CheckSum16, CheckSum32, CRC, None = -1 };
Q_DECLARE_METATYPE(CheckType)

enum class Endian : uint8_t { BigEndian = 0, LittleEndian };
Q_DECLARE_METATYPE(Endian)

enum class ItemType : uint8_t { Text = 0, Combo };
Q_DECLARE_METATYPE(ItemType)

enum class ItemZone : uint8_t { Head = 0, Data, Check, Tail };
Q_DECLARE_METATYPE(ItemZone)

enum class PortMode : int32_t { Serial = 0, Net };
Q_DECLARE_METATYPE(PortMode)

enum class NetType : int32_t { UDP = 0, TCP_C, TCP_S };
Q_DECLARE_METATYPE(NetType)

enum class PageMode : int32_t { Embeded = 0, Detached };
Q_DECLARE_METATYPE(PageMode)

typedef struct {
    QString itemId;
    // Info
    ItemType itemType;
    ItemZone itemZone;
    DataType dataType;
    QString comment;
    // Text
    double upperLimit;
    double lowerLimit;
    bool upperLimitEnabled = false;
    bool lowerLimitEnabled = false;
    QString regex;
    QString ioFormula;
    QString reverseFormula;
    QString valueStr;
    // Combo
    int32_t comboIndex;
    int32_t comboCnt;
    QList<QString> optionList;
    QList<QString> keyList;
} s_itemUnit;

enum CellType { ctDataType = 1000, ctDataHex, ctDataDec, ctComment };
enum ColFieldNum { colDataType = 0, colDataHex, colDataDec, colComment };

#endif // COMMONDEFINE_H
