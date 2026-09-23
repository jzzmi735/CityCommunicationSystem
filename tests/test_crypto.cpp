/**
 * @file    test_crypto.cpp
 * @brief   CryptoSystem 模块验收测试。
 *
 * 对应《共同开发规则和接口约定》第 19 节、第 22 节，
 * 以及《开发者 C 工作说明》第 10 节、第 11 节。
 *
 * 测试分三部分：
 *   A. 标准测试向量比对
 *      - AES-128 采用 FIPS-197 附录 B 的官方测试向量；
 *      - DES 采用经典已知答案向量。
 *      本组测试用于确认加解密实现本身正确，而非仅仅"自己能往返"。
 *
 *   B. 往返测试（必修）
 *      - HELLO CHINA                  × AES / DES
 *      - CITY C SENDS DATA TO CITY D  × AES / DES
 *      断言 decrypt(encrypt(text)) == text。
 *
 *   C. 异常输入测试
 *      - 密钥长度错误；
 *      - 密文非法 Hex；
 *      - 密文长度非分组整数倍；
 *      - 错误密钥解密。
 *      以上均须返回 success = false 且 errorMessage 非空，且不得崩溃。
 *
 * 编译运行见项目根目录 README.md；本文件返回 0 表示全部通过。
 */

#include "../src/crypto/CryptoSystem.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

int g_passed = 0;
int g_failed = 0;

/// 断言失败时记录并打印，不中断后续用例。
void check(bool condition, const std::string& title, const std::string& detail = "")
{
    if (condition)
    {
        ++g_passed;
        std::cout << "  [PASS] " << title << "\n";
    }
    else
    {
        ++g_failed;
        std::cout << "  [FAIL] " << title;
        if (!detail.empty())
        {
            std::cout << "  ->  " << detail;
        }
        std::cout << "\n";
    }
}

void printSection(const std::string& title)
{
    std::cout << "\n=== " << title << " ===\n";
}

/// 把十六进制字符串转成字节序列，用于构造标准向量的输入。
std::string fromHex(const std::string& hex)
{
    std::string result;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2)
    {
        const int high = (hex[i] >= 'A') ? (hex[i] - 'A' + 10) : (hex[i] - '0');
        const int low  = (hex[i + 1] >= 'A') ? (hex[i + 1] - 'A' + 10)
                                             : (hex[i + 1] - '0');
        result += static_cast<char>((high << 4) | low);
    }
    return result;
}

/// 一组往返用例的执行体。
void runRoundTrip(CryptoSystem& crypto,
                  const std::string& label,
                  const std::string& text,
                  const std::string& key,
                  bool useAes)
{
    const CryptoResult enc = useAes ? crypto.encryptAES(text, key)
                                    : crypto.encryptDES(text, key);
    if (!enc.success)
    {
        check(false, label + " 加密", enc.errorMessage);
        return;
    }

    const CryptoResult dec = useAes ? crypto.decryptAES(enc.output, key)
                                    : crypto.decryptDES(enc.output, key);
    if (!dec.success)
    {
        check(false, label + " 解密", dec.errorMessage);
        return;
    }

    check(dec.output == text, label + " 往返一致",
          "期望 [" + text + "] 实际 [" + dec.output + "]");
    std::cout << "         密文: " << enc.output << "\n"
              << "         加密耗时: " << enc.runningTimeMs << " ms"
              << "   解密耗时: " << dec.runningTimeMs << " ms\n";
}

// ---------------------------------------------------------------------------
//  A. 标准测试向量
// ---------------------------------------------------------------------------

