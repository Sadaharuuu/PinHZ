#include "AppCalcCheck.h"
#include <QRegularExpression>
#include <algorithm>

namespace {
QCryptographicHash::Algorithm hashAlgorithm(AppCalcCheck::Type type) {
    switch (type) {
    case AppCalcCheck::Md5:
        return QCryptographicHash::Md5;
    case AppCalcCheck::Sha1:
        return QCryptographicHash::Sha1;
    case AppCalcCheck::Md4:
        return QCryptographicHash::Md4;
    case AppCalcCheck::Sha3_224:
        return QCryptographicHash::Sha3_224;
    case AppCalcCheck::Sha3_256:
        return QCryptographicHash::Sha3_256;
    case AppCalcCheck::Sha3_384:
        return QCryptographicHash::Sha3_384;
    case AppCalcCheck::Sha3_512:
        return QCryptographicHash::Sha3_512;
    case AppCalcCheck::Sha224:
        return QCryptographicHash::Sha224;
    case AppCalcCheck::Sha384:
        return QCryptographicHash::Sha384;
    case AppCalcCheck::Sha512:
        return QCryptographicHash::Sha512;
    default:
        return QCryptographicHash::Sha256;
    }
}
} // namespace

AppCalcCheck::AppCalcCheck(const Settings &settings)
    : m_settings(settings), m_value(settings.type == CRC ? settings.crc.init : 0),
      m_hash(hashAlgorithm(settings.type)) {
    if (settings.type == CRC && settings.crc.width >= 8 && settings.crc.width <= 32) {
        s_crc_conf tableConf = settings.crc;
        tableConf.init = 0;
        tableConf.ref_in = false;
        tableConf.ref_out = false;
        tableConf.xor_out = 0;
        for (unsigned i = 0; i < 256; ++i) {
            const uint8_t byte = uint8_t(i);
            m_crcTable[i] = AppCalcCRC().calcCRC(tableConf, &byte, 1);
        }
    }
    if (settings.type == Adler32)
        m_value = 1;
    if (settings.type == Blake2sp256)
        blake2sp_init(&m_blakeSp, 32);
    if (settings.type == Blake2s256)
        blake2s_init(&m_blakeS, 32);
    if (settings.type == Blake2b512)
        blake2b_init(&m_blakeB, 64);
    if (settings.type == Crc64Xz)
        m_wideValue = Q_UINT64_C(0xffffffffffffffff);
    if (settings.type == Xxh32) {
        m_xxh32 = XXH32_createState();
        if (m_xxh32)
            XXH32_reset(m_xxh32, 0);
    }
    if (settings.type == Xxh64) {
        m_xxh64 = XXH64_createState();
        if (m_xxh64)
            XXH64_reset(m_xxh64, 0);
    }
    if (settings.type == Xxh3_64 || settings.type == Xxh3_128) {
        m_xxh3 = XXH3_createState();
        if (m_xxh3) {
            if (settings.type == Xxh3_64)
                XXH3_64bits_reset(m_xxh3);
            else
                XXH3_128bits_reset(m_xxh3);
        }
    }
}

AppCalcCheck::~AppCalcCheck() {
    XXH32_freeState(m_xxh32);
    XXH64_freeState(m_xxh64);
    XXH3_freeState(m_xxh3);
}

bool AppCalcCheck::isValid() const {
    if (m_settings.type < Sum8 || m_settings.type >= TypeCount)
        return false;
    if (m_settings.type == Xxh32)
        return m_xxh32 != nullptr;
    if (m_settings.type == Xxh64)
        return m_xxh64 != nullptr;
    if (m_settings.type == Xxh3_64 || m_settings.type == Xxh3_128)
        return m_xxh3 != nullptr;
    const int width = m_settings.crc.width;
    if (m_settings.type != CRC)
        return true;
    if (width < 4 || width > 32)
        return false;
    const quint32 mask = quint32((quint64(1) << width) - 1);
    return m_settings.crc.poly && m_settings.crc.poly <= mask && m_settings.crc.init <= mask &&
           m_settings.crc.xor_out <= mask;
}

