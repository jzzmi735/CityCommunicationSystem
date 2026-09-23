// 开发者 A —— MST 模块验收测试
//
// 覆盖《开发者A_MST与城市网络》第 13 节要求的 6 组测试，
// 以及第 17 节的性能实验（n = 10 / 50 / 100 / 200 / 500）。
//
// 构建后直接运行：
//   test_mst              （使用 data/graph1.txt 作为题目图1）
//   test_mst <path>       （指定其它图数据文件）

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "../src/common/Constants.h"
#include "../src/common/Types.h"
#include "../src/mst/MSTService.h"

namespace {

int g_failed = 0;
int g_passed = 0;

void check(bool condition, const std::string& what) {
    if (condition) {
        ++g_passed;
        std::cout << "    [PASS] " << what << "\n";
    } else {
        ++g_failed;
        std::cout << "    [FAIL] " << what << "\n";
    }
}

// 生成一个连通的带权无向图：先随机生成一棵树保证连通，再按密度补充随机边。
void buildRandomConnectedGraph(MSTService& service, int n, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> costDist(1, 1000);
    std::uniform_real_distribution<double> probDist(0.0, 1.0);

    service.setCityCount(n);
    for (int city = 1; city < n; ++city) {
        std::uniform_int_distribution<int> parentDist(0, city - 1);
        const int parent = parentDist(rng);
        service.setCost(parent, city, costDist(rng));
    }
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (service.getCost(i, j) < INF) {
                continue;
            }
            if (probDist(rng) < 0.6) {
                service.setCost(i, j, costDist(rng));
            }
        }
    }
}

// 比较 Prim 与 Kruskal 的结果是否一致。
bool sameTree(const MSTResult& lhs, const MSTResult& rhs) {
    return lhs.success == rhs.success &&
           lhs.edges.size() == rhs.edges.size() &&
           lhs.totalCost == rhs.totalCost;
}

// 判断由给定边集构成的图是否连通（用于独立参考实现）。
bool connectedUsingEdges(int n, const std::vector<Edge>& edges) {
    if (n <= 1) {
        return true;
    }

    std::vector<std::vector<int>> adjacency(n);
    for (const Edge& edge : edges) {
        adjacency[edge.from].push_back(edge.to);
        adjacency[edge.to].push_back(edge.from);
    }

    std::vector<bool> visited(n, false);
    std::vector<int> stack{0};
    visited[0] = true;
    int reached = 1;
    while (!stack.empty()) {
        const int city = stack.back();
        stack.pop_back();
        for (int next : adjacency[city]) {
            if (!visited[next]) {
                visited[next] = true;
                ++reached;
                stack.push_back(next);
            }
        }
    }
    return reached == n;
}

// 独立的参考实现：反向删除法（reverse-delete），仅用于交叉验证 Prim / Kruskal。
// 思路与 Prim / Kruskal 都不同：按边权从大到小尝试删除，只要图仍连通就删掉该边。
// 返回 -1 表示图不连通。
long long referenceMstCost(const MSTService& service) {
    const int n = service.cityCount();
    if (n <= 0) {
        return -1;
    }
    if (n == 1) {
        return 0;
    }

    std::vector<Edge> edges;
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            const int cost = service.getCost(i, j);
            if (cost < INF) {
                edges.push_back(Edge{i, j, cost});
            }
        }
    }

    if (!connectedUsingEdges(n, edges)) {
        return -1;
    }

    std::sort(edges.begin(), edges.end(),
              [](const Edge& lhs, const Edge& rhs) { return lhs.cost > rhs.cost; });

    std::vector<Edge> remaining = edges;
    for (const Edge& edge : edges) {
        std::vector<Edge> candidate;
        for (const Edge& kept : remaining) {
            if (!(kept.from == edge.from && kept.to == edge.to)) {
                candidate.push_back(kept);
            }
        }
        if (connectedUsingEdges(n, candidate)) {
            remaining = candidate;
        }
    }

    long long total = 0;
    for (const Edge& edge : remaining) {
        total += edge.cost;
    }
    return total;
}

// 从文件读取图：第一行 n，随后 n 行每行 n 个权值。
// 权值可以是整数，也可以是 INF / inf / - 表示该城市对之间没有直接链路。
bool loadGraph(const std::string& path, MSTService& service) {
    std::ifstream input(path);
    if (!input) {
        return false;
    }

    int n = 0;
    if (!(input >> n) || n <= 0) {
        return false;
    }

    service.setCityCount(n);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            std::string token;
            if (!(input >> token)) {
                return false;
            }

            const bool noLink = (token == "INF" || token == "inf" || token == "-");
            long long value = 0;
            if (!noLink) {
                std::istringstream parser(token);
                if (!(parser >> value)) {
                    return false;
                }
            }

            if (i == j || noLink || value < 0 || value >= INF) {
                continue;
            }
            service.setCost(i, j, static_cast<int>(value));
        }
    }
    return true;
}

