/**
 * @file    CryptoSystem.cpp
 * @brief   AES-128-CBC / DES-CBC 加解密的实现。
 *
 * 本文件为纯 C++ 实现（C++11 起），不依赖 OpenSSL / Crypto++ 等第三方库，
 * 以保证在任意 MinGW + Qt 环境下均可直接编译通过。
 *
 * 实现要点：
 *  1. AES 的 S 盒由 GF(2^8) 乘法逆元 + 仿射变换在运行时生成，
 *     DES 的置换表完全采用 FIPS 46-3 标准给出的数据；
 *  2. 分组密码工作模式一律为 CBC，IV 固定（见下方常量），
 *     固定 IV 仅用于课程实验演示，真实系统应使用随机且不可重复的 IV；
 *  3. 填充方式为 PKCS#7；
 *  4. 用户口令先经 SHA-256 派生，再截取为对应算法的密钥长度，
 *     避免 DES 的 8 字节密钥在校验位处理上产生歧义；
 *  5. 解密时若密文长度非法或填充校验失败，返回 success = false。
 */

#include "CryptoSystem.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

// ===========================================================================
//  一、通用工具：Hex 编解码、PKCS#7 填充、计时
// ===========================================================================

typedef std::vector<std::uint8_t> ByteVec;

/// 单字节转两位大写十六进制。
std::string byteToHex(std::uint8_t value)
{
    static const char* kDigits = "0123456789ABCDEF";
    std::string result;
    result += kDigits[(value >> 4) & 0x0F];
    result += kDigits[value & 0x0F];
    return result;
}

/// 字节序列转大写十六进制字符串。
std::string bytesToHex(const ByteVec& data)
{
    std::string result;
    result.reserve(data.size() * 2);
    for (std::size_t i = 0; i < data.size(); ++i)
    {
        result += byteToHex(data[i]);
    }
    return result;
}

/// 十六进制字符转数值，非法字符返回 -1。
int hexDigitValue(char ch)
{
    if (ch >= '0' && ch <= '9') { return ch - '0'; }
    if (ch >= 'a' && ch <= 'f') { return ch - 'a' + 10; }
    if (ch >= 'A' && ch <= 'F') { return ch - 'A' + 10; }
    return -1;
}

/**
 * @brief 十六进制字符串转字节序列。
 * @param[in]  hex   输入字符串，允许大小写混用。
 * @param[out] out   解析结果。
 * @return 长度非偶数或含非法字符时返回 false。
 */
bool hexToBytes(const std::string& hex, ByteVec& out)
{
    if (hex.size() % 2 != 0)
    {
        return false;
    }

    out.clear();
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2)
    {
        const int high = hexDigitValue(hex[i]);
        const int low  = hexDigitValue(hex[i + 1]);
        if (high < 0 || low < 0)
        {
            return false;
        }
        out.push_back(static_cast<std::uint8_t>((high << 4) | low));
    }
    return true;
}

/// PKCS#7 填充：在末尾补齐 padLen 个值为 padLen 的字节。
void pkcs7Pad(ByteVec& data, std::size_t blockSize)
{
    const std::size_t padLen = blockSize - (data.size() % blockSize);
    data.insert(data.end(), padLen, static_cast<std::uint8_t>(padLen));
}

/**
 * @brief PKCS#7 去填充。
 * @return 填充格式非法时返回 false（通常意味着密钥错误或密文被篡改）。
 */
bool pkcs7Unpad(ByteVec& data, std::size_t blockSize)
{
    if (data.empty() || data.size() % blockSize != 0)
    {
        return false;
    }

    const std::size_t padLen = data.back();
    if (padLen == 0 || padLen > blockSize || padLen > data.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < padLen; ++i)
    {
        if (data[data.size() - 1 - i] != padLen)
        {
            return false;
        }
    }

    data.resize(data.size() - padLen);
    return true;
}

/// 返回当前单调时钟的毫秒数，用于统计算法耗时。
double nowMs()
{
    const std::chrono::steady_clock::time_point tp =
        std::chrono::steady_clock::now();
    return static_cast<double>(
               std::chrono::duration_cast<std::chrono::microseconds>(
                   tp.time_since_epoch()).count()) / 1000.0;
}

// ===========================================================================
//  二、SHA-256：可选的口令派生工具
//
//  注意：encrypt / decrypt 系列接口**不**使用本节的函数。
//  按《共同开发规则和接口约定》第 12 节，接口直接使用调用方给出的密钥字节，
//  这样可以与标准测试向量逐字节比对，行为对调用方完全透明。
//  derivePassphraseKey 仅供界面层在"用户随意输入口令"的场景下调用。
// ===========================================================================

/// SHA-256 消息扩展常量表，取自 FIPS 180-4。
const std::uint32_t kSha256K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

/// 32 位循环右移。
inline std::uint32_t rotr32(std::uint32_t value, int bits)
{
    return (value >> bits) | (value << (32 - bits));
}

