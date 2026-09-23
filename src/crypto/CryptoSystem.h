/**
 * @file    CryptoSystem.h
 * @brief   城市通信网络设计系统 —— 安全通信模块（AES-128-CBC / DES-CBC）
 *
 * 对应题目第（3）问：AES/DES 加密与解密。
 * 对应《共同开发规则和接口约定》第 10 ~ 12 节。
 *
 * 设计约束：
 *  1. 本模块不依赖 Qt，禁止 include 任何 Qt 头文件；
 *  2. 错误一律通过 CryptoResult::success / errorMessage 返回，不抛异常、不弹窗；
 *  3. 密文统一以 Hex 字符串形式对外，界面层不接触原始二进制；
 *  4. 密钥长度严格校验，AES 必须 16 字节、DES 必须 8 字节，长度不符直接失败，
 *     绝不自动截断或补零；
 *  5. 计时统一使用 std::chrono，单位为毫秒。
 */

#pragma once

#include <string>

#include "../common/Types.h"

/**
 * @brief 支持的对称加密算法。
 */
enum class CryptoAlgorithm
{
    AES,    ///< AES-128-CBC
    DES     ///< DES-CBC
};

/**
 * @brief 对称加解密服务。
 *
 * @note 本类全部接口均为 const，内部不持有可变状态，可安全地重复使用同一实例。
 */
class CryptoSystem
{
public:
    /**
     * @brief 按指定算法加密。
     * @param algorithm 算法类型，AES 或 DES。
     * @param plainText 明文（UTF-8 字节序列，可含中文）。
     * @param key       密钥原文（ASCII 口令），AES 须 16 字节、DES 须 8 字节。
     * @return CryptoResult，成功时 output 为密文的 Hex 字符串。
     */
    CryptoResult encrypt(CryptoAlgorithm algorithm,
                         const std::string& plainText,
                         const std::string& key) const;

    /**
     * @brief 按指定算法解密。
     * @param algorithm  算法类型，AES 或 DES。
     * @param cipherText 密文的 Hex 字符串。
     * @param key        密钥原文，须与加密时一致。
     * @return CryptoResult，成功时 output 为还原出的明文。
     */
    CryptoResult decrypt(CryptoAlgorithm algorithm,
                         const std::string& cipherText,
                         const std::string& key) const;

    /**
     * @brief AES-128-CBC 加密。
     * @param plainText 明文。
     * @param key       密钥，必须为 16 字节。
     * @return CryptoResult，成功时 output 为 Hex 密文。
     */
    CryptoResult encryptAES(const std::string& plainText,
                            const std::string& key) const;

    /**
     * @brief AES-128-CBC 解密。
     * @param cipherText Hex 密文。
     * @param key        密钥，必须为 16 字节。
     * @return CryptoResult，成功时 output 为明文。
     */
    CryptoResult decryptAES(const std::string& cipherText,
                            const std::string& key) const;

    /**
     * @brief DES-CBC 加密。
     * @param plainText 明文。
     * @param key       密钥，必须为 8 字节。
     * @return CryptoResult，成功时 output 为 Hex 密文。
     */
    CryptoResult encryptDES(const std::string& plainText,
                            const std::string& key) const;

    /**
     * @brief DES-CBC 解密。
     * @param cipherText Hex 密文。
     * @param key        密钥，必须为 8 字节。
     * @return CryptoResult，成功时 output 为明文。
     */
    CryptoResult decryptDES(const std::string& cipherText,
                            const std::string& key) const;

    /**
     * @brief AES-128-CBC 加密，并显式指定初始向量。
     *
     * 主要用于两类场景：
     *  1. 测试时传入全零 IV，使 CBC 退化为 ECB，从而可直接比对
     *     FIPS-197 等标准测试向量；
     *  2. 将来需要换用随机 IV 时，由界面层生成并随密文一并保存。
     *
     * @param plainText 明文。
     * @param key       密钥，必须为 16 字节。
     * @param iv        初始向量，必须为 16 字节。
     */
    CryptoResult encryptAESWithIv(const std::string& plainText,
                                  const std::string& key,
                                  const std::string& iv) const;

    /**
     * @brief AES-128-CBC 解密，并显式指定初始向量。
     * @param cipherText Hex 密文。
     * @param key        密钥，必须为 16 字节。
     * @param iv         初始向量，必须与加密时一致，且为 16 字节。
     */
    CryptoResult decryptAESWithIv(const std::string& cipherText,
                                  const std::string& key,
                                  const std::string& iv) const;

    /**
     * @brief 由任意长度口令派生定长密钥。
     *
     * 内部先对 passphrase 做 SHA-256，再截取前 keyLen 字节。
     * 供界面层在用户随意输入口令、又不希望直接报错的场景下使用：
     * 界面可自行决定是拒绝，还是派生成合规长度后再交给本类。
     *
     * @param passphrase 用户口令，长度不限。
     * @param keyLen     目标密钥长度，取 AES_KEY_BYTES 或 DES_KEY_BYTES。
     * @return 长度为 keyLen 的派生密钥。
     */
    static std::string derivePassphraseKey(const std::string& passphrase,
                                           std::size_t keyLen);

    /// AES-128 要求的密钥字节数。
    static constexpr std::size_t AES_KEY_BYTES = 16;
    /// DES 要求的密钥字节数。
    static constexpr std::size_t DES_KEY_BYTES = 8;
};
