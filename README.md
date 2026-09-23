# 城市通信网络设计系统 CityCommunicationSystem

课程实验项目：以最小生成树设计城市间通信网络造价，用 Huffman 编码压缩传输数据，
并以 AES/DES 保障通信安全，全部功能集成在一个 Qt 桌面程序中。

---

## 一、功能概览

| 模块 | 内容 | 负责人 |
|---|---|---|
| 网络设计 | Prim / Kruskal 求最小生成树，计算最低总造价，网络图形化显示 | 开发者 A |
| Huffman | 建树、编码、译码、文件读写、树形输出 | 开发者 B |
| 安全通信 | AES-128-CBC / DES-CBC 加解密 | 开发者 C |
| 性能比较 | 各算法的运行时间横向比较（可选扩展） | 开发者 C |

---

## 二、环境依赖

| 组件 | 版本要求 | 本项目验证环境 |
|---|---|---|
| Qt | 6.2 及以上 | Qt 6.9.3（mingw_64） |
| 编译器 | 须与 Qt 预编译版 ABI 一致 | MinGW-W64 13.1.0（Qt 自带 `Tools/mingw1310_64`） |
| CMake | 3.16 及以上 | 4.1.2 |
| 构建工具 | 任一 | Ninja |

> **重要**：Qt 的 MinGW 预编译版使用 **UCRT** 运行时。若使用 Strawberry Perl 等
> 发行版自带的 MinGW（MSVCRT 运行时）链接，会出现
> `undefined reference to __imp___argc` 之类的错误。
> **请务必使用 Qt 安装目录下的 `Tools/mingw1310_64`。**

第三方加密库（OpenSSL / Crypto++）**不需要安装**：
Crypto 模块为纯 C++ 实现，无外部依赖。

---

## 三、构建与运行

以 Windows + PowerShell 为例，先确认 `g++` 来自 Qt 自带的 MinGW：

```powershell
$env:PATH = "D:\Qt\Tools\mingw1310_64\bin;D:\Qt\6.9.3\mingw_64\bin;$env:PATH"

cmake -S . -B build -G "Ninja" `
      -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_PREFIX_PATH="D:/Qt/6.9.3/mingw_64" `
      -DCMAKE_CXX_COMPILER="D:/Qt/Tools/mingw1310_64/bin/g++.exe"

cmake --build build
```

构建产物位于 `build/bin/`：

```text
build/bin/CityCommunicationSystem.exe     主程序
build/bin/test_crypto.exe                 Crypto 模块验收测试
```

### 部署运行时依赖

首次运行前需要把 Qt 的 DLL 复制到可执行文件旁，否则程序会直接退出（退出码 127）：

```powershell
D:\Qt\6.9.3\mingw_64\bin\windeployqt.exe --release build\bin\CityCommunicationSystem.exe
```

### 运行测试

```powershell
cd build
ctest --output-on-failure
```

或直接执行 `build\bin\test_crypto.exe`，返回 0 表示全部通过。

---

## 四、目录结构

```text
CityCommunicationSystem/
├── src/
│   ├── common/Types.h          公共数据结构（三人共用）
│   ├── mst/                    开发者 A：Prim / Kruskal
│   ├── huffman/                开发者 B：Huffman 编码
│   ├── crypto/                 开发者 C：AES / DES
│   │   ├── CryptoSystem.h
│   │   └── CryptoSystem.cpp
│   ├── gui/                    开发者 C：Qt 界面
│   │   ├── MainWindow.h/.cpp
│   │   └── NetworkView.h/.cpp
│   └── main.cpp
├── data/
│   ├── graph1.txt              题目图 1 的邻接矩阵（MST 测试输入）
│   ├── TobeTran                待传输文本，内容为 I AM FROM CHINA（Huffman 输入）
│   └── hfmTree / CodeFile / TextFile / TreePrint
│                               Huffman 运行产物，由测试或界面按钮生成
├── tests/
│   ├── test_mst.cpp
│   ├── test_huffman.cpp
│   └── test_crypto.cpp
├── docs/                       开发规范与分工说明
├── CMakeLists.txt
└── README.md
```

