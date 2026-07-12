#pragma once
#include <QGraphicsView>
#include <QMap>
#include <QString>
#include <QVector>

class QGraphicsScene;
class QGraphicsRectItem;
class QGraphicsSimpleTextItem;
class QGraphicsPathItem;
class QMouseEvent;
class QTimer;

namespace maestro::desktop {

// A live node-graph of the orchestration: each task/agent is a node, each
// forwarded-context dependency is an edge. Nodes recolor by state and the graph
// re-lays-out automatically as tasks (including spawned subagents) appear.
//
// v2 additions: running nodes pulse, a message can be animated as a particle
// travelling along an edge (flashMessage), and clicking a node emits
// nodeClicked so the UI can open an inspector / steer that agent.
class GraphCanvas : public QGraphicsView {
    Q_OBJECT
public:
    explicit GraphCanvas(QWidget* parent = nullptr);

public slots:
    void clearGraph();
    void addNode(const QString& name, const QString& provider);
    void setNodeState(const QString& name, const QString& state);
    void addEdge(const QString& from, const QString& to);
    // Animate a dot travelling from -> to along their edge (a message on the
    // "string"). No-op if the edge does not exist.
    void flashMessage(const QString& from, const QString& to);

signals:
    void nodeClicked(const QString& name);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void onPulse();

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
    QTimer* pulseTimer_{nullptr};
    bool pulseOn_{false};
};

} // namespace maestro::desktop
