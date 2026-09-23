#include "HuffmanSystem.h"

#include <fstream>
#include <iomanip>
#include <ostream>
#include <queue>
#include <sstream>

namespace {

// 将字符转成便于错误提示的可读形式。
std::string describeChar(char ch) {
    unsigned char uc = static_cast<unsigned char>(ch);
    if (uc >= 32 && uc <= 126) {
        return std::string("'") + ch + "'";
    }
    std::ostringstream oss;
    oss << "ASCII " << static_cast<int>(uc);
    return oss.str();
}

// 叶子的显示名：空格等控制字符用可读标签代替。
std::string leafLabel(char ch) {
    switch (ch) {
        case ' ':  return "[SPACE]";
        case '\t': return "[TAB]";
        case '\n': return "[LF]";
        case '\r': return "[CR]";
        default: break;
    }
    unsigned char uc = static_cast<unsigned char>(ch);
    if (uc < 32 || uc == 127) {
        std::ostringstream oss;
        oss << "[0x" << std::hex << std::uppercase << std::setw(2)
            << std::setfill('0') << static_cast<int>(uc) << "]";
        return oss.str();
    }
    return std::string(1, ch);
}

} // namespace

// ---- 内部节点定义（仅模块内部使用） ----
struct HuffmanSystem::Node {
    char ch;             // 叶子：字符本身；内部节点：子树中的最小字符（仅用于建树排序稳定）
    long long weight;
    Node* left;
    Node* right;

    Node(char c, long long w)
        : ch(c), weight(w), left(nullptr), right(nullptr) {}
};

HuffmanSystem::HuffmanSystem() : root_(nullptr) {}

HuffmanSystem::~HuffmanSystem() {
    deleteTree(root_);
}

void HuffmanSystem::clear() {
    deleteTree(root_);
    root_ = nullptr;
    codeTable_.clear();
    lastError_.clear();
}

void HuffmanSystem::setError(const std::string& message) const {
    lastError_ = message;
}

void HuffmanSystem::deleteTree(Node* node) {
    if (node == nullptr) return;
    deleteTree(node->left);
    deleteTree(node->right);
    delete node;
}

bool HuffmanSystem::initialize(
    const std::vector<char>& chars,
    const std::vector<int>& weights
) {
    clear();  // 反复调用（重新建树）时先释放旧树

    if (chars.size() != weights.size()) {
        setError("initialize failed: chars and weights sizes do not match");
        return false;
    }
    if (chars.empty()) {
        setError("initialize failed: chars and weights must not be empty");
        return false;
    }

    bool seen[256] = {};
    for (std::size_t i = 0; i < chars.size(); ++i) {
        if (weights[i] <= 0) {
            setError("initialize failed: all weights must be positive");
            return false;
        }
        unsigned char uc = static_cast<unsigned char>(chars[i]);
        if (seen[uc]) {
            setError("initialize failed: duplicate character "
                     + describeChar(chars[i]));
            return false;
        }
        seen[uc] = true;
    }

    // 建树比较器：先比权值，再比子树最小字符，保证建树结果确定。
    struct NodeCmp {
        bool operator()(const Node* a, const Node* b) const {
            if (a->weight != b->weight) {
                return a->weight > b->weight;
            }
            return static_cast<unsigned char>(a->ch)
                 > static_cast<unsigned char>(b->ch);
        }
    };

    // 最小堆（priority_queue 默认取“最大”，配合 NodeCmp 取最小权值）。
    std::priority_queue<Node*, std::vector<Node*>, NodeCmp> queue;
    for (std::size_t i = 0; i < chars.size(); ++i) {
        queue.push(new Node(chars[i], weights[i]));
    }

    // 每次取两个最小权值节点合并成父节点，直到只剩一个根。
    while (queue.size() > 1) {
        Node* first = queue.top();
        queue.pop();
        Node* second = queue.top();
        queue.pop();

        char minCh = (static_cast<unsigned char>(first->ch)
                      <= static_cast<unsigned char>(second->ch))
                         ? first->ch
                         : second->ch;
        Node* parent = new Node(minCh, first->weight + second->weight);
        parent->left = first;
        parent->right = second;
        queue.push(parent);
    }

    root_ = queue.top();
    rebuildCodeTable();
    return true;
}

bool HuffmanSystem::isInitialized() const {
    return root_ != nullptr;
}