> `data/` 下只有 `graph1.txt` 与 `TobeTran` 是随仓库提供的输入文件，
> Huffman 的四个产物已列入 `.gitignore`——它们每次运行都会重新生成，
> 提交进仓库只会在每次运行时产生无意义的差异。
> 执行 `ctest` 或使用界面上的文件按钮即可生成它们。

---

## 五、使用说明

### 网络设计页

1. 设置城市数量（1 ~ 100），造价矩阵会同步调整为 n×n；
2. 双击单元格填写造价；不直连的两地填 `INF`。
   矩阵关于对角线对称，编辑 `[i][j]` 会自动同步 `[j][i]`；对角线固定为 0。
3. 选择 `Prim 算法` 或 `Kruskal 算法`，点击「生成网络」；
4. 右侧显示最小生成树的边、最低总造价与运行时间，
   网络图中 **蓝色加粗** 的即为最小生成树的链路，浅灰虚线为全部可选链路。

「加载示例」可一键填入一组 5 城市的演示数据；「清空」恢复初始状态。

### Huffman 页

1. 在「字符集与权值」表格中填写字符及其出现次数（每行一个 ASCII 字符，
   不可重复，权值须为正整数）；点击「载入示例」可一键填入
   `I AM FROM CHINA` 的字符统计；
2. 点击「建树」，右侧同步显示**编码表**与 **Huffman 树**的直观形式
   （`*` 为内部节点，括号内为权值，`[SPACE]` 表示空格）；
3. 在下方输入文本点击「编码」得到 0/1 编码串；把编码串放回输入框
   点击「译码」即可还原原文。译码时若输入框内不是 0/1 串，
   会自动改从 `data/CodeFile` 读取；
4. 文件操作按钮对应文档第 20 节要求的完整流程：

   ```text
   读取 TobeTran  →  编码到 CodeFile  →  译码到 TextFile
   保存 hfmTree   ←→  加载 hfmTree
   导出 TreePrint
   ```

> 说明：Huffman 的压缩优势体现在字符频率差异大且文本足够长时。
> 对 `I AM FROM CHINA` 这类短文本，编码后为 48 bit（平均 3.2 bit/字符），
> 虽然优于 8 bit 的定长 ASCII，但增益有限，属正常现象。

### 安全通信页

1. 选择算法：`AES-128-CBC` 或 `DES-CBC`；
2. 输入密钥 —— AES 须 **16 字节**，DES 须 **8 字节**，
   长度不符会弹窗提示，程序不会自动截断或补零；
3. 输入明文，点击「加密」，密文以 **十六进制字符串** 显示，
   并自动填回输入框，可直接点击「解密」完成往返演示；
4. 输出区同时给出加密与解密的耗时（毫秒）。

---

## 六、开发约定摘要

完整规则见 [`docs/共同开发规则和接口.md`](docs/共同开发规则和接口.md)。

- **分支**：`main` 为集成分支，各人在 `dev-mst` / `dev-huffman` / `dev-gui-crypto` 上开发；
  禁止直接在 `main` 上开发，禁止提交编译失败的代码。
- **提交信息**：使用 `feat:` / `fix:` / `refactor:` / `test:` / `docs:` / `ui:` / `chore:` 前缀。
- **公共接口**：`src/common/Types.h` 与各模块头文件中的声明不得私自改动，
  确需变更须先与另外两名成员确认。
- **算法模块禁止依赖 Qt**：不得 `#include <QMessageBox>` 等界面头文件，
  错误一律通过返回值传递，由 GUI 决定如何显示。
