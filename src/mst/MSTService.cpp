#include "MSTService.h"

#include <algorithm>
#include <chrono>
#include <numeric>

#include "../common/Constants.h"
#include "Graph.h"

namespace {

// 并查集：路径压缩 + 按秩合并。
// 仅 Kruskal 内部使用，不暴露给 GUI。
class DisjointSet {
public:
    explicit DisjointSet(int n) : parent_(n), rank_(n, 0) {
        std::iota(parent_.begin(), parent_.end(), 0);
    }

    int find(int x) {
        while (parent_[x] != x) {
            parent_[x] = parent_[parent_[x]];  // 路径压缩（折叠）
            x = parent_[x];
        }
        return x;
    }

    bool unite(int a, int b) {
        int rootA = find(a);
        int rootB = find(b);
        if (rootA == rootB) {
            return false;  // 已经在同一集合，这条边会成环
        }
        if (rank_[rootA] < rank_[rootB]) {
            std::swap(rootA, rootB);
        }
        parent_[rootB] = rootA;
        if (rank_[rootA] == rank_[rootB]) {
            ++rank_[rootA];
        }
        return true;
    }

private:
    std::vector<int> parent_;
    std::vector<int> rank_;
};

double elapsedMs(const std::chrono::high_resolution_clock::time_point& begin,
                 const std::chrono::high_resolution_clock::time_point& end) {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

long long sumCost(const std::vector<Edge>& edges) {
    long long total = 0;
    for (const Edge& edge : edges) {
        total += edge.cost;
    }
    return total;
}

}  // namespace

MSTService::MSTService() : n_(0) {}

void MSTService::setCityCount(int n) {
    if (n <= 0) {
        clear();
        return;
    }

    n_ = n;
    cost_.assign(n, std::vector<int>(n, INF));
    for (int i = 0; i < n; ++i) {
        cost_[i][i] = 0;
    }
}

int MSTService::cityCount() const {
    return n_;
}

void MSTService::clear() {
    n_ = 0;
    cost_.clear();
}

bool MSTService::setCost(int from, int to, int cost) {
    if (from < 0 || to < 0 || from >= n_ || to >= n_) {
        return false;
    }
    if (from == to) {
        return false;
    }
    if (cost < 0) {
        return false;
    }

    // 无向图：对称写入，保证 cost[i][j] == cost[j][i]。
    cost_[from][to] = cost;
    cost_[to][from] = cost;
    return true;
}

int MSTService::getCost(int from, int to) const {
    if (from < 0 || to < 0 || from >= n_ || to >= n_) {
        return INF;
    }
    return cost_[from][to];
}

MSTResult MSTService::prim(int startCity) const {
    MSTResult result;

    if (n_ <= 0) {
        result.errorMessage = ERR_INVALID_CITY_COUNT;
        return result;
    }
    if (startCity < 0 || startCity >= n_) {
        result.errorMessage = ERR_INVALID_CITY_INDEX;
        return result;
    }

    const auto begin = std::chrono::high_resolution_clock::now();

    if (n_ == 1) {
        result.success = true;
        result.runningTimeMs = elapsedMs(begin, std::chrono::high_resolution_clock::now());
        return result;
    }

    if (!graphutil::isConnected(cost_, n_)) {
        result.runningTimeMs = elapsedMs(begin, std::chrono::high_resolution_clock::now());
        result.errorMessage = ERR_DISCONNECTED;
        return result;
    }

    // O(n^2) 邻接矩阵版本 Prim。
    std::vector<int> minCost(n_, INF);   // 每个城市到当前生成树的最小边权
    std::vector<int> fromCity(n_, -1);   // 该最小边来自哪个已入树城市
    std::vector<bool> inTree(n_, false);

    minCost[startCity] = 0;

    for (int step = 0; step < n_; ++step) {
        int best = -1;
        for (int city = 0; city < n_; ++city) {
            if (!inTree[city] && (best == -1 || minCost[city] < minCost[best])) {
                best = city;
            }
        }

        if (best == -1 || minCost[best] >= INF) {
            result.success = false;
            result.edges.clear();
            result.totalCost = 0;
            result.runningTimeMs = elapsedMs(begin, std::chrono::high_resolution_clock::now());
            result.errorMessage = ERR_DISCONNECTED;
            return result;
        }

        inTree[best] = true;
        if (startCity != best) {
            result.edges.push_back(Edge{fromCity[best], best, minCost[best]});
        }

        for (int city = 0; city < n_; ++city) {
            if (!inTree[city] && cost_[best][city] < minCost[city]) {
                minCost[city] = cost_[best][city];
                fromCity[city] = best;
            }
        }
    }

    result.success = true;
    result.totalCost = sumCost(result.edges);
    result.runningTimeMs = elapsedMs(begin, std::chrono::high_resolution_clock::now());
    return result;
}

MSTResult MSTService::kruskal() const {
    MSTResult result;

    if (n_ <= 0) {
        result.errorMessage = ERR_INVALID_CITY_COUNT;
        return result;
    }

    const auto begin = std::chrono::high_resolution_clock::now();

    if (n_ == 1) {
        result.success = true;
        result.runningTimeMs = elapsedMs(begin, std::chrono::high_resolution_clock::now());
        return result;
    }

    std::vector<Edge> candidates = graphutil::collectEdges(cost_, n_);
    std::sort(candidates.begin(), candidates.end(),
              [](const Edge& lhs, const Edge& rhs) { return lhs.cost < rhs.cost; });

    DisjointSet dsu(n_);
    for (const Edge& edge : candidates) {
        if (dsu.unite(edge.from, edge.to)) {
            result.edges.push_back(edge);
            if (static_cast<int>(result.edges.size()) == n_ - 1) {
                break;
            }
        }
    }

    result.runningTimeMs = elapsedMs(begin, std::chrono::high_resolution_clock::now());

    if (static_cast<int>(result.edges.size()) != n_ - 1) {
        result.success = false;
        result.edges.clear();
        result.totalCost = 0;
        result.errorMessage = ERR_DISCONNECTED;
        return result;
    }

    result.success = true;
    result.totalCost = sumCost(result.edges);
    return result;
}

bool MSTService::isConnected() const {
    return graphutil::isConnected(cost_, n_);
}
