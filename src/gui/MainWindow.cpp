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
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QSignalBlocker>

#include <cstdint>
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

    QLabel* title = new QLabel(QStringLiteral("Huffman 编码与译码"), page);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    title->setFont(titleFont);

#ifdef CCS_HAS_HUFFMAN_HEADER
    QLabel* body = new QLabel(
        QStringLiteral("Huffman 模块已检测到，界面接入中。"), page);
#else
    QLabel* body = new QLabel(
        QStringLiteral(
            "本页面用于 Huffman 建树、编码、译码与树形输出。\n\n"
            "当前状态：开发者 B 的 HuffmanSystem 尚未合入 main 分支。\n"
            "模块到位后，本页面会自动启用，无需改动其他代码。"), page);
#endif
    body->setWordWrap(true);

    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->addWidget(title);
    layout->addWidget(body);
    layout->addStretch();

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
