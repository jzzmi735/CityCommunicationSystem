/**
 * @file    MainWindow.cpp
 * @brief   主窗口各功能页面的搭建与事件处理。
 *
 * 界面层严格遵守《共同开发规则和接口约定》第 13 节的分工：
 *   接收输入 → 调用算法模块接口 → 显示结果。
 * 本文件不包含任何算法实现。
 */

#include "MainWindow.h"
#include "NetworkView.h"

#include "../common/Constants.h"   // INF、MAX_CITY_COUNT 与统一错误文本

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace {

/// 把城市编号转成界面显示名：0..25 → A..Z，超出范围 → City n。
QString displayName(int index)
{
    if (index >= 0 && index < 26)
    {
        return QString(QChar('A' + index));
    }
    return QStringLiteral("City %1").arg(index + 1);
}

/// 输入回显时用于截断过长的明文/密文，避免撑爆文本框。
QString brief(const QString& text, int limit = 48)
{
    if (text.size() <= limit)
    {
        return text;
    }
    return text.left(limit) + QStringLiteral("…");
}

/// 把毫秒数格式化成便于阅读的形式。
QString formatMs(double ms)
{
    return QStringLiteral("%1 ms").arg(ms, 0, 'f', 3);
}

} // namespace

// ===========================================================================
//  构造与析构
// ===========================================================================

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("城市通信网络设计系统"));

    tabs_ = new QTabWidget(this);
    tabs_->addTab(buildNetworkPage(),    QStringLiteral("网络设计"));
    tabs_->addTab(buildHuffmanPage(),    QStringLiteral("Huffman"));
    tabs_->addTab(buildSecurityPage(),   QStringLiteral("安全通信"));
    tabs_->addTab(buildPerformancePage(), QStringLiteral("性能比较"));

    setCentralWidget(tabs_);
    resize(1180, 760);

    // 初始化界面状态：默认 5 座城市，加载一组示例造价。
    cityCountSpin_->setValue(5);
    onLoadSample();
}

MainWindow::~MainWindow() = default;

// ===========================================================================
//  网络设计页
// ===========================================================================

QWidget* MainWindow::buildNetworkPage()
{
    QWidget* page = new QWidget(this);

    // ---- 左侧：参数输入 ----
    cityCountSpin_ = new QSpinBox(page);
    cityCountSpin_->setRange(1, MAX_CITY_COUNT);
    cityCountSpin_->setValue(5);

    algorithmCombo_ = new QComboBox(page);
    algorithmCombo_->addItem(QStringLiteral("Prim 算法"));
    algorithmCombo_->addItem(QStringLiteral("Kruskal 算法"));

    QPushButton* generateButton = new QPushButton(QStringLiteral("生成网络"), page);
    QPushButton* clearButton    = new QPushButton(QStringLiteral("清空"), page);
    QPushButton* sampleButton   = new QPushButton(QStringLiteral("加载示例"), page);

    QFormLayout* paramLayout = new QFormLayout();
    paramLayout->addRow(QStringLiteral("城市数量："), cityCountSpin_);
    paramLayout->addRow(QStringLiteral("算法选择："), algorithmCombo_);

    QGroupBox* paramBox = new QGroupBox(QStringLiteral("参数设置"), page);
    QVBoxLayout* paramBoxLayout = new QVBoxLayout(paramBox);
    paramBoxLayout->addLayout(paramLayout);
    paramBoxLayout->addWidget(generateButton);
    paramBoxLayout->addWidget(sampleButton);
    paramBoxLayout->addWidget(clearButton);

    // ---- 左侧下半：造价矩阵 ----
    costTable_ = new QTableWidget(page);
    costTable_->setEditTriggers(QAbstractItemView::DoubleClicked
                                | QAbstractItemView::EditKeyPressed);
    costTable_->verticalHeader()->setDefaultSectionSize(28);
    costTable_->horizontalHeader()->setDefaultSectionSize(52);

    QLabel* tableHint = new QLabel(
        QStringLiteral("双击单元格可修改造价；不直接相连的城市输入 INF。\n"
                       "矩阵关于对角线对称，编辑后会自动同步。"), page);
    tableHint->setWordWrap(true);
    tableHint->setStyleSheet(QStringLiteral("color: #666;"));

    QGroupBox* tableBox = new QGroupBox(QStringLiteral("城市间通信造价矩阵"), page);
    QVBoxLayout* tableBoxLayout = new QVBoxLayout(tableBox);
    tableBoxLayout->addWidget(costTable_);
    tableBoxLayout->addWidget(tableHint);

    QWidget* leftPanel = new QWidget(page);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->addWidget(paramBox);
    leftLayout->addWidget(tableBox, 1);

    // ---- 右侧：网络图 + 结果 ----
    networkView_ = new NetworkView(page);

    mstSummaryLabel_ = new QLabel(QStringLiteral("尚未生成网络。"), page);
    mstSummaryLabel_->setWordWrap(true);

    mstResultEdit_ = new QPlainTextEdit(page);
    mstResultEdit_->setReadOnly(true);
    mstResultEdit_->setPlaceholderText(
        QStringLiteral("选择城市数量与算法后，点击「生成网络」，此处显示最小生成树的边。"));

    QGroupBox* resultBox = new QGroupBox(QStringLiteral("计算结果"), page);
    QVBoxLayout* resultLayout = new QVBoxLayout(resultBox);
    resultLayout->addWidget(mstSummaryLabel_);
    resultLayout->addWidget(mstResultEdit_, 1);

    QSplitter* rightSplitter = new QSplitter(Qt::Vertical, page);
    rightSplitter->addWidget(networkView_);
    rightSplitter->addWidget(resultBox);
    rightSplitter->setStretchFactor(0, 3);
    rightSplitter->setStretchFactor(1, 2);

    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal, page);
    mainSplitter->addWidget(leftPanel);
    mainSplitter->addWidget(rightSplitter);
    mainSplitter->setStretchFactor(0, 2);
    mainSplitter->setStretchFactor(1, 3);

    QVBoxLayout* pageLayout = new QVBoxLayout(page);
    pageLayout->addWidget(mainSplitter);

    // ---- 信号连接 ----
    connect(cityCountSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onCityCountChanged);
    connect(costTable_, &QTableWidget::cellChanged,
            this, &MainWindow::onCostItemChanged);
    connect(generateButton, &QPushButton::clicked,
            this, &MainWindow::onGenerateNetwork);
    connect(clearButton, &QPushButton::clicked,
            this, &MainWindow::onClearNetwork);
    connect(sampleButton, &QPushButton::clicked,
            this, &MainWindow::onLoadSample);

    // 按初始城市数建表。
    rebuildCostTable(cityCountSpin_->value());

    return page;
}

