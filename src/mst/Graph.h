#pragma once

#include <vector>

#include "../common/Types.h"

// 无向带权图的公共工具函数。
//
// 图的存储统一使用邻接矩阵：
//
//   cost[i][j] == INF  表示城市 i 与 j 之间没有直接通信链路
//   cost[i][i] == 0
//   无向图满足 cost[i][j] == cost[j][i]
//
// 这里只提供算法层需要的工具，不保存任何图状态；
// 图状态由 MSTService 持有，避免两份数据不一致。
namespace graphutil {

// 邻接矩阵按 BFS 判断连通性（n <= 0 视为不连通）。
bool isConnected(const std::vector<std::vector<int>>& cost, int n);

// 提取所有无向边，保证 from < to，每条边只出现一次。
std::vector<Edge> collectEdges(const std::vector<std::vector<int>>& cost, int n);

}  // namespace graphutil
