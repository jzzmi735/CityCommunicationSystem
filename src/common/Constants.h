/**
 * @file    Constants.h
 * @brief   城市通信网络设计系统 —— 公共常量
 *
 * 全项目共用的编译期常量统一放在此处，各模块不得自行重复定义。
 * 对应《共同开发规则和接口约定》第 8.1 节关于 INF 的约定。
 */

#pragma once

/// 两城市之间不存在直接通信链路时，成本矩阵中填此值。
///
/// 取 0x3f3f3f3f 而非 INT_MAX，可保证两条 INF 相加时不发生有符号整数溢出，
/// 便于在 MST 算法中安全地做加法比较。
constexpr int INF = 0x3f3f3f3f;

// ---------------------------------------------------------------------------
//  跨模块统一的错误提示文本
// ---------------------------------------------------------------------------

constexpr const char* ERR_DISCONNECTED       = "Graph is disconnected";
constexpr const char* ERR_INVALID_CITY_COUNT = "City count must be positive";
constexpr const char* ERR_INVALID_CITY_INDEX = "City index out of range";
constexpr const char* ERR_INVALID_COST       = "Cost must be non-negative";