void MainWindow::onCityCountChanged(int count)
{
    rebuildCostTable(count);
    networkView_->setCityCount(count);
    mstSummaryLabel_->setText(QStringLiteral("城市数量已改为 %1，请重新生成网络。")
                                  .arg(count));
    mstResultEdit_->clear();
}

void MainWindow::rebuildCostTable(int cityCount)
{
    currentCityCount_ = cityCount;

    // 重建表头与行列时会触发 cellChanged，先屏蔽以免误同步。
    const QSignalBlocker blocker(costTable_);

    costTable_->clear();
    costTable_->setRowCount(cityCount);
    costTable_->setColumnCount(cityCount);

    QStringList headers;
    for (int i = 0; i < cityCount; ++i)
    {
        headers << displayName(i);
    }
    costTable_->setHorizontalHeaderLabels(headers);
    costTable_->setVerticalHeaderLabels(headers);

    for (int row = 0; row < cityCount; ++row)
    {
        for (int column = 0; column < cityCount; ++column)
        {
            QTableWidgetItem* item = new QTableWidgetItem();
            item->setTextAlignment(Qt::AlignCenter);

            if (row == column)
            {
                // 对角线固定为 0，且不允许编辑。
                item->setText(QStringLiteral("0"));
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                item->setBackground(QColor(0xEE, 0xEE, 0xEE));
            }
            else
            {
                item->setText(QStringLiteral("INF"));
            }
            costTable_->setItem(row, column, item);
        }
    }

    // 城市数变化后，网络图也应重画。
    if (networkView_ != nullptr)
    {
        networkView_->setCityCount(cityCount);
    }
}

void MainWindow::onCostItemChanged(int row, int column)
{
    // 无向图：编辑 [i][j] 时同步 [j][i]，避免用户输入两份不一致的造价。
    if (row == column || row < 0 || column < 0
        || row >= currentCityCount_ || column >= currentCityCount_)
    {
        return;
    }

    QTableWidgetItem* source = costTable_->item(row, column);
    QTableWidgetItem* mirror = costTable_->item(column, row);
    if (source == nullptr || mirror == nullptr)
    {
        return;
    }

    if (mirror->text() != source->text())
    {
        const QSignalBlocker blocker(costTable_);
        mirror->setText(source->text());
    }
}

bool MainWindow::collectCostMatrix(int cityCount,
                                   std::vector<std::vector<int> >& costs)
{
    costs.assign(static_cast<std::size_t>(cityCount),
                 std::vector<int>(static_cast<std::size_t>(cityCount), INF));

    for (int row = 0; row < cityCount; ++row)
    {
        for (int column = 0; column < cityCount; ++column)
        {
            if (row == column)
            {
                costs[row][column] = 0;
                continue;
            }

            const QTableWidgetItem* item = costTable_->item(row, column);
            if (item == nullptr)
            {
                QMessageBox::warning(this, QStringLiteral("输入不完整"),
                    QStringLiteral("单元格 %1-%2 没有内容。")
                        .arg(displayName(row), displayName(column)));
                return false;
            }

            const QString text = item->text().trimmed();
            if (text.isEmpty() || text.compare(QStringLiteral("INF"),
                                               Qt::CaseInsensitive) == 0)
            {
                // 没有直接链路，保持 INF。
                continue;
            }

            bool ok = false;
            const int value = text.toInt(&ok);
            if (!ok)
            {
                QMessageBox::warning(this, QStringLiteral("造价格式错误"),
                    QStringLiteral("单元格 %1-%2 的内容「%3」不是合法整数。\n"
                                   "请填写非负整数，或填写 INF 表示两地不直连。")
                        .arg(displayName(row), displayName(column), text));
                return false;
            }
            if (value < 0)
            {
                QMessageBox::warning(this, QStringLiteral("造价超出范围"),
                    QStringLiteral("单元格 %1-%2 的造价不能为负数。")
                        .arg(displayName(row), displayName(column)));
                return false;
            }
            // 防止用户填入过大的数值导致累加溢出。
            if (value >= INF)
            {
                QMessageBox::warning(this, QStringLiteral("造价超出范围"),
                    QStringLiteral("单元格 %1-%2 的造价过大，请填小于 %3 的值。")
                        .arg(displayName(row), displayName(column))
                        .arg(INF));
                return false;
            }

            costs[row][column] = value;
        }
    }
    return true;
}

void MainWindow::onGenerateNetwork()
{
    const int cityCount = cityCountSpin_->value();

#ifndef CCS_HAS_MST_HEADER
    Q_UNUSED(cityCount)
    QMessageBox::information(this, QStringLiteral("模块尚未就绪"),
        QStringLiteral("MST 模块（开发者 A）尚未合入，网络设计功能暂不可用。\n\n"
                       "安全通信页面已可正常使用。"));
    return;
#else
    std::vector<std::vector<int> > costs;
    if (!collectCostMatrix(cityCount, costs))
    {
        return;   // 具体原因已在 collectCostMatrix 内提示。
    }

    // 按文档第 16 节的调用流程：先配置城市数与造价，再执行算法。
    mst_.clear();
    mst_.setCityCount(cityCount);
    for (int row = 0; row < cityCount; ++row)
    {
        for (int column = 0; column < cityCount; ++column)
        {
            if (row == column)
            {
                continue;
            }
            if (costs[row][column] < INF)
            {
                mst_.setCost(row, column, costs[row][column]);
            }
        }
    }

    const bool usePrim = (algorithmCombo_->currentIndex() == 0);
    const QString algorithmName = usePrim ? QStringLiteral("Prim")
                                          : QStringLiteral("Kruskal");
    const MSTResult result = usePrim ? mst_.prim() : mst_.kruskal();

    QStringList edgeTexts;
    if (result.success)
    {
        for (std::size_t i = 0; i < result.edges.size(); ++i)
        {
            const Edge& edge = result.edges[i];
            edgeTexts << QStringLiteral("%1 — %2    造价 %3")
                             .arg(displayName(edge.from),
                                  displayName(edge.to))
                             .arg(edge.cost);
        }
    }

    showMstResult(algorithmName, result.success,
                  QString::fromStdString(result.errorMessage),
                  edgeTexts, result.totalCost, result.runningTimeMs);

    // 绘制网络：先画全部通信边，再高亮 MST 选中的边。
    networkView_->clear();
    networkView_->setCityCount(cityCount);
    for (int row = 0; row < cityCount; ++row)
    {
        for (int column = row + 1; column < cityCount; ++column)
        {
            if (costs[row][column] < INF)
            {
                networkView_->addEdge(row, column, costs[row][column]);
            }
        }
    }
    if (result.success)
    {
        for (std::size_t i = 0; i < result.edges.size(); ++i)
        {
            networkView_->markMstEdge(result.edges[i].from, result.edges[i].to);
        }
    }
    networkView_->fitContent();
#endif
}

