#include "AppCalcCRC.h"

#define POLY_CRC4_ITU (0x03)            // x4+x1+x0
#define POLY_CRC5_EPC (0x09)            // x5+x3+x0
#define POLY_CRC5_ITU (0x15)            // x5+x4+x2+x0
#define POLY_CRC5_USB (0x05)            // x5+x2+x0
#define POLY_CRC6_ITU (0x03)            // x6+x1+x0
#define POLY_CRC7_MMC (0x09)            // x7+x3+x0
#define POLY_CRC8 (0x07)                // x8+x2+x1+x0
#define POLY_CRC8_ITU (0x07)            // x8+x2+x1+x0
#define POLY_CRC8_ROHC (0x07)           // x8+x2+x1+x0
#define POLY_CRC8_MAXIM (0x31)          // x8+x5+x4+x0
#define POLY_CRC16_IBM (0x8005)         // x16+x15+x2+x0
#define POLY_CRC16_MAXIM (0x8005)       // x16+x15+x2+x0
#define POLY_CRC16_USB (0x8005)         // x16+x15+x2+x0
#define POLY_CRC16_MODBUS (0x8005)      // x16+x15+x2+x0
#define POLY_CRC16_CCITT (0x1021)       // x16+x12+x5+x0
#define POLY_CRC16_CCITT_FALSE (0x1021) // x16+x12+x5+x0
#define POLY_CRC16_X25 (0x1021)         // x16+x12+x5+x0
#define POLY_CRC16_XMODEM (0x1021)      // x16+x12+x5+x0
#define POLY_CRC16_DNP (0x3D65)         // x16+x13+x12+x11+x10+x8+x6+x5+x2+x0
#define POLY_CRC24 (0x864cfb)           // x24+x23+x18+x17+x14+x11+x10+x7+x6+x5+x4+x3+x2+x1+x0
#define POLY_CRC32 (0x04c11db7)         // x32+x26+x23+x22+x16+x12+x11+x10+x8+x7+x5+x4+x2+x1+x0
#define POLY_CRC32_MPEG2 (0x04c11db7)   // x32+x26+x23+x22+x16+x12+x11+x10+x8+x7+x5+x4+x2+x1+x0

AppCalcCRC::AppCalcCRC() {}