- **测试代码**：一律放在 `tests/`，不得写入 `src/main.cpp`。
- **CMake**：由开发者 C 统一维护；A、B 新增 `src/mst/*.cpp`、`src/huffman/*.cpp`
  后会被自动纳入构建，无需改动 `CMakeLists.txt`。

### 模块就绪状态

主窗口使用 `__has_include` 探测 `MSTService.h` 与 `HuffmanSystem.h`，
按探测结果决定对应页面是否启用：

| 模块 | 状态 | 说明 |
|---|---|---|
| 网络设计（MST） | ✅ 已就绪 | 开发者 A 的 `src/mst/` 已合入 |
| Huffman | ✅ 已就绪 | 开发者 B 的 `src/huffman/` 已合入 |
| 安全通信（Crypto） | ✅ 已就绪 | 开发者 C |
| 性能比较 | ⏳ 待完善 | 可选扩展 |

三个模块均已接入，`ctest` 共三项验收测试，全部通过：

```text
crypto_acceptance    34 项断言  含 FIPS-197 官方向量比对
mst_acceptance       70 项断言  含独立参考实现交叉验证
huffman_acceptance   60 项断言  含 TobeTran -> CodeFile -> TextFile 文件流程
```

模块**缺位时**主程序照常编译运行，对应页面提示"模块未就绪"，其余功能不受影响；
模块**就位后**只需重新执行一次 CMake 配置即可自动启用，无需改动界面代码。

### 集成进度

三模块集成成果位于 `dev-gui-crypto` 分支，已提交 PR #1 等待评审：

```text
https://github.com/jzzmi735/CityCommunicationSystem/pull/1
```

按《共同开发规则和接口约定》第 4.2 节，需至少一名成员检查后再合并到 `main`。
`main` 目前仍是初始状态，尚未包含任何模块代码。

### 图数据文件

`data/graph1.txt` 为题目图 1 的邻接矩阵（8 个城市，`INF` 表示不直连），
由 MST 验收测试作为输入：

```powershell
build\bin\test_mst.exe data\graph1.txt
```

---

## 七、加密模块实现说明

| 项目 | 说明 |
|---|---|
| 算法 | AES-128-CBC、DES-CBC |
| 填充 | PKCS#7 |
| 密钥 | 直接使用调用方给出的字节，长度严格校验（AES 16 / DES 8），不截断、不补零 |
| 密文格式 | 大写十六进制字符串，界面层不接触二进制 |
| IV | 固定 IV，加密解密一致。**仅用于课程实验演示**，真实系统应使用随机且不可重复的 IV |
| 计时 | `std::chrono::steady_clock`，单位毫秒 |
| 依赖 | 无（纯 C++ 实现，不依赖 OpenSSL / Crypto++） |

### 正确性验证

`tests/test_crypto.cpp` 共 34 项断言，其中：

- **AES-128 用 FIPS-197 附录 B 的官方测试向量逐字节比对**
  （`key = 000102…0F`、`plaintext = 001122…FF`、
  `ciphertext = 69C4E0D86A7B0430D8CDB78070B4C55A`）。
  本实现为 CBC，但单分组且 IV 全零时首块与 ECB 等价，故可直接比对。
  该断言用于证明 AES 实现本身正确，而非仅仅"自己能往返"；
- 文档要求的两条消息 `HELLO CHINA` 与
  `CITY C SENDS DATA TO CITY D` × AES/DES 共 4 组完整往返；
- 异常输入：密钥长度错误、密文非法 Hex、密文长度非分组整数倍、
  错误密钥解密，均须返回失败且不得崩溃。

> 说明：接口另有 `encryptAESWithIv` / `decryptAESWithIv` 两个显式指定 IV 的版本，
> 除供测试比对标准向量外，也为将来改用随机 IV 留出扩展点。
> 另有静态方法 `derivePassphraseKey`，可将任意长度口令 SHA-256 派生成合规密钥，
> 供界面在"用户随意输入口令"的场景下选用。