void MainWindow::showMstResult(const QString& algorithmName,
                               bool success,
                               const QString& errorMessage,
                               const QStringList& edgeTexts,
                               long long totalCost,
                               double runningTimeMs)
{
    if (!success)
    {
        mstSummaryLabel_->setText(QStringLiteral("计算失败。"));
        mstResultEdit_->setPlainText(errorMessage);

        QMessageBox::warning(this,
            QStringLiteral("%1 算法执行失败").arg(algorithmName),
            errorMessage.isEmpty() ? QStringLiteral("未提供具体原因。")
                                   : errorMessage);
        return;
    }

    mstSummaryLabel_->setText(
        QStringLiteral("%1 算法完成：最低总造价 %2，运行时间 %3。")
            .arg(algorithmName)
            .arg(totalCost)
            .arg(formatMs(runningTimeMs)));

    if (edgeTexts.isEmpty())
    {
        mstResultEdit_->setPlainText(QStringLiteral("没有需要铺设的链路。"));
        return;
    }

    QString text = QStringLiteral("最小生成树共 %1 条链路：\n\n")
                       .arg(edgeTexts.size());
    for (int i = 0; i < edgeTexts.size(); ++i)
    {
        text += QStringLiteral("%1.  %2\n").arg(i + 1).arg(edgeTexts[i]);
    }
    text += QStringLiteral("\n最低总造价：%1\n运行时间：%2")
                .arg(totalCost).arg(formatMs(runningTimeMs));
    mstResultEdit_->setPlainText(text);
}

void MainWindow::onClearNetwork()
{
    // 若原来就是 5 座城市，setValue 不会发出信号，故此处显式重建一次，
    // 保证表格无论城市数是否变化都被清空为初始状态。
    cityCountSpin_->setValue(5);
    if (currentCityCount_ != 5)
    {
        rebuildCostTable(5);
    }
    else
    {
        // 城市数未变，只需把非对角线项恢复为 INF。
        const QSignalBlocker blocker(costTable_);
        for (int row = 0; row < currentCityCount_; ++row)
        {
            for (int column = 0; column < currentCityCount_; ++column)
            {
                if (row == column)
                {
                    continue;
                }
                QTableWidgetItem* item = costTable_->item(row, column);
                if (item != nullptr)
                {
                    item->setText(QStringLiteral("INF"));
                }
            }
        }
    }

    networkView_->clear();
    networkView_->setCityCount(5);
    mstSummaryLabel_->setText(QStringLiteral("已清空，请重新填写造价。"));
    mstResultEdit_->clear();
}

void MainWindow::onLoadSample()
{
    // 一组便于演示的示例造价：5 座城市，含若干条不直连的链路。
    static const int kSample[5][5] = {
        {   0,  12, INF,  18, INF },
        {  12,   0,  14, INF,  22 },
        { INF,  14,   0,  16,  11 },
        {  18, INF,  16,   0,  13 },
        { INF,  22,  11,  13,   0 }
    };

    // 先确保城市数为 5；setValue 未触发信号时（原值即为 5）手动重建表格。
    cityCountSpin_->setValue(5);
    if (currentCityCount_ != 5)
    {
        rebuildCostTable(5);
    }

    const QSignalBlocker blocker(costTable_);
    for (int row = 0; row < 5; ++row)
    {
        for (int column = 0; column < 5; ++column)
        {
            if (row == column)
            {
                continue;
            }
            QTableWidgetItem* item = costTable_->item(row, column);
            if (item == nullptr)
            {
                continue;
            }
            if (kSample[row][column] >= INF)
            {
                item->setText(QStringLiteral("INF"));
            }
            else
            {
                item->setText(QString::number(kSample[row][column]));
            }
        }
    }

    networkView_->clear();
    networkView_->setCityCount(5);
    mstSummaryLabel_->setText(
        QStringLiteral("已载入 5 座城市的示例造价，点击「生成网络」查看结果。"));
    mstResultEdit_->clear();
}

// ===========================================================================
//  Huffman 页
// ===========================================================================

void MainWindow::onLoadCharsetSample()
{
    // 以题目要求的 "I AM FROM CHINA" 为例，权值取各字符在该句中的出现次数。
    static const char kChars[] = { 'I', 'A', 'M', 'F', 'R', 'O', 'C', 'H', 'N', ' ' };
    static const int  kWeights[] = { 2, 2, 2, 1, 1, 1, 1, 1, 1, 3 };
    const int count = static_cast<int>(sizeof(kWeights) / sizeof(kWeights[0]));

    const QSignalBlocker blocker(charsetTable_);
    charsetTable_->setRowCount(count);
    for (int i = 0; i < count; ++i)
    {
        QTableWidgetItem* charItem = new QTableWidgetItem(QString(QChar(kChars[i])));
        charItem->setTextAlignment(Qt::AlignCenter);
        charsetTable_->setItem(i, 0, charItem);

        QTableWidgetItem* weightItem = new QTableWidgetItem(QString::number(kWeights[i]));
        weightItem->setTextAlignment(Qt::AlignCenter);
        charsetTable_->setItem(i, 1, weightItem);
    }

    huffmanInputEdit_->setPlainText(QStringLiteral("I AM FROM CHINA"));
    onBuildHuffmanTree();
}

