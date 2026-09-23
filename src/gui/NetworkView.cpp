/**
 * @file    NetworkView.cpp
 * @brief   网络可视化控件的绘制实现。
 *
 * 绘制层次（自下而上）：
 *   1. 全部非 MST 通信边   —— 浅灰细线 + 边权标注
 *   2. MST 边              —— 加粗高亮线 + 边权标注
 *   3. 城市节点与标号      —— 实心圆 + 城市名
 *
 * 边权标注会略微偏移出连线，避免与连线本身重叠。
 */

#include "NetworkView.h"

#include <QBrush>
#include <QFont>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QPainter>
#include <QPen>
#include <QVector>
#include <QWheelEvent>

#include <cmath>

namespace {

/// 画布配色（与 Qt 默认浅色主题协调）。
const QColor kEdgeColor(0xB0, 0xB4, 0xB8);      ///< 普通通信边：浅灰
const QColor kMstColor(0x1B, 0x6B, 0xC4);       ///< MST 边：蓝色高亮
const QColor kNodeColor(0x2E, 0x86, 0x3E);      ///< 城市节点：绿色
const QColor kNodeBorder(0x1E, 0x5A, 0x2A);     ///< 节点描边
const QColor kLabelColor(0x33, 0x33, 0x33);     ///< 城市标号
const QColor kWeightColor(0x55, 0x55, 0x55);    ///< 边权文字

/// 给文字加一层白色描边，使其压在连线之上依然清晰可读。
void applyHalo(QGraphicsSimpleTextItem* item, const QColor& color)
{
    item->setBrush(QBrush(color));
    QGraphicsSimpleTextItem* halo = new QGraphicsSimpleTextItem(item->text(), item);
    halo->setBrush(QBrush(Qt::white));
    halo->setZValue(-0.1);
    for (int dx = -1; dx <= 1; ++dx)
    {
        for (int dy = -1; dy <= 1; ++dy)
        {
            if (dx == 0 && dy == 0)
            {
                continue;
            }
            QGraphicsSimpleTextItem* ghost =
                new QGraphicsSimpleTextItem(item->text(), halo);
            ghost->setBrush(QBrush(Qt::white));
            ghost->setPos(dx, dy);
        }
    }
}

} // namespace

// ---------------------------------------------------------------------------

NetworkView::NetworkView(QWidget* parent)
    : QGraphicsView(parent)
{
    scene_ = new QGraphicsScene(this);
    setScene(scene_);
    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::TextAntialiasing, true);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setMinimumSize(360, 280);
    setBackgroundBrush(QBrush(Qt::white));

    // 给场景一个足够大的初始范围，避免拖动时内容被裁掉。
    scene_->setSceneRect(-400, -400, 800, 800);
}

NetworkView::~NetworkView() = default;

void NetworkView::clear()
{
    scene_->clear();
    positions_.clear();
    hasMstEdge_ = false;
}

int NetworkView::cityCount() const
{
    return positions_.size();
}

QString NetworkView::cityLabel(int index)
{
    if (index >= 0 && index < 26)
    {
        return QString(QChar('A' + index));
    }
    return QStringLiteral("City %1").arg(index + 1);
}

void NetworkView::setCityCount(int count)
{
    // 重新排布节点，已有的边随之作废。
    scene_->clear();
    positions_.clear();
    hasMstEdge_ = false;

    if (count <= 0)
    {
        return;
    }

    if (count == 1)
    {
        // 单个城市无圆周可言，直接放在原点。
        positions_.append(QPointF(0.0, 0.0));
    }
    else
    {
        // 从正上方开始顺时针均匀分布，标号可读性最好。
        const double step = 2.0 * M_PI / static_cast<double>(count);
        for (int i = 0; i < count; ++i)
        {
            const double angle = -M_PI / 2.0 + step * static_cast<double>(i);
            positions_.append(QPointF(kRadius * std::cos(angle),
                                      kRadius * std::sin(angle)));
        }
    }

    // 圆周半径随城市数适当放大，避免节点挤在一起。
    if (count > 10)
    {
        const double scale = 1.0 + (static_cast<double>(count) - 10.0) * 0.08;
        for (int i = 0; i < positions_.size(); ++i)
        {
            positions_[i] *= scale;
        }
    }

    layoutCities();
    fitContent();
}