void printTree(const MSTResult& result) {
    for (const Edge& edge : result.edges) {
        std::cout << "      City " << edge.from << " -- City " << edge.to
                  << " : " << edge.cost << "\n";
    }
}

// ---------------------------------------------------------------------------
// 测试 1：单城市
// ---------------------------------------------------------------------------
void testSingleCity() {
    std::cout << "[测试 1] 单城市 n = 1\n";

    MSTService service;
    service.setCityCount(1);

    const MSTResult prim = service.prim();
    check(prim.success, "prim().success == true");
    check(prim.edges.empty(), "prim().edges.size() == 0");
    check(prim.totalCost == 0, "prim().totalCost == 0");

    const MSTResult kruskal = service.kruskal();
    check(kruskal.success, "kruskal().success == true");
    check(kruskal.edges.empty(), "kruskal().edges.size() == 0");
    check(kruskal.totalCost == 0, "kruskal().totalCost == 0");

    check(service.isConnected(), "isConnected() == true");

    // 非法的 0 / 负数城市数量必须被安全处理。
    MSTService empty;
    empty.setCityCount(0);
    check(empty.cityCount() == 0, "setCityCount(0) 后 cityCount() == 0");
    check(!empty.prim().success, "空图 prim().success == false");
    check(!empty.kruskal().success, "空图 kruskal().success == false");
}

// ---------------------------------------------------------------------------
// 测试 2：两个城市 A-B = 5
// ---------------------------------------------------------------------------
void testTwoCities() {
    std::cout << "[测试 2] 两个城市 A-B = 5\n";

    MSTService service;
    service.setCityCount(2);
    check(service.setCost(0, 1, 5), "setCost(0, 1, 5) == true");
    check(service.getCost(1, 0) == 5, "无向图对称：getCost(1, 0) == 5");

    const MSTResult prim = service.prim();
    check(prim.success, "prim().success == true");
    check(prim.edges.size() == 1, "prim().edges.size() == 1");
    check(prim.totalCost == 5, "prim().totalCost == 5");

    const MSTResult kruskal = service.kruskal();
    check(kruskal.success, "kruskal().success == true");
    check(kruskal.edges.size() == 1, "kruskal().edges.size() == 1");
    check(kruskal.totalCost == 5, "kruskal().totalCost == 5");

    check(sameTree(prim, kruskal), "Prim 与 Kruskal 结果一致");

    // 起点指定为 1 也应得到同样结果。
    const MSTResult primFromOne = service.prim(1);
    check(primFromOne.success && primFromOne.totalCost == 5, "prim(1).totalCost == 5");
}

// ---------------------------------------------------------------------------
// 测试 3：普通连通图，Prim == Kruskal
// ---------------------------------------------------------------------------
void testGeneralConnectedGraph() {
    std::cout << "[测试 3] 普通连通图（Prim == Kruskal）\n";

    MSTService service;
    service.setCityCount(6);
    service.setCost(0, 1, 6);
    service.setCost(0, 2, 1);
    service.setCost(0, 3, 5);
    service.setCost(1, 2, 5);
    service.setCost(1, 4, 3);
    service.setCost(2, 3, 5);
    service.setCost(2, 4, 6);
    service.setCost(2, 5, 4);
    service.setCost(3, 5, 2);
    service.setCost(4, 5, 6);

    const MSTResult prim = service.prim();
    const MSTResult kruskal = service.kruskal();

    std::cout << "    Prim 选中边：\n";
    printTree(prim);

    check(prim.success && kruskal.success, "两种算法均成功");
    check(prim.edges.size() == 5, "prim().edges.size() == n - 1 == 5");
    check(kruskal.edges.size() == 5, "kruskal().edges.size() == n - 1 == 5");
    check(prim.totalCost == kruskal.totalCost, "Prim 总造价 == Kruskal 总造价");
    check(prim.totalCost == 15, "最低总造价 == 15");
    check(service.isConnected(), "isConnected() == true");

    // 从不同起点出发，总造价必须相同。
    for (int start = 0; start < 6; ++start) {
        const MSTResult fromStart = service.prim(start);
        check(fromStart.success && fromStart.totalCost == prim.totalCost,
              "起点 " + std::to_string(start) + " 的总造价一致");
    }
}