s_crc_conf AppCalcCRC::GetTemplate(e_CRC_Temp tempID) {
    s_crc_conf conf = {
        .width = 0,
        .poly = 0,
        .init = 0,
        .ref_in = false,
        .ref_out = false,
        .xor_out = 0,
    };

    switch (tempID) {
    case CRC4_ITU: {
        s_crc_conf m_crc4_ITU = {4, POLY_CRC4_ITU, 0x00, true, true, 0x00};
        conf = m_crc4_ITU;
        break;
    }
    case CRC5_EPC: {
        s_crc_conf m_crc5_EPC = {5, POLY_CRC5_EPC, 0x09, false, false, 0x00};
        conf = m_crc5_EPC;
        break;
    }
    case CRC5_ITU: {
        s_crc_conf m_crc5_ITU = {5, POLY_CRC5_ITU, 0x00, true, true, 0x00};
        conf = m_crc5_ITU;
        break;
    }
    case CRC5_USB: {
        s_crc_conf m_crc5_USB = {5, POLY_CRC5_USB, 0x1f, true, true, 0x1f};
        conf = m_crc5_USB;
        break;
    }
    case CRC6_ITU: {
        s_crc_conf m_crc6_ITU = {6, POLY_CRC6_ITU, 0x00, true, true, 0x00};
        conf = m_crc6_ITU;
        break;
    }
    case CRC7_MMC: {
        s_crc_conf m_crc7_MMC = {7, POLY_CRC7_MMC, 0x00, false, false, 0x00};
        conf = m_crc7_MMC;
        break;
    }
    case CRC8: {
        s_crc_conf m_crc8 = {8, POLY_CRC8, 0x00, false, false, 0x00};
        conf = m_crc8;
        break;
    }
    case CRC8_ITU: {
        s_crc_conf m_crc8_ITU = {8, POLY_CRC8_ITU, 0x00, false, false, 0x55};
        conf = m_crc8_ITU;
        break;
    }
    case CRC8_ROHC: {
        s_crc_conf m_crc8_ROHC = {8, POLY_CRC8_ROHC, 0xff, true, true, 0x00};
        conf = m_crc8_ROHC;
        break;
    }
    case CRC8_MAXIM: {
        s_crc_conf m_crc8_MAXIM = {8, POLY_CRC8_MAXIM, 0x00, true, true, 0x00};
        conf = m_crc8_MAXIM;
        break;
    }
    case CRC16_IBM: {
        s_crc_conf m_crc16_IBM = {16, POLY_CRC16_IBM, 0x0000, true, true, 0x0000};
        conf = m_crc16_IBM;
        break;
    }
    case CRC16_MAXIM: {
        s_crc_conf m_crc16_MAXIM = {16, POLY_CRC16_MAXIM, 0x0000, true, true, 0xffff};
        conf = m_crc16_MAXIM;
        break;
    }
    case CRC16_USB: {
        s_crc_conf m_crc16_USB = {16, POLY_CRC16_USB, 0xffff, true, true, 0xffff};
        conf = m_crc16_USB;
        break;
    }
    case CRC16_MODBUS: {
        s_crc_conf m_crc16_MODBUS = {16, POLY_CRC16_MODBUS, 0xffff, true, true, 0x0000};
        conf = m_crc16_MODBUS;
        break;
    }
    case CRC16_CCITT: {
        s_crc_conf m_crc16_CCITT = {16, POLY_CRC16_CCITT, 0x0000, true, true, 0x0000};
        conf = m_crc16_CCITT;
        break;
    }
    case CRC16_CCITT_FALSE: {
        s_crc_conf m_crc16_CCITT_FALSE = {16, POLY_CRC16_CCITT, 0xffff, false, false, 0x0000};
        conf = m_crc16_CCITT_FALSE;
        break;
    }
    case CRC16_X25: {
        s_crc_conf m_crc16_X25 = {16, POLY_CRC16_X25, 0xffff, true, true, 0xffff};
        conf = m_crc16_X25;
        break;
    }
    case CRC16_XMODEM: {
        s_crc_conf m_crc16_XMODEM = {16, POLY_CRC16_XMODEM, 0x0000, false, false, 0x0000};
        conf = m_crc16_XMODEM;
        break;
    }
    case CRC16_DNP: {
        s_crc_conf m_crc16_DNP = {16, POLY_CRC16_DNP, 0x0000, true, true, 0xffff};
        conf = m_crc16_DNP;
        break;
    }
    case CRC24: {
        s_crc_conf m_crc24 = {24, 0x00864cfb, 0x000000, false, false, 0x000000};
        conf = m_crc24;
        break;
    }
    case CRC32: {
        s_crc_conf m_crc32 = {32, POLY_CRC32, 0xffffffff, true, true, 0xffffffff};
        conf = m_crc32;
        break;
    }
    case CRC32_MPEG2: {
        s_crc_conf m_crc32_MPEG2 = {32, POLY_CRC32_MPEG2, 0xffffffff, false, false, 0x00000000};
        conf = m_crc32_MPEG2;
        break;
    }
    case CRC32_CASTAGNOLI:
        conf = {32, 0x1edc6f41, 0xffffffff, true, true, 0xffffffff};
        break;
    case CRC24_OPENPGP:
        conf = {24, 0x864cfb, 0xb704ce, false, false, 0};
        break;
    case CRC16_GENIBUS:
        conf = {16, 0x1021, 0xffff, false, false, 0xffff};
        break;
    case CRC8_SAEJ1850:
        conf = {8, 0x1d, 0xff, false, false, 0xff};
        break;
    case CRC32_BZIP2:
        conf = {32, 0x04c11db7, 0xffffffff, false, false, 0xffffffff};
        break;
    default:
        break;
    }

    return conf;
}

uint32_t AppCalcCRC::calcCRC(s_crc_conf conf, const uint8_t *buffer, uint32_t length) {
    if (conf.width < 4 || conf.width > 32 || (!buffer && length))
        return 0;
    const uint32_t mask = uint32_t((uint64_t(1) << conf.width) - 1);
    const uint32_t top = uint32_t(1) << (conf.width - 1);
    uint32_t crc = conf.init & mask;
    for (uint32_t i = 0; i < length; ++i) {
        for (int bit = 0; bit < 8; ++bit) {
            const unsigned input = (buffer[i] >> (conf.ref_in ? bit : 7 - bit)) & 1;
            const bool feedback = bool(crc & top) != bool(input);
            crc = ((crc << 1) ^ (feedback ? conf.poly : 0)) & mask;
        }
    }
    if (conf.ref_out) {
        uint32_t reversed = 0;
        for (unsigned bit = 0; bit < conf.width; ++bit) {
            reversed = (reversed << 1) | (crc & 1);
            crc >>= 1;
        }
        crc = reversed;
    }
    return (crc ^ conf.xor_out) & mask;
}
