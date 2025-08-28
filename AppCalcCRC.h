#ifndef APPCALCCRC_H
#define APPCALCCRC_H

#include <cstdint>

typedef enum
{
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
    CRC32_MPEG2
} e_CRC_Temp;

typedef struct T_CRC_CONF
{
    uint8_t width;    // data width
    uint32_t poly;    // poly
    uint32_t init;    // init status
    bool ref_in;      // input direction
    bool ref_out;     // output direction
    uint32_t xor_out; // xor output
} t_crc_conf;

class AppCalcCRC
{
public:
    AppCalcCRC();

    uint32_t calcCRC(t_crc_conf crcType, const uint8_t *buffer, uint32_t length);
    t_crc_conf GetTemplate(e_CRC_Temp tempID);

private:
    typedef enum
    {
        REF_4BIT = 4,
        REF_5BIT = 5,
        REF_6BIT = 6,
        REF_7BIT = 7,
        REF_8BIT = 8,
        REF_16BIT = 16,
        REF_24BIT = 24,
        REF_32BIT = 32
    } e_reflectedMode;
    uint32_t reflected_data(uint32_t data, e_reflectedMode mode);
    uint8_t calcCRC_4(uint8_t poly, uint8_t init, bool ref_in, bool ref_out, uint8_t xor_out, \
                      const uint8_t *buffer, uint32_t length);
    uint8_t calcCRC_5(uint8_t poly, uint8_t init, bool ref_in, bool ref_out, uint8_t xor_out, \
                      const uint8_t *buffer, uint32_t length);
    uint8_t calcCRC_6(uint8_t poly, uint8_t init, bool ref_in, bool ref_out, uint8_t xor_out, \
                      const uint8_t *buffer, uint32_t length);
    uint8_t calcCRC_7(uint8_t poly, uint8_t init, bool ref_in, bool ref_out, uint8_t xor_out, \
                      const uint8_t *buffer, uint32_t length);
    uint8_t calcCRC_8(uint8_t poly, uint8_t init, bool ref_in, bool ref_out, uint8_t xor_out, \
                      const uint8_t *buffer, uint32_t length);
    uint16_t calcCRC_16(uint16_t poly, uint16_t init, bool ref_in, bool ref_out, uint16_t xor_out,
                        const uint8_t *buffer, uint32_t length);
    uint32_t calcCRC_24(uint32_t poly, uint32_t init, bool ref_in, bool ref_out, uint32_t xor_out,
                        const uint8_t *buffer, uint32_t length);
    uint32_t calcCRC_32(uint32_t poly, uint32_t init, bool ref_in, bool ref_out, uint32_t xor_out,
                        const uint8_t *buffer, uint32_t length);
};

#endif // APPCALCCRC_H