/// 计算 SHA-256，返回 32 字节摘要。
ByteVec sha256(const std::string& message)
{
    std::uint32_t h[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
    };

    ByteVec block(message.begin(), message.end());
    const std::uint64_t bitLen =
        static_cast<std::uint64_t>(message.size()) * 8u;

    // 附加 0x80，再补零至长度 ≡ 56 (mod 64)，最后写入 64 位大端比特长度。
    block.push_back(0x80u);
    while (block.size() % 64 != 56)
    {
        block.push_back(0x00u);
    }
    for (int i = 7; i >= 0; --i)
    {
        block.push_back(static_cast<std::uint8_t>((bitLen >> (i * 8)) & 0xFFu));
    }

    for (std::size_t offset = 0; offset < block.size(); offset += 64)
    {
        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i)
        {
            const std::size_t p = offset + static_cast<std::size_t>(i) * 4;
            w[i] = (static_cast<std::uint32_t>(block[p])     << 24)
                 | (static_cast<std::uint32_t>(block[p + 1]) << 16)
                 | (static_cast<std::uint32_t>(block[p + 2]) << 8)
                 |  static_cast<std::uint32_t>(block[p + 3]);
        }
        for (int i = 16; i < 64; ++i)
        {
            const std::uint32_t s0 = rotr32(w[i - 15], 7)
                                   ^ rotr32(w[i - 15], 18)
                                   ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr32(w[i - 2], 17)
                                   ^ rotr32(w[i - 2], 19)
                                   ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        std::uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

        for (int i = 0; i < 64; ++i)
        {
            const std::uint32_t s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
            const std::uint32_t ch = (e & f) ^ ((~e) & g);
            const std::uint32_t t1 = hh + s1 + ch + kSha256K[i] + w[i];
            const std::uint32_t s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t t2 = s0 + maj;

            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }

        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    ByteVec digest;
    digest.reserve(32);
    for (int i = 0; i < 8; ++i)
    {
        for (int shift = 24; shift >= 0; shift -= 8)
        {
            digest.push_back(static_cast<std::uint8_t>((h[i] >> shift) & 0xFFu));
        }
    }
    return digest;
}

/// 由任意长度口令派生出 keyLen 字节密钥（SHA-256 后截断）。
/// 命名与 CryptoSystem::derivePassphraseKey 区分，避免调用时歧义。
ByteVec sha256DerivedKey(const std::string& passphrase, std::size_t keyLen)
{
    ByteVec digest = sha256(passphrase);
    digest.resize(keyLen);
    return digest;
}

// ===========================================================================
//  三、AES-128 分组算法
// ===========================================================================

/// AES-128 的参数：分组字节数与轮数。
constexpr std::size_t AES_BLOCK_BYTES = 16;
constexpr std::size_t AES_KEY_LEN     = 16;
constexpr int         AES_ROUNDS      = 10;

/**
 * @brief 在 GF(2^8) 上求乘法逆元（模不可约多项式 0x11B）。
 *
 * 采用经典的"乘法 + 反复平方"扩展欧几里得替代方案：利用 a^254 = a^{-1}
 * （因 GF(2^8)* 的阶为 255，故 a^255 = 1）。0 的逆元定义为 0。
 */
std::uint8_t gfInverse(std::uint8_t a)
{
    if (a == 0)
    {
        return 0;
    }

    // 计算 a^254：二进制 254 = 11111110b
    std::uint8_t result = 1;
    std::uint8_t base   = a;
    for (int i = 0; i < 8; ++i)
    {
        if ((254 >> i) & 1)
        {
            // GF(2^8) 乘法（模 0x11B）
            std::uint8_t product = 0;
            std::uint8_t x = result;
            std::uint8_t y = base;
            for (int bit = 0; bit < 8; ++bit)
            {
                if (y & 1)
                {
                    product ^= x;
                }
                const std::uint8_t highBit = static_cast<std::uint8_t>(x & 0x80u);
                x = static_cast<std::uint8_t>(x << 1);
                if (highBit)
                {
                    x ^= 0x1Bu;   // 约简多项式 x^8+x^4+x^3+x+1 的低 8 位
                }
                y = static_cast<std::uint8_t>(y >> 1);
            }
            result = product;
        }
        // base = base^2
        std::uint8_t square = 0;
        std::uint8_t x = base;
        std::uint8_t y = base;
        for (int bit = 0; bit < 8; ++bit)
        {
            if (y & 1)
            {
                square ^= x;
            }
            const std::uint8_t highBit = static_cast<std::uint8_t>(x & 0x80u);
            x = static_cast<std::uint8_t>(x << 1);
            if (highBit)
            {
                x ^= 0x1Bu;
            }
            y = static_cast<std::uint8_t>(y >> 1);
        }
        base = square;
    }
    return result;
}

/// 仿射变换：b_i = s_i ^ s_{(i+4)%8} ^ s_{(i+5)%8} ^ s_{(i+6)%8} ^ s_{(i+7)%8} ^ c_i
std::uint8_t aesAffine(std::uint8_t value)
{
    std::uint8_t result = 0;
    for (int i = 0; i < 8; ++i)
    {
        const std::uint8_t bit = static_cast<std::uint8_t>(
            ((value >> i) & 1)
            ^ ((value >> ((i + 4) % 8)) & 1)
            ^ ((value >> ((i + 5) % 8)) & 1)
            ^ ((value >> ((i + 6) % 8)) & 1)
            ^ ((value >> ((i + 7) % 8)) & 1)
            ^ ((0x63u >> i) & 1));
        result = static_cast<std::uint8_t>(result | (bit << i));
    }
    return result;
}

/// AES 查表与 S 盒的惰性初始化缓存。
struct AesTables
{
    std::uint8_t sbox[256];
    std::uint8_t invSbox[256];
    std::uint8_t rcon[11];

    /// 预计算的 GF(2^8) 乘法表：mul2[a] = 2·a，mul3[a] = 3·a。
    /// MixColumns 与 InvMixColumns 的系数全部可由它们异或组合得到，
    /// 从而把热路径上的乘法化简为查表与异或。
    std::uint8_t mul2[256];
    std::uint8_t mul3[256];

    AesTables()
    {
        for (int i = 0; i < 256; ++i)
        {
            sbox[i] = aesAffine(gfInverse(static_cast<std::uint8_t>(i)));
        }
        for (int i = 0; i < 256; ++i)
        {
            invSbox[sbox[i]] = static_cast<std::uint8_t>(i);
        }
        rcon[0] = 0x00;
        for (int round = 1; round <= AES_ROUNDS; ++round)
        {
            // rcon[i] = x^(i-1) 在 GF(2^8) 中的值
            std::uint8_t value = 1;
            for (int step = 1; step < round; ++step)
            {
                const std::uint8_t highBit =
                    static_cast<std::uint8_t>(value & 0x80u);
                value = static_cast<std::uint8_t>(value << 1);
                if (highBit)
                {
                    value ^= 0x1Bu;
                }
            }
            rcon[round] = value;
        }

        // mul2 与 mul3：Multiply-by-2 即左移一位，溢出时异或约简多项式 0x1B。
        for (int i = 0; i < 256; ++i)
        {
            const std::uint8_t a = static_cast<std::uint8_t>(i);
            const std::uint8_t doubled =
                static_cast<std::uint8_t>((a << 1) ^ ((a & 0x80u) ? 0x1Bu : 0x00u));
            mul2[i] = doubled;
            mul3[i] = static_cast<std::uint8_t>(doubled ^ a);
        }
    }
};

const AesTables& aesTables()
{
    static const AesTables tables;
    return tables;
}

/// AES-128 密钥扩展：16 字节密钥 -> 11 组轮密钥，每组 16 字节。
void aesExpandKey(const ByteVec& key, std::uint8_t roundKeys[AES_ROUNDS + 1][AES_BLOCK_BYTES])
{
    const AesTables& tables = aesTables();

    // 前 16 字节即原始密钥。
    for (std::size_t i = 0; i < AES_BLOCK_BYTES; ++i)
    {
        roundKeys[0][i] = key[i];
    }

    std::uint8_t temp[4];
    for (int round = 1; round <= AES_ROUNDS; ++round)
    {
        // tmp = SubWord(RotWord(W[i-1])) ^ Rcon[i]，
        // 即取上一轮密钥的最后一列循环左移一个字节，过 S 盒后异或轮常量。
        const std::uint8_t* prev = roundKeys[round - 1];
        temp[0] = static_cast<std::uint8_t>(tables.sbox[prev[13]] ^ tables.rcon[round]);
        temp[1] = tables.sbox[prev[14]];
        temp[2] = tables.sbox[prev[15]];
        temp[3] = tables.sbox[prev[12]];

        std::uint8_t* cur = roundKeys[round];
        for (int i = 0; i < 4; ++i)
        {
            cur[i] = static_cast<std::uint8_t>(prev[i] ^ temp[i]);
        }
        for (int col = 1; col < 4; ++col)
        {
            for (int i = 0; i < 4; ++i)
            {
                const int index = col * 4 + i;
                cur[index] = static_cast<std::uint8_t>(prev[index] ^ cur[index - 4]);
            }
        }
    }
}

/// GF(2^8) 上的乘法，用于 MixColumns / InvMixColumns。
///
/// 实现采用 xtime 分解而非逐位乘法：把系数写成 2 的幂之和后反复调用
/// multiply-by-2，每次只需一次移位与一次条件异或。
/// 早期的逐位版本在 MixColumns 的热路径上被调用数百万次，
/// 是本模块的主要性能瓶颈。
inline std::uint8_t gfMul(std::uint8_t a, std::uint8_t b)
{
    std::uint8_t result = 0;
    std::uint8_t value  = a;

    for (std::uint8_t factor = b; factor != 0; factor >>= 1)
    {
        if (factor & 1u)
        {
            result ^= value;
        }
        // value *= 2（模约简多项式 0x11B）
        const std::uint8_t highBit = static_cast<std::uint8_t>(value & 0x80u);
        value = static_cast<std::uint8_t>(value << 1);
        if (highBit)
        {
            value ^= 0x1Bu;
        }
    }
    return result;
}

/// 状态按列优先排列：state[col * 4 + row]。
inline void aesAddRoundKey(std::uint8_t state[AES_BLOCK_BYTES],
                           const std::uint8_t roundKey[AES_BLOCK_BYTES])
{
    for (std::size_t i = 0; i < AES_BLOCK_BYTES; ++i)
    {
        state[i] = static_cast<std::uint8_t>(state[i] ^ roundKey[i]);
    }
}

inline void aesSubBytes(std::uint8_t state[AES_BLOCK_BYTES], bool inverse)
{
    const AesTables& tables = aesTables();
    for (std::size_t i = 0; i < AES_BLOCK_BYTES; ++i)
    {
        state[i] = inverse ? tables.invSbox[state[i]] : tables.sbox[state[i]];
    }
}

inline void aesShiftRows(std::uint8_t state[AES_BLOCK_BYTES], bool inverse)
{
    // 第 r 行循环左移 r 字节（逆变换为循环右移 r 字节）。
    for (int row = 1; row < 4; ++row)
    {
        std::uint8_t line[4];
        for (int col = 0; col < 4; ++col)
        {
            line[col] = state[col * 4 + row];
        }

        std::uint8_t shifted[4];
        for (int col = 0; col < 4; ++col)
        {
            const int source = inverse ? (col - row + 4) % 4 : (col + row) % 4;
            shifted[col] = line[source];
        }

        for (int col = 0; col < 4; ++col)
        {
            state[col * 4 + row] = shifted[col];
        }
    }
}

inline void aesMixColumns(std::uint8_t state[AES_BLOCK_BYTES], bool inverse)
{
    // 取一次查表引用，避免在内层循环里反复调用 aesTables()。
    const AesTables& tables = aesTables();
    const std::uint8_t* const m2 = tables.mul2;
    const std::uint8_t* const m3 = tables.mul3;

    for (int col = 0; col < 4; ++col)
    {
        const std::uint8_t a0 = state[col * 4 + 0];
        const std::uint8_t a1 = state[col * 4 + 1];
        const std::uint8_t a2 = state[col * 4 + 2];
        const std::uint8_t a3 = state[col * 4 + 3];

        if (!inverse)
        {
            // 系数矩阵为 [2 3 1 1] 的循环移位。
            state[col * 4 + 0] = static_cast<std::uint8_t>(
                m2[a0] ^ m3[a1] ^ a2 ^ a3);
            state[col * 4 + 1] = static_cast<std::uint8_t>(
                a0 ^ m2[a1] ^ m3[a2] ^ a3);
            state[col * 4 + 2] = static_cast<std::uint8_t>(
                a0 ^ a1 ^ m2[a2] ^ m3[a3]);
            state[col * 4 + 3] = static_cast<std::uint8_t>(
                m3[a0] ^ a1 ^ a2 ^ m2[a3]);
        }
        else
        {
            // 逆矩阵系数为 [14 11 13 9] 的循环移位。
            // 14 = 2·(2·(2·2)) ⊕ 2·(2·2) ⊕ 2·2，11 = 9 ⊕ 2，13 = 9 ⊕ 2·2，
            // 统一用 mul2 反复组合，避免逐次调用通用乘法。
            const std::uint8_t a0_2 = m2[a0], a0_4 = m2[a0_2], a0_8 = m2[a0_4];
            const std::uint8_t a0_9 = static_cast<std::uint8_t>(a0_8 ^ a0);
            const std::uint8_t a0_11 = static_cast<std::uint8_t>(a0_9 ^ a0_2);
            const std::uint8_t a0_13 = static_cast<std::uint8_t>(a0_9 ^ a0_4);
            const std::uint8_t a0_14 = static_cast<std::uint8_t>(
                a0_8 ^ a0_4 ^ a0_2);

            const std::uint8_t a1_2 = m2[a1], a1_4 = m2[a1_2], a1_8 = m2[a1_4];
            const std::uint8_t a1_9 = static_cast<std::uint8_t>(a1_8 ^ a1);
            const std::uint8_t a1_11 = static_cast<std::uint8_t>(a1_9 ^ a1_2);
            const std::uint8_t a1_13 = static_cast<std::uint8_t>(a1_9 ^ a1_4);
            const std::uint8_t a1_14 = static_cast<std::uint8_t>(
                a1_8 ^ a1_4 ^ a1_2);

            const std::uint8_t a2_2 = m2[a2], a2_4 = m2[a2_2], a2_8 = m2[a2_4];
            const std::uint8_t a2_9 = static_cast<std::uint8_t>(a2_8 ^ a2);
            const std::uint8_t a2_11 = static_cast<std::uint8_t>(a2_9 ^ a2_2);
            const std::uint8_t a2_13 = static_cast<std::uint8_t>(a2_9 ^ a2_4);
            const std::uint8_t a2_14 = static_cast<std::uint8_t>(
                a2_8 ^ a2_4 ^ a2_2);

            const std::uint8_t a3_2 = m2[a3], a3_4 = m2[a3_2], a3_8 = m2[a3_4];
            const std::uint8_t a3_9 = static_cast<std::uint8_t>(a3_8 ^ a3);
            const std::uint8_t a3_11 = static_cast<std::uint8_t>(a3_9 ^ a3_2);
            const std::uint8_t a3_13 = static_cast<std::uint8_t>(a3_9 ^ a3_4);
            const std::uint8_t a3_14 = static_cast<std::uint8_t>(
                a3_8 ^ a3_4 ^ a3_2);

            state[col * 4 + 0] = static_cast<std::uint8_t>(
                a0_14 ^ a1_11 ^ a2_13 ^ a3_9);
            state[col * 4 + 1] = static_cast<std::uint8_t>(
                a0_9 ^ a1_14 ^ a2_11 ^ a3_13);
            state[col * 4 + 2] = static_cast<std::uint8_t>(
                a0_13 ^ a1_9 ^ a2_14 ^ a3_11);
            state[col * 4 + 3] = static_cast<std::uint8_t>(
                a0_11 ^ a1_13 ^ a2_9 ^ a3_14);
        }
    }
}

/**
 * @brief 对单个 16 字节分组做 AES-128 变换。
 * @note  以 ECB 方式处理单块，供 CBC 调用；公开出来是为了便于与 FIPS-197
 *        标准测试向量做逐字节比对。
 */
void aesProcessBlock(const std::uint8_t input[AES_BLOCK_BYTES],
                     const ByteVec& key,
                     bool encrypt,
                     std::uint8_t output[AES_BLOCK_BYTES])
{
    std::uint8_t roundKeys[AES_ROUNDS + 1][AES_BLOCK_BYTES];
    aesExpandKey(key, roundKeys);

    std::uint8_t state[AES_BLOCK_BYTES];
    std::memcpy(state, input, AES_BLOCK_BYTES);

    if (encrypt)
    {
        aesAddRoundKey(state, roundKeys[0]);
        for (int round = 1; round < AES_ROUNDS; ++round)
        {
            aesSubBytes(state, false);
            aesShiftRows(state, false);
            aesMixColumns(state, false);
            aesAddRoundKey(state, roundKeys[round]);
        }
        aesSubBytes(state, false);
        aesShiftRows(state, false);
        aesAddRoundKey(state, roundKeys[AES_ROUNDS]);
    }
    else
    {
        aesAddRoundKey(state, roundKeys[AES_ROUNDS]);
        for (int round = AES_ROUNDS - 1; round >= 1; --round)
        {
            aesShiftRows(state, true);
            aesSubBytes(state, true);
            aesAddRoundKey(state, roundKeys[round]);
            aesMixColumns(state, true);
        }
        aesShiftRows(state, true);
        aesSubBytes(state, true);
        aesAddRoundKey(state, roundKeys[0]);
    }

    std::memcpy(output, state, AES_BLOCK_BYTES);
}

// ===========================================================================
//  四、DES 分组算法
// ===========================================================================

constexpr std::size_t DES_BLOCK_BYTES = 8;
constexpr std::size_t DES_KEY_BYTES_LOCAL = 8;

/// 密钥置换 PC-1：64 位 -> 56 位（去掉 8 个校验位）。
const int kDesPc1[56] = {
    57, 49, 41, 33, 25, 17,  9,
     1, 58, 50, 42, 34, 26, 18,
    10,  2, 59, 51, 43, 35, 27,
    19, 11,  3, 60, 52, 44, 36,
    63, 55, 47, 39, 31, 23, 15,
     7, 62, 54, 46, 38, 30, 22,
    14,  6, 61, 53, 45, 37, 29,
    21, 13,  5, 28, 20, 12,  4
};

/// 压缩置换 PC-2：56 位 -> 48 位轮密钥。
const int kDesPc2[48] = {
    14, 17, 11, 24,  1,  5,
     3, 28, 15,  6, 21, 10,
    23, 19, 12,  4, 26,  8,
    16,  7, 27, 20, 13,  2,
    41, 52, 31, 37, 47, 55,
    30, 40, 51, 45, 33, 48,
    44, 49, 39, 56, 34, 53,
    46, 42, 50, 36, 29, 32
};

/// 每轮密钥左移的位数。
const int kDesShifts[16] = {
    1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1
};

/// 初始置换 IP。
const int kDesIp[64] = {
    58, 50, 42, 34, 26, 18, 10,  2,
    60, 52, 44, 36, 28, 20, 12,  4,
    62, 54, 46, 38, 30, 22, 14,  6,
    64, 56, 48, 40, 32, 24, 16,  8,
    57, 49, 41, 33, 25, 17,  9,  1,
    59, 51, 43, 35, 27, 19, 11,  3,
    61, 53, 45, 37, 29, 21, 13,  5,
    63, 55, 47, 39, 31, 23, 15,  7
};

/// 逆初始置换 IP^-1。
const int kDesIpInv[64] = {
    40,  8, 48, 16, 56, 24, 64, 32,
    39,  7, 47, 15, 55, 23, 63, 31,
    38,  6, 46, 14, 54, 22, 62, 30,
    37,  5, 45, 13, 53, 21, 61, 29,
    36,  4, 44, 12, 52, 20, 60, 28,
    35,  3, 43, 11, 51, 19, 59, 27,
    34,  2, 42, 10, 50, 18, 58, 26,
    33,  1, 41,  9, 49, 17, 57, 25
};

/// 扩展置换 E：32 位 -> 48 位。
const int kDesE[48] = {
    32,  1,  2,  3,  4,  5,
     4,  5,  6,  7,  8,  9,
     8,  9, 10, 11, 12, 13,
    12, 13, 14, 15, 16, 17,
    16, 17, 18, 19, 20, 21,
    20, 21, 22, 23, 24, 25,
    24, 25, 26, 27, 28, 29,
    28, 29, 30, 31, 32,  1
};

/// 轮函数末尾的置换 P。
const int kDesP[32] = {
    16,  7, 20, 21, 29, 12, 28, 17,
     1, 15, 23, 26,  5, 18, 31, 10,
     2,  8, 24, 14, 32, 27,  3,  9,
    19, 13, 30,  6, 22, 11,  4, 25
};

/// 8 个 S 盒，取自 FIPS 46-3。每盒 4 行 × 16 列。
const std::uint8_t kDesSBox[8][4][16] = {
    {   // S1
        {14,  4, 13,  1,  2, 15, 11,  8,  3, 10,  6, 12,  5,  9,  0,  7},
        { 0, 15,  7,  4, 14,  2, 13,  1, 10,  6, 12, 11,  9,  5,  3,  8},
        { 4,  1, 14,  8, 13,  6,  2, 11, 15, 12,  9,  7,  3, 10,  5,  0},
        {15, 12,  8,  2,  4,  9,  1,  7,  5, 11,  3, 14, 10,  0,  6, 13}
    },
    {   // S2
        {15,  1,  8, 14,  6, 11,  3,  4,  9,  7,  2, 13, 12,  0,  5, 10},
        { 3, 13,  4,  7, 15,  2,  8, 14, 12,  0,  1, 10,  6,  9, 11,  5},
        { 0, 14,  7, 11, 10,  4, 13,  1,  5,  8, 12,  6,  9,  3,  2, 15},
        {13,  8, 10,  1,  3, 15,  4,  2, 11,  6,  7, 12,  0,  5, 14,  9}
    },
    {   // S3
        {10,  0,  9, 14,  6,  3, 15,  5,  1, 13, 12,  7, 11,  4,  2,  8},
        {13,  7,  0,  9,  3,  4,  6, 10,  2,  8,  5, 14, 12, 11, 15,  1},
        {13,  6,  4,  9,  8, 15,  3,  0, 11,  1,  2, 12,  5, 10, 14,  7},
        { 1, 10, 13,  0,  6,  9,  8,  7,  4, 15, 14,  3, 11,  5,  2, 12}
    },
    {   // S4
        { 7, 13, 14,  3,  0,  6,  9, 10,  1,  2,  8,  5, 11, 12,  4, 15},
        {13,  8, 11,  5,  6, 15,  0,  3,  4,  7,  2, 12,  1, 10, 14,  9},
        {10,  6,  9,  0, 12, 11,  7, 13, 15,  1,  3, 14,  5,  2,  8,  4},
        { 3, 15,  0,  6, 10,  1, 13,  8,  9,  4,  5, 11, 12,  7,  2, 14}
    },
    {   // S5
        { 2, 12,  4,  1,  7, 10, 11,  6,  8,  5,  3, 15, 13,  0, 14,  9},
        {14, 11,  2, 12,  4,  7, 13,  1,  5,  0, 15, 10,  3,  9,  8,  6},
        { 4,  2,  1, 11, 10, 13,  7,  8, 15,  9, 12,  5,  6,  3,  0, 14},
        {11,  8, 12,  7,  1, 14,  2, 13,  6, 15,  0,  9, 10,  4,  5,  3}
    },
    {   // S6
        {12,  1, 10, 15,  9,  2,  6,  8,  0, 13,  3,  4, 14,  7,  5, 11},
        {10, 15,  4,  2,  7, 12,  9,  5,  6,  1, 13, 14,  0, 11,  3,  8},
        { 9, 14, 15,  5,  2,  8, 12,  3,  7,  0,  4, 10,  1, 13, 11,  6},
        { 4,  3,  2, 12,  9,  5, 15, 10, 11, 14,  1,  7,  6,  0,  8, 13}
    },
    {   // S7
        { 4, 11,  2, 14, 15,  0,  8, 13,  3, 12,  9,  7,  5, 10,  6,  1},
        {13,  0, 11,  7,  4,  9,  1, 10, 14,  3,  5, 12,  2, 15,  8,  6},
        { 1,  4, 11, 13, 12,  3,  7, 14, 10, 15,  6,  8,  0,  5,  9,  2},
        { 6, 11, 13,  8,  1,  4, 10,  7,  9,  5,  0, 15, 14,  2,  3, 12}
    },
    {   // S8
        {13,  2,  8,  4,  6, 15, 11,  1, 10,  9,  3, 14,  5,  0, 12,  7},
        { 1, 15, 13,  8, 10,  3,  7,  4, 12,  5,  6, 11,  0, 14,  9,  2},
        { 7, 11,  4,  1,  9, 12, 14,  2,  0,  6, 10, 13, 15,  3,  5,  8},
        { 2,  1, 14,  7,  4, 10,  8, 13, 15, 12,  9,  0,  3,  5,  6, 11}
    }
};

/// 取 64 位数据中第 pos 位（pos 从 1 开始，按标准编号），返回 0 或 1。
inline int desBit(const std::uint8_t block[8], int pos)
{
    const int byteIndex = (pos - 1) / 8;
    const int bitIndex  = 7 - ((pos - 1) % 8);
    return (block[byteIndex] >> bitIndex) & 1;
}

/// 按 pos 从 1 开始编号，把第 pos 位写入 64 位缓冲区。
inline void desSetBit(std::uint8_t block[8], int pos, int value)
{
    if (value == 0)
    {
        return;
    }
    const int byteIndex = (pos - 1) / 8;
    const int bitIndex  = 7 - ((pos - 1) % 8);
    block[byteIndex] = static_cast<std::uint8_t>(block[byteIndex] | (1 << bitIndex));
}

/// 按置换表对 8 字节输入做置换，输出写入 outBits 位。
void desPermute(const std::uint8_t in[8], const int* table, int outBits, std::uint8_t out[8])
{
    std::memset(out, 0, 8);
    for (int i = 0; i < outBits; ++i)
    {
        desSetBit(out, i + 1, desBit(in, table[i]));
    }
}

/// DES 密钥调度：8 字节密钥 -> 16 个 48 位子密钥（各存于 8 字节缓冲区，高位对齐）。
void desExpandKey(const std::uint8_t key[8], std::uint8_t subKeys[16][8])
{
    std::uint8_t permuted[8];
    desPermute(key, kDesPc1, 56, permuted);

    // 把 56 位结果拆成左右各 28 位。
    std::uint32_t c = 0;
    std::uint32_t d = 0;
    for (int i = 0; i < 28; ++i)
    {
        c = (c << 1) | static_cast<std::uint32_t>(desBit(permuted, i + 1));
        d = (d << 1) | static_cast<std::uint32_t>(desBit(permuted, i + 29));
    }

    for (int round = 0; round < 16; ++round)
    {
        const int shift = kDesShifts[round];
        c = ((c << shift) | (c >> (28 - shift))) & 0x0FFFFFFFu;
        d = ((d << shift) | (d >> (28 - shift))) & 0x0FFFFFFFu;

        // 合并 C||D 为 56 位并做 PC-2 压缩。
        std::uint8_t merged[8];
        std::memset(merged, 0, 8);
        for (int i = 0; i < 28; ++i)
        {
            desSetBit(merged, i + 1,
                      static_cast<int>((c >> (27 - i)) & 1u));
            desSetBit(merged, i + 29,
                      static_cast<int>((d >> (27 - i)) & 1u));
        }
        desPermute(merged, kDesPc2, 48, subKeys[round]);
    }
}

/// DES 轮函数 F：32 位右半部分与 48 位子密钥 -> 32 位输出。
std::uint32_t desFeistel(std::uint32_t right, const std::uint8_t subKey[8])
{
    // 1) 扩展置换 E，把 32 位扩到 48 位。
    std::uint8_t expanded[8];
    std::memset(expanded, 0, 8);
    for (int i = 0; i < 48; ++i)
    {
        const int sourcePos = kDesE[i];               // 1..32
        const int bit = static_cast<int>((right >> (32 - sourcePos)) & 1u);
        desSetBit(expanded, i + 1, bit);
    }

    // 2) 与子密钥异或，结果按 6 位一组分成 8 组。
    std::uint8_t xored[8];
    for (int i = 0; i < 8; ++i)
    {
        xored[i] = static_cast<std::uint8_t>(expanded[i] ^ subKey[i]);
    }

    // 3) 过 S 盒，得到 32 位。
    std::uint32_t substituted = 0;
    for (int box = 0; box < 8; ++box)
    {
        const std::uint8_t six = xored[box];
        // 每组 6 位：首位与末位构成行号，中间 4 位构成列号。
        const int row = ((six >> 5) & 1) * 2 + (six & 1);
        const int col = (six >> 1) & 0x0F;
        substituted = (substituted << 4)
                    | static_cast<std::uint32_t>(kDesSBox[box][row][col]);
    }

    // 4) 过置换 P。
    std::uint8_t beforeP[8];
    std::memset(beforeP, 0, 8);
    for (int i = 0; i < 32; ++i)
    {
        desSetBit(beforeP, i + 1,
                  static_cast<int>((substituted >> (31 - i)) & 1u));
    }

    std::uint8_t afterP[8];
    desPermute(beforeP, kDesP, 32, afterP);

    std::uint32_t result = 0;
    for (int i = 0; i < 32; ++i)
    {
        result = (result << 1) | static_cast<std::uint32_t>(desBit(afterP, i + 1));
    }
    return result;
}

/// 对单个 8 字节分组做 DES 变换（encrypt 为 false 时子密钥逆序使用）。
void desProcessBlock(const std::uint8_t input[8],
                     const ByteVec& key,
                     bool encrypt,
                     std::uint8_t output[8])
{
    std::uint8_t subKeys[16][8];
    desExpandKey(&key[0], subKeys);

    std::uint8_t permuted[8];
    desPermute(input, kDesIp, 64, permuted);

    std::uint32_t left = 0;
    std::uint32_t right = 0;
    for (int i = 0; i < 32; ++i)
    {
        left  = (left  << 1) | static_cast<std::uint32_t>(desBit(permuted, i + 1));
        right = (right << 1) | static_cast<std::uint32_t>(desBit(permuted, i + 33));
    }

    for (int round = 0; round < 16; ++round)
    {
        const int keyIndex = encrypt ? round : (15 - round);
        const std::uint32_t f = desFeistel(right, subKeys[keyIndex]);
        const std::uint32_t next = left ^ f;
        left  = right;
        right = next;
    }

    // 最后一轮后左右不交换，合并为 R16 || L16。
    std::uint8_t preOutput[8];
    std::memset(preOutput, 0, 8);
    for (int i = 0; i < 32; ++i)
    {
        desSetBit(preOutput, i + 1,
                  static_cast<int>((right >> (31 - i)) & 1u));
        desSetBit(preOutput, i + 33,
                  static_cast<int>((left >> (31 - i)) & 1u));
    }

    desPermute(preOutput, kDesIpInv, 64, output);
}

// ===========================================================================
//  五、CBC 工作模式（对两种分组算法通用）
// ===========================================================================

/// AES-CBC 使用的固定初始向量。仅用于课程实验演示。
const std::uint8_t kAesIv[16] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
};

