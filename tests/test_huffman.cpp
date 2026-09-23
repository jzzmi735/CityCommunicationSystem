// tests/test_huffman.cpp
// 开发者 B：Huffman 编译码模块测试
//
// 构建方式（由开发者 C 统一维护 CMake；此处给出手工编译命令）：
//   MSVC:
//     cl /nologo /utf-8 /std:c++17 /EHsc /I src tests\test_huffman.cpp src\huffman\HuffmanSystem.cpp
//   GCC / MinGW:
//     g++ -std=c++17 -I src tests/test_huffman.cpp src/huffman/HuffmanSystem.cpp -o test_huffman
//
// 请在项目根目录运行，保证 data/ 相对路径有效。

#include "huffman/HuffmanSystem.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

int gPassed = 0;
int gFailed = 0;

void expect(bool ok, const std::string& name) {
    if (ok) {
        ++gPassed;
        std::cout << "  [PASS] " << name << "\n";
    } else {
        ++gFailed;
        std::cout << "  [FAIL] " << name << "\n";
    }
}

void section(const std::string& name) {
    std::cout << "\n---- " << name << " ----\n";
}

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string{
        std::istreambuf_iterator<char>(in),
        std::istreambuf_iterator<char>()};
}

// 统计文本中的字符频率，作为 initialize 的输入。
void charsetFromText(
    const std::string& text,
    std::vector<char>& chars,
    std::vector<int>& weights
) {
    std::map<char, int> freq;
    for (char c : text) {
        ++freq[c];
    }
    chars.clear();
    weights.clear();
    for (std::map<char, int>::const_iterator it = freq.begin();
         it != freq.end(); ++it) {
        chars.push_back(it->first);
        weights.push_back(it->second);
    }
}

bool isPrefixOf(const std::string& a, const std::string& b) {
    return a.size() <= b.size() && b.compare(0, a.size(), a) == 0;
}

// 任意两个编码互不为前缀（Huffman 编码的必要性质）。
bool tableIsPrefixFree(const std::map<char, std::string>& table) {
    for (std::map<char, std::string>::const_iterator i = table.begin();
         i != table.end(); ++i) {
        for (std::map<char, std::string>::const_iterator j = table.begin();
             j != table.end(); ++j) {
            if (i != j && isPrefixOf(i->second, j->second)) {
                return false;
            }
        }
    }
    return true;
}

bool codeOnlyContainsBits(const std::string& code) {
    for (char c : code) {
        if (c != '0' && c != '1') return false;
    }
    return true;
}

// 测试 1/2/3 的公共流程：建树 -> 编码 -> 译码 -> 与原串一致。
void testRoundTrip(const std::string& text) {
    std::vector<char> chars;
    std::vector<int> weights;
    charsetFromText(text, chars, weights);

    HuffmanSystem hs;
    expect(hs.initialize(chars, weights), "初始化: \"" + text + "\"");

    std::string code = hs.encode(text);
    expect(!code.empty(), "encode 非空: \"" + text + "\"");
    expect(codeOnlyContainsBits(code), "编码只含 0/1: \"" + text + "\"");
    expect(hs.decode(code) == text, "decode(encode(text)) == text: \"" + text + "\"");
    expect(tableIsPrefixFree(hs.getCodeTable()), "编码表无前缀冲突: \"" + text + "\"");
}

// 测试 4：单字符字符集。
void testSingleCharacter() {
    HuffmanSystem hs;
    std::vector<char> chars{'A'};
    std::vector<int> weights{100};

    expect(hs.initialize(chars, weights), "单字符字符集初始化");
    expect(hs.getCodeTable().at('A') == "0", "单字符编码为 \"0\"");
    expect(hs.encode("AAAA") == "0000", "encode(\"AAAA\") == \"0000\"");
    expect(hs.decode("0000") == "AAAA", "decode(\"0000\") == \"AAAA\"");

    std::string bad = hs.decode("1");
    expect(bad.empty() && !hs.lastError().empty(), "单字符树拒绝编码 \"1\"");
}

// 测试 5：文本包含未定义字符。
void testUndefinedCharacter() {
    HuffmanSystem hs;
    std::vector<char> chars{'A', 'B'};
    std::vector<int> weights{1, 2};
    expect(hs.initialize(chars, weights), "初始化字符集 {A, B}");

    std::string code = hs.encode("ABC");
    expect(code.empty() && !hs.lastError().empty(),
           "encode 含未定义字符 'C' 返回空串并记录错误");
}