// ---------------------------------------------------------------------------
// 测试 4：完全图
// ---------------------------------------------------------------------------
void testCompleteGraph() {
    std::cout << "[测试 4] 完全图 n = 8（边权 = |i - j|）\n";

    const int n = 8;
    MSTService service;
    service.setCityCount(n);
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            service.setCost(i, j, j - i);
        }
    }

    // 该完全图的最优解是链 0-1-2-...-7，每段代价 1，共 n-1 = 7。
    const MSTResult prim = service.prim();
    const MSTResult kruskal = service.kruskal();

    check(prim.success && kruskal.success, "两种算法均成功");
    check(prim.edges.size() == static_cast<std::size_t>(n - 1), "prim().edges.size() == 7");
    check(kruskal.edges.size() == static_cast<std::size_t>(n - 1), "kruskal().edges.size() == 7");
    check(prim.totalCost == 7, "prim().totalCost == 7");
    check(prim.totalCost == kruskal.totalCost, "Prim 总造价 == Kruskal 总造价");

    // 随机完全图（多次）交叉验证。
    for (unsigned seed = 1; seed <= 20; ++seed) {
        MSTService random;
        buildRandomConnectedGraph(random, 12, seed);

        const MSTResult a = random.prim();
        const MSTResult b = random.kruskal();
        if (!sameTree(a, b) || a.edges.size() != 11) {
            check(false, "随机完全图 seed = " + std::to_string(seed) + " 校验失败");
            return;
        }
        if (referenceMstCost(random) != a.totalCost) {
            check(false, "随机图 seed = " + std::to_string(seed) + " 与参考实现不一致");
            return;
        }
    }
    check(true, "20 组随机连通图 Prim / Kruskal / 参考实现结果全部一致");
}

// ---------------------------------------------------------------------------
// 测试 5：非连通图
// ---------------------------------------------------------------------------
void testDisconnectedGraph() {
    std::cout << "[测试 5] 非连通图\n";

    MSTService service;
    service.setCityCount(5);
    service.setCost(0, 1, 3);
    service.setCost(1, 2, 4);
    service.setCost(3, 4, 7);  // {0,1,2} 与 {3,4} 之间没有链路

    check(!service.isConnected(), "isConnected() == false");

    const MSTResult prim = service.prim();
    check(!prim.success, "prim().success == false");
    check(prim.errorMessage == "Graph is disconnected",
          "prim().errorMessage == \"Graph is disconnected\"");

    const MSTResult kruskal = service.kruskal();
    check(!kruskal.success, "kruskal().success == false");
    check(kruskal.errorMessage == "Graph is disconnected",
          "kruskal().errorMessage == \"Graph is disconnected\"");

    // 单点孤立同样属于非连通。
    MSTService isolated;
    isolated.setCityCount(3);
    isolated.setCost(0, 1, 1);  // 城市 2 完全孤立
    check(!isolated.isConnected(), "孤立城市的 isConnected() == false");
    check(!isolated.prim().success, "孤立城市 prim().success == false");
    check(!isolated.kruskal().success, "孤立城市 kruskal().success == false");

    // 非法起点不能崩溃。
    MSTService normal;
    normal.setCityCount(3);
    normal.setCost(0, 1, 1);
    normal.setCost(1, 2, 1);
    check(!normal.prim(-1).success, "prim(-1).success == false");
    check(!normal.prim(99).success, "prim(99).success == false");
}

// ---------------------------------------------------------------------------
// 测试 6：题目图1
// ---------------------------------------------------------------------------
void testProblemFigure(const std::string& path) {
    std::cout << "[测试 6] 题目图1  —— " << path << "\n";

    MSTService service;
    if (!loadGraph(path, service)) {
        check(false, "无法读取图数据文件（请提供题目图1的矩阵）");
        return;
    }

    std::cout << "    城市数量 n = " << service.cityCount() << "\n";
    check(service.isConnected(), "题目图是连通图");

    const MSTResult prim = service.prim();
    const MSTResult kruskal = service.kruskal();

    std::cout << "    Prim 选中的边：\n";
    printTree(prim);

    check(prim.success && kruskal.success, "两种算法均成功");
    check(prim.edges.size() == static_cast<std::size_t>(service.cityCount() - 1),
          "prim().edges.size() == n - 1");
    check(kruskal.edges.size() == static_cast<std::size_t>(service.cityCount() - 1),
          "kruskal().edges.size() == n - 1");
    check(prim.totalCost == kruskal.totalCost, "Prim 总造价 == Kruskal 总造价");

    const long long reference = referenceMstCost(service);
    check(reference == prim.totalCost,
          "独立参考实现（反向删除法）总造价一致：" + std::to_string(reference));

    std::cout << "    最低总造价 = " << prim.totalCost << "\n";
    std::cout << "    Prim   耗时 = " << prim.runningTimeMs << " ms\n";
    std::cout << "    Kruskal 耗时 = " << kruskal.runningTimeMs << " ms\n";
}