/// DES-CBC 使用的固定初始向量。仅用于课程实验演示。
const std::uint8_t kDesIv[8] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77
};

/// AES-CBC 加密，输入需已按 16 字节对齐。
/// @param iv 为 nullptr 时使用固定 IV，否则使用调用方给定的 16 字节 IV。
ByteVec aesCbcEncrypt(const ByteVec& data, const ByteVec& key, const std::uint8_t* iv)
{
    ByteVec result;
    result.reserve(data.size());

    std::uint8_t previous[16];
    std::memcpy(previous, iv != nullptr ? iv : kAesIv, 16);

    for (std::size_t offset = 0; offset < data.size(); offset += 16)
    {
        std::uint8_t blockIn[16];
        for (std::size_t i = 0; i < 16; ++i)
        {
            blockIn[i] = static_cast<std::uint8_t>(data[offset + i] ^ previous[i]);
        }

        std::uint8_t blockOut[16];
        aesProcessBlock(blockIn, key, true, blockOut);

        result.insert(result.end(), blockOut, blockOut + 16);
        std::memcpy(previous, blockOut, 16);
    }
    return result;
}

/// AES-CBC 解密，输入需已按 16 字节对齐。
ByteVec aesCbcDecrypt(const ByteVec& data, const ByteVec& key, const std::uint8_t* iv)
{
    ByteVec result;
    result.reserve(data.size());

    std::uint8_t previous[16];
    std::memcpy(previous, iv != nullptr ? iv : kAesIv, 16);

    for (std::size_t offset = 0; offset < data.size(); offset += 16)
    {
        std::uint8_t blockIn[16];
        std::memcpy(blockIn, &data[offset], 16);

        std::uint8_t blockOut[16];
        aesProcessBlock(blockIn, key, false, blockOut);

        for (std::size_t i = 0; i < 16; ++i)
        {
            result.push_back(static_cast<std::uint8_t>(blockOut[i] ^ previous[i]));
        }
        std::memcpy(previous, blockIn, 16);
    }
    return result;
}

