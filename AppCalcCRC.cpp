#include "AppCalcCRC.h"

#define POLY_CRC4_ITU          (0x03)       // x4+x1+x0
#define POLY_CRC5_EPC          (0x09)       // x5+x3+x0
#define POLY_CRC5_ITU          (0x15)       // x5+x4+x2+x0
#define POLY_CRC5_USB          (0x05)       // x5+x2+x0
#define POLY_CRC6_ITU          (0x03)       // x6+x1+x0
#define POLY_CRC7_MMC          (0x09)       // x7+x3+x0
#define POLY_CRC8              (0x07)       // x8+x2+x1+x0
#define POLY_CRC8_ITU          (0x07)       // x8+x2+x1+x0
#define POLY_CRC8_ROHC         (0x07)       // x8+x2+x1+x0
#define POLY_CRC8_MAXIM        (0x31)       // x8+x5+x4+x0
#define POLY_CRC16_IBM         (0x8005)     // x16+x15+x2+x0
#define POLY_CRC16_MAXIM       (0x8005)     // x16+x15+x2+x0
#define POLY_CRC16_USB         (0x8005)     // x16+x15+x2+x0
#define POLY_CRC16_MODBUS      (0x8005)     // x16+x15+x2+x0
#define POLY_CRC16_CCITT       (0x1021)     // x16+x12+x5+x0
#define POLY_CRC16_CCITT_FALSE (0x1021)     // x16+x12+x5+x0
#define POLY_CRC16_X25         (0x1021)     // x16+x12+x5+x0
#define POLY_CRC16_XMODEM      (0x1021)     // x16+x12+x5+x0
#define POLY_CRC16_DNP         (0x3D65)     // x16+x13+x12+x11+x10+x8+x6+x5+x2+x0
#define POLY_CRC24             (0x864cfb)   // x24+x23+x18+x17+x14+x11+x10+x7+x6+x5+x4+x3+x2+x1+x0
#define POLY_CRC32             (0x04c11db7) // x32+x26+x23+x22+x16+x12+x11+x10+x8+x7+x5+x4+x2+x1+x0
#define POLY_CRC32_MPEG2       (0x04c11db7) // x32+x26+x23+x22+x16+x12+x11+x10+x8+x7+x5+x4+x2+x1+x0

AppCalcCRC::AppCalcCRC()
{
}