void NetworkView::layoutCities()
{
    QFont labelFont = font();
    labelFont.setBold(true);
    labelFont.setPointSize(labelFont.pointSize() + 1);

    for (int i = 0; i < positions_.size(); ++i)
    {
        const QPointF center = positions_[i];

        // 节点圆。
        QGraphicsEllipseItem* node = new QGraphicsEllipseItem(
            center.x() - kNodeRadius, center.y() - kNodeRadius,
            kNodeRadius * 2.0, kNodeRadius * 2.0);
        node->setBrush(QBrush(kNodeColor));
        node->setPen(QPen(kNodeBorder, 1.6));
        node->setZValue(2);
        scene_->addItem(node);

        // 城市标号，居中显示在圆内。
        QGraphicsSimpleTextItem* label =
            new QGraphicsSimpleTextItem(cityLabel(i));
        label->setFont(labelFont);
        label->setBrush(QBrush(Qt::white));
        label->setZValue(3);

        const QRectF bounds = label->boundingRect();
        label->setPos(center.x() - bounds.width() / 2.0,
                      center.y() - bounds.height() / 2.0);
        scene_->addItem(label);
    }
}

void NetworkView::addEdge(int from, int to, int cost)
{
    if (from < 0 || to < 0
        || from >= positions_.size() || to >= positions_.size())
    {
        return;
    }

    const QPointF a = positions_[from];
    const QPointF b = positions_[to];

    QGraphicsLineItem* line = new QGraphicsLineItem(a.x(), a.y(), b.x(), b.y());
    QPen pen(kEdgeColor, 1.4, Qt::DashLine);
    pen.setCapStyle(Qt::RoundCap);
    line->setPen(pen);
    line->setZValue(1);
    scene_->addItem(line);

    // 边权标注：置于两城中点，并沿垂直方向略微外移，避免压住连线。
    const QPointF mid((a.x() + b.x()) / 2.0, (a.y() + b.y()) / 2.0);

    QGraphicsSimpleTextItem* weight =
        new QGraphicsSimpleTextItem(QString::number(cost));
    QFont weightFont = font();
    weightFont.setPointSize(weightFont.pointSize() - 1);
    weight->setFont(weightFont);
    applyHalo(weight, kWeightColor);
    weight->setZValue(2);

    const QRectF bounds = weight->boundingRect();
    weight->setPos(mid.x() - bounds.width() / 2.0,
                   mid.y() - bounds.height() / 2.0);
    scene_->addItem(weight);
}

void NetworkView::markMstEdge(int from, int to)
{
    if (from < 0 || to < 0
        || from >= positions_.size() || to >= positions_.size())
    {
        return;
    }

    const QPointF a = positions_[from];
    const QPointF b = positions_[to];

    // 覆盖在普通边之上，加粗并高亮，形成明显的视觉区分。
    QGraphicsLineItem* line = new QGraphicsLineItem(a.x(), a.y(), b.x(), b.y());
    QPen pen(kMstColor, 3.4, Qt::SolidLine);
    pen.setCapStyle(Qt::RoundCap);
    line->setPen(pen);
    line->setZValue(1.5);
    scene_->addItem(line);

    hasMstEdge_ = true;
}

void NetworkView::fitContent()
{
    if (positions_.isEmpty())
    {
        return;
    }

    // 以全部城市为基准计算包围盒，再向外扩出边权文字的余量。
    double minX = positions_[0].x();
    double maxX = minX;
    double minY = positions_[0].y();
    double maxY = minY;

    for (int i = 1; i < positions_.size(); ++i)
    {
        minX = std::min(minX, positions_[i].x());
        maxX = std::max(maxX, positions_[i].x());
        minY = std::min(minY, positions_[i].y());
        maxY = std::max(maxY, positions_[i].y());
    }

    const QRectF content(minX - kMargin, minY - kMargin,
                         (maxX - minX) + kMargin * 2.0,
                         (maxY - minY) + kMargin * 2.0);
    scene_->setSceneRect(content);
    fitInView(content, Qt::KeepAspectRatio);
}

void NetworkView::wheelEvent(QWheelEvent* event)
{
    // 滚轮缩放，范围限制在 0.2x ~ 5x，避免缩到看不见或放到失真。
    const double current = transform().m11();
    const double factor = (event->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
    const double target = current * factor;

    if (target < 0.2 || target > 5.0)
    {
        event->accept();
        return;
    }

    scale(factor, factor);
    event->accept();
}
