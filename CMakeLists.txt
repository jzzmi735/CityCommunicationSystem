# ============================================================================
#  城市通信网络设计系统 —— 构建脚本（MST 模块分支）
#
#  本分支只包含开发者 A 的 MST 模块，用于其独立开发与验收。
#  界面、Crypto 模块及完整集成在主分支的后续提交中引入。
#
#  构建步骤：
#    cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
#    cmake --build build
#    ctest --test-dir build --output-on-failure
# ============================================================================

cmake_minimum_required(VERSION 3.16)

project(CityCommunicationSystem
    VERSION 1.0.0
    DESCRIPTION "城市通信网络设计系统"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 源文件含中文注释，统一按 UTF-8 解析（文档第 28 节）。
if(MSVC)
    add_compile_options(/utf-8)
else()
    add_compile_options(-Wall -Wextra)
endif()

# 可执行文件集中输出到 bin/，便于文档第 29 节的发布整理。
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---------------------------------------------------------------------------
#  MST 模块（开发者 A）
# ---------------------------------------------------------------------------

add_library(mst STATIC
    src/mst/MSTService.cpp
    src/mst/Graph.cpp
)
target_include_directories(mst PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ---------------------------------------------------------------------------
#  Huffman 模块（开发者 B）
#
#  纯 C++ 实现，不依赖 Qt；错误通过返回值与 lastError() 上报。
# ---------------------------------------------------------------------------

add_library(huffman STATIC
    src/huffman/HuffmanSystem.cpp
    src/huffman/HuffmanSystem.h
)
target_include_directories(huffman PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ---------------------------------------------------------------------------
#  模块验收测试
# ---------------------------------------------------------------------------

enable_testing()

add_executable(test_mst tests/test_mst.cpp)
target_link_libraries(test_mst PRIVATE mst)

# 开发者 A 的验收测试接收一个图数据文件路径作为命令行参数。
add_test(NAME mst_acceptance
         COMMAND test_mst ${CMAKE_CURRENT_SOURCE_DIR}/data/graph1.txt)
set_tests_properties(mst_acceptance PROPERTIES
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_huffman tests/test_huffman.cpp)
target_link_libraries(test_huffman PRIVATE huffman)

# 开发者 B 的验收测试使用 data/ 下的相对路径读写文件，
# 因此必须把工作目录固定为项目根目录。
add_test(NAME huffman_acceptance COMMAND test_huffman)
set_tests_properties(huffman_acceptance PROPERTIES
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
