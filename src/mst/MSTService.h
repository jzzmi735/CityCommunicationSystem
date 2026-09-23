#pragma once

#include <vector>

#include "../common/Types.h"

// 城市通信网络的最小生成树服务（Prim / Kruskal）。
//
// 图结构：无向带权图，顶点为城市（编号 0..n-1），边权为两城市间的建设成本。
// 存储：邻接矩阵 cost_[i][j]，INF 表示无直接链路，cost_[i][i] == 0。
//
// GUI 只需要 setCityCount / setCost / prim / kruskal / isConnected，
// 不需要了解 Prim / Kruskal 的任何内部状态。
class MSTService {
public:
    MSTService();

    void setCityCount(int n);

    int cityCount() const;

    void clear();

    bool setCost(int from, int to, int cost);

    int getCost(int from, int to) const;

    MSTResult prim(int startCity = 0) const;

    MSTResult kruskal() const;

    bool isConnected() const;

private:
    int n_;
    std::vector<std::vector<int>> cost_;
};