void MainWindow::onBuildHuffmanTree()
{
#ifndef CCS_HAS_HUFFMAN_HEADER
    return;
#else
    std::vector<char> chars;
    std::vector<int>  weights;
    std::vector<char> seen;

    const int rowCount = charsetTable_->rowCount();
    if (rowCount == 0)
    {
        QMessageBox::warning(this, QStringLiteral("字符集为空"),
                             QStringLiteral("请先填写字符与权值，或点击「载入示例」。"));
        return;
    }

    for (int row = 0; row < rowCount; ++row)
    {
        const QTableWidgetItem* charItem = charsetTable_->item(row, 0);
        const QTableWidgetItem* weightItem = charsetTable_->item(row, 1);
        if (charItem == nullptr || weightItem == nullptr)
        {
            QMessageBox::warning(this, QStringLiteral("输入不完整"),
                QStringLiteral("第 %1 行没有填写完整。").arg(row + 1));
            return;
        }

        const QString charText = charItem->text();
        if (charText.isEmpty())
        {
            QMessageBox::warning(this, QStringLiteral("字符为空"),
                QStringLiteral("第 %1 行的字符为空。").arg(row + 1));
            return;
        }
        // 单个字符可能由多个 UTF-8 字节组成，此处按字节处理，
        // 与 Huffman 模块的 char 接口保持一致。
        if (charText.toUtf8().size() != 1)
        {
            QMessageBox::warning(this, QStringLiteral("字符不合法"),
                QStringLiteral("第 %1 行填写了「%2」，本模块按单字节字符处理，"
                               "请每行只填一个 ASCII 字符。")
                    .arg(row + 1).arg(charText));
            return;
        }

        const char ch = charText.toUtf8().at(0);
        if (std::find(seen.begin(), seen.end(), ch) != seen.end())
        {
            QMessageBox::warning(this, QStringLiteral("字符重复"),
                QStringLiteral("字符「%1」出现了多次，编码表要求字符互不相同。")
                    .arg(charText));
            return;
        }
        seen.push_back(ch);

        bool ok = false;
        const int weight = weightItem->text().trimmed().toInt(&ok);
        if (!ok || weight <= 0)
        {
            QMessageBox::warning(this, QStringLiteral("权值不合法"),
                QStringLiteral("第 %1 行的权值「%2」无效，请输入正整数。")
                    .arg(row + 1).arg(weightItem->text()));
            return;
        }

        chars.push_back(ch);
        weights.push_back(weight);
    }

    if (!huffman_.initialize(chars, weights))
    {
        QMessageBox::warning(this, QStringLiteral("建树失败"),
                             QString::fromStdString(huffman_.lastError()));
        return;
    }

    // 填充编码表。
    const std::map<char, std::string> table = huffman_.getCodeTable();
    codeTableWidget_->setRowCount(static_cast<int>(table.size()));
    int row = 0;
    for (std::map<char, std::string>::const_iterator it = table.begin();
         it != table.end(); ++it, ++row)
    {
        const QString display = (it->first == ' ')
                              ? QStringLiteral("[SPACE]")
                              : QString(QChar(it->first));

        QTableWidgetItem* charItem = new QTableWidgetItem(display);
        charItem->setTextAlignment(Qt::AlignCenter);
        codeTableWidget_->setItem(row, 0, charItem);

        QTableWidgetItem* codeItem = new QTableWidgetItem(
            QString::fromStdString(it->second));
        codeItem->setTextAlignment(Qt::AlignCenter);
        codeTableWidget_->setItem(row, 1, codeItem);
    }

    treePrintEdit_->setPlainText(QString::fromStdString(huffman_.getTreePrint()));
    huffmanStatusLabel_->setText(
        QStringLiteral("建树完成：共 %1 个不同字符。").arg(table.size()));
#endif
}

void MainWindow::onHuffmanEncode()
{
#ifndef CCS_HAS_HUFFMAN_HEADER
    return;
#else
    const QString text = huffmanInputEdit_->toPlainText();
    if (text.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("没有输入"),
                             QStringLiteral("请先输入待编码的文本。"));
        return;
    }

    const std::string code = huffman_.encode(text.toUtf8().toStdString());
    if (code.empty())
    {
        QMessageBox::warning(this, QStringLiteral("编码失败"),
            QString::fromStdString(huffman_.lastError()));
        return;
    }

    huffmanOutputEdit_->setPlainText(QString::fromStdString(code));

    // 顺带给出编码前后的位长对比，便于报告与演示。
    // 注意：Huffman 的优势体现在字符出现频率差异大、文本足够长时；
    // 短文本因编码表本身的开销，编码后未必比定长编码更短，属正常现象。
    const qulonglong originalBytes = static_cast<qulonglong>(text.toUtf8().size());
    const qulonglong originalBits  = originalBytes * 8u;
    const qulonglong encodedBits   = static_cast<qulonglong>(code.size());

    QString summary = QStringLiteral("编码完成：原文 %1 字节（%2 bit），编码后 %3 bit")
                          .arg(originalBytes).arg(originalBits).arg(encodedBits);
    if (originalBytes > 0)
    {
        summary += QStringLiteral("，平均每字符 %1 bit")
                       .arg(static_cast<double>(encodedBits)
                            / static_cast<double>(originalBytes), 0, 'f', 2);
    }
    huffmanStatusLabel_->setText(summary);
#endif
}

