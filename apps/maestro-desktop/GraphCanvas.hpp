#pragma once
#include <QGraphicsView>
#include <QMap>
#include <QString>
#include <QVector>

class QGraphicsScene;
class QGraphicsRectItem;
class QGraphicsSimpleTextItem;
class QGraphicsPathItem;

namespace maestro::desktop {

// A live node-graph of the orchestration: each task/agent is a node, each
// forwarded-context dependency is an edge. Nodes recolor by state and the graph
// re-lays-out automatically as tasks (including spawned subagents) appear.
class GraphCanvas : public QGraphicsView {
    Q_OBJECT
public:
    explicit GraphCanvas(QWidget* parent = nullptr);

public slots:
    void clearGraph();
    void addNode(const QString& name, const QString& provider);
    void setNodeState(const QString& name, const QString& state);
    void addEdge(const QString& from, const QString& to);

private:
    struct Node {
        QGraphicsRectItem* box{nullptr};
        QGraphicsSimpleTextItem* title{nullptr};
        QGraphicsSimpleTextItem* subtitle{nullptr};
        QString provider;
        QString state{"pending"};
        int depth{0};
    };
    struct Edge {
        QString from;
        QString to;
        QGraphicsPathItem* path{nullptr};
    };

    void relayout();
    void updateEdgePath(Edge& edge);
    [[nodiscard]] static QColor colorForState(const QString& state);

    QGraphicsScene* scene_{nullptr};
    QMap<QString, Node> nodes_;
    QVector<Edge> edges_;
};

} // namespace maestro::desktop