void HuffmanSystem::rebuildCodeTable() {
    codeTable_.clear();
    if (root_ == nullptr) return;
    if (root_->left == nullptr && root_->right == nullptr) {
        // 单字符字符集：不能生成空编码，统一编码为 "0"。
        codeTable_[root_->ch] = "0";
        return;
    }
    generateCodes(root_, "", codeTable_);
}

void HuffmanSystem::generateCodes(
    const Node* node,
    const std::string& prefix,
    std::map<char, std::string>& table
) {
    if (node == nullptr) return;
    if (node->left == nullptr && node->right == nullptr) {
        table[node->ch] = prefix;
        return;
    }
    // 统一编码规则：左边 = 0，右边 = 1。
    generateCodes(node->left, prefix + "0", table);
    generateCodes(node->right, prefix + "1", table);
}

std::string HuffmanSystem::encode(const std::string& text) const {
    setError("");
    if (!isInitialized()) {
        setError("encode failed: Huffman tree is not initialized");
        return "";
    }

    std::string code;
    for (char c : text) {
        std::map<char, std::string>::const_iterator it = codeTable_.find(c);
        if (it == codeTable_.end()) {
            setError("encode failed: character " + describeChar(c)
                     + " is not in the Huffman code table");
            return "";
        }
        code += it->second;
    }
    return code;
}

std::string HuffmanSystem::decode(const std::string& code) const {
    setError("");
    if (!isInitialized()) {
        setError("decode failed: Huffman tree is not initialized");
        return "";
    }
    for (std::size_t i = 0; i < code.size(); ++i) {
        if (code[i] != '0' && code[i] != '1') {
            setError("decode failed: invalid character " + describeChar(code[i])
                     + " in code at position " + std::to_string(i));
            return "";
        }
    }

    std::string text;

    // 单字符字符集：唯一合法编码是 "0"，每个 '0' 输出该字符。
    if (root_->left == nullptr && root_->right == nullptr) {
        for (char c : code) {
            if (c == '1') {
                setError("decode failed: bit '1' is invalid "
                         "for a single-character Huffman tree");
                return "";
            }
            text += root_->ch;
        }
        return text;
    }

    const Node* node = root_;
    for (char c : code) {
        node = (c == '0') ? node->left : node->right;
        if (node == nullptr) {
            setError("decode failed: code path does not exist in the tree");
            return "";
        }
        if (node->left == nullptr && node->right == nullptr) {
            text += node->ch;
            node = root_;  // 到达叶子，回到根继续译下一个字符
        }
    }
    if (node != root_) {
        setError("decode failed: code ends inside the tree (truncated code)");
        return "";
    }
    return text;
}

std::map<char, std::string> HuffmanSystem::getCodeTable() const {
    return codeTable_;
}

bool HuffmanSystem::encodeFile(
    const std::string& inputPath,
    const std::string& outputPath
) const {
    setError("");
    if (!isInitialized()) {
        setError("encodeFile failed: Huffman tree is not initialized");
        return false;
    }
    std::ifstream input(inputPath, std::ios::binary);
    if (!input) {
        setError("encodeFile failed: cannot open file for reading: " + inputPath);
        return false;
    }
    std::string text{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
    input.close();

    std::string code = encode(text);
    if (!text.empty() && code.empty()) {
        return false;  // encode() 已设置错误信息
    }

    std::ofstream output(outputPath, std::ios::binary);
    if (!output) {
        setError("encodeFile failed: cannot open file for writing: " + outputPath);
        return false;
    }
    output << code;
    if (!output) {
        setError("encodeFile failed: failed to write: " + outputPath);
        return false;
    }
    return true;
}

bool HuffmanSystem::decodeFile(
    const std::string& inputPath,
    const std::string& outputPath
) const {
    setError("");
    if (!isInitialized()) {
        setError("decodeFile failed: Huffman tree is not initialized");
        return false;
    }
    std::ifstream input(inputPath, std::ios::binary);
    if (!input) {
        setError("decodeFile failed: cannot open file for reading: " + inputPath);
        return false;
    }
    std::string code{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
    input.close();

    // 容忍编码文件中混入的空白字符（空格、换行等），方便人工查看文件。
    std::string cleaned;
    cleaned.reserve(code.size());
    for (char c : code) {
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            cleaned += c;
        }
    }

    std::string text = decode(cleaned);
    if (!cleaned.empty() && text.empty()) {
        return false;  // decode() 已设置错误信息
    }

    std::ofstream output(outputPath, std::ios::binary);
    if (!output) {
        setError("decodeFile failed: cannot open file for writing: " + outputPath);
        return false;
    }
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) {
        setError("decodeFile failed: failed to write: " + outputPath);
        return false;
    }
    return true;
}

