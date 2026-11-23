#ifndef HEX2DEC_H
#define HEX2DEC_H

#include "CommonDefine.h"
#include <QString>

class Hex2Dec {
public:
    Hex2Dec();
    ~Hex2Dec();

    uint8_t str2CodeStream(QString &hexStr, uint8_t *buf, bool isHexStr, Endian endian);
    QString strFormat(QString srcStr, DataType dataType, bool isHex, Endian endian);
    QString hex2DecString(QString hexStr, DataType dataType, Endian endian);
    QString dec2HexString(QString decStr, DataType dataType, Endian endian);
    QString hex2bit8(uint8_t *buf, bool isSigned);
    QString hex2bit16(uint8_t *buf, bool isSigned);
    QString hex2bit32(uint8_t *buf, bool isSigned, bool isFloat);
    QString hex2bit64(uint8_t *buf, bool isSigned, bool isFloat);
    bool hexStrTurnOrder(QString &srcStr);
};

#endif // HEX2DEC_H