void AppCalcCheck::addData(const QByteArray &data) {
    if (!isValid())
        return;
    if (m_settings.type == Xxh32) {
        XXH32_update(m_xxh32, data.constData(), size_t(data.size()));
    } else if (m_settings.type == Xxh64) {
        XXH64_update(m_xxh64, data.constData(), size_t(data.size()));
    } else if (m_settings.type == Xxh3_64) {
        XXH3_64bits_update(m_xxh3, data.constData(), size_t(data.size()));
    } else if (m_settings.type == Xxh3_128) {
        XXH3_128bits_update(m_xxh3, data.constData(), size_t(data.size()));
    } else if (m_settings.type == Crc64Ecma || m_settings.type == Crc64Xz) {
        struct Tables {
            quint64 values[2][256];
            Tables() {
                for (int i = 0; i < 256; ++i) {
                    quint64 msb = quint64(i) << 56, lsb = quint64(i);
                    for (int bit = 0; bit < 8; ++bit) {
                        msb = (msb << 1) ^ ((msb >> 63) ? Q_UINT64_C(0x42f0e1eba9ea3693) : 0);
                        lsb = (lsb >> 1) ^ ((lsb & 1) ? Q_UINT64_C(0xc96c5795d7870f42) : 0);
                    }
                    values[0][i] = msb;
                    values[1][i] = lsb;
                }
            }
        };
        static const Tables tables;
        for (char byte : data) {
            if (m_settings.type == Crc64Ecma)
                m_wideValue = (m_wideValue << 8) ^
                              tables.values[0][quint8((m_wideValue >> 56) ^ quint8(byte))];
            else
                m_wideValue =
                    (m_wideValue >> 8) ^ tables.values[1][quint8(m_wideValue ^ quint8(byte))];
        }
    } else if (m_settings.type == Fletcher16) {
        quint32 a = m_value & 255, b = m_value >> 8;
        for (char byte : data) {
            a = (a + quint8(byte)) % 255;
            b = (b + a) % 255;
        }
        m_value = (b << 8) | a;
    } else if (m_settings.type == Internet16) {
        for (char byte : data) {
            if (m_pendingByte) {
                m_value += (quint32(m_lastByte) << 8) | quint8(byte);
                m_value = (m_value & 65535) + (m_value >> 16);
            } else
                m_lastByte = quint8(byte);
            m_pendingByte = !m_pendingByte;
        }
    } else if (m_settings.type == Blake2sp256) {
        blake2sp_update(&m_blakeSp, data.constData(), size_t(data.size()));
    } else if (m_settings.type == Blake2s256) {
        blake2s_update(&m_blakeS, data.constData(), size_t(data.size()));
    } else if (m_settings.type == Blake2b512) {
        blake2b_update(&m_blakeB, data.constData(), size_t(data.size()));
    } else if (isDigest(m_settings.type)) {
        m_hash.addData(data);
    } else if (m_settings.type == Adler32) {
        quint32 a = m_value & 65535, b = m_value >> 16;
        for (int offset = 0; offset < data.size();) {
            const int end = offset + qMin(5552, data.size() - offset);
            while (offset < end) {
                a += quint8(data.at(offset++));
                b += a;
            }
            a %= 65521;
            b %= 65521;
        }
        m_value = (b << 16) | a;
    } else if (m_settings.type == Xor8) {
        for (char byte : data)
            m_value ^= quint8(byte);
    } else if (m_settings.type == CRC) {
        // Generic table update for all 8–32 bit presets, including IEEE and Castagnoli.
        // Keep the unfinalized MSB register across blocks; finalize only in result().
        if (m_settings.crc.width >= 8) {
            const unsigned shift = m_settings.crc.width - 8;
            const quint32 mask = quint32((quint64(1) << m_settings.crc.width) - 1);
            for (unsigned char byte : data) {
                if (m_settings.crc.ref_in) {
                    byte = (byte >> 4) | (byte << 4);
                    byte = ((byte & 0xcc) >> 2) | ((byte & 0x33) << 2);
                    byte = ((byte & 0xaa) >> 1) | ((byte & 0x55) << 1);
                }
                m_value = ((m_value << 8) ^ m_crcTable[((m_value >> shift) ^ byte) & 255]) & mask;
            }
        } else {
            s_crc_conf block = m_settings.crc;
            block.init = m_value;
            block.ref_out = false;
            block.xor_out = 0;
            m_value = AppCalcCRC().calcCRC(
                block, reinterpret_cast<const uint8_t *>(data.constData()), data.size());
        }
    } else {
        for (char byte : data)
            m_value += static_cast<quint8>(byte);
    }
}

