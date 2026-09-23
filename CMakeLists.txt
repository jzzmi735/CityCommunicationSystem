# ============================================================================
#  城市通信网络设计系统 —— 顶层构建脚本
#
#  对应《共同开发规则和接口约定》第 24 节：本文件属公共文件，由开发者 C 统一维护。
#
#  开发者 A / B 新增源文件时，**无需修改本文件**：
#  构建脚本会自动收集 src/mst/*.cpp 与 src/huffman/*.cpp，
#  以及 tests/ 下对应的测试文件。
#  若确需修改本文件，请先同步 main 并通知其他成员。
#
#  构建方法见 README.md。
# ============================================================================

cmake_minimum_required(VERSION 3.16)

project(CityCommunicationSystem
    VERSION 1.0.0
    DESCRIPTION "城市通信网络设计系统"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
#  一、编译选项
# ---------------------------------------------------------------------------

# 算法层为纯 C++，不使用 Qt 的元对象系统以外的扩展；
# 统一 C++17 以便使用结构化绑定与 std::filesystem，且完全兼容 Qt 6。
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 源文件统一使用 UTF-8 保存（文档第 28 节），此处确保 MSVC / MinGW 正确解析。
if(MSVC)
    add_compile_options(/utf-8 /W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

# 单配置生成器下，把可执行文件集中输出到 bin/，便于文档第 29 节的发布整理。
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY          ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG    ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE  ${CMAKE_BINARY_DIR}/bin)

# ---------------------------------------------------------------------------
#  二、Qt 依赖
# ---------------------------------------------------------------------------

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets)

# Qt 的 CMake 包会自带 AUTOMOC / AUTOUIC / AUTORCC 开关。
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# ---------------------------------------------------------------------------
#  三、Crypto 模块（开发者 C）
#
#  纯 C++ 实现（AES-128-CBC / DES-CBC），不依赖 Qt，也不依赖
#  OpenSSL / Crypto++ 等第三方库，因此在任何 MinGW 环境下都能直接编译。
# ---------------------------------------------------------------------------

add_library(crypto STATIC
    src/crypto/CryptoSystem.cpp
    src/crypto/CryptoSystem.h
)
target_include_directories(crypto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ---------------------------------------------------------------------------
#  四、MST 模块（开发者 A）
#
#  源文件按目录自动收集，开发者 A 新增 Graph.cpp / MSTService.cpp 之外的文件
#  时无需改动本脚本。目录为空时本目标不会被创建。
# ---------------------------------------------------------------------------

file(GLOB MST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mst/*.cpp)

if(MST_SOURCES)
    add_library(mst STATIC ${MST_SOURCES})
    target_include_directories(mst PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    message(STATUS "MST 模块源文件: ${MST_SOURCES}")
else()
    message(STATUS "MST 模块尚未提供源文件，跳过（等待开发者 A）")
endif()

# ---------------------------------------------------------------------------
#  五、Huffman 模块（开发者 B）
#
#  纯 C++ 实现，不依赖 Qt；错误通过返回值与 lastError() 上报。
#  同样按目录自动收集源文件。
# ---------------------------------------------------------------------------

file(GLOB HUFFMAN_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/huffman/*.cpp)

if(HUFFMAN_SOURCES)
    add_library(huffman STATIC ${HUFFMAN_SOURCES})
    target_include_directories(huffman PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    message(STATUS "Huffman 模块源文件: ${HUFFMAN_SOURCES}")
else()
    message(STATUS "Huffman 模块尚未提供源文件，跳过（等待开发者 B）")
endif()

# ---------------------------------------------------------------------------
#  六、主程序
# ---------------------------------------------------------------------------

# 界面完全由 C++ 代码搭建（未使用 Qt Designer 的 .ui 文件），
# 这样三名成员无需同步二进制资源，界面改动在 diff 中也一目了然。
set(GUI_SOURCES
    src/gui/MainWindow.cpp
    src/gui/MainWindow.h
    src/gui/NetworkView.cpp
    src/gui/NetworkView.h
    src/main.cpp
)

add_executable(CityCommunicationSystem WIN32 ${GUI_SOURCES})

target_link_libraries(CityCommunicationSystem PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    crypto
)

# 仅当对应模块已就绪时才链接，保证在 A / B 提交代码之前主程序仍可编译运行。
if(TARGET mst)
    target_link_libraries(CityCommunicationSystem PRIVATE mst)
    target_compile_definitions(CityCommunicationSystem PRIVATE HAS_MST_MODULE)
endif()

if(TARGET huffman)
    target_link_libraries(CityCommunicationSystem PRIVATE huffman)
    target_compile_definitions(CityCommunicationSystem PRIVATE HAS_HUFFMAN_MODULE)
endif()

# ---------------------------------------------------------------------------
#  七、测试目标
#
#  对应文档第 19 节：测试代码一律放在 tests/，不得写入 src/main.cpp。
#  运行方式：cmake --build build && ctest --test-dir build --output-on-failure
# ---------------------------------------------------------------------------

enable_testing()

add_executable(test_crypto
    tests/test_crypto.cpp
    src/crypto/CryptoSystem.cpp
)
target_include_directories(test_crypto PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME crypto_acceptance COMMAND test_crypto)

# MST / Huffman 的测试目标同样按文件是否存在自动启用。

file(GLOB MST_TESTS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_mst.cpp)

if(MST_TESTS AND TARGET mst)
    add_executable(test_mst ${MST_TESTS})
    target_link_libraries(test_mst PRIVATE mst)

    # 开发者 A 的验收测试接收一个图数据文件路径作为命令行参数。
    add_test(NAME mst_acceptance
             COMMAND test_mst ${CMAKE_CURRENT_SOURCE_DIR}/data/graph1.txt)
    set_tests_properties(mst_acceptance PROPERTIES
                         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

file(GLOB HUFFMAN_TESTS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_huffman.cpp)

if(HUFFMAN_TESTS AND TARGET huffman)
    add_executable(test_huffman ${HUFFMAN_TESTS})
    target_link_libraries(test_huffman PRIVATE huffman)

    # 开发者 B 的验收测试使用 data/ 下的相对路径读写文件，
    # 因此必须把工作目录固定为项目根目录。
    add_test(NAME huffman_acceptance COMMAND test_huffman)
    set_tests_properties(huffman_acceptance PROPERTIES
                         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# ---------------------------------------------------------------------------
#  八、部署提示
# ---------------------------------------------------------------------------

if(WIN32)
    message(STATUS "")
    message(STATUS "构建完成后可在 Qt 环境中部署运行时依赖：")
    message(STATUS "  windeployqt --release ${CMAKE_BINARY_DIR}/bin/CityCommunicationSystem.exe")
    message(STATUS "")
endif()