// 测试 6：错误编码。
void testInvalidCode() {
    HuffmanSystem hs;
    std::vector<char> chars;
    std::vector<int> weights;
    charsetFromText("I AM FROM CHINA", chars, weights);
    expect(hs.initialize(chars, weights), "初始化 I AM FROM CHINA");

    std::string bad1 = hs.decode("010201");
    expect(bad1.empty() && !hs.lastError().empty(),
           "decode(\"010201\") 含非法字符返回失败");

    std::string bad2 = hs.decode("0");  // 多字符树中 "0" 必然停在内部节点
    expect(bad2.empty() && !hs.lastError().empty(),
           "decode(\"0\") 停在非叶子节点返回失败");

    std::string empty = hs.decode("");
    expect(empty.empty() && hs.lastError().empty(), "decode(\"\") 成功且无错误");
}

// 测试 7：文件完整流程 TobeTran -> CodeFile -> TextFile，以及树的保存/读取。
void testFileWorkflow() {
    namespace fs = std::filesystem;
    try {
        fs::create_directories("data");
    } catch (...) {
        // data/ 目录已随仓库提供，创建失败则继续。
    }

    const std::string tobePath = "data/TobeTran";
    if (!fs::exists(tobePath)) {
        std::ofstream out(tobePath, std::ios::binary);
        out << "I AM FROM CHINA";
    }
    const std::string original = readFile(tobePath);

    std::vector<char> chars;
    std::vector<int> weights;
    charsetFromText(original, chars, weights);
    HuffmanSystem hs;
    expect(hs.initialize(chars, weights), "按 TobeTran 内容初始化");

    expect(hs.encodeFile(tobePath, "data/CodeFile"), "encodeFile: TobeTran -> CodeFile");
    expect(readFile("data/CodeFile") == hs.encode(original),
           "CodeFile 内容与 encode() 结果一致");
    expect(hs.decodeFile("data/CodeFile", "data/TextFile"),
           "decodeFile: CodeFile -> TextFile");
    expect(readFile("data/TextFile") == original, "TextFile 内容与 TobeTran 一致");

    expect(hs.saveTree("data/hfmTree"), "saveTree -> data/hfmTree");
    expect(hs.writeTreePrint("data/TreePrint"), "writeTreePrint -> data/TreePrint");

    const std::string treePrint = hs.getTreePrint();
    expect(!treePrint.empty(), "getTreePrint 非空");
    expect(treePrint.find("[SPACE]") != std::string::npos, "TreePrint 包含 [SPACE]");
    expect(readFile("data/TreePrint") == treePrint, "TreePrint 文件与 getTreePrint() 一致");

    // 重新加载 hfmTree 后应得到等价的系统。
    HuffmanSystem hs2;
    expect(hs2.loadTree("data/hfmTree"), "loadTree <- data/hfmTree");
    expect(hs2.getCodeTable() == hs.getCodeTable(), "加载后的编码表与原编码表一致");
    expect(hs2.getTreePrint() == treePrint, "加载后的树形打印与原树一致");
    expect(hs2.decode(readFile("data/CodeFile")) == original,
           "加载树后译码 CodeFile 恢复原文");
}

// 补充：initialize 参数校验。
void testInitializeValidation() {
    HuffmanSystem hs;
    expect(!hs.initialize({}, {}), "空字符集初始化失败");
    expect(!hs.initialize({'A', 'B'}, {1}), "字符/权值数量不一致失败");
    expect(!hs.initialize({'A'}, {0}), "权值为 0 失败");
    expect(!hs.initialize({'A'}, {-1}), "负权值失败");
    expect(!hs.initialize({'A', 'A'}, {1, 2}), "重复字符失败");
    expect(!hs.isInitialized(), "全部失败后系统未初始化");
    expect(!hs.lastError().empty(), "失败时记录了错误信息");
}

// 补充：反复初始化（GUI 点击“重新建树”）。
void testReinitialize() {
    HuffmanSystem hs;
    expect(hs.initialize({'A', 'B'}, {1, 2}), "第一次初始化 {A, B}");
    expect(!hs.encode("AB").empty(), "旧字符集可编码");

    expect(hs.initialize({'X', 'Y'}, {3, 4}), "重新建树 {X, Y}（旧树自动清理）");
    expect(!hs.encode("XY").empty(), "新字符集可编码");
    expect(hs.getCodeTable().count('A') == 0 && hs.getCodeTable().count('B') == 0,
           "旧字符已从编码表移除");
    std::string old = hs.encode("AB");
    expect(old.empty() && !hs.lastError().empty(), "旧字符编码失败并记录错误");
}