QByteArray AppCalcCheck::result() const {
    if (!isValid())
        return QByteArray();
    if (m_settings.type >= Xxh32 && m_settings.type <= Xxh3_128) {
        if (m_settings.type == Xxh32) {
            XXH32_canonical_t out;
            XXH32_canonicalFromHash(&out, XXH32_digest(m_xxh32));
            return QByteArray(reinterpret_cast<const char *>(out.digest), sizeof out.digest);
        }
        if (m_settings.type == Xxh3_128) {
            XXH128_canonical_t out;
            XXH128_canonicalFromHash(&out, XXH3_128bits_digest(m_xxh3));
            return QByteArray(reinterpret_cast<const char *>(out.digest), sizeof out.digest);
        }
        XXH64_canonical_t out;
        XXH64_canonicalFromHash(&out, m_settings.type == Xxh64 ? XXH64_digest(m_xxh64)
                                                               : XXH3_64bits_digest(m_xxh3));
        return QByteArray(reinterpret_cast<const char *>(out.digest), sizeof out.digest);
    }
    if (m_settings.type == Crc64Ecma || m_settings.type == Crc64Xz) {
        const quint64 value =
            m_wideValue ^ (m_settings.type == Crc64Xz ? Q_UINT64_C(0xffffffffffffffff) : 0);
        QByteArray out;
        for (int i = 7; i >= 0; --i)
            out.append(char(value >> (8 * i)));
        if (m_settings.littleEndian)
            std::reverse(out.begin(), out.end());
        return out;
    }
    // Finalize a copy so reading the result does not consume the streaming state.
    if (m_settings.type >= Blake2sp256 && m_settings.type <= Blake2b512) {
        QByteArray digest(m_settings.type == Blake2b512 ? 64 : 32, 0);
        if (m_settings.type == Blake2sp256) {
            auto state = m_blakeSp;
            blake2sp_final(&state, digest.data(), size_t(digest.size()));
        } else if (m_settings.type == Blake2s256) {
            auto state = m_blakeS;
            blake2s_final(&state, digest.data(), size_t(digest.size()));
        } else {
            auto state = m_blakeB;
            blake2b_final(&state, digest.data(), size_t(digest.size()));
        }
        return digest;
    }
    if (isDigest(m_settings.type))
        return m_hash.result();
    quint32 value = m_value;
    if (m_settings.type == Lrc8)
        value = (0U - value) & 255;
    if (m_settings.type == Internet16) {
        if (m_pendingByte)
            value += quint32(m_lastByte) << 8;
        while (value >> 16)
            value = (value & 65535) + (value >> 16);
        value = (~value) & 65535;
    }
    int length =
        m_settings.type == Sum8 || m_settings.type == Xor8 || m_settings.type == Lrc8 ? 1
        : m_settings.type == Sum16 || m_settings.type == Fletcher16 || m_settings.type == Internet16
            ? 2
            : 4;
    if (m_settings.type == CRC) {
        s_crc_conf final = m_settings.crc;
        final.init = value;
        AppCalcCRC calculator;
        value = calculator.calcCRC(final, nullptr, 0);
        length = (final.width + 7) / 8;
    }
    QByteArray bytes;
    for (int i = 0; i < length; ++i)
        bytes.append(static_cast<char>((value >> (8 * (length - i - 1))) & 0xFF));
    if (m_settings.littleEndian)
        std::reverse(bytes.begin(), bytes.end());
    return bytes;
}

QByteArray AppCalcCheck::calculate(const QByteArray &data, Type type, const s_crc_conf &crc,
                                   bool littleEndian) {
    Settings settings;
    settings.type = type;
    settings.crc = crc;
    settings.littleEndian = littleEndian;
    AppCalcCheck checksum(settings);
    checksum.addData(data);
    return checksum.result();
}

bool AppCalcCheck::parseHex(const QString &text, QByteArray &data, QString &error) {
    data.clear();
    error.clear();
    const QStringList groups = text.split(QRegularExpression("\\s+"), QString::SkipEmptyParts);
    if (groups.isEmpty()) {
        error = QStringLiteral("请输入 HEX 数据，例如 AA 55 11 22 33。");
        return false;
    }
    static const QRegularExpression hex("^[0-9A-Fa-f]+$");
    QByteArray parsed;
    for (int i = 0; i < groups.size(); ++i) {
        const QString &group = groups.at(i);
        if (!hex.match(group).hasMatch()) {
            error =
                QStringLiteral("第 %1 组包含非法字符，仅支持 0-9、A-F 和空白分隔符。").arg(i + 1);
            return false;
        }
        if (group.size() % 2) {
            error = QStringLiteral("第 %1 组 HEX 位数为奇数，请为每个字节输入两位，例如 0A。")
                        .arg(i + 1);
            return false;
        }
        parsed.append(QByteArray::fromHex(group.toLatin1()));
    }
    data = parsed;
    return true;
}