void MainWindow::onHuffmanDecode()
{
#ifndef CCS_HAS_HUFFMAN_HEADER
    return;
#else
    // 译码默认读取输入框内容；若其中不是 0/1 串，则回退到 data/CodeFile。
    QString codeText = huffmanInputEdit_->toPlainText().trimmed();
    if (codeText.isEmpty() || codeText.contains(QRegularExpression("[^01]")))
    {
        QFile file(QStringLiteral("data/CodeFile"));
        if (!file.exists() || !file.open(QIODevice::ReadOnly))
        {
            QMessageBox::warning(this, QStringLiteral("没有可译码的数据"),
                QStringLiteral("输入框中没有 0/1 编码串，且读取 data/CodeFile 失败。\n\n"
                               "请先执行「编码」，或点击「编码到 CodeFile」生成该文件。"));
            return;
        }
        codeText = QString::fromUtf8(file.readAll()).trimmed();
        file.close();
        huffmanStatusLabel_->setText(QStringLiteral("已从 data/CodeFile 读取编码。"));
    }

    if (codeText.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("没有输入"),
                             QStringLiteral("编码串为空，无法译码。"));
        return;
    }

    const std::string text = huffman_.decode(codeText.toUtf8().toStdString());
    if (text.empty())
    {
        QMessageBox::warning(this, QStringLiteral("译码失败"),
            QString::fromStdString(huffman_.lastError()));
        return;
    }

    const QString plain = QString::fromUtf8(text.c_str(),
                                            static_cast<int>(text.size()));
    huffmanOutputEdit_->setPlainText(plain);
    // 把译文回填到输入框，便于再次编码验证往返一致。
    huffmanInputEdit_->setPlainText(plain);
    huffmanStatusLabel_->setText(QStringLiteral("译码完成。"));
#endif
}

void MainWindow::onSaveTree()
{
#ifndef CCS_HAS_HUFFMAN_HEADER
    return;
#else
    const QString path = QStringLiteral("data/hfmTree");
    if (!huffman_.saveTree(path.toStdString()))
    {
        QMessageBox::warning(this, QStringLiteral("保存失败"),
            QString::fromStdString(huffman_.lastError()));
        return;
    }
    huffmanStatusLabel_->setText(
        QStringLiteral("Huffman 树已保存到 data/hfmTree。"));
#endif
}

void MainWindow::onLoadTree()
{
#ifndef CCS_HAS_HUFFMAN_HEADER
    return;
#else
    const QString path = QStringLiteral("data/hfmTree");
    if (!huffman_.loadTree(path.toStdString()))
    {
        QMessageBox::warning(this, QStringLiteral("加载失败"),
            QString::fromStdString(huffman_.lastError()));
        return;
    }

    const std::map<char, std::string> table = huffman_.getCodeTable();
    codeTableWidget_->setRowCount(static_cast<int>(table.size()));
    int row = 0;
    for (std::map<char, std::string>::const_iterator it = table.begin();
         it != table.end(); ++it, ++row)
    {
        const QString display = (it->first == ' ')
                              ? QStringLiteral("[SPACE]")
                              : QString(QChar(it->first));
        codeTableWidget_->setItem(row, 0, new QTableWidgetItem(display));
        codeTableWidget_->setItem(
            row, 1, new QTableWidgetItem(QString::fromStdString(it->second)));
    }
    treePrintEdit_->setPlainText(QString::fromStdString(huffman_.getTreePrint()));
    huffmanStatusLabel_->setText(
        QStringLiteral("已从 data/hfmTree 载入 Huffman 树，编码表已同步更新。"));
#endif
}

void MainWindow::onExportTreePrint()
{
#ifndef CCS_HAS_HUFFMAN_HEADER
    return;
#else
    const QString path = QStringLiteral("data/TreePrint");
    if (!huffman_.writeTreePrint(path.toStdString()))
    {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
            QString::fromStdString(huffman_.lastError()));
        return;
    }
    huffmanStatusLabel_->setText(
        QStringLiteral("树形输出已导出到 data/TreePrint。"));
#endif
}

void MainWindow::onReadTobeTran()
{
    const QString path = QStringLiteral("data/TobeTran");
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
    {
        QMessageBox::warning(this, QStringLiteral("读取失败"),
            QStringLiteral("无法读取 %1。\n\n"
                           "请确认程序的工作目录是项目根目录。").arg(path));
        return;
    }
    const QString content = QString::fromUtf8(file.readAll());
    file.close();

    huffmanInputEdit_->setPlainText(content);
    huffmanStatusLabel_->setText(
        QStringLiteral("已读取 data/TobeTran（%1 字节）。").arg(content.toUtf8().size()));
}

void MainWindow::onEncodeToCodeFile()
{
#ifndef CCS_HAS_HUFFMAN_HEADER
    return;
#else
    // 从文件编码（对应文档要求的 TobeTran -> CodeFile 流程）。
    if (!huffman_.encodeFile("data/TobeTran", "data/CodeFile"))
    {
        QMessageBox::warning(this, QStringLiteral("编码失败"),
            QString::fromStdString(huffman_.lastError()));
        return;
    }

    // 顺带把编码结果读出来显示，便于与源文件对照。
    QFile file(QStringLiteral("data/CodeFile"));
    if (file.open(QIODevice::ReadOnly))
    {
        const QString code = QString::fromUtf8(file.readAll()).trimmed();
        file.close();
        huffmanOutputEdit_->setPlainText(code);
        huffmanStatusLabel_->setText(
            QStringLiteral("已编码 data/TobeTran 并写入 data/CodeFile（%1 bit）。")
                .arg(code.size()));
    }
    else
    {
        huffmanStatusLabel_->setText(
            QStringLiteral("已编码 data/TobeTran 并写入 data/CodeFile。"));
    }
#endif
}

void MainWindow::onDecodeToTextFile()
{
#ifndef CCS_HAS_HUFFMAN_HEADER
    return;
#else
    if (!huffman_.decodeFile("data/CodeFile", "data/TextFile"))
    {
        QMessageBox::warning(this, QStringLiteral("译码失败"),
            QString::fromStdString(huffman_.lastError()));
        return;
    }

    QFile file(QStringLiteral("data/TextFile"));
    if (file.open(QIODevice::ReadOnly))
    {
        const QString text = QString::fromUtf8(file.readAll());
        file.close();
        huffmanInputEdit_->setPlainText(text);
        huffmanOutputEdit_->setPlainText(text);
    }
    huffmanStatusLabel_->setText(
        QStringLiteral("已译码 data/CodeFile 并写入 data/TextFile。"));
#endif
}

// ===========================================================================
//  安全通信页
// ===========================================================================

