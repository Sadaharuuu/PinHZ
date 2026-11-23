#ifndef APPCALCCRC_H
#define APPCALCCRC_H

#include <cstdint>

typedef enum {
    CRC4_ITU = 0,
    CRC5_EPC,
    CRC5_ITU,
    CRC5_USB,
    CRC6_ITU,
    CRC7_MMC,
    CRC8,
    CRC8_ITU,
    CRC8_ROHC,
    CRC8_MAXIM,
    CRC16_IBM,
    CRC16_MAXIM,
    CRC16_USB,
    CRC16_MODBUS,
    CRC16_CCITT,
    CRC16_CCITT_FALSE,
    CRC16_X25,
    CRC16_XMODEM,
    CRC16_DNP,
    CRC24,
    CRC32,
    CRC32_MPEG2,
    CRC32_CASTAGNOLI,
    CRC24_OPENPGP,
    CRC16_GENIBUS,
    CRC8_SAEJ1850,
    CRC32_BZIP2,
    CRC_TEMP_COUNT
} e_CRC_Temp;

typedef struct S_CRC_CONF {
    uint8_t width;    // data width
    uint32_t poly;    // poly
    uint32_t init;    // init status
    bool ref_in;      // input direction
    bool ref_out;     // output direction
    uint32_t xor_out; // xor output
} s_crc_conf;

class AppCalcCRC {
public:
    AppCalcCRC();

    uint32_t calcCRC(s_crc_conf crcType, const uint8_t *buffer, uint32_t length);
    s_crc_conf GetTemplate(e_CRC_Temp tempID);
};

#endif // APPCALCCRC_H
