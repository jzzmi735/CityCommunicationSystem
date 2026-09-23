#include "Graph.h"

#include <queue>

#include "../common/Constants.h"

namespace graphutil {

bool isConnected(const std::vector<std::vector<int>>& cost, int n) {
    if (n <= 0) {
        return false;
    }

    std::vector<bool> visited(n, false);
    std::queue<int> pending;
    visited[0] = true;
    pending.push(0);

    int reached = 0;
    while (!pending.empty()) {
        const int city = pending.front();
        pending.pop();
        ++reached;

        for (int next = 0; next < n; ++next) {
            if (next == city || visited[next]) {
                continue;
            }
            if (cost[city][next] >= INF) {
                continue;
            }
            visited[next] = true;
            pending.push(next);
        }
    }

    return reached == n;
}

std::vector<Edge> collectEdges(const std::vector<std::vector<int>>& cost, int n) {
    std::vector<Edge> edges;
    if (n <= 0) {
        return edges;
    }

    edges.reserve(static_cast<std::size_t>(n) * (n - 1) / 2);
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (cost[i][j] >= INF) {
                continue;
            }
            edges.push_back(Edge{i, j, cost[i][j]});
        }
    }
    return edges;
}

}  // namespace graphutil