/// DES-CBC 加密，输入需已按 8 字节对齐。
/// @param iv 为 nullptr 时使用固定 IV，否则使用调用方给定的 8 字节 IV。
ByteVec desCbcEncrypt(const ByteVec& data, const ByteVec& key, const std::uint8_t* iv)
{
    ByteVec result;
    result.reserve(data.size());

    std::uint8_t previous[8];
    std::memcpy(previous, iv != nullptr ? iv : kDesIv, 8);

    for (std::size_t offset = 0; offset < data.size(); offset += 8)
    {
        std::uint8_t blockIn[8];
        for (std::size_t i = 0; i < 8; ++i)
        {
            blockIn[i] = static_cast<std::uint8_t>(data[offset + i] ^ previous[i]);
        }

        std::uint8_t blockOut[8];
        desProcessBlock(blockIn, key, true, blockOut);

        result.insert(result.end(), blockOut, blockOut + 8);
        std::memcpy(previous, blockOut, 8);
    }
    return result;
}

/// DES-CBC 解密，输入需已按 8 字节对齐。
ByteVec desCbcDecrypt(const ByteVec& data, const ByteVec& key, const std::uint8_t* iv)
{
    ByteVec result;
    result.reserve(data.size());

    std::uint8_t previous[8];
    std::memcpy(previous, iv != nullptr ? iv : kDesIv, 8);

    for (std::size_t offset = 0; offset < data.size(); offset += 8)
    {
        std::uint8_t blockIn[8];
        std::memcpy(blockIn, &data[offset], 8);

        std::uint8_t blockOut[8];
        desProcessBlock(blockIn, key, false, blockOut);

        for (std::size_t i = 0; i < 8; ++i)
        {
            result.push_back(static_cast<std::uint8_t>(blockOut[i] ^ previous[i]));
        }
        std::memcpy(previous, blockIn, 8);
    }
    return result;
}