// ---------------------------------------------------------------------------
// 测试 7：setCost 非法输入
// ---------------------------------------------------------------------------
void testIllegalInput() {
    std::cout << "[测试 7] setCost 非法输入与边界\n";

    MSTService service;
    service.setCityCount(3);

    check(!service.setCost(-1, 1, 5), "from < 0 返回 false");
    check(!service.setCost(0, -1, 5), "to < 0 返回 false");
    check(!service.setCost(3, 1, 5), "from >= n 返回 false");
    check(!service.setCost(0, 3, 5), "to >= n 返回 false");
    check(!service.setCost(0, 1, -1), "cost < 0 返回 false");
    check(!service.setCost(1, 1, 5), "from == to 返回 false");
    check(service.setCost(0, 1, 0), "cost == 0 是合法成本");
    check(service.getCost(0, 4) == INF, "越界 getCost 返回 INF");

    service.clear();
    check(service.cityCount() == 0, "clear() 后 cityCount() == 0");
    check(!service.setCost(0, 1, 1), "clear() 后 setCost 返回 false");
}

// ---------------------------------------------------------------------------
// 测试 8：性能实验（第 17 节）
// ---------------------------------------------------------------------------
void testPerformance() {
    std::cout << "[测试 8] 性能实验（稠密连通随机图，各规模取 5 次平均）\n";

    const int sizes[] = {10, 50, 100, 200, 500};
    const int rounds = 5;

    struct Row {
        int n;
        double primMs;
        double kruskalMs;
        bool consistent;
    };
    std::vector<Row> rows;
    std::vector<std::string> failures;

    for (int n : sizes) {
        double primTotal = 0.0;
        double kruskalTotal = 0.0;
        bool consistent = true;

        for (int round = 0; round < rounds; ++round) {
            MSTService service;
            buildRandomConnectedGraph(service, n, 20240923u + static_cast<unsigned>(n * 100 + round));

            const MSTResult prim = service.prim();
            const MSTResult kruskal = service.kruskal();

            if (!prim.success || !kruskal.success ||
                prim.totalCost != kruskal.totalCost ||
                prim.edges.size() != static_cast<std::size_t>(n - 1)) {
                consistent = false;
            }
            primTotal += prim.runningTimeMs;
            kruskalTotal += kruskal.runningTimeMs;
        }

        rows.push_back(Row{n, primTotal / rounds, kruskalTotal / rounds, consistent});
        if (!consistent) {
            failures.push_back("n = " + std::to_string(n));
        }
    }

    // 先输出完整表格，再输出断言，避免相互穿插。
    std::cout << "    +--------+------------------+------------------+\n";
    std::cout << "    |    n   |   Prim 用时(ms)  |  Kruskal 用时(ms)|\n";
    std::cout << "    +--------+------------------+------------------+\n";
    for (const Row& row : rows) {
        std::printf("    | %6d | %16.3f | %16.3f |\n", row.n, row.primMs, row.kruskalMs);
    }
    std::cout << "    +--------+------------------+------------------+\n";

    for (const Row& row : rows) {
        check(row.consistent,
              "n = " + std::to_string(row.n) + " 时两种算法结果一致且边数为 n - 1");
    }
    check(failures.empty(), "全部规模性能实验结果正确");
}

}  // namespace

int main(int argc, char** argv) {
#if defined(_WIN32)
    // Windows 控制台默认使用 GBK 代码页，这里切到 UTF-8 以便正确显示中文。
    std::system("chcp 65001 > nul");
#endif

    std::string graphPath = "data/graph1.txt";
    if (argc > 1) {
        graphPath = argv[1];
    }

    std::cout << "===== MST 模块测试 =====\n\n";
    testSingleCity();
    std::cout << "\n";
    testTwoCities();
    std::cout << "\n";
    testGeneralConnectedGraph();
    std::cout << "\n";
    testCompleteGraph();
    std::cout << "\n";
    testDisconnectedGraph();
    std::cout << "\n";
    testIllegalInput();
    std::cout << "\n";
    testProblemFigure(graphPath);
    std::cout << "\n";
    testPerformance();

    std::cout << "\n===== 结果：通过 " << g_passed << " 项，失败 " << g_failed << " 项 =====\n";
    return g_failed == 0 ? 0 : 1;
}
