/**
 * @file    Types.h
 * @brief   城市通信网络设计系统 —— 公共数据结构
 *
 * 本文件是全项目唯一的公共类型定义处，由三名开发者共同使用。
 * 严禁在各模块内部自行定义与之重复的类型（如 MyEdge、GraphEdge、MSTAns 等）。
 *
 * 对应《共同开发规则和接口约定》第 6 节。
 * 编译期常量（INF、错误提示文本等）见同目录下的 Constants.h。
 *
 * 注意：类型一律声明在全局作用域，不引入命名空间，
 *       以保证与文档中给出的接口声明逐字一致。
 */

#pragma once

#include <string>
#include <vector>

/**
 * @brief 一条通信链路（无向图的边）。
 *
 * 城市编号统一使用 0,1,2,... ；界面层需要显示为 A,B,C,... 时由 GUI 自行转换。
 * 由于是无向图，约定 from < to，由算法模块负责规范化。
 */
struct Edge
{
    int from = 0;   ///< 起点城市编号
    int to   = 0;   ///< 终点城市编号
    int cost = 0;   ///< 该链路的建造代价
};

/**
 * @brief 最小生成树的计算结果。
 *
 * 算法模块一律通过本结构返回结果，不抛异常、不弹窗、不打印。
 */
struct MSTResult
{
    bool              success       = false;   ///< 计算是否成功
    std::vector<Edge> edges;                   ///< 最小生成树的边集
    long long         totalCost     = 0;       ///< 最低总造价
    double            runningTimeMs = 0.0;     ///< 算法运行时间（毫秒）
    std::string       errorMessage;            ///< 失败原因，success 为 true 时为空
};

/**
 * @brief 加解密结果。
 *
 * 密文统一以 Hex 字符串形式返回，GUI 不接触原始二进制数据。
 */
struct CryptoResult
{
    bool        success       = false;   ///< 操作是否成功
    std::string output;                  ///< 结果：密文为 Hex 字符串，明文为原始文本
    double      runningTimeMs = 0.0;     ///< 操作耗时（毫秒）
    std::string errorMessage;            ///< 失败原因，success 为 true 时为空
};
