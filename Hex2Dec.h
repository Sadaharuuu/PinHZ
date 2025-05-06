#ifndef HEX2DEC_H
#define HEX2DEC_H

#include <QString>

class Hex2Dec
{
public:
    Hex2Dec();
    ~Hex2Dec();

    uint8_t Str2Array(QString &hexStr, uint8_t *buf, bool isHexStr, bool isLittleEndian);
    QString StrFix(QString srcStr, uint8_t dataType, bool isHex, bool isLittleEndian);
    QString Hex2DecString(QString hexStr, uint8_t dataType, bool isLittleEndian);
    QString Dec2HexString(QString decStr, uint8_t dataType, bool isLittleEndian);
    QString Hex2bit8(uint8_t *buf, bool isSigned);
    QString Hex2bit16(uint8_t *buf, bool isSigned);
    QString Hex2bit32(uint8_t *buf, bool isSigned, bool isFloat);
    QString Hex2bit64(uint8_t *buf, bool isSigned, bool isFloat);
    bool HexStrTurnOrder(QString &srcStr);
};

#endif // HEX2DEC_H