t_crc_conf AppCalcCRC::GetTemplate(e_CRC_Temp tempID)
{
    t_crc_conf conf = {
        .width = 0,
        .poly = 0,
        .init = 0,
        .ref_in = false,
        .ref_out = false,
        .xor_out = 0,
    };

    switch (tempID)
    {
    case CRC4_ITU:           {t_crc_conf m_crc4_ITU            = {4, POLY_CRC4_ITU, 0x00, true, true, 0x00};                     conf = m_crc4_ITU;          break;}
    case CRC5_EPC:           {t_crc_conf m_crc5_EPC            = {5, POLY_CRC5_EPC, 0x09, false, false, 0x00};                   conf = m_crc5_EPC;          break;}
    case CRC5_ITU:           {t_crc_conf m_crc5_ITU            = {5, POLY_CRC5_ITU, 0x00, true, true, 0x00};                     conf = m_crc5_ITU;          break;}
    case CRC5_USB:           {t_crc_conf m_crc5_USB            = {5, POLY_CRC5_USB, 0x1f, true, true, 0x1f};                     conf = m_crc5_USB;          break;}
    case CRC6_ITU:           {t_crc_conf m_crc6_ITU            = {6, POLY_CRC6_ITU, 0x00, true, true, 0x00};                     conf = m_crc6_ITU;          break;}
    case CRC7_MMC:           {t_crc_conf m_crc7_MMC            = {7, POLY_CRC7_MMC, 0x00, false, false, 0x00};                   conf = m_crc7_MMC;          break;}
    case CRC8:               {t_crc_conf m_crc8                = {8, POLY_CRC8, 0x00, false, false, 0x00};                       conf = m_crc8;              break;}
    case CRC8_ITU:           {t_crc_conf m_crc8_ITU            = {8, POLY_CRC8_ITU, 0x00, false, false, 0x55};                   conf = m_crc8_ITU;          break;}
    case CRC8_ROHC:          {t_crc_conf m_crc8_ROHC           = {8, POLY_CRC8_ROHC, 0xff, true, true, 0x00};                    conf = m_crc8_ROHC;         break;}
    case CRC8_MAXIM:         {t_crc_conf m_crc8_MAXIM          = {8, POLY_CRC8_MAXIM, 0x00, true, true, 0x00};                   conf = m_crc8_MAXIM;        break;}
    case CRC16_IBM:          {t_crc_conf m_crc16_IBM           = {16, POLY_CRC16_IBM, 0x0000, true, true, 0x0000};               conf = m_crc16_IBM;         break;}
    case CRC16_MAXIM:        {t_crc_conf m_crc16_MAXIM         = {16, POLY_CRC16_MAXIM, 0x0000, true, true, 0xffff};             conf = m_crc16_MAXIM;       break;}
    case CRC16_USB:          {t_crc_conf m_crc16_USB           = {16, POLY_CRC16_USB, 0xffff, true, true, 0xffff};               conf = m_crc16_USB;         break;}
    case CRC16_MODBUS:       {t_crc_conf m_crc16_MODBUS        = {16, POLY_CRC16_MODBUS, 0xffff, true, true, 0x0000};            conf = m_crc16_MODBUS;      break;}
    case CRC16_CCITT:        {t_crc_conf m_crc16_CCITT         = {16, POLY_CRC16_CCITT, 0x0000, true, true, 0x0000};             conf = m_crc16_CCITT;       break;}
    case CRC16_CCITT_FALSE:  {t_crc_conf m_crc16_CCITT_FALSE   = {16, POLY_CRC16_CCITT, 0xffff, false, false, 0x0000};           conf = m_crc16_CCITT_FALSE; break;}
    case CRC16_X25:          {t_crc_conf m_crc16_X25           = {16, POLY_CRC16_X25, 0xffff, true, true, 0xffff};               conf = m_crc16_X25;         break;}
    case CRC16_XMODEM:       {t_crc_conf m_crc16_XMODEM        = {16, POLY_CRC16_XMODEM, 0x0000, false, false, 0x0000};          conf = m_crc16_XMODEM;      break;}
    case CRC16_DNP:          {t_crc_conf m_crc16_DNP           = {16, POLY_CRC16_DNP, 0x0000, true, true, 0xffff};               conf = m_crc16_DNP;         break;}
    case CRC24:              {t_crc_conf m_crc24               = {24, 0x00864cfb, 0x000000, false, false, 0x000000};             conf = m_crc24;             break;}
    case CRC32:              {t_crc_conf m_crc32               = {32, POLY_CRC32, 0xffffffff, true, true, 0xffffffff};           conf = m_crc32;             break;}
    case CRC32_MPEG2:        {t_crc_conf m_crc32_MPEG2         = {32, POLY_CRC32_MPEG2, 0xffffffff, false, false, 0x00000000};   conf = m_crc32_MPEG2;       break;}
    default: break;
    }

    return conf;
}

uint32_t AppCalcCRC::reflected_data(uint32_t data, e_reflectedMode mode)
{
    data = ((data & 0xffff0000) >> 16) | ((data & 0x0000ffff) << 16);
    data = ((data & 0xff00ff00) >> 8) | ((data & 0x00ff00ff) << 8);
    data = ((data & 0xf0f0f0f0) >> 4) | ((data & 0x0f0f0f0f) << 4);
    data = ((data & 0xcccccccc) >> 2) | ((data & 0x33333333) << 2);
    data = ((data & 0xaaaaaaaa) >> 1) | ((data & 0x55555555) << 1);

    switch (mode)
    {
    case REF_32BIT: return data;
    case REF_24BIT: return (data >> 8) & 0xffffff;
    case REF_16BIT: return (data >> 16) & 0xffff;
    case REF_8BIT: return (data >> 24) & 0xff;
    case REF_7BIT: return (data >> 25) & 0x7f;
    case REF_6BIT: return (data >> 26) & 0x7f;
    case REF_5BIT: return (data >> 27) & 0x1f;
    case REF_4BIT: return (data >> 28) & 0x0f;
    }
    return 0;
}