bool HuffmanSystem::saveTree(const std::string& path) const {
    setError("");
    if (!isInitialized()) {
        setError("saveTree failed: Huffman tree is not initialized");
        return false;
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        setError("saveTree failed: cannot open file for writing: " + path);
        return false;
    }
    saveTreeRec(out, root_);
    if (!out) {
        setError("saveTree failed: failed to write: " + path);
        return false;
    }
    return true;
}

void HuffmanSystem::saveTreeRec(std::ostream& out, const Node* node) {
    if (node == nullptr) return;
    if (node->left == nullptr && node->right == nullptr) {
        // 字符统一保存 ASCII 整数值，便于处理空格、换行等特殊字符。
        out << "L " << static_cast<int>(static_cast<unsigned char>(node->ch))
            << " " << node->weight << "\n";
    } else {
        out << "I " << node->weight << "\n";
        saveTreeRec(out, node->left);
        saveTreeRec(out, node->right);
    }
}

bool HuffmanSystem::loadTree(const std::string& path) {
    setError("");
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        setError("loadTree failed: cannot open file for reading: " + path);
        return false;
    }

    Node* newRoot = nullptr;
    std::string error;
    if (!loadTreeRec(in, newRoot, error)) {
        deleteTree(newRoot);
        setError("loadTree failed: " + error);
        return false;
    }

    // 解析成功后才替换旧树，保证加载失败时原状态不被破坏。
    deleteTree(root_);
    root_ = newRoot;
    rebuildCodeTable();
    return true;
}

bool HuffmanSystem::loadTreeRec(
    std::istream& in,
    Node*& node,
    std::string& error
) {
    std::string type;
    if (!(in >> type)) {
        error = "unexpected end of file";
        return false;
    }

    if (type == "L") {
        int ascii = 0;
        long long weight = 0;
        if (!(in >> ascii >> weight)) {
            error = "invalid leaf entry";
            return false;
        }
        if (ascii < 0 || ascii > 255) {
            error = "leaf character code out of range: " + std::to_string(ascii);
            return false;
        }
        if (weight <= 0) {
            error = "leaf weight must be positive: " + std::to_string(weight);
            return false;
        }
        node = new Node(static_cast<char>(ascii), weight);
        return true;
    }

    if (type == "I") {
        long long weight = 0;
        if (!(in >> weight)) {
            error = "invalid internal node entry";
            return false;
        }
        if (weight <= 0) {
            error = "internal node weight must be positive: "
                    + std::to_string(weight);
            return false;
        }
        Node* left = nullptr;
        Node* right = nullptr;
        if (!loadTreeRec(in, left, error)) {
            deleteTree(left);
            return false;
        }
        if (!loadTreeRec(in, right, error)) {
            deleteTree(left);
            deleteTree(right);
            return false;
        }
        // 内部节点权值由孩子求和重算，保证树不变式成立。
        char minCh = (static_cast<unsigned char>(left->ch)
                      <= static_cast<unsigned char>(right->ch))
                         ? left->ch
                         : right->ch;
        node = new Node(minCh, left->weight + right->weight);
        node->left = left;
        node->right = right;
        return true;
    }

    error = "unknown node type: \"" + type + "\"";
    return false;
}

std::string HuffmanSystem::getTreePrint() const {
    if (!isInitialized()) return "";
    std::string out;
    buildTreePrint(root_, 0, out);
    return out;
}

void HuffmanSystem::buildTreePrint(const Node* node, int depth, std::string& out) {
    if (node == nullptr) return;
    buildTreePrint(node->left, depth + 1, out);
    out.append(static_cast<std::size_t>(depth) * 4, ' ');
    if (node->left == nullptr && node->right == nullptr) {
        out += leafLabel(node->ch);
    } else {
        out += "*";
    }
    out += "(" + std::to_string(node->weight) + ")\n";
    buildTreePrint(node->right, depth + 1, out);
}

bool HuffmanSystem::writeTreePrint(const std::string& path) const {
    setError("");
    if (!isInitialized()) {
        setError("writeTreePrint failed: Huffman tree is not initialized");
        return false;
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        setError("writeTreePrint failed: cannot open file for writing: " + path);
        return false;
    }
    out << getTreePrint();
    if (!out) {
        setError("writeTreePrint failed: failed to write: " + path);
        return false;
    }
    return true;
}

std::string HuffmanSystem::lastError() const {
    return lastError_;
}
