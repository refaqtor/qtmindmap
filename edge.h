#ifndef EDGE_H
#define EDGE_H

#include <QGraphicsItem>

class Node;

class Edge : public QGraphicsItem
{
public:

    Edge(Node *sourceNode, Node *destNode);
    ~Edge();

    Node *sourceNode() const;
    Node *destNode() const;
    double getAngle() const;

    void adjust();

protected:

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

private:

    Node *m_sourceNode;
    Node *m_destNode;

    QPointF m_sourcePoint;
    QPointF m_destPoint;
    qreal m_arrowSize;
    double m_angle;
};

#endif