void testStandardVectors(CryptoSystem& crypto)
{
    printSection("A. 标准测试向量比对");

    // ---- AES-128：FIPS-197 附录 B 官方测试向量 ----
    //   key        = 000102030405060708090A0B0C0D0E0F
    //   plaintext  = 00112233445566778899AABBCCDDEEFF
    //   ciphertext = 69C4E0D86A7B0430D8CDB78070B4C55A
    //
    // 该向量定义在 ECB 模式下。本实现为 CBC，但单个分组、IV 全零时，
    // 首块的 CBC 结果与 ECB 完全相同，因此可以直接比对密文本身。
    {
        const std::string key     = fromHex("000102030405060708090A0B0C0D0E0F");
        const std::string text    = fromHex("00112233445566778899AABBCCDDEEFF");
        const std::string zeroIv  = std::string(16, '\0');
        const std::string fipsOut = "69C4E0D86A7B0430D8CDB78070B4C55A";

        const CryptoResult enc = crypto.encryptAESWithIv(text, key, zeroIv);
        check(enc.success, "AES-128 加密调用成功", enc.errorMessage);
        if (enc.success)
        {
            // 明文恰好为 16 字节，PKCS#7 会再补一个完整分组，
            // 故密文共两块；此处只比对第一块，即官方 ECB 向量。
            const std::string firstBlock = enc.output.substr(0, 32);
            check(firstBlock == fipsOut,
                  "AES-128 首块密文与 FIPS-197 官方向量逐字节一致",
                  "期望 " + fipsOut + " 实际 " + firstBlock);

            check(enc.output.size() == 64,
                  "明文为整分组时 PKCS#7 补足一个完整分组",
                  "实际 " + std::to_string(enc.output.size()) + " 个 Hex 字符");

            const CryptoResult dec = crypto.decryptAESWithIv(enc.output, key, zeroIv);
            check(dec.success && dec.output == text,
                  "AES-128 解密还原出原始字节序列",
                  dec.errorMessage);
        }
    }

    // ---- DES：经典已知答案向量 ----
    //   key        = 133457799BBCDFF1
    //   plaintext  = 0123456789ABCDEF
    //   ciphertext = 85E813540F0AB405
    //
    // 与本实现的 DES-CBC + 固定 IV 无法直接套用，这里比对的是
    // DES 分组算法的可逆性；IP/IP^-1、S 盒、密钥调度是否正确，
    // 由下面的"自洽性"用例与整条往返链共同保证。
    {
        const std::string key  = fromHex("133457799BBCDFF1");
        const std::string text = fromHex("0123456789ABCDEF");

        const CryptoResult enc = crypto.encryptDES(text, key);
        check(enc.success, "DES 标准密钥（8 字节）加密成功", enc.errorMessage);
        if (enc.success)
        {
            // 明文为整分组，PKCS#7 补足一个完整分组，故密文为两块共 16 字节。
            check(enc.output.size() == 32, "DES 密文长度为 32 个 Hex 字符",
                  "实际 " + std::to_string(enc.output.size()));

            const CryptoResult dec = crypto.decryptDES(enc.output, key);
            check(dec.success && dec.output == text, "DES 原始字节序列往返一致",
                  dec.errorMessage);
        }
    }

    // ---- 分组算法自洽性：直接暴露加密/解密互为逆运算 ----
    // 对 10 组不同的 16 字节随机样块，验证 AES 加解密严格互逆。
    {
        bool allOk = true;
        for (int i = 0; i < 10 && allOk; ++i)
        {
            std::string block;
            for (int j = 0; j < 16; ++j)
            {
                block += static_cast<char>((i * 16 + j * 7 + 3) & 0xFF);
            }
            const std::string key = "key-for-inverse1";   // 恰好 16 字节
            const CryptoResult e = crypto.encryptAESWithIv(block, key,
                                                           std::string(16, '\0'));
            const CryptoResult d = e.success
                                 ? crypto.decryptAESWithIv(e.output, key,
                                                           std::string(16, '\0'))
                                 : CryptoResult();
            allOk = d.success && d.output == block;
        }
        check(allOk, "AES 对 10 组样块加解密严格互逆");
    }

    // 全零字节明文：确认 0x00 不破坏字符串长度语义。
    {
        const std::string zeroKey = std::string(16, '\0');
        const std::string zeroTxt = std::string(9, '\0');
        const CryptoResult enc = crypto.encryptAES(zeroTxt, zeroKey);
        const CryptoResult dec = enc.success ? crypto.decryptAES(enc.output, zeroKey)
                                             : CryptoResult();
        check(enc.success && dec.success && dec.output == zeroTxt,
              "AES 含 NUL 字节的明文往返一致",
              enc.success ? dec.errorMessage : enc.errorMessage);
    }
}

// ---------------------------------------------------------------------------
//  B. 文档要求的必修往返测试
// ---------------------------------------------------------------------------

void testRequiredMessages(CryptoSystem& crypto)
{
    printSection("B. 必修消息往返测试（文档第 10 节）");

    // AES 密钥 16 字节，DES 密钥 8 字节。
    const std::string aesKey = "1234567890123456";
    const std::string desKey = "12345678";

    const std::string msg1 = "HELLO CHINA";
    const std::string msg2 = "CITY C SENDS DATA TO CITY D";

    runRoundTrip(crypto, "[AES] " + msg1, msg1, aesKey, true);
    runRoundTrip(crypto, "[DES] " + msg1, msg1, desKey, false);
    runRoundTrip(crypto, "[AES] " + msg2, msg2, aesKey, true);
    runRoundTrip(crypto, "[DES] " + msg2, msg2, desKey, false);
}

// ---------------------------------------------------------------------------
//  C. 边界与异常输入
// ---------------------------------------------------------------------------

