# 界面端到端冒烟测试

## 这个测试解决什么问题

`tests/` 下的三个测试（`test_mst` / `test_huffman` / `test_crypto`）都是**模块级**的：
它们直接调用算法接口，验证计算结果。但"模块算得对"不等于"界面连得上"——
按钮绑错槽函数、控件取错、结果显示到别处，模块测试全都发现不了。

本测试构造真实的 `MainWindow`，用 `QTimer` 触发**真实的按钮点击**，
再读取界面上的实际文本做断言。全程走的是与人工操作完全相同的代码路径。

## 构建与运行

需要在项目根目录执行（测试读写 `data/` 下的相对路径）：

```powershell
$env:PATH = "D:\Qt\Tools\mingw1310_64\bin;D:\Qt\6.9.3\mingw_64\bin;$env:PATH"
$QTI = "D:/Qt/6.9.3/mingw_64/include"

# 生成元对象代码（本测试不经 CMake，需手工跑 moc）
New-Item -ItemType Directory -Force build/manual | Out-Null
moc src/gui/MainWindow.h  -o build/manual/moc_MainWindow.cpp  -I src -I $QTI -I $QTI/QtCore -I $QTI/QtGui -I $QTI/QtWidgets
moc src/gui/NetworkView.h -o build/manual/moc_NetworkView.cpp -I src -I $QTI -I $QTI/QtCore -I $QTI/QtGui -I $QTI/QtWidgets

# 编译
g++ -std=c++17 -O1 -o build/manual/gui_smoke_test.exe `
    tests/manual/gui_smoke_test.cpp `
    build/manual/moc_MainWindow.cpp build/manual/moc_NetworkView.cpp `
    src/gui/MainWindow.cpp src/gui/NetworkView.cpp `
    src/crypto/CryptoSystem.cpp src/mst/MSTService.cpp src/mst/Graph.cpp `
    src/huffman/HuffmanSystem.cpp `
    -I src -I $QTI -I $QTI/QtCore -I $QTI/QtGui -I $QTI/QtWidgets `
    -L "D:/Qt/6.9.3/mingw_64/lib" -lQt6Widgets -lQt6Gui -lQt6Core

# 运行
.\build\manual\gui_smoke_test.exe
```

返回 0 表示全部通过。当前共 **38 项断言**，覆盖：

| 页面 | 验证内容 |
|---|---|
| 网络设计 | 示例数据载入、造价矩阵对称同步、Prim 与 Kruskal 各自算出总造价 50 |
| Huffman | 自动建树、编码得到 48 位 0/1 串、译码还原、三个文件按钮的完整流程 |
| 安全通信 | AES/DES 往返、密文格式与长度、切换算法时密钥长度自动调整、耗时显示 |
| 性能比较 | 加密比较生成 10 行结果、1 MiB 的 DES 往返校验通过 |

## 为什么没有覆盖错误输入

三个模块的界面在出错时都会弹 `QMessageBox::warning`，这是**模态对话框**，
在无人点击的情况下会永久阻塞自动化测试（本测试开发时正是这样卡死过两次）。

因此本测试只走成功路径。"错误输入不崩溃"这一要求由两侧共同保证：

- **模块侧**：`tests/test_crypto.cpp` 等已覆盖密钥长度错误、非法 Hex、
  非法字符、数据不足等异常输入，均返回失败且不崩溃；
- **界面侧**：错误处理是统一的 `if (!result.success) { 弹窗(result.errorMessage); }`，
  逻辑简单，属人工验收范围。

若要人工验证错误提示，直接运行主程序并故意触发即可，例如：

以下文案均已对照模块实际返回值核实（见各模块的 `lastError()` / `errorMessage`）：

| 场景 | 操作 | 实际弹窗文案 |
|---|---|---|
| AES 密钥长度错误 | 安全通信页把密钥改成 15 个字符后点「加密」 | `AES key must be 16 bytes` |
| DES 密钥长度错误 | 切到 DES 后把密钥改成 7 个字符 | `DES key must be 8 bytes` |
| Huffman 未初始化 | 清空字符集后直接点「编码」 | `encode failed: Huffman tree is not initialized` |
| 编码含字符集外字符 | 输入 `Z`（示例字符集中没有）后点「编码」 | `encode failed: character 'Z' is not in the Huffman code table` |
| 图不连通 | 造价矩阵全部填 INF 后点「生成网络」 | `Graph is disconnected` |
| 密文非法 | 安全通信页输入 `ZZZZ` 后点「解密」 | `cipher text is not a valid hex string` |

另有一个不算错误但值得知道的边界：城市数为 1 时点「生成网络」不会报错，
而是正常返回 0 条边、总造价 0 —— 单座城市本就不需要铺设任何链路。

## 说明

本测试**不纳入 `ctest`**，需手工构建运行。原因是它依赖图形环境，
在无显示器的 CI 或远程会话中无法运行；而三个模块测试已经覆盖了算法正确性，
本测试的价值在于集成验收，手工执行一次即可。
