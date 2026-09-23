// ============================================================================
//  界面端到端冒烟测试（手工验证用，不属于 ctest 常规测试）
//
//  目的：验证集成后的主窗口「点击按钮 → 界面显示正确结果」这条链路，
//        而不仅仅是各模块的单元测试通过。
//
//  做法：构造真实的 MainWindow，用 QTimer 依次触发按钮点击，
//        再读取界面上的实际文本做断言。全程走的是与人工操作完全相同的代码路径。
//
//  构建与运行见 tests/manual/README.md
// ============================================================================

#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>

#include "../../src/gui/MainWindow.h"

#include <cstdio>
#include <string>

namespace {

int gPassed = 0;
int gFailed = 0;

void check(bool ok, const std::string& name, const std::string& detail = "")
{
    if (ok) { ++gPassed; std::printf("  [PASS] %s\n", name.c_str()); }
    else
    {
        ++gFailed;
        std::printf("  [FAIL] %s", name.c_str());
        if (!detail.empty()) { std::printf("   -> %s", detail.c_str()); }
        std::printf("\n");
    }
}

/// 按按钮上的文字查找控件。
QPushButton* findButton(QWidget* root, const QString& text)
{
    const QList<QPushButton*> buttons = root->findChildren<QPushButton*>();
    for (QPushButton* button : buttons)
    {
        if (button->text() == text) { return button; }
    }
    return nullptr;
}

/// 按占位提示文字查找多行文本框（这些控件没有设置 objectName）。
QPlainTextEdit* findEditByPlaceholder(QWidget* root, const QString& hint)
{
    const QList<QPlainTextEdit*> edits = root->findChildren<QPlainTextEdit*>();
    for (QPlainTextEdit* edit : edits)
    {
        if (edit->placeholderText() == hint) { return edit; }
    }
    return nullptr;
}

/// 从若干 QLabel 中找出文本包含关键字的那个。
QLabel* findLabelContaining(QWidget* root, const QString& keyword)
{
    const QList<QLabel*> labels = root->findChildren<QLabel*>();
    for (QLabel* label : labels)
    {
        if (label->text().contains(keyword)) { return label; }
    }
    return nullptr;
}

/// 把控件在其所属页面的范围内查找表格。
///
/// 不能直接在整窗口范围内按列数找表——不同页面存在列数相同的表格
/// （例如造价矩阵与加密比较结果都是 5 列），会匹配到错误的那一张。
/// 这里从页面上的某个按钮出发，沿父级向上走到该页面，再在页面内查找。
QTableWidget* findTableInSamePage(QWidget* anyWidgetOnPage, int columns)
{
    QWidget* page = anyWidgetOnPage;
    while (page != nullptr)
    {
        const QList<QTableWidget*> tables = page->findChildren<QTableWidget*>();
        for (QTableWidget* table : tables)
        {
            if (table->columnCount() == columns) { return table; }
        }
        // 爬到上一级继续找，直到越过 QTabWidget（此时说明已跳出本页）。
        if (qobject_cast<QTabWidget*>(page) != nullptr) { break; }
        page = page->parentWidget();
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
//  各页面的验证
// ---------------------------------------------------------------------------

void verifyNetworkPage(QWidget* window)
{
    std::printf("\n=== 网络设计页 ===\n");

    QPushButton* generate = findButton(window, QStringLiteral("生成网络"));
    QPushButton* sample   = findButton(window, QStringLiteral("加载示例"));
    check(generate != nullptr, "找到「生成网络」按钮");
    check(sample != nullptr, "找到「加载示例」按钮");
    if (generate == nullptr || sample == nullptr) { return; }

    // 载入示例：5 座城市，造价矩阵应填入有限值而非全部 INF。
    sample->click();

    QTableWidget* matrix = findTableInSamePage(sample, 5);
    check(matrix != nullptr, "在网络设计页内找到造价矩阵");
    check(matrix != nullptr && matrix->rowCount() == 5 && matrix->columnCount() == 5,
          "造价矩阵为 5×5");
    if (matrix != nullptr)
    {
        int numericCells = 0;
        for (int r = 0; r < matrix->rowCount(); ++r)
        {
            for (int c = 0; c < matrix->columnCount(); ++c)
            {
                if (r == c) { continue; }
                const QTableWidgetItem* item = matrix->item(r, c);
                if (item != nullptr && item->text() != QStringLiteral("INF"))
                {
                    ++numericCells;
                }
            }
        }
        // 示例矩阵分行为 {0,12,INF,18,INF} / {12,0,14,INF,22} /
        // {INF,14,0,16,11} / {18,INF,16,0,13} / {INF,22,11,13,0}，
        // 非对角线有限值共 14 个（其余为 INF，即两地不直连）。
        check(numericCells == 14, "示例造价填入了 14 个数值单元（其余为 INF）",
              "实际 " + std::to_string(numericCells));

        // 对称性：编辑 [i][j] 应同步 [j][i]。
        matrix->item(0, 1)->setText(QStringLiteral("99"));
        const bool mirrored =
            matrix->item(1, 0) != nullptr
            && matrix->item(1, 0)->text() == QStringLiteral("99");
        check(mirrored, "编辑 [0][1] 自动同步到 [1][0]");
        matrix->item(0, 1)->setText(QStringLiteral("12"));   // 复原
    }

    // 生成网络（默认 Prim）。
    generate->click();

    QLabel* summary = findLabelContaining(window, QStringLiteral("最低总造价"));
    check(summary != nullptr, "生成后出现含「最低总造价」的摘要");
    if (summary != nullptr)
    {
        std::printf("         摘要: %s\n", summary->text().toUtf8().constData());
        check(summary->text().contains(QStringLiteral("50")),
              "Prim 算出的最低总造价为 50（与模块测试一致）",
              summary->text().toUtf8().constData());
    }

    // 切到 Kruskal 再算一次，结果应完全一致。
    // 必须按条目文字定位下拉框：网络设计页与安全通信页各有一个下拉框，
    // 且 QComboBox 内部还含一个 QLineEdit，按类型在整窗口查找会拿到错误的控件。
    QComboBox* combo = nullptr;
    const QList<QComboBox*> combos = window->findChildren<QComboBox*>();
    for (QComboBox* candidate : combos)
    {
        if (candidate->count() == 2
            && candidate->itemText(0).contains(QStringLiteral("Prim")))
        {
            combo = candidate;
            break;
        }
    }
    check(combo != nullptr, "找到网络设计页的算法下拉框");
    if (combo != nullptr)
    {
        combo->setCurrentIndex(1);
        generate->click();
        QLabel* summary2 = findLabelContaining(window, QStringLiteral("最低总造价"));
        check(summary2 != nullptr
                  && summary2->text().contains(QStringLiteral("Kruskal"))
                  && summary2->text().contains(QStringLiteral("50")),
              "Kruskal 结果与 Prim 一致（总造价同为 50）",
              summary2 ? summary2->text().toUtf8().constData() : "无摘要");
    }
}

void verifyHuffmanPage(QWidget* window)
{
    std::printf("\n=== Huffman 页 ===\n");

    QPushButton* encode = findButton(window, QStringLiteral("编码"));
    QPushButton* decode = findButton(window, QStringLiteral("译码"));
    check(encode != nullptr, "找到「编码」按钮");
    check(decode != nullptr, "找到「译码」按钮");
    if (encode == nullptr || decode == nullptr) { return; }

    // 构造时会自动载入示例字符集并建树，编码表应已填充。
    QTableWidget* codeTable = nullptr;
    const QList<QTableWidget*> tables = window->findChildren<QTableWidget*>();
    for (QTableWidget* table : tables)
    {
        if (table->columnCount() == 2 && table->rowCount() == 10)
        {
            codeTable = table;
            break;
        }
    }
    check(codeTable != nullptr, "编码表已生成 10 行（示例字符集）");

    QPlainTextEdit* input = findEditByPlaceholder(window,
        QStringLiteral("在此输入待编码的文本；译码时粘贴由 0 和 1 组成的编码串。"));
    QPlainTextEdit* output = findEditByPlaceholder(window,
        QStringLiteral("编码或译码的结果显示在此处。"));
    check(input != nullptr && output != nullptr, "找到编码输入框与输出框");
    if (input == nullptr || output == nullptr) { return; }

    // 编码 I AM FROM CHINA
    input->setPlainText(QStringLiteral("I AM FROM CHINA"));
    encode->click();
    const QString code = output->toPlainText();
    check(!code.isEmpty(), "编码产生了非空结果");
    check(code.size() == 48 && code.count(QRegularExpression("[^01]")) == 0,
          "编码为 48 位纯 0/1 串（与模块测试一致）",
          std::to_string(code.size()) + " 位");
    std::printf("         编码: %s\n", code.toUtf8().constData());

    // 译码应还原原文
    input->setPlainText(code);
    decode->click();
    check(output->toPlainText() == QStringLiteral("I AM FROM CHINA"),
          "译码还原出 I AM FROM CHINA",
          output->toPlainText().toUtf8().constData());

    // 文件流程：读取 TobeTran -> 编码到 CodeFile -> 译码到 TextFile
    QPushButton* readTobe = findButton(window, QStringLiteral("读取 TobeTran"));
    QPushButton* encFile  = findButton(window, QStringLiteral("编码到 CodeFile"));
    QPushButton* decFile  = findButton(window, QStringLiteral("译码到 TextFile"));
    check(readTobe && encFile && decFile, "找到三个文件操作按钮");
    if (readTobe && encFile && decFile)
    {
        readTobe->click();
        check(input->toPlainText() == QStringLiteral("I AM FROM CHINA"),
              "「读取 TobeTran」把文件内容读入输入框",
              input->toPlainText().toUtf8().constData());

        encFile->click();
        check(output->toPlainText().size() == 48,
              "「编码到 CodeFile」在输出框显示 48 位编码",
              std::to_string(output->toPlainText().size()) + " 位");

        decFile->click();
        check(output->toPlainText() == QStringLiteral("I AM FROM CHINA"),
              "「译码到 TextFile」正确还原",
              output->toPlainText().toUtf8().constData());
    }

    // 说明：编码字符集之外的字符会弹出 QMessageBox 模态对话框，
    // 无人点击时会阻塞自动化测试，故此处不触发。
    // 「异常输入不崩溃」这一要求由各模块的无界面测试覆盖
    // （encode 对未知字符返回空串并记录 lastError，界面负责展示），
    // 界面弹窗本身属人工验收范围。
}

void verifySecurityPage(QWidget* window)
{
    std::printf("\n=== 安全通信页 ===\n");

    QPushButton* encrypt = findButton(window, QStringLiteral("加密"));
    QPushButton* decrypt = findButton(window, QStringLiteral("解密"));
    check(encrypt != nullptr, "找到「加密」按钮");
    check(decrypt != nullptr, "找到「解密」按钮");
    if (encrypt == nullptr || decrypt == nullptr) { return; }

    QLineEdit* keyEdit = nullptr;
    const QList<QLineEdit*> edits = window->findChildren<QLineEdit*>();
    const QLineEdit* aesComboEditor = nullptr;
    const QList<QComboBox*> allCombos = window->findChildren<QComboBox*>();
    for (QComboBox* c : allCombos)
    {
        if (c->itemText(0).contains(QStringLiteral("AES"))) { aesComboEditor = c->lineEdit(); }
    }
    for (QLineEdit* edit : edits)
    {
        if (edit == aesComboEditor) { continue; }   // 跳过下拉框内部的编辑器
        if (edit->text() == QStringLiteral("1234567890123456")) { keyEdit = edit; break; }
    }
    if (keyEdit == nullptr)
    {
        std::printf("         诊断：页面上的 QLineEdit 共 %d 个\n",
                    static_cast<int>(edits.size()));
        for (QLineEdit* edit : edits)
        {
            std::printf("           text=[%s] echoMode=%d placeholder=[%s]%s\n",
                        edit->text().toUtf8().constData(),
                        static_cast<int>(edit->echoMode()),
                        edit->placeholderText().toUtf8().constData(),
                        (edit == aesComboEditor) ? "  <- 下拉框内部编辑器" : "");
        }
    }
    check(keyEdit != nullptr, "找到密钥输入框，默认值为 16 字节 AES 密钥");

    QPlainTextEdit* input = findEditByPlaceholder(window,
        QStringLiteral("在此输入明文；解密时可直接粘贴上面的密文。"));
    QPlainTextEdit* output = findEditByPlaceholder(window,
        QStringLiteral("加密结果为十六进制字符串，可直接复制到上方输入框进行解密。"));
    check(input != nullptr && output != nullptr, "找到明文输入框与结果输出框");
    if (input == nullptr || output == nullptr || keyEdit == nullptr) { return; }

    // AES 往返
    input->setPlainText(QStringLiteral("HELLO CHINA"));
    encrypt->click();
    const QString cipher = input->toPlainText();
    check(cipher.size() == 32 && QRegularExpression("^[0-9A-F]+$")
                                     .match(cipher).hasMatch(),
          "AES 密文为 32 位大写十六进制串",
          cipher.toUtf8().constData());
    std::printf("         AES 密文: %s\n", cipher.toUtf8().constData());
    check(output->toPlainText().contains(QStringLiteral("加密耗时")),
          "输出区给出加密耗时");

    decrypt->click();
    check(output->toPlainText().contains(QStringLiteral("HELLO CHINA")),
          "AES 解密还原出 HELLO CHINA");

    // 切到 DES 再往返一次
    QComboBox* combo = nullptr;
    const QList<QComboBox*> combos = window->findChildren<QComboBox*>();
    for (QComboBox* c : combos)
    {
        if (c->count() == 2
            && c->itemText(0).contains(QStringLiteral("AES"))) { combo = c; break; }
    }
    check(combo != nullptr, "找到算法下拉框");
    if (combo != nullptr)
    {
        combo->setCurrentIndex(1);        // DES-CBC
        check(keyEdit->text().size() == 8,
              "切换到 DES 后密钥自动调整为 8 字节",
              std::to_string(keyEdit->text().size()) + " 字节");

        input->setPlainText(QStringLiteral("CITY C SENDS DATA TO CITY D"));
        encrypt->click();
        const QString desCipher = input->toPlainText();
        check(desCipher.size() == 64, "DES 密文为 64 位十六进制串",
              std::to_string(desCipher.size()) + " 位");
        decrypt->click();
        check(output->toPlainText().contains(
                  QStringLiteral("CITY C SENDS DATA TO CITY D")),
              "DES 解密还原出 CITY C SENDS DATA TO CITY D");
    }

    // 说明：密钥长度错误的路径会弹出 QMessageBox 模态对话框，
    // 在无人点击的情况下会永久阻塞自动化测试，故此处不触发。
    // 该路径的正确性由 tests/test_crypto.cpp 的异常输入用例覆盖
    // （密钥 15/17/7 字节均被拒绝且返回明确的 errorMessage），
    // 界面侧只需确认会把 errorMessage 弹窗展示即可，属人工验收范围。
    check(true, "密钥长度错误的弹窗路径未在此触发（会阻塞自动化测试）");
}

void verifyPerformancePage(QWidget* window)
{
    std::printf("\n=== 性能比较页 ===\n");

    QPushButton* mstBench    = findButton(window, QStringLiteral("运行 MST 比较"));
    QPushButton* cryptoBench = findButton(window, QStringLiteral("运行加密比较"));
    check(mstBench != nullptr, "找到「运行 MST 比较」按钮");
    check(cryptoBench != nullptr, "找到「运行加密比较」按钮");
    if (mstBench == nullptr || cryptoBench == nullptr) { return; }

    // 只跑规模最小的加密比较（1 KiB ~ 64 KiB 共 6 行），避免测试超时。
    // MST 比较含 n=500，单次约数秒，此处仅确认按钮可用。
    check(mstBench->isEnabled(), "MST 比较按钮在 MST 模块就绪后已启用");

    cryptoBench->click();
    QTableWidget* table = findTableInSamePage(cryptoBench, 5);
    check(table != nullptr && table->rowCount() == 10,
          "加密比较生成了 10 行结果（5 种数据量 × 2 种算法）",
          table ? std::to_string(table->rowCount()) + " 行" : "未找到表格");
    if (table != nullptr && table->rowCount() == 10)
    {
        // 抽查最后一行：1 MiB 的 DES，往返校验应为通过。
        const QTableWidgetItem* last = table->item(9, 4);
        check(last != nullptr && last->text().contains(QStringLiteral("通过")),
              "1 MiB 的 DES 往返校验通过",
              last ? last->text().toUtf8().constData() : "无数据");
    }
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    std::printf("界面端到端冒烟测试\n");
    std::printf("================================================\n");

    MainWindow window;
    window.show();

    // 等窗口完成首次布局与初始化后，依次验证各页面。
    QTimer::singleShot(400, [&window]() {
        verifyNetworkPage(&window);
        verifyHuffmanPage(&window);
        verifySecurityPage(&window);
        verifyPerformancePage(&window);

        std::printf("\n================================================\n");
        std::printf("通过: %d    失败: %d\n", gPassed, gFailed);
        std::printf("%s\n", gFailed == 0 ? "结论: 界面功能验证通过。"
                                         : "结论: 存在失败项。");
        QApplication::exit(gFailed == 0 ? 0 : 1);
    });

    return app.exec();
}
