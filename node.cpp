#include "node.h"

#include <QPainter>
#include <QStyleOption>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>
#include <QTextDocument>

const double Node::m_pi = 3.14159265358979323846264338327950288419717;
const double Node::m_oneAndHalfPi = Node::m_pi * 1.5;
const double Node::m_twoPi = Node::m_pi * 2.0;

Node::Node(GraphWidget *parent) :
    m_graph(parent),
    m_isActive(false),
    m_number(-1),
    m_hasBorder(true),
    m_numberIsSpecial(false)
{
    setFlag(ItemIsMovable);
    setFlag(ItemSendsGeometryChanges);
    setCacheMode(DeviceCoordinateCache);
    setDefaultTextColor(QColor(0,0,0));
}

Node::~Node()
{
    // dtor of Edge will call removeEdgeFromList on booth nodes.
    foreach (EdgeElement element, m_edgeList) delete element.edge;
}

void Node::addEdge(Edge *edge, bool startsFromThisNode)
{
    m_edgeList.push_back(EdgeElement(edge, startsFromThisNode));
    edge->adjust();
}

void Node::deleteEdge(Node *otherEnd)
{
    for(QList<EdgeElement>::iterator it = m_edgeList.begin();
        it != m_edgeList.end(); it++)
    {
        if ((it->edge->sourceNode() == otherEnd &&
             it->edge->destNode() == this)
         || (it->edge->sourceNode() == this &&
             it->edge->destNode() == otherEnd))
        {
            delete it->edge;
            return;
        }
    }
}

void Node::removeEdgeFromList(Edge *edge)
{
    for(QList<EdgeElement>::iterator it = m_edgeList.begin();
        it != m_edgeList.end(); it++)
    {
        if (it->edge == edge)
        {
            m_edgeList.erase(it);
            return;
        }
    }
}

void Node::setBorder(const bool &hasBorder)
{
   m_hasBorder = hasBorder;
   update();
}

void Node::setActive(const bool &active)
{
    m_isActive = active;
    update();
}

void Node::setEditable(const bool &editable)
{
    setTextInteractionFlags(
                editable ?
                    Qt::TextEditable :
                    Qt::NoTextInteraction);

    QTextCursor c = textCursor();
    c.setPosition(c.document()->toPlainText().length());
    setTextCursor(c);
}

void Node::showNumber(const int &number,
                      const bool& show,
                      const bool &numberIsSpecial)
{
    m_number = show ? number : -1;
    m_numberIsSpecial = numberIsSpecial;
    update();
}

double Node::calculateBiggestAngle()
{
    if (m_edgeList.empty())
        return Node::m_oneAndHalfPi;

    if (m_edgeList.size()==1)
        return m_edgeList.first().startsFromThisNode ?
                    Node::m_pi - m_edgeList.first().edge->getAngle() :
                    Node::m_twoPi - m_edgeList.first().edge->getAngle();

    QList<double> tmp;
    for(QList<EdgeElement>::iterator it = m_edgeList.begin();
        it != m_edgeList.end(); it++)
    {
        tmp.push_back(it->startsFromThisNode ?
                 it->edge->getAngle() :
                 doubleModulo(Node::m_pi + it->edge->getAngle(), Node::m_twoPi));
    }
    qSort(tmp.begin(), tmp.end());

    double prev(tmp.first());
    double max_prev(tmp.last());
    double max(Node::m_twoPi - tmp.last() + tmp.first());

    for(QList<double>::const_iterator it = ++tmp.begin(); it!=tmp.end(); it++)
    {
        if (*it - prev > max )
        {
            max = *it - prev;
            max_prev = prev;
        }
        prev = *it;
    }

    return Node::m_twoPi - doubleModulo(max_prev + max / 2, Node::m_twoPi);
}

