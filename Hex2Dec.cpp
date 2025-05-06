#include "Hex2Dec.h"

Hex2Dec::Hex2Dec()
{

}

Hex2Dec::~Hex2Dec()
{

}

/* **************************************************
 * @brief 字符串转换
 * @para:
 *      [input]hexStr: hex string
 *      [input]dataType: uint8 = 0, ..., uint64, int..., float, double
 *      [input]byteOrder: Little-Endian = 0, Big-Endian
 * @return:
 *      Dec String
 * **************************************************/
QString Hex2Dec::Hex2DecString(QString hexStr, uint8_t dataType, bool isLittleEndian)
{
    QString str = "";
    uint8_t buf[8] = {0, }, bufLen = 0;
    bufLen = Str2Array(hexStr, buf, true, isLittleEndian);
    bool isSigned = true, isFloat = false;

    switch (dataType) {
    case 0: /* uint8 */ isSigned = false; /* fall-through */
    case 4: /* sint8 */
        if (bufLen != 1) str = "00";
        else str = Hex2bit8(buf, isSigned);
        break;
    case 1: /* uint16 */ isSigned = false; /* fall-through */
    case 5: /* sint16 */
        if (bufLen != 2) str = "0000";
        else str = Hex2bit16(buf, isSigned);
        break;
    case 8: /* float  */ isFloat = true;   /* fall-through */
    case 2: /* uint32 */ isSigned = false; /* fall-through */
    case 6: /* sint32 */
        if (bufLen != 4) str = "00000000";
        else str = Hex2bit32(buf, isSigned, isFloat);
        break;
    case 9: /* double */ isFloat = true;   /* fall-through */
    case 3: /* uint64 */ isSigned = false; /* fall-through */
    case 7: /* sint64 */
        if (bufLen != 8) str = "0000000000000000";
        else str = Hex2bit64(buf, isSigned, isFloat);
        break;
    default: break;
    }

    return str;
}

/* **************************************************
 * @brief 字符串转换
 * @para:
 *      [input]hexStr: dec string
 *      [input]dataType: uint8 = 0, ..., uint64, int..., float, double
 *      [input]byteOrder: Little-Endian = 0, Big-Endian
 * @return:
 *      Hex String
 * **************************************************/
QString Hex2Dec::Dec2HexString(QString decStr, uint8_t dataType, bool isLittleEndian)
{
    QString str = "";
    bool isNeg = false;

    if (decStr.at(0) == '-')
    {
        isNeg = true;
    }
    if (dataType < 8)
    {
        if (isNeg)
        {
            int64_t stmp64 = decStr.toLongLong();
            str = str.number(stmp64, 16);
        }
        else
        {
            uint64_t utmp64 = decStr.toULongLong();
            str = str.number(utmp64, 16);
        }
    }
    else if (dataType == 8)
    {
        float ftmp32 = decStr.toFloat();
        uint8_t *p = (uint8_t *)&ftmp32;

        str.sprintf("%02X%02X%02X%02X", p[3], p[2], p[1], p[0]);
    }
    else
    {
        double ftmp64 = decStr.toDouble();
        uint8_t *p = (uint8_t *)&ftmp64;

        str.sprintf("%02X%02X%02X%02X%02X%02X%02X%02X",
                    p[7], p[6], p[5], p[4],
                    p[3], p[2], p[1], p[0]);
    }

    str = StrFix(str, dataType, true, isLittleEndian);

    return str;
}

/* **************************************************
 * @brief 字符串转数组
 * @para:
 *      [input]srcStr: string
 *      [input]buf: data buffer
 * @return:
 *      array len
 * **************************************************/
uint8_t Hex2Dec::Str2Array(QString &srcStr, uint8_t *buf, bool isHexStr, bool isLittleEndian)
{
    uint32_t dataLen = 0;
    QString str = srcStr.trimmed();

    if (isHexStr)
    {
        if (str.length() % 2 != 0)
            str = "0" + str;

        int32_t byteValue = 0, i = isLittleEndian ? 0 : str.length() - 2;
        QString byteStr = "";
        for (;;)
        {
            if (isLittleEndian && i >= str.length())
                break;
            if (!isLittleEndian && i < 0)
                break;

            byteStr = str.mid(i, 2);
            byteValue = byteStr.toInt(nullptr, 16);
            buf[dataLen++] = byteValue;

            i = isLittleEndian ? i + 2 : i - 2;
        }
    }
    else
    {
        int32_t i = 0;

        for (i = 0; i < str.length(); i++)
        {
            QString byteStr = str.mid(i, 1);
            bool isDec;
            int32_t byteValue = byteStr.toInt(&isDec, 10);
            if (isDec)
            {
                buf[dataLen++] = byteValue;
            }
            else
            {
                buf[dataLen++] = 0x00;
            }
        }
    }

    return dataLen;
}

QString Hex2Dec::Hex2bit8(uint8_t *buf, bool isSigned)
{
    uint8_t utmp8 = buf[0];
    QString str = "";

    if (isSigned)
        str = QString::number((int8_t)utmp8);
    else
        str = QString::number(utmp8);

    return str;
}