// ===========================================================================
//  六、统一的加解密流程
// ===========================================================================

/// 供 AES 与 DES 共用的参数描述。
struct AlgorithmSpec
{
    CryptoAlgorithm algorithm;
    std::size_t     keyBytes;
    std::size_t     blockBytes;
    const char*     name;
};

AlgorithmSpec specOf(CryptoAlgorithm algorithm)
{
    if (algorithm == CryptoAlgorithm::AES)
    {
        return AlgorithmSpec{ CryptoAlgorithm::AES, AES_KEY_LEN,
                              AES_BLOCK_BYTES, "AES" };
    }
    return AlgorithmSpec{ CryptoAlgorithm::DES, DES_KEY_BYTES_LOCAL,
                          DES_BLOCK_BYTES, "DES" };
}

/// 按算法分发单次加密。iv 为 nullptr 时使用该算法的固定 IV。
ByteVec encryptBlocks(const AlgorithmSpec& spec,
                      const ByteVec& data,
                      const ByteVec& key,
                      const std::uint8_t* iv)
{
    if (spec.algorithm == CryptoAlgorithm::AES)
    {
        return aesCbcEncrypt(data, key, iv);
    }
    return desCbcEncrypt(data, key, iv);
}

/// 按算法分发单次解密。iv 为 nullptr 时使用该算法的固定 IV。
ByteVec decryptBlocks(const AlgorithmSpec& spec,
                      const ByteVec& data,
                      const ByteVec& key,
                      const std::uint8_t* iv)
{
    if (spec.algorithm == CryptoAlgorithm::AES)
    {
        return aesCbcDecrypt(data, key, iv);
    }
    return desCbcDecrypt(data, key, iv);
}