uint8_t AppCalcCRC::calcCRC_4(uint8_t poly, uint8_t init, bool ref_in, bool ref_out, uint8_t xor_out,
                   const uint8_t *buffer, uint32_t length)
{
    uint8_t i;
    uint8_t crc;

    if (ref_in == true)
    {
        crc = init;
        poly = reflected_data(poly, REF_4BIT);

        while (length--)
        {
            crc ^= *buffer++;
            for (i = 0; i < 8; i++)
            {
                if (crc & 0x01)
                {
                    crc >>= 1;
                    crc ^= poly;
                }
                else
                {
                    crc >>= 1;
                }
            }
        }

        return crc ^ xor_out;
    }
    else
    {
        crc = init << 4;
        poly <<= 4;

        while (length--)
        {
            crc ^= *buffer++;
            for (i = 0; i < 8; i++)
            {
                if (crc & 0x80)
                {
                    crc <<= 1;
                    crc ^= poly;
                }
                else
                {
                    crc <<= 1;
                }
            }
        }

        return (crc >> 4) ^ xor_out;
    }
}

uint8_t AppCalcCRC::calcCRC_5(uint8_t poly, uint8_t init, bool ref_in, bool ref_out, uint8_t xor_out,
                   const uint8_t *buffer, uint32_t length)
{
    uint8_t i;
    uint8_t crc;

    if (ref_in == true)
    {
        crc = init;
        poly = reflected_data(poly, REF_5BIT);

        while (length--)
        {
            crc ^= *buffer++;
            for (i = 0; i < 8; i++)
            {
                if (crc & 0x01)
                {
                    crc >>= 1;
                    crc ^= poly;
                }
                else
                {
                    crc >>= 1;
                }
            }
        }

        return crc ^ xor_out;
    }
    else
    {
        crc = init << 3;
        poly <<= 3;

        while (length--)
        {
            crc ^= *buffer++;
            for (i = 0; i < 8; i++)
            {
                if (crc & 0x80)
                {
                    crc <<= 1;
                    crc ^= poly;
                }
                else
                {
                    crc <<= 1;
                }
            }
        }

        return (crc >> 3) ^ xor_out;
    }
}

uint8_t AppCalcCRC::calcCRC_6(uint8_t poly, uint8_t init, bool ref_in, bool ref_out, uint8_t xor_out,
                   const uint8_t *buffer, uint32_t length)
{
    uint8_t i;
    uint8_t crc;

    if (ref_in == true)
    {
        crc = init;
        poly = reflected_data(poly, REF_6BIT);

        while (length--)
        {
            crc ^= *buffer++;
            for (i = 0; i < 8; i++)
            {
                if (crc & 0x01)
                {
                    crc >>= 1;
                    crc ^= poly;
                }
                else
                {
                    crc >>= 1;
                }
            }
        }

        return crc ^ xor_out;
    }
    else
    {
        crc = init << 2;
        poly <<= 2;

        while (length--)
        {
            crc ^= *buffer++;
            for (i = 0; i < 8; i++)
            {
                if (crc & 0x80)
                {
                    crc <<= 1;
                    crc ^= poly;
                }
                else
                {
                    crc <<= 1;
                }
            }
        }

        return (crc >> 2) ^ xor_out;
    }
}

uint8_t AppCalcCRC::calcCRC_7(uint8_t poly, uint8_t init, bool ref_in, bool ref_out, uint8_t xor_out,
                              const uint8_t *buffer, uint32_t length)
{
    uint8_t i;
    uint8_t crc;

    if (ref_in == true)
    {
        crc = init;
        poly = reflected_data(poly, REF_7BIT);

        while (length--)
        {
            crc ^= *buffer++;
            for (i = 0; i < 8; i++)
            {
                if (crc & 0x01)
                {
                    crc >>= 1;
                    crc ^= poly;
                }
                else
                {
                    crc >>= 1;
                }
            }
        }

        return crc ^ xor_out;
    }
    else
    {
        crc = init << 1;
        poly <<= 1;

        while (length--)
        {
            crc ^= *buffer++;
            for (i = 0; i < 8; i++)
            {
                if (crc & 0x80)
                {
                    crc <<= 1;
                    crc ^= poly;
                }
                else
                {
                    crc <<= 1;
                }
            }
        }

        return (crc >> 1) ^ xor_out;
    }
}