void Node::keyPressEvent(QKeyEvent *event)
{
    // cursor movements
    switch (event->key()) {

    case Qt::Key_Left:
    {
        QTextCursor c = textCursor(); // textcursor() return just a copy
        c.movePosition(
                    event->modifiers() == Qt::ControlModifier ?
                    QTextCursor::PreviousWord :
                    QTextCursor::PreviousCharacter);
        setTextCursor(c);
        break;
    }
    case Qt::Key_Right:
    {
        QTextCursor c = textCursor();
        c.movePosition(
                    event->modifiers() == Qt::ControlModifier ?
                    QTextCursor::NextWord :
                    QTextCursor::NextCharacter);
        setTextCursor(c);
        break;
    }
    case Qt::Key_Up:
    {
        QTextCursor c = textCursor();
        c.movePosition(QTextCursor::Up);
        setTextCursor(c);
        break;
    }
    case Qt::Key_Down:
    {
        QTextCursor c = textCursor();
        c.movePosition(QTextCursor::Down);
        setTextCursor(c);
        break;
    }

    default:

        // not cursor movement: editing
        QGraphicsTextItem::keyPressEvent(event);
        m_graph->contentChanged();
        foreach (EdgeElement element, m_edgeList) element.edge->adjust();
    }

    ///@note leaving editing mode is done with esc, handled by graphwidget
}

bool Node::isConnected(const Node *node) const
{
    foreach (EdgeElement element, m_edgeList)
        if (element.edge->sourceNode() == node ||
                element.edge->destNode() == node)
            return true;

    return false;
}

QList<Edge *> Node::edgesFrom() const
{
    QList<Edge *> list;

    foreach(EdgeElement element, m_edgeList)
        if (element.startsFromThisNode)
            list.push_back(element.edge);

    return list;
}

void Node::paint(QPainter *painter,
                 const QStyleOptionGraphicsItem *option,
                 QWidget *w)
{
    // draw background in hint mode. num == -1 : not in hint mode
    // if m_numberIsSpecial (can be selected with enter) bg is green, not yellow
    if (m_number != -1)
    {
        painter->setPen(Qt::transparent);
        painter->setBrush(m_numberIsSpecial ? Qt::green : Qt::yellow);

        /// @bug is there a 1pixel wide yellow line at the
        /// bottom of borderless items?
        painter->drawRect(QRect(boundingRect().topLeft().toPoint(),
                          boundingRect().bottomRight().toPoint()));
        painter->setBrush(Qt::NoBrush);
    }

    // the text itself
    QGraphicsTextItem::paint(painter, option, w);

    if (m_hasBorder)
    {
        painter->setPen(m_isActive ? Qt::red : Qt::blue);
        painter->drawRect(QRect(boundingRect().topLeft().toPoint(),
                          boundingRect().bottomRight().toPoint() -
                          QPoint(1,1)));
    }

    // print num to topleft corner in hint mode.
    if (m_number != -1)
    {
        painter->setPen(Qt::white);
        painter->setBackground(Qt::red);
        painter->setBackgroundMode(Qt::OpaqueMode);
        painter->drawText(boundingRect().topLeft()+QPointF(0,11),
                          QString("%1").arg(m_number));
    }
}

QVariant Node::itemChange(GraphicsItemChange change, const QVariant &value)
{
    switch (change) {

    case ItemPositionChange:

        if (change == ItemPositionChange && scene())
        {
            // value is the new position.
            QPointF newPos = value.toPointF();

            // the fence is reduced with the size of the node
            QRectF rect (scene()->sceneRect().topLeft(),
                         scene()->sceneRect().bottomRight() -
                            boundingRect().bottomRight());

            if (!rect.contains(newPos))
            {
                // Keep the item inside the scene rect.
                newPos.setX(qMin(rect.right(), qMax(newPos.x(), rect.left())));
                newPos.setY(qMin(rect.bottom(), qMax(newPos.y(), rect.top())));
                return newPos;
            }
        }
        break;

    case ItemPositionHasChanged:

        m_graph->contentChanged();
        foreach (EdgeElement element, m_edgeList) element.edge->adjust();
        break;

    default:
        break;
    };

    return QGraphicsItem::itemChange(change, value);
}

void Node::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_graph->nodeSelected(this);

    QGraphicsItem::mousePressEvent(event);
}

void Node::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    m_graph->setActiveNodeEditable();
}

void Node::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseReleaseEvent(event);
}

void Node::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseMoveEvent(event);
}

double Node::doubleModulo(const double &devided, const double &devisor)
{
    return devided - static_cast<double>(devisor * static_cast<int>(devided
                                                                    / devisor));
}