/**
 * @brief 加密的统一实现，encryptAES / encryptDES / encrypt 均转调此处。
 */
CryptoResult doEncrypt(CryptoAlgorithm algorithm,
                       const std::string& plainText,
                       const std::string& key,
                       const std::uint8_t* iv = nullptr)
{
    CryptoResult result;
    const AlgorithmSpec spec = specOf(algorithm);

    // 密钥长度严格校验，不做任何截断或补零。
    if (key.size() != spec.keyBytes)
    {
        result.errorMessage = std::string(spec.name) + " key must be "
                            + std::to_string(spec.keyBytes) + " bytes";
        return result;
    }

    const double start = nowMs();

    // 直接使用调用方给出的密钥字节。
    const ByteVec rawKey(key.begin(), key.end());

    ByteVec buffer(plainText.begin(), plainText.end());
    pkcs7Pad(buffer, spec.blockBytes);
    const ByteVec cipher = encryptBlocks(spec, buffer, rawKey, iv);

    result.runningTimeMs = nowMs() - start;
    result.output        = bytesToHex(cipher);
    result.success       = true;
    return result;
}

/**
 * @brief 解密的统一实现，decryptAES / decryptDES / decrypt 均转调此处。
 */
CryptoResult doDecrypt(CryptoAlgorithm algorithm,
                       const std::string& cipherText,
                       const std::string& key,
                       const std::uint8_t* iv = nullptr)
{
    CryptoResult result;
    const AlgorithmSpec spec = specOf(algorithm);

    if (key.size() != spec.keyBytes)
    {
        result.errorMessage = std::string(spec.name) + " key must be "
                            + std::to_string(spec.keyBytes) + " bytes";
        return result;
    }

    ByteVec cipher;
    if (!hexToBytes(cipherText, cipher))
    {
        result.errorMessage = "cipher text is not a valid hex string";
        return result;
    }
    if (cipher.empty() || cipher.size() % spec.blockBytes != 0)
    {
        result.errorMessage = std::string(spec.name)
                            + " cipher text length must be a multiple of "
                            + std::to_string(spec.blockBytes) + " bytes";
        return result;
    }

    const double start = nowMs();

    const ByteVec rawKey(key.begin(), key.end());
    ByteVec buffer = decryptBlocks(spec, cipher, rawKey, iv);

    // 填充非法通常意味着密钥错误或密文被篡改。
    if (!pkcs7Unpad(buffer, spec.blockBytes))
    {
        result.errorMessage = "decryption failed: invalid padding "
                              "(wrong key or corrupted cipher text)";
        return result;
    }

    result.runningTimeMs = nowMs() - start;
    result.output.assign(buffer.begin(), buffer.end());
    result.success = true;
    return result;
}

} // namespace