QWidget* MainWindow::buildSecurityPage()
{
    QWidget* page = new QWidget(this);

    cryptoAlgoCombo_ = new QComboBox(page);
    cryptoAlgoCombo_->addItem(QStringLiteral("AES-128-CBC"));
    cryptoAlgoCombo_->addItem(QStringLiteral("DES-CBC"));

    cryptoKeyEdit_ = new QLineEdit(page);
    cryptoKeyEdit_->setEchoMode(QLineEdit::Normal);
    cryptoKeyEdit_->setPlaceholderText(QStringLiteral("16 字节密钥"));

    QCheckBox* showKeyCheck = new QCheckBox(QStringLiteral("显示密钥"), page);

    cryptoHintLabel_ = new QLabel(page);
    cryptoHintLabel_->setStyleSheet(QStringLiteral("color: #666;"));

    QFormLayout* keyLayout = new QFormLayout();
    keyLayout->addRow(QStringLiteral("算法："), cryptoAlgoCombo_);
    keyLayout->addRow(QStringLiteral("密钥："), cryptoKeyEdit_);
    keyLayout->addRow(QString(), showKeyCheck);

    QGroupBox* keyBox = new QGroupBox(QStringLiteral("加解密参数"), page);
    QVBoxLayout* keyBoxLayout = new QVBoxLayout(keyBox);
    keyBoxLayout->addLayout(keyLayout);
    keyBoxLayout->addWidget(cryptoHintLabel_);

    cryptoInputEdit_ = new QPlainTextEdit(page);
    cryptoInputEdit_->setPlaceholderText(
        QStringLiteral("在此输入明文；解密时可直接粘贴上面的密文。"));

    cryptoOutputEdit_ = new QPlainTextEdit(page);
    cryptoOutputEdit_->setReadOnly(true);
    cryptoOutputEdit_->setPlaceholderText(
        QStringLiteral("加密结果为十六进制字符串，可直接复制到上方输入框进行解密。"));

    QPushButton* encryptButton = new QPushButton(QStringLiteral("加密"), page);
    QPushButton* decryptButton = new QPushButton(QStringLiteral("解密"), page);
    QPushButton* clearButton   = new QPushButton(QStringLiteral("清空"), page);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(encryptButton);
    buttonLayout->addWidget(decryptButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(clearButton);

    QGroupBox* inputBox = new QGroupBox(QStringLiteral("明文 / 密文输入"), page);
    QVBoxLayout* inputLayout = new QVBoxLayout(inputBox);
    inputLayout->addWidget(cryptoInputEdit_);

    QGroupBox* outputBox = new QGroupBox(QStringLiteral("输出结果"), page);
    QVBoxLayout* outputLayout = new QVBoxLayout(outputBox);
    outputLayout->addWidget(cryptoOutputEdit_);

    QVBoxLayout* pageLayout = new QVBoxLayout(page);
    pageLayout->addWidget(keyBox);
    pageLayout->addWidget(inputBox, 1);
    pageLayout->addLayout(buttonLayout);
    pageLayout->addWidget(outputBox, 2);

    // ---- 信号连接 ----
    connect(encryptButton, &QPushButton::clicked, this, &MainWindow::onEncrypt);
    connect(decryptButton, &QPushButton::clicked, this, &MainWindow::onDecrypt);
    connect(clearButton,   &QPushButton::clicked, this, &MainWindow::onClearCrypto);
    connect(cryptoAlgoCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onAlgorithmChanged);
    connect(showKeyCheck, &QCheckBox::toggled, this, [this](bool checked) {
        cryptoKeyEdit_->setEchoMode(checked ? QLineEdit::Normal
                                            : QLineEdit::Password);
    });

    // 默认填入一个符合长度要求的演示密钥。
    cryptoAlgoCombo_->setCurrentIndex(0);
    cryptoKeyEdit_->setText(QStringLiteral("1234567890123456"));
    cryptoKeyEdit_->setEchoMode(QLineEdit::Password);
    refreshCryptoHint();

    return page;
}

void MainWindow::onAlgorithmChanged()
{
    const bool isAes = (cryptoAlgoCombo_->currentIndex() == 0);
    const int required = static_cast<int>(isAes ? CryptoSystem::AES_KEY_BYTES
                                                : CryptoSystem::DES_KEY_BYTES);

    // 仅当现有密钥长度对不上新算法时，才替换为等长的默认演示密钥，
    // 避免用户已经敲好的内容被无端清掉。
    if (cryptoKeyEdit_->text().toUtf8().size() != required)
    {
        cryptoKeyEdit_->setText(isAes ? QStringLiteral("1234567890123456")
                                      : QStringLiteral("12345678"));
    }

    refreshCryptoHint();
}

void MainWindow::refreshCryptoHint()
{
    const bool isAes = (cryptoAlgoCombo_->currentIndex() == 0);
    const int required = static_cast<int>(isAes ? CryptoSystem::AES_KEY_BYTES
                                                : CryptoSystem::DES_KEY_BYTES);
    cryptoHintLabel_->setText(
        QStringLiteral("当前算法要求密钥长度为 %1 字节（当前 %2 字节）。\n"
                       "本模块采用固定 IV 与 PKCS#7 填充；"
                       "固定 IV 仅用于课程实验演示，真实系统应使用随机且不可重复的 IV。")
            .arg(required)
            .arg(cryptoKeyEdit_->text().toUtf8().size()));
}

void MainWindow::onEncrypt()
{
    const bool isAes = (cryptoAlgoCombo_->currentIndex() == 0);
    const std::string key = cryptoKeyEdit_->text().toUtf8().toStdString();
    const std::string plain = cryptoInputEdit_->toPlainText().toUtf8().toStdString();

    const CryptoResult result = isAes ? crypto_.encryptAES(plain, key)
                                      : crypto_.encryptDES(plain, key);

    if (!result.success)
    {
        QMessageBox::warning(this, QStringLiteral("加密失败"),
                             QString::fromStdString(result.errorMessage));
        cryptoOutputEdit_->setPlainText(
            QStringLiteral("加密失败：%1").arg(
                QString::fromStdString(result.errorMessage)));
        return;
    }

    const QString cipherHex = QString::fromStdString(result.output);

    // 密文同时显示在输入框，便于直接点击「解密」完成往返演示。
    cryptoInputEdit_->setPlainText(cipherHex);
    cryptoOutputEdit_->setPlainText(
        QStringLiteral("算法：%1\n"
                       "明文：%2\n"
                       "密文（Hex）：\n%3\n\n"
                       "加密耗时：%4")
            .arg(cryptoAlgoCombo_->currentText(),
                 brief(QString::fromUtf8(plain.c_str())))
            .arg(cipherHex)
            .arg(formatMs(result.runningTimeMs)));
}

void MainWindow::onDecrypt()
{
    const bool isAes = (cryptoAlgoCombo_->currentIndex() == 0);
    const std::string key = cryptoKeyEdit_->text().toUtf8().toStdString();
    const std::string cipherHex = cryptoInputEdit_->toPlainText()
                                      .trimmed().toUtf8().toStdString();

    const CryptoResult result = isAes ? crypto_.decryptAES(cipherHex, key)
                                      : crypto_.decryptDES(cipherHex, key);

    if (!result.success)
    {
        QMessageBox::warning(this, QStringLiteral("解密失败"),
                             QString::fromStdString(result.errorMessage));
        cryptoOutputEdit_->setPlainText(
            QStringLiteral("解密失败：%1").arg(
                QString::fromStdString(result.errorMessage)));
        return;
    }

    const QString plain = QString::fromUtf8(result.output.c_str(),
                                            static_cast<int>(result.output.size()));

    cryptoOutputEdit_->setPlainText(
        QStringLiteral("算法：%1\n"
                       "密文（Hex）：\n%2\n\n"
                       "明文：\n%3\n\n"
                       "解密耗时：%4")
            .arg(cryptoAlgoCombo_->currentText())
            .arg(brief(QString::fromStdString(cipherHex), 96))
            .arg(plain)
            .arg(formatMs(result.runningTimeMs)));
}

void MainWindow::onClearCrypto()
{
    cryptoInputEdit_->clear();
    cryptoOutputEdit_->clear();
    refreshCryptoHint();
}

// ===========================================================================
//  Huffman 页（开发者 B 的模块就绪后接入）
// ===========================================================================

QWidget* MainWindow::buildHuffmanPage()
{
    QWidget* page = new QWidget(this);

    huffmanStatusLabel_ = new QLabel(page);
    huffmanStatusLabel_->setWordWrap(true);

    // ---- 左侧：字符集与权值 ----
    charsetTable_ = new QTableWidget(page);
    charsetTable_->setColumnCount(2);
    charsetTable_->setHorizontalHeaderLabels(
        QStringList() << QStringLiteral("字符") << QStringLiteral("权值"));
    charsetTable_->horizontalHeader()->setStretchLastSection(true);
    charsetTable_->setSelectionBehavior(QAbstractItemView::SelectItems);

    QLabel* charsetHint = new QLabel(
        QStringLiteral("第一列填单个字符（空格直接填一个空格），"
                       "第二列填该字符的出现次数。字符不可重复，权值须为正整数。"),
        page);
    charsetHint->setWordWrap(true);
    charsetHint->setStyleSheet(QStringLiteral("color: #666;"));

    QPushButton* buildButton  = new QPushButton(QStringLiteral("建树"), page);
    QPushButton* sampleButton = new QPushButton(QStringLiteral("载入示例"), page);
    QPushButton* clearButton  = new QPushButton(QStringLiteral("清空"), page);

    QHBoxLayout* buttonRow = new QHBoxLayout();
    buttonRow->addWidget(buildButton);
    buttonRow->addWidget(sampleButton);
    buttonRow->addWidget(clearButton);

    QGroupBox* charsetBox = new QGroupBox(QStringLiteral("字符集与权值"), page);
    QVBoxLayout* charsetLayout = new QVBoxLayout(charsetBox);
    charsetLayout->addWidget(charsetTable_, 1);
    charsetLayout->addWidget(charsetHint);
    charsetLayout->addLayout(buttonRow);

    // ---- 右侧：编码表 ----
    codeTableWidget_ = new QTableWidget(page);
    codeTableWidget_->setColumnCount(2);
    codeTableWidget_->setHorizontalHeaderLabels(
        QStringList() << QStringLiteral("字符") << QStringLiteral("编码"));
    codeTableWidget_->horizontalHeader()->setStretchLastSection(true);
    codeTableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    QGroupBox* codeBox = new QGroupBox(QStringLiteral("编码表"), page);
    QVBoxLayout* codeLayout = new QVBoxLayout(codeBox);
    codeLayout->addWidget(codeTableWidget_);

    // ---- 右侧：树形输出 ----
    treePrintEdit_ = new QPlainTextEdit(page);
    treePrintEdit_->setReadOnly(true);
    QFont monoFont(QStringLiteral("Consolas"));
    monoFont.setStyleHint(QFont::Monospace);
    treePrintEdit_->setFont(monoFont);
    treePrintEdit_->setPlaceholderText(
        QStringLiteral("建树后此处显示 Huffman 树的直观形式。\n"
                       "* 表示内部节点，括号内为权值，[SPACE] 表示空格字符。"));

    QGroupBox* treeBox = new QGroupBox(QStringLiteral("Huffman 树"), page);
    QVBoxLayout* treeLayout = new QVBoxLayout(treeBox);
    treeLayout->addWidget(treePrintEdit_);

    QSplitter* rightSplitter = new QSplitter(Qt::Vertical, page);
    rightSplitter->addWidget(codeBox);
    rightSplitter->addWidget(treeBox);
    rightSplitter->setStretchFactor(0, 2);
    rightSplitter->setStretchFactor(1, 3);

    QSplitter* topSplitter = new QSplitter(Qt::Horizontal, page);
    topSplitter->addWidget(charsetBox);
    topSplitter->addWidget(rightSplitter);
    topSplitter->setStretchFactor(0, 2);
    topSplitter->setStretchFactor(1, 3);

    // ---- 下半：编码 / 译码 ----
    huffmanInputEdit_ = new QPlainTextEdit(page);
    huffmanInputEdit_->setPlaceholderText(
        QStringLiteral("在此输入待编码的文本；译码时粘贴由 0 和 1 组成的编码串。"));

    huffmanOutputEdit_ = new QPlainTextEdit(page);
    huffmanOutputEdit_->setReadOnly(true);
    huffmanOutputEdit_->setPlaceholderText(QStringLiteral("编码或译码的结果显示在此处。"));

    QPushButton* encodeButton = new QPushButton(QStringLiteral("编码"), page);
    QPushButton* decodeButton = new QPushButton(QStringLiteral("译码"), page);

    QHBoxLayout* codecButtons = new QHBoxLayout();
    codecButtons->addWidget(encodeButton);
    codecButtons->addWidget(decodeButton);
    codecButtons->addStretch();

    QGroupBox* inputBox = new QGroupBox(QStringLiteral("输入"), page);
    QVBoxLayout* inputLayout = new QVBoxLayout(inputBox);
    inputLayout->addWidget(huffmanInputEdit_, 1);
    inputLayout->addLayout(codecButtons);

    QGroupBox* outputBox = new QGroupBox(QStringLiteral("输出"), page);
    QVBoxLayout* outputLayout = new QVBoxLayout(outputBox);
    outputLayout->addWidget(huffmanOutputEdit_);

    QSplitter* codecSplitter = new QSplitter(Qt::Horizontal, page);
    codecSplitter->addWidget(inputBox);
    codecSplitter->addWidget(outputBox);

    // ---- 文件操作 ----
    QPushButton* readTobeButton   = new QPushButton(QStringLiteral("读取 TobeTran"), page);
    QPushButton* encodeFileButton = new QPushButton(QStringLiteral("编码到 CodeFile"), page);
    QPushButton* decodeFileButton = new QPushButton(QStringLiteral("译码到 TextFile"), page);
    QPushButton* saveTreeButton   = new QPushButton(QStringLiteral("保存 hfmTree"), page);
    QPushButton* loadTreeButton   = new QPushButton(QStringLiteral("加载 hfmTree"), page);
    QPushButton* exportTreeButton = new QPushButton(QStringLiteral("导出 TreePrint"), page);

    QHBoxLayout* fileButtons = new QHBoxLayout();
    fileButtons->addWidget(readTobeButton);
    fileButtons->addWidget(encodeFileButton);
    fileButtons->addWidget(decodeFileButton);
    fileButtons->addSpacing(18);
    fileButtons->addWidget(saveTreeButton);
    fileButtons->addWidget(loadTreeButton);
    fileButtons->addWidget(exportTreeButton);
    fileButtons->addStretch();

    QGroupBox* fileBox = new QGroupBox(
        QStringLiteral("文件操作（均针对项目根目录下的 data/ 文件夹）"), page);
    QVBoxLayout* fileLayout = new QVBoxLayout(fileBox);
    fileLayout->addLayout(fileButtons);

    // ---- 组装 ----
    QSplitter* mainSplitter = new QSplitter(Qt::Vertical, page);
    mainSplitter->addWidget(topSplitter);
    mainSplitter->addWidget(codecSplitter);
    mainSplitter->setStretchFactor(0, 3);
    mainSplitter->setStretchFactor(1, 2);

    QVBoxLayout* pageLayout = new QVBoxLayout(page);
    pageLayout->addWidget(huffmanStatusLabel_);
    pageLayout->addWidget(mainSplitter, 1);
    pageLayout->addWidget(fileBox);

    // ---- 信号连接 ----
    connect(buildButton,  &QPushButton::clicked, this, &MainWindow::onBuildHuffmanTree);
    connect(sampleButton, &QPushButton::clicked, this, &MainWindow::onLoadCharsetSample);
    connect(clearButton,  &QPushButton::clicked, this, [this]() {
        charsetTable_->setRowCount(0);
        codeTableWidget_->setRowCount(0);
        treePrintEdit_->clear();
        huffmanOutputEdit_->clear();
#ifndef CCS_HAS_HUFFMAN_HEADER
        return;
#else
        huffman_.clear();
        huffmanStatusLabel_->setText(QStringLiteral("已清空。"));
#endif
    });
    connect(encodeButton, &QPushButton::clicked, this, &MainWindow::onHuffmanEncode);
    connect(decodeButton, &QPushButton::clicked, this, &MainWindow::onHuffmanDecode);
    connect(readTobeButton,   &QPushButton::clicked, this, &MainWindow::onReadTobeTran);
    connect(encodeFileButton, &QPushButton::clicked, this, &MainWindow::onEncodeToCodeFile);
    connect(decodeFileButton, &QPushButton::clicked, this, &MainWindow::onDecodeToTextFile);
    connect(saveTreeButton,   &QPushButton::clicked, this, &MainWindow::onSaveTree);
    connect(loadTreeButton,   &QPushButton::clicked, this, &MainWindow::onLoadTree);
    connect(exportTreeButton, &QPushButton::clicked, this, &MainWindow::onExportTreePrint);

#ifndef CCS_HAS_HUFFMAN_HEADER
    huffmanStatusLabel_->setText(
        QStringLiteral("开发者 B 的 HuffmanSystem 尚未合入，本页面暂不可用。"));
    charsetTable_->setEnabled(false);
    buildButton->setEnabled(false);
    sampleButton->setEnabled(false);
    encodeButton->setEnabled(false);
    decodeButton->setEnabled(false);
    fileBox->setEnabled(false);
#else
    // 默认载入一组示例字符集，便于直接演示「建树 → 编码 → 译码」。
    onLoadCharsetSample();
#endif

    return page;
}

// ===========================================================================
//  性能比较页
// ===========================================================================

QWidget* MainWindow::buildPerformancePage()
{
    QWidget* page = new QWidget(this);

    QLabel* title = new QLabel(QStringLiteral("算法性能比较"), page);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    title->setFont(titleFont);

    QLabel* body = new QLabel(
        QStringLiteral(
            "本页面将汇总以下运行时间，便于横向比较：\n\n"
            "  · 最小生成树：Prim 与 Kruskal 在不同城市规模下的耗时\n"
            "  · 对称加密：  AES-128 与 DES 对相同数据的加解密耗时\n\n"
            "各算法的耗时数据已由对应模块随结果一并返回，"
            "本页面属于可选的扩展功能，将在基础功能稳定后再行完善。"), page);
    body->setWordWrap(true);

    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->addWidget(title);
    layout->addWidget(body);
    layout->addStretch();

    return page;
}