uint8_t AppCalcCRC::calcCRC_8(uint8_t poly, uint8_t init, bool ref_in, bool ref_out, uint8_t xor_out,
                              const uint8_t *buffer, uint32_t length)
{
    uint32_t i = 0;
    uint8_t crc = init;

    while (length--)
    {
        if (ref_in == true)
        {
            crc ^= reflected_data(*buffer++, REF_8BIT);
        }
        else
        {
            crc ^= *buffer++;
        }

        for (i = 0; i < 8; i++)
        {
            if (crc & 0x80)
            {
                crc <<= 1;
                crc ^= poly;
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    if (ref_out == true)
    {
        crc = reflected_data(crc, REF_8BIT);
    }

    return crc ^ xor_out;
}

uint16_t AppCalcCRC::calcCRC_16(uint16_t poly, uint16_t init, bool ref_in, bool ref_out, uint16_t xor_out,
                                const uint8_t *buffer, uint32_t length)
{
    uint32_t i = 0;
    uint16_t crc = init;

    while (length--)
    {
        if (ref_in == true)
        {
            crc ^= reflected_data(*buffer++, REF_8BIT) << 8;
        }
        else
        {
            crc ^= (*buffer++) << 8;
        }

        for (i = 0; i < 8; i++)
        {
            if (crc & 0x8000)
            {
                crc <<= 1;
                crc ^= poly;
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    if (ref_out == true)
    {
        crc = reflected_data(crc, REF_16BIT);
    }

    return crc ^ xor_out;
}

uint32_t AppCalcCRC::calcCRC_24(uint32_t poly, uint32_t init, bool ref_in, bool ref_out, uint32_t xor_out,
                                const uint8_t *buffer, uint32_t length)
{
    uint32_t i = 0;
    uint32_t crc = init;

    while (length--)
    {
        if (ref_in == true)
        {
            crc ^= reflected_data(*buffer++, REF_8BIT) << 16;
        }
        else
        {
            crc ^= (*buffer++) << 16;
        }

        for (i = 0; i < 8; i++)
        {
            if (crc & 0x800000)
            {
                crc <<= 1;
                crc ^= poly;
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    if (ref_out == true)
    {
        crc = reflected_data(crc, REF_24BIT);
    }

    return (crc ^ xor_out) & 0xffffff;
}

uint32_t AppCalcCRC::calcCRC_32(uint32_t poly, uint32_t init, bool ref_in, bool ref_out, uint32_t xor_out,
                                const uint8_t *buffer, uint32_t length)
{
    uint32_t i = 0;
    uint32_t crc = init;

    while (length--)
    {
        if (ref_in == true)
        {
            crc ^= reflected_data(*buffer++, REF_8BIT) << 24;
        }
        else
        {
            crc ^= (*buffer++) << 24;
        }

        for (i = 0; i < 8; i++)
        {
            if (crc & 0x80000000)
            {
                crc <<= 1;
                crc ^= poly;
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    if (ref_out == true)
    {
        crc = reflected_data(crc, REF_32BIT);
    }

    return crc ^ xor_out;
}

uint32_t AppCalcCRC::calcCRC(t_crc_conf crc_conf, const uint8_t *buffer, uint32_t length)
{
    uint32_t ret = 0;

    switch (crc_conf.width)
    {
    case 4:
        ret = calcCRC_4(crc_conf.poly, crc_conf.init, \
                        crc_conf.ref_in, crc_conf.ref_out, crc_conf.xor_out, buffer, length);
        break;
    case 5:
        ret = calcCRC_5(crc_conf.poly, crc_conf.init, \
                        crc_conf.ref_in, crc_conf.ref_out, crc_conf.xor_out, buffer, length);
        break;
    case 6:
        ret = calcCRC_6(crc_conf.poly, crc_conf.init, \
                        crc_conf.ref_in, crc_conf.ref_out, crc_conf.xor_out, buffer, length);
        break;
    case 7:
        ret = calcCRC_7(crc_conf.poly, crc_conf.init, \
                        crc_conf.ref_in, crc_conf.ref_out, crc_conf.xor_out, buffer, length);
        break;
    case 8:
        ret = calcCRC_8(crc_conf.poly, crc_conf.init, \
                        crc_conf.ref_in, crc_conf.ref_out, crc_conf.xor_out, buffer, length);
        break;
    case 16:
        ret = calcCRC_16(crc_conf.poly, crc_conf.init, \
                         crc_conf.ref_in, crc_conf.ref_out, crc_conf.xor_out, buffer, length);
        break;
    case 24:
        ret = calcCRC_24(crc_conf.poly, crc_conf.init, \
                         crc_conf.ref_in, crc_conf.ref_out, crc_conf.xor_out, buffer, length);
        break;
    case 32:
        ret = calcCRC_32(crc_conf.poly, crc_conf.init, \
                         crc_conf.ref_in, crc_conf.ref_out, crc_conf.xor_out, buffer, length);
        break;
    default: break;
    }

    return ret;
}