// ===========================================================================
//  七、CryptoSystem 公有接口
// ===========================================================================

CryptoResult CryptoSystem::encrypt(CryptoAlgorithm algorithm,
                                   const std::string& plainText,
                                   const std::string& key) const
{
    return doEncrypt(algorithm, plainText, key);
}

CryptoResult CryptoSystem::decrypt(CryptoAlgorithm algorithm,
                                   const std::string& cipherText,
                                   const std::string& key) const
{
    return doDecrypt(algorithm, cipherText, key);
}

CryptoResult CryptoSystem::encryptAES(const std::string& plainText,
                                      const std::string& key) const
{
    return doEncrypt(CryptoAlgorithm::AES, plainText, key);
}

CryptoResult CryptoSystem::decryptAES(const std::string& cipherText,
                                      const std::string& key) const
{
    return doDecrypt(CryptoAlgorithm::AES, cipherText, key);
}

CryptoResult CryptoSystem::encryptDES(const std::string& plainText,
                                      const std::string& key) const
{
    return doEncrypt(CryptoAlgorithm::DES, plainText, key);
}

CryptoResult CryptoSystem::decryptDES(const std::string& cipherText,
                                      const std::string& key) const
{
    return doDecrypt(CryptoAlgorithm::DES, cipherText, key);
}

CryptoResult CryptoSystem::encryptAESWithIv(const std::string& plainText,
                                            const std::string& key,
                                            const std::string& iv) const
{
    if (iv.size() != AES_KEY_BYTES)
    {
        CryptoResult result;
        result.errorMessage = "AES iv must be 16 bytes";
        return result;
    }
    return doEncrypt(CryptoAlgorithm::AES, plainText, key,
                     reinterpret_cast<const std::uint8_t*>(iv.data()));
}

CryptoResult CryptoSystem::decryptAESWithIv(const std::string& cipherText,
                                            const std::string& key,
                                            const std::string& iv) const
{
    if (iv.size() != AES_KEY_BYTES)
    {
        CryptoResult result;
        result.errorMessage = "AES iv must be 16 bytes";
        return result;
    }
    return doDecrypt(CryptoAlgorithm::AES, cipherText, key,
                     reinterpret_cast<const std::uint8_t*>(iv.data()));
}

std::string CryptoSystem::derivePassphraseKey(const std::string& passphrase,
                                              std::size_t keyLen)
{
    const ByteVec digest = sha256DerivedKey(passphrase, keyLen);
    return std::string(digest.begin(), digest.end());
}
