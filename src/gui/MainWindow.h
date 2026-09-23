/**
 * @file    MainWindow.h
 * @brief   城市通信网络设计系统 —— 主窗口。
 *
 * 对应《共同开发规则和接口约定》第 13 节与《开发者 C 工作说明》第 13 节：
 * 采用 QMainWindow + QTabWidget，共四个功能页面：
 *   1. 网络设计   —— 调用开发者 A 的 MSTService
 *   2. Huffman    —— 调用开发者 B 的 HuffmanSystem
 *   3. 安全通信   —— 调用本模块的 CryptoSystem
 *   4. 性能比较   —— 汇总各算法的运行时间
 *
 * 界面层职责仅限于「输入 → 调用接口 → 显示结果」，
 * 严禁把任何算法实现写进本文件（文档第 13 节）。
 *
 * 关于跨模块依赖：
 *   开发者 A 的 mst 与开发者 B 的 huffman 模块可能尚未合入 main。
 *   此处使用 __has_include 做条件包含，使主窗口在其缺位时仍可编译运行，
 *   对应页面会自动降级为"模块未就绪"的提示状态，不影响其余功能演示。
 *   待两个模块就绪后，本文件的逻辑无需改动即可自动启用。
 */

#pragma once

#include <QMainWindow>
#include <QString>

#include "../crypto/CryptoSystem.h"

// 条件包含：模块就绪时启用真实类型，否则退化为编译期占位。
#if defined(__has_include)
#  if __has_include("../mst/MSTService.h")
#    include "../mst/MSTService.h"
#    define CCS_HAS_MST_HEADER 1
#  endif
#  if __has_include("../huffman/HuffmanSystem.h")
#    include "../huffman/HuffmanSystem.h"
#    define CCS_HAS_HUFFMAN_HEADER 1
#  endif
#endif

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTableWidget;

class NetworkView;

/**
 * @brief 程序主窗口，承载四个功能页面。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    // ---- 网络设计页 ----
    void onCityCountChanged(int count);
    void onCostItemChanged(int row, int column);
    void onGenerateNetwork();
    void onClearNetwork();
    void onLoadSample();

    // ---- 安全通信页 ----
    void onEncrypt();
    void onDecrypt();
    void onClearCrypto();
    void onAlgorithmChanged();

private:
    QWidget* buildNetworkPage();
    QWidget* buildHuffmanPage();
    QWidget* buildSecurityPage();
    QWidget* buildPerformancePage();

    /// 构造造价矩阵表格，行列数随城市数变化，对角线置 0 且不可编辑。
    void rebuildCostTable(int cityCount);

    /// 读取造价矩阵并做合法性校验，失败时返回 false 并弹窗说明。
    /// 因校验失败需要以本窗口为父窗口弹窗，故不能声明为 const。
    bool collectCostMatrix(int cityCount,
                           std::vector<std::vector<int> >& costs);

    /// 统一展示 MST 计算结果。
    void showMstResult(const QString& algorithmName,
                       bool success,
                       const QString& errorMessage,
                       const QStringList& edgeTexts,
                       long long totalCost,
                       double runningTimeMs);

    /// 更新安全通信页的密钥长度提示。
    void refreshCryptoHint();

    // ---- 界面控件 ----
    QTabWidget* tabs_ = nullptr;

    // 网络设计页
    QSpinBox*       cityCountSpin_   = nullptr;
    QComboBox*      algorithmCombo_  = nullptr;
    QTableWidget*   costTable_       = nullptr;
    NetworkView*    networkView_     = nullptr;
    QPlainTextEdit* mstResultEdit_   = nullptr;
    QLabel*         mstSummaryLabel_ = nullptr;

    // 安全通信页
    QComboBox*      cryptoAlgoCombo_  = nullptr;
    QLineEdit*      cryptoKeyEdit_    = nullptr;
    QPlainTextEdit* cryptoInputEdit_  = nullptr;
    QPlainTextEdit* cryptoOutputEdit_ = nullptr;
    QLabel*         cryptoHintLabel_  = nullptr;

#ifdef CCS_HAS_MST_HEADER
    MSTService mst_;
#endif

#ifdef CCS_HAS_HUFFMAN_HEADER
    HuffmanSystem huffman_;
#endif

    /// 加解密服务，接口全部为 const，可安全复用同一实例。
    CryptoSystem crypto_;

    /// 当前表格中的城市数量。
    int currentCityCount_ = 0;
};