// 补充：未初始化时的行为。
void testUninitialized() {
    HuffmanSystem hs;
    expect(!hs.isInitialized(), "构造后未初始化");
    expect(hs.encode("A").empty() && !hs.lastError().empty(), "未初始化 encode 失败");
    expect(hs.decode("0").empty() && !hs.lastError().empty(), "未初始化 decode 失败");
    expect(hs.getCodeTable().empty(), "未初始化编码表为空");
    expect(hs.getTreePrint().empty(), "未初始化树形打印为空");
    expect(!hs.saveTree("data/hfmTreeNone"), "未初始化 saveTree 失败");
    expect(!hs.encodeFile("data/TobeTran", "data/CodeFileNone"),
           "未初始化 encodeFile 失败");
}

// 实验数据输出（供报告第 22/23 节使用）。
void printExperiment() {
    const std::string text = "I AM FROM CHINA";
    std::vector<char> chars;
    std::vector<int> weights;
    charsetFromText(text, chars, weights);

    HuffmanSystem hs;
    if (!hs.initialize(chars, weights)) {
        std::cout << "实验初始化失败: " << hs.lastError() << "\n";
        return;
    }

    std::string code = hs.encode(text);
    long long huffBits = static_cast<long long>(code.size());
    long long fixedBits = static_cast<long long>(text.size()) * 8;
    double avgLen = static_cast<double>(huffBits) / text.size();
    double ratio = (1.0 - static_cast<double>(huffBits) / fixedBits) * 100.0;

    std::cout << "\n==================== I AM FROM CHINA 实验数据 ====================\n";
    std::cout << "字符频率:\n";
    for (char c : text) {
        std::cout << "  " << (c == ' ' ? "[SPACE]" : std::string(1, c));
    }
    std::cout << "\n\n编码表:\n";
    const std::map<char, std::string>& table = hs.getCodeTable();
    for (std::map<char, std::string>::const_iterator it = table.begin();
         it != table.end(); ++it) {
        std::cout << "  "
                  << (it->first == ' ' ? "[SPACE]" : std::string(1, it->first))
                  << " (w=" << weights[std::distance(table.begin(), it)] << "): "
                  << it->second << "\n";
    }

    std::cout << "\n编码结果 (" << code.size() << " bit):\n  " << code << "\n";
    std::cout << "译码结果:\n  " << hs.decode(code) << "\n";

    std::cout << "\nHuffman 树形打印:\n" << hs.getTreePrint();

    std::cout << "压缩统计:\n"
              << "  字符数 L      = " << text.size() << "\n"
              << "  等长 8bit 码  = " << fixedBits << " bit\n"
              << "  Huffman 码   = " << huffBits << " bit\n"
              << "  平均码长     = " << std::fixed << std::setprecision(2)
              << avgLen << " bit/字符\n"
              << "  压缩率       = " << std::setprecision(2) << ratio << "%\n";
    std::cout << "==================================================================\n";
}

} // namespace

int main() {
    std::cout << "==================== Huffman 模块测试（开发者 B）====================\n";

    section("测试 1: I AM FROM CHINA");
    testRoundTrip("I AM FROM CHINA");

    section("测试 2: HELLO WORLD");
    testRoundTrip("HELLO WORLD");

    section("测试 3: AAAAABBBBB（重复字符）");
    testRoundTrip("AAAAABBBBB");

    section("测试 4: 单字符字符集");
    testSingleCharacter();

    section("测试 5: 文本包含未定义字符");
    testUndefinedCharacter();

    section("测试 6: 错误编码");
    testInvalidCode();

    section("测试 7: 文件完整流程与树保存/读取");
    testFileWorkflow();

    section("补充: initialize 参数校验");
    testInitializeValidation();

    section("补充: 反复初始化（重新建树）");
    testReinitialize();

    section("补充: 未初始化行为");
    testUninitialized();

    printExperiment();

    std::cout << "\n==================== 结果 ====================\n"
              << gPassed << " 项通过, " << gFailed << " 项失败\n";
    return gFailed == 0 ? 0 : 1;
}