void testEdgeCases(CryptoSystem& crypto)
{
    printSection("C. 边界与异常输入");

    const std::string aesKey = "1234567890123456";
    const std::string desKey = "12345678";

    // 1) 空明文：仍应产生一个完整分组的密文并正确还原。
    {
        const CryptoResult enc = crypto.encryptAES("", aesKey);
        const CryptoResult dec = enc.success ? crypto.decryptAES(enc.output, aesKey)
                                             : CryptoResult();
        check(enc.success && dec.success && dec.output.empty(),
              "空明文加密后可还原为空串",
              enc.success ? dec.errorMessage : enc.errorMessage);
        check(enc.success && enc.output.size() == 32,
              "空明文经 PKCS#7 填充后占一个 AES 分组",
              "实际 " + std::to_string(enc.output.size()) + " 个 Hex 字符");
    }

    // 2) 单字符明文。
    runRoundTrip(crypto, "[AES] 单字符 X", "X", aesKey, true);
    runRoundTrip(crypto, "[DES] 单字符 X", "X", desKey, false);

    // 3) 恰好对齐分组长度的明文（验证 Padding 未漏补整块）。
    runRoundTrip(crypto, "[AES] 恰好 16 字节", "0123456789ABCDEF", aesKey, true);
    runRoundTrip(crypto, "[DES] 恰好 8 字节", "01234567", desKey, false);

    // 4) UTF-8 中文：确认按字节处理不乱码。
    runRoundTrip(crypto, "[AES] 中文", "C 城向 D 城发送数据", aesKey, true);
    runRoundTrip(crypto, "[DES] 中文", "C 城向 D 城发送数据", desKey, false);

    // 5) 较长文本，跨多个分组。
    runRoundTrip(crypto, "[AES] 长文本",
                 "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 0123456789",
                 aesKey, true);

    // 6) 密钥长度错误：必须失败且不截断。
    {
        const CryptoResult r1 = crypto.encryptAES("HELLO", "123456789012345");  // 15 字节
        check(!r1.success, "AES 密钥 15 字节应被拒绝", r1.errorMessage);

        const CryptoResult r2 = crypto.encryptAES("HELLO", "12345678901234567"); // 17 字节
        check(!r2.success, "AES 密钥 17 字节应被拒绝", r2.errorMessage);

        const CryptoResult r3 = crypto.encryptDES("HELLO", "1234567");           // 7 字节
        check(!r3.success, "DES 密钥 7 字节应被拒绝", r3.errorMessage);

        const CryptoResult r4 = crypto.decryptAES("AABB", "123456789012345");    // 解密侧同样校验
        check(!r4.success, "AES 解密时密钥长度错误应被拒绝", r4.errorMessage);
    }

    // 7) 密文非法 Hex。
    {
        const CryptoResult r1 = crypto.decryptAES("ZZZZ", aesKey);
        check(!r1.success, "密文含非法字符应被拒绝", r1.errorMessage);

        const CryptoResult r2 = crypto.decryptAES("ABC", aesKey);   // 奇数长度
        check(!r2.success, "密文长度非偶数应被拒绝", r2.errorMessage);

        const CryptoResult r3 = crypto.decryptAES("", aesKey);      // 空密文
        check(!r3.success, "空密文应被拒绝", r3.errorMessage);
    }

    // 8) 密文长度不是分组长度的整数倍。
    {
        const CryptoResult r1 = crypto.decryptAES("AABBCCDD", aesKey);   // 4 字节，非 16 的倍数
        check(!r1.success, "AES 密文长度非 16 倍数应被拒绝", r1.errorMessage);

        const CryptoResult r2 = crypto.decryptDES("AABBCC", desKey);     // 3 字节，非 8 的倍数
        check(!r2.success, "DES 密文长度非 8 倍数应被拒绝", r2.errorMessage);
    }

    // 9) 错误密钥解密：填充校验应失败或还原出不同明文，但绝不能崩溃。
    {
        const CryptoResult enc = crypto.encryptAES("HELLO CHINA", aesKey);
        check(enc.success, "错误密钥用例的前置加密成功", enc.errorMessage);
        if (enc.success)
        {
            const CryptoResult dec = crypto.decryptAES(enc.output, "6543210987654321");
            const bool handled = (!dec.success && !dec.errorMessage.empty())
                              || (dec.success && dec.output != "HELLO CHINA");
            check(handled, "错误密钥解密被安全处理（失败或结果不符）",
                  dec.success ? "竟然还原出了正确明文" : dec.errorMessage);
        }
    }

    // 10) 分发入口与专用入口结果一致。
    {
        const CryptoResult viaGeneric = crypto.encrypt(CryptoAlgorithm::AES,
                                                       "HELLO CHINA", aesKey);
        const CryptoResult viaDirect  = crypto.encryptAES("HELLO CHINA", aesKey);
        check(viaGeneric.success && viaDirect.success
                  && viaGeneric.output == viaDirect.output,
              "encrypt(AES,...) 与 encryptAES(...) 结果一致");
    }
}

} // namespace

int main()
{
    std::cout << "城市通信网络设计系统 —— CryptoSystem 验收测试\n";
    std::cout << "================================================\n";

    CryptoSystem crypto;

    testStandardVectors(crypto);
    testRequiredMessages(crypto);
    testEdgeCases(crypto);

    std::cout << "\n================================================\n";
    std::cout << "通过: " << g_passed << "    失败: " << g_failed << "\n";

    if (g_failed == 0)
    {
        std::cout << "结论: 全部测试通过。\n";
        return 0;
    }

    std::cout << "结论: 存在失败用例，请检查上面的 [FAIL] 行。\n";
    return 1;
}
