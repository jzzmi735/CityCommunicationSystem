#pragma once

#include <iosfwd>
#include <map>
#include <string>
#include <vector>

// 城市通信网络设计系统 —— Huffman 编译码模块（开发者 B）
//
// 本模块不依赖 Qt，GUI（开发者 C）只通过本公共接口调用：
//   initialize()  getCodeTable()  encode()  decode()
//   getTreePrint()  encodeFile()  decodeFile()
//   saveTree()  loadTree()  writeTreePrint()
//
// 错误通过返回值（false / 空串）+ lastError() 上报，由 GUI 决定如何显示。
class HuffmanSystem {
public:
    HuffmanSystem();
    ~HuffmanSystem();

    // HuffmanSystem 内部持有树节点指针，禁止拷贝（浅拷贝会导致双重释放）。
    HuffmanSystem(const HuffmanSystem&) = delete;
    HuffmanSystem& operator=(const HuffmanSystem&) = delete;

    // 清空当前 Huffman 树与编码表。
    void clear();

    // 用字符集与对应权值初始化；可反复调用，旧树自动清理。
    // 要求：chars/weights 非空、长度一致、权值 > 0、字符不重复，否则返回 false。
    bool initialize(
        const std::vector<char>& chars,
        const std::vector<int>& weights
    );

    bool isInitialized() const;

    // 编码：文本含字符集之外的字符时返回空串并记录错误，不偷偷跳过。
    std::string encode(const std::string& text) const;

    // 译码：仅接受 '0'/'1'；含非法字符、停在非叶子节点或未初始化时返回空串并记录错误。
    std::string decode(const std::string& code) const;

    bool encodeFile(
        const std::string& inputPath,
        const std::string& outputPath
    ) const;

    bool decodeFile(
        const std::string& inputPath,
        const std::string& outputPath
    ) const;

    // 树的保存 / 读取（先序序列化，叶子存字符 ASCII 整数值）。
    bool saveTree(const std::string& path) const;

    bool loadTree(const std::string& path);

    std::map<char, std::string> getCodeTable() const;

    std::string getTreePrint() const;

    bool writeTreePrint(const std::string& path) const;

    std::string lastError() const;

private:
    struct Node;   // 内部节点类型，不向 GUI 暴露
    Node* root_;

    std::map<char, std::string> codeTable_;
    mutable std::string lastError_;

    // ---- 以下为模块内部辅助函数，不属于公共接口 ----
    static void deleteTree(Node* node);

    // 递归生成编码表；统一规则：左 = '0'，右 = '1'。
    static void generateCodes(
        const Node* node,
        const std::string& prefix,
        std::map<char, std::string>& table
    );

    // 中序遍历生成树的直观文本形式（左子树在上、右子树在下）。
    static void buildTreePrint(const Node* node, int depth, std::string& out);

    // 先序序列化：内部节点 "I <权值>"，叶子 "L <字符ASCII> <权值>"。
    static void saveTreeRec(std::ostream& out, const Node* node);

    // 先序反序列化；失败时 error 记录原因。
    static bool loadTreeRec(std::istream& in, Node*& node, std::string& error);

    void rebuildCodeTable();
    void setError(const std::string& message) const;
};
