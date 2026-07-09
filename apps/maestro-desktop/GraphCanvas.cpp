#include "GraphCanvas.hpp"

#include <QBrush>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QPainterPath>
#include <QPen>

#include <algorithm>

namespace maestro::desktop {

namespace {
constexpr qreal kNodeW = 170.0;
constexpr qreal kNodeH = 56.0;
constexpr qreal kColGap = 240.0;
constexpr qreal kRowGap = 84.0;
constexpr qreal kMargin = 30.0;
} // namespace

GraphCanvas::GraphCanvas(QWidget* parent) : QGraphicsView(parent) {
    scene_ = new QGraphicsScene(this);
    setScene(scene_);
    setRenderHint(QPainter::Antialiasing, true);
    setBackgroundBrush(QColor("#1a1b1e"));
    setDragMode(QGraphicsView::ScrollHandDrag);
}

QColor GraphCanvas::colorForState(const QString& state) {
    if (state == "running") return QColor("#4c6ef5");
    if (state == "succeeded") return QColor("#37b24d");
    if (state == "failed") return QColor("#e03131");
    if (state == "blocked") return QColor("#868e96");
    if (state == "retrying") return QColor("#f59f00");
    return QColor("#3a3d42"); // pending
}

void GraphCanvas::clearGraph() {
    scene_->clear();
    nodes_.clear();
    edges_.clear();
    scene_->setSceneRect(0, 0, 400, 300);
}

void GraphCanvas::addNode(const QString& name, const QString& provider) {
    if (nodes_.contains(name)) {
        return;
    }
    Node node;
    node.provider = provider;
    node.box = scene_->addRect(0, 0, kNodeW, kNodeH, QPen(QColor("#4c6ef5"), 2),
                               QBrush(colorForState("pending")));
    node.box->setZValue(1);
    node.title = scene_->addSimpleText(name);
    node.title->setBrush(QBrush(Qt::white));
    node.title->setZValue(2);
    node.subtitle = scene_->addSimpleText(provider + " · pending");
    node.subtitle->setBrush(QBrush(QColor("#c1c2c5")));
    node.subtitle->setZValue(2);
    nodes_.insert(name, node);
    relayout();
}

void GraphCanvas::setNodeState(const QString& name, const QString& state) {
    const auto it = nodes_.find(name);
    if (it == nodes_.end()) {
        return;
    }
    it->state = state;
    it->box->setBrush(QBrush(colorForState(state)));
    it->subtitle->setText(it->provider + " · " + state);
    // Highlight outgoing edges once a node has produced output.
    if (state == "succeeded") {
        for (Edge& e : edges_) {
            if (e.from == name && e.path) {
                e.path->setPen(QPen(QColor("#37b24d"), 2.5));
            }
        }
    }
}

void GraphCanvas::addEdge(const QString& from, const QString& to) {
    Edge edge;
    edge.from = from;
    edge.to = to;
    edge.path = scene_->addPath(QPainterPath(), QPen(QColor("#5c5f66"), 2.0));
    edge.path->setZValue(0);
    edges_.push_back(edge);
    relayout();
}

void GraphCanvas::updateEdgePath(Edge& edge) {
    const auto from = nodes_.find(edge.from);
    const auto to = nodes_.find(edge.to);
    if (from == nodes_.end() || to == nodes_.end() || !edge.path) {
        return;
    }
    const QPointF a = from->box->pos() + QPointF(kNodeW, kNodeH / 2.0);
    const QPointF b = to->box->pos() + QPointF(0, kNodeH / 2.0);
    QPainterPath path(a);
    const qreal midX = (a.x() + b.x()) / 2.0;
    path.cubicTo(QPointF(midX, a.y()), QPointF(midX, b.y()), b); // smooth curve
    edge.path->setPath(path);
}

void GraphCanvas::relayout() {
    // Compute depth = longest path from a root along edges (fixpoint).
    for (auto& node : nodes_) {
        node.depth = 0;
    }
    for (int iter = 0; iter < nodes_.size() + 1; ++iter) {
        bool changed = false;
        for (const Edge& e : edges_) {
            const auto from = nodes_.find(e.from);
            const auto to = nodes_.find(e.to);
            if (from == nodes_.end() || to == nodes_.end()) {
                continue;
            }
            if (to->depth < from->depth + 1) {
                to->depth = from->depth + 1;
                changed = true;
            }
        }
        if (!changed) {
            break;
        }
    }

    // Place nodes in columns by depth, stacked by insertion order.
    QMap<int, int> rowInColumn;
    for (auto it = nodes_.begin(); it != nodes_.end(); ++it) {
        const int depth = it->depth;
        const int row = rowInColumn.value(depth, 0);
        rowInColumn[depth] = row + 1;
        const qreal x = kMargin + static_cast<qreal>(depth) * kColGap;
        const qreal y = kMargin + static_cast<qreal>(row) * kRowGap;
        it->box->setPos(x, y);
        it->title->setPos(x + 10, y + 8);
        it->subtitle->setPos(x + 10, y + 30);
    }

    for (Edge& e : edges_) {
        updateEdgePath(e);
    }

    scene_->setSceneRect(scene_->itemsBoundingRect().adjusted(-40, -40, 40, 40));
}

} // namespace maestro::desktop