QString Hex2Dec::Hex2bit16(uint8_t *buf, bool isSigned)
{
    uint16_t utmp16 = 0;
    QString str = "";

    memcpy(&utmp16, buf, 2);
    if (isSigned)
        str = QString::number((int16_t)utmp16);
    else
        str = QString::number(utmp16);

    return str;
}

QString Hex2Dec::Hex2bit32(uint8_t *buf, bool isSigned, bool isFloat)
{
    uint32_t utmp32 = 0;
    QString str = "";

    memcpy(&utmp32, buf, 4);
    if (isFloat)
        str = QString::number(*(float *)&utmp32, 'f', 3);
    else if (isSigned)
        str = QString::number((int32_t)utmp32);
    else
        str = QString::number(utmp32);

    return str;
}

QString Hex2Dec::Hex2bit64(uint8_t *buf, bool isSigned, bool isFloat)
{
    uint64_t utmp64 = 0;
    QString str = "";

    memcpy(&utmp64, buf, 8);
    if (isFloat)
        str = QString::number(*(double *)&utmp64, 'f', 3);
    else if (isSigned)
        str = QString::number((int64_t)utmp64);
    else
        str = QString::number(utmp64);

    return str;
}

/* **************************************************
 * @brief 数字字符串format，uint8: hex补0，非法返回0，HexStr须是大端字符串
 * @para:
 *      [input]srcStr: 源字符串如果是十六进制的话必须是大端
 *      [input]dataType: uint8 = 0, ..., uint64, int..., float, double
 *      [input]isHex:
 * @return:
 *      Dst String
 * **************************************************/
QString Hex2Dec::StrFix(QString srcStr, uint8_t dataType, bool isHex, bool isLittleEndian)
{
    int32_t dataLen = 0;
    bool isSigned = true, isFloat = false;
    QString fixStr = "";
    int32_t fixLen = 0;
    QChar ch = ' ';
    bool isNeg = false;

    switch (dataType)
    {
    case 0: /* uint8  */ isSigned = false; /* fall-through */
    case 4: /* sint8  */ dataLen = 1; break;
    case 1: /* uint16 */ isSigned = false; /* fall-through */
    case 5: /* sint16 */ dataLen = 2; break;
    case 8: /* float  */ isFloat = true;   /* fall-through */
    case 2: /* uint32 */ isSigned = false; /* fall-through */
    case 6: /* sint32 */ dataLen = 4; break;
    case 9: /* double */ isFloat = true;   /* fall-through */
    case 3: /* uint64 */ isSigned = false; /* fall-through */
    case 7: /* sint64 */ dataLen = 8; break;
    default: break;
    }

    // 检查数据内容合法性
    if (!isHex && dataLen <= 4)
    {
        // 考虑4个及以下字节时十进制输入超范围的情况
        if (isSigned)
        {
            int64_t stmp64 = srcStr.toLongLong();
            if (stmp64 > (1 << (dataLen * 8 - 1)) - 1 ||
                stmp64 < -1 * (1 << (dataLen * 8 - 1)))
            {
                srcStr.sprintf("0");
            }
        }
        else
        {
            uint64_t utmp64 = srcStr.toULongLong();
            if (utmp64 > (0xFFFFFFFF >> (4 - dataLen) * 8))
            {
                srcStr.sprintf("0");
            }
        }
    }

    if ((srcStr.contains('.') || (srcStr.contains('-') && isSigned == false)) &&
        (isFloat == false))
    {
        fixStr.sprintf("0");
        isNeg = false;
    }
    else
    {
        if (isHex)
        {
            fixLen = srcStr.length() <= dataLen * 2 ? srcStr.length() : dataLen * 2;
        }
        else
        {
            if (srcStr.at(0) == '-')
            {
                isNeg = true;
                srcStr = srcStr.mid(1);
            }
            fixLen = srcStr.length();
        }

        isFloat = false;
        for (int32_t i = 0; i < fixLen; i++)
        {
            ch = srcStr.right(fixLen).at(i).toUpper();

            if (ch == '.')
            {
                if (isFloat || i == 0)
                {
                    fixStr.sprintf("0");
                    isNeg = false;
                    break;
                }
                isFloat = true;
            }
            else if ((ch < '0' || ch > '9') && (isHex ? (ch < 'A' || ch > 'Z') : true))
            {
                fixStr.sprintf("0");
                isNeg = false;
                break;
            }
            fixStr += srcStr.right(fixLen).at(i).toUpper();
        }
    }

    // 根据数据类型整理数据内容
    if (isHex)
    {
        if (fixStr.length() % 2 != 0)
            fixStr = "0" + fixStr;
        if (isLittleEndian)
            HexStrTurnOrder(fixStr);
        for (int32_t i = fixStr.length(); i < dataLen * 2; i++)
        {
            if (isLittleEndian)
                fixStr = fixStr + "0";
            else
                fixStr = "0" + fixStr;
        }
    }
    if (isFloat)
    {
        double ftmp64 = fixStr.toDouble();
        fixStr = QString::number(ftmp64, 'f', 3);
    }
    if (isNeg)
        fixStr = "-" + fixStr;

    return fixStr;
}

bool Hex2Dec::HexStrTurnOrder(QString &srcStr)
{
    QString str = "";
    for (int32_t i = srcStr.length() - 2; i >= 0; i -= 2)
    {
        str += srcStr.mid(i, 2);
    }
    srcStr = str;
    return true;
}
