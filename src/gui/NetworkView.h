/**
 * @file    NetworkView.h
 * @brief   城市通信网络的图形化显示控件。
 *
 * 对应《开发者 C 工作说明》第 17 节：
 * 基于 QGraphicsView / QGraphicsScene，把城市按圆周排列，
 * 绘制城市节点、全部通信边、边权，并用加粗高亮区分最小生成树的边。
 *
 * 本控件只负责"画"，不含任何图算法逻辑：
 * 传入的边集由 MSTService 计算得到。
 */

#pragma once

#include <QGraphicsView>
#include <QPointF>
#include <QString>
#include <QVector>

class QGraphicsScene;

/**
 * @brief 网络可视化控件。
 */
class NetworkView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit NetworkView(QWidget* parent = nullptr);
    ~NetworkView() override;

    /// 设置城市数量，按圆周重新排布节点。会清空已有的边。
    void setCityCount(int count);

    /// 当前显示的城市数量。
    int cityCount() const;

    /// 添加一条普通通信边（绘制为细灰线，标注造价）。
    void addEdge(int from, int to, int cost);

    /// 把指定的边标记为最小生成树的一部分（绘制为粗实线并高亮）。
    /// 须在 addEdge 之后调用。
    void markMstEdge(int from, int to);

    /// 清空全部节点与边。
    void clear();

    /// 自动缩放，使全部内容可见。
    void fitContent();

protected:
    /// 滚轮缩放。
    void wheelEvent(QWheelEvent* event) override;

private:
    /// 按圆周计算各城市坐标，并绘制节点与标号。
    void layoutCities();

    /// 生成城市显示名：0..25 显示为 A..Z，超出则显示为 City 1、City 2…。
    static QString cityLabel(int index);

    QGraphicsScene*  scene_      = nullptr;
    QVector<QPointF> positions_;          ///< 各城市的圆周坐标
    bool             hasMstEdge_ = false; ///< 是否已标记过 MST 边（用于自适应缩放）

    static constexpr double kRadius    = 200.0;  ///< 圆周半径
    static constexpr double kNodeRadius = 18.0;  ///< 节点圆半径
    static constexpr double kMargin     = 110.0; ///< 场景四周留白，容纳边权文字
};
