#include "include/graphwidget.h"

#include <QDebug>
#include <QStatusBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QtXml>
#include <QColorDialog>
#include <QApplication>

#include "include/node.h"
#include "include/edge.h"
#include "include/mainwindow.h"

#include <math.h>

const QColor GraphWidget::m_paper(255,255,153);


GraphWidget::GraphWidget(MainWindow *parent) :
    QGraphicsView(parent),
    m_parent(parent),
    m_activeNode(0),
    m_showingNodeNumbers(false),
    m_hintNode(0),
    m_editingNode(false),
    m_edgeAdding(false),
    m_edgeDeleting(false),
    m_contentChanged(false)
{
    m_scene = new QGraphicsScene(this);
    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    m_scene->setSceneRect(-400, -400, 800, 800);
    setScene(m_scene);

    setCacheMode(CacheBackground);
    setViewportUpdateMode(BoundingRectViewportUpdate);
    setRenderHint(QPainter::Antialiasing);
    setTransformationAnchor(AnchorUnderMouse);
    setMinimumSize(400, 400);
}

void GraphWidget::newScene()
{
    removeAllNodes();
    addFirstNode();
    this->show();
}

void GraphWidget::closeScene()
{
    removeAllNodes();
    this->hide();
}

bool GraphWidget::readContentFromXmlFile(const QString &fileName)
{
    QDomDocument doc("QtMindMap");
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        m_parent->statusBarMsg(tr("Couldn't read file."));
        return false;
    }

    if (!doc.setContent(&file))
    {
        m_parent->statusBarMsg(tr("Couldn't parse XML file."));
        file.close();
        return false;
    }
    file.close();

    removeAllNodes();

    QDomElement docElem = doc.documentElement();

    QDomNodeList nodes = docElem.childNodes().item(0).childNodes();
    for (unsigned int i = 0; i < nodes.length(); i++)
    {
        QDomElement e = nodes.item(i).toElement();
        if(!e.isNull())
        {
            Node *node = new Node(this);
            node->setHtml(e.attribute("htmlContent"));
            m_scene->addItem(node);
            node->setPos(e.attribute("x").toFloat(),
                         e.attribute("y").toFloat());
            node->setScale(e.attribute("scale").toFloat(),sceneRect());
            node->setColor(QColor(e.attribute("bg_red").toFloat(),
                                  e.attribute("bg_green").toFloat(),
                                  e.attribute("bg_blue").toFloat()));
            node->setTextColor(QColor(e.attribute("text_red").toFloat(),
                                      e.attribute("text_green").toFloat(),
                                      e.attribute("text_blue").toFloat()));

            m_nodeList.append(node);
        }
    }

    QDomNodeList edges = docElem.childNodes().item(1).childNodes();
    for (unsigned int i = 0; i < edges.length(); i++)
    {
        QDomElement e = edges.item(i).toElement();
        if(!e.isNull())
        {
            Edge *edge = new Edge(
                        m_nodeList[e.attribute("source").toInt()],
                        m_nodeList[e.attribute("destination").toInt()]);
            edge->setColor(QColor(e.attribute("red").toFloat(),
                                  e.attribute("green").toFloat(),
                                  e.attribute("blue").toFloat()));
            edge->setWidth(e.attribute("width").toFloat());
            edge->setSecondary(e.attribute("secondary").toInt() );

            m_scene->addItem(edge);
        }
    }

    m_activeNode = m_nodeList.first();
    m_activeNode->setActive();
    m_activeNode->setFocus();

    this->show();
    return true;
}

void GraphWidget::writeContentToXmlFile(const QString &fileName)
{
    QDomDocument doc("QtMindMap");

    QDomElement root = doc.createElement("qtmindmap");
    doc.appendChild( root );

    // nodes
    QDomElement nodes_root = doc.createElement("nodes");
    root.appendChild(nodes_root);
    foreach(Node *node, m_nodeList)
    {
        QDomElement cn = doc.createElement("node");

        // no need to store ID: parsing order is preorder.
        // cn.setAttribute( "id", QString::number(m_nodeList.indexOf(node)));
        cn.setAttribute( "x", QString::number(node->pos().x()));
        cn.setAttribute( "y", QString::number(node->pos().y()));
        cn.setAttribute( "htmlContent", node->toHtml());
        cn.setAttribute( "scale", QString::number(node->scale()));
        cn.setAttribute( "bg_red", QString::number(node->color().red()));
        cn.setAttribute( "bg_green", QString::number(node->color().green()));
        cn.setAttribute( "bg_blue", QString::number(node->color().blue()));
        cn.setAttribute( "text_red", QString::number(node->textColor().red()));
        cn.setAttribute( "text_green", QString::number(node->textColor().green()));
        cn.setAttribute( "text_blue", QString::number(node->textColor().blue()));
        nodes_root.appendChild(cn);
    }

    //edges
    QDomElement edges_root = doc.createElement("edges");
    root.appendChild(edges_root);
    foreach(Edge *edge, edges())
    {
        QDomElement cn = doc.createElement("edge");
        cn.setAttribute( "source",
                      QString::number(m_nodeList.indexOf(edge->sourceNode())));
        cn.setAttribute( "destination",
                      QString::number(m_nodeList.indexOf(edge->destNode())));
        cn.setAttribute( "red", QString::number(edge->color().red()));
        cn.setAttribute( "green", QString::number(edge->color().green()));
        cn.setAttribute( "blue", QString::number(edge->color().blue()));
        cn.setAttribute( "width", QString::number(edge->width()));
        cn.setAttribute( "secondary", QString::number(edge->secondary()));

        edges_root.appendChild(cn);
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly))
    {
        m_parent->statusBarMsg(tr("Couldn't open file to write."));
        return;
    }

    QTextStream ts( &file );
    ts << doc.toString();
    file.close();

    m_parent->statusBarMsg(tr("Saved."));
}

void GraphWidget::writeContentToPngFile(const QString &fileName)
{
    QImage img(m_scene->sceneRect().width(),
               m_scene->sceneRect().height(),
               QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&img);

    painter.setRenderHint(QPainter::Antialiasing);

    m_scene->setBackgroundBrush(GraphWidget::m_paper);

    m_scene->render(&painter);
    painter.setBackground(GraphWidget::m_paper);
    painter.end();

    img.save(fileName);

    m_parent->statusBarMsg(tr("MindMap exported as ") + fileName);
}

void GraphWidget::insertPicture(const QString &picture)
{
    if (!m_activeNode)
    {
        m_parent->statusBarMsg(tr("No active node."));
        return;
    }

    m_activeNode->insertPicture(picture);
}

void GraphWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
    {
        nodeLostFocus();
        return;
    }

    if (m_editingNode)
    {
        m_activeNode->keyPressEvent(event);
        return;
    }

    switch (event->key())
    {
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_Left:
    case Qt::Key_Right:

        if (!m_activeNode)
        {
            m_parent->statusBarMsg(tr("No active node."));
            return;
        }

        if (event->modifiers() &  Qt::ControlModifier)
        {
            if (event->modifiers() &  Qt::ShiftModifier)
            {
                QList <Node *> nodeList = m_activeNode->subtree();
                foreach(Node *node, nodeList)
                {
                    if (event->key() == Qt::Key_Up) node->moveBy(0, -20);
                    else if (event->key() == Qt::Key_Down) node->moveBy(0, 20);
                    else if (event->key() == Qt::Key_Left) node->moveBy(-20, 0);
                    else if (event->key() == Qt::Key_Right) node->moveBy(20, 0);
                    contentChanged();
                }
            }
            else
            {
                if (event->key() == Qt::Key_Up) m_activeNode->moveBy(0, -20);
                else if (event->key() == Qt::Key_Down) m_activeNode->moveBy(0, 20);
                else if (event->key() == Qt::Key_Left) m_activeNode->moveBy(-20, 0);
                else if (event->key() == Qt::Key_Right) m_activeNode->moveBy(20, 0);
                contentChanged();
            }
        }
        else // move scene
        {
            QGraphicsView::keyPressEvent(event);
        }
        break;

    case Qt::Key_Plus:

        zoomIn();
        break;

    case Qt::Key_Minus:

        zoomOut();
        break;

        // Hint mode: select a node vimperator style
    case Qt::Key_F:

        hintMode();
        break;

    case Qt::Key_Insert:
        insertNode();

        break;

        // used in node selection mode, to select node with numbers/enter
    case Qt::Key_0:
    case Qt::Key_1:
    case Qt::Key_2:
    case Qt::Key_3:
    case Qt::Key_4:
    case Qt::Key_5:
    case Qt::Key_6:
    case Qt::Key_7:
    case Qt::Key_8:
    case Qt::Key_9:
        if (!m_showingNodeNumbers)
            break;

        m_hintNumber.append(QString::number(event->key()-48));
        showingAllNodeNumbers(false);
        showingNodeNumbersBeginWithNumber(m_hintNumber.toInt(), true);

        break;

        // delete one letter back in node selection
    case Qt::Key_Backspace:
        if (!m_showingNodeNumbers && m_hintNumber.isEmpty())
            break;

        m_hintNumber.remove(m_hintNumber.length()-1,1);
        showNodeNumbers();
        break;

        // in node selection select node if nudenum = enterednum
    case Qt::Key_Return:
    case Qt::Key_Enter:

        if (m_hintNode && m_showingNodeNumbers)
            nodeSelected(m_hintNode);

        break;

    case Qt::Key_F2:

        editNode();
        break;

    case Qt::Key_Delete:

        removeNode();
        break;

    case Qt::Key_A:

        addEdge();
        break;

    case Qt::Key_D:

        removeEdge();
        break;

    case Qt::Key_C:

        nodeColor();
        break;

    case Qt::Key_T:

        nodeTextColor();
        break;

    default:
        QGraphicsView::keyPressEvent(event);
    }
}



void GraphWidget::wheelEvent(QWheelEvent *event)
{
    event->delta() > 0 ?
            zoomIn() :
            zoomOut();
}

void GraphWidget::drawBackground(QPainter *painter, const QRectF &rect)
{
    Q_UNUSED(rect);

    painter->fillRect(m_scene->sceneRect(), GraphWidget::m_paper);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(m_scene->sceneRect());
}

void GraphWidget::scaleView(qreal scaleFactor)
{
    qreal factor = transform().scale(scaleFactor, scaleFactor).
            mapRect(QRectF(0, 0, 1, 1)).width();
    if (factor < 0.2 || factor > 10) return;

    scale(scaleFactor, scaleFactor);
}

void GraphWidget::setActiveNode(Node *node)
{
    if (m_activeNode!=0)
        m_activeNode->setActive(false);

    m_activeNode = node;
    m_activeNode->setActive();
}

void GraphWidget::zoomIn()
{
    if (QApplication::keyboardModifiers() &  Qt::ControlModifier)
    {
        if (!m_activeNode)
        {
            m_parent->statusBarMsg(tr("No active node."));
            return;
        }

        if (QApplication::keyboardModifiers() &  Qt::ShiftModifier)
        {
            QList <Node *> nodeList = m_activeNode->subtree();
            foreach(Node *node, nodeList)
                node->setScale(qreal(1.2),sceneRect());
        }
        else
        {
            m_activeNode->setScale(qreal(1.2),sceneRect());
        }
    }
    else
    {
        scaleView(qreal(1.2));
    }
}

void GraphWidget::zoomOut()
{
    if (QApplication::keyboardModifiers() &  Qt::ControlModifier)
    {
        if (!m_activeNode)
        {
            m_parent->statusBarMsg(tr("No active node."));
            return;
        }

        if (QApplication::keyboardModifiers() &  Qt::ShiftModifier)
        {
            QList <Node *> nodeList = m_activeNode->subtree();
            foreach(Node *node, nodeList)
                node->setScale(qreal(1 / 1.2),sceneRect());
        }
        else
        {
            m_activeNode->setScale(qreal(1 / 1.2),sceneRect());
        }
    }
    else
    {
        scaleView(qreal(1 / 1.2));
    }
}

void GraphWidget::insertNode()
{
    if (!m_activeNode)
    {
        m_parent->statusBarMsg(tr("No active node."));
        return;
    }

    double angle(m_activeNode->calculateBiggestAngle());

    qreal length(100);

    QPointF pos(length * cos(angle), length * sin(angle));

    Node *node = new Node(this);
    node->setColor(m_activeNode->color());
    node->setTextColor(m_activeNode->textColor());
    node->setHtml(QString(""));
    m_scene->addItem(node);
    node->setPos(m_activeNode->sceneBoundingRect().center() +
                 pos -
                 node->boundingRect().center());
    m_nodeList.append(node);

    addEdge(m_activeNode, node);

    setActiveNode(node);
    editNode();

    contentChanged();
    if (m_showingNodeNumbers)
        showNodeNumbers();
}

void GraphWidget::removeNode()
{
    if (!m_activeNode)
    {
        m_parent->statusBarMsg(tr("No active node."));
        return;
    }

    if (m_activeNode == m_nodeList.first())
    {
        m_parent->statusBarMsg(tr("Base node cannot be deleted."));
        return;
    }

    QList <Node *> nodeList;
    if (QApplication::keyboardModifiers() & Qt::ControlModifier &&
        QApplication::keyboardModifiers() & Qt::ShiftModifier)
    {
        nodeList = m_activeNode->subtree();
    }
    else
    {
        nodeList.push_back(m_activeNode);
    }

    foreach(Node *node, nodeList)
    {
        if (m_hintNode==node)
            m_hintNode=0;

        m_nodeList.removeAll(node);
        delete node;
    }

    m_activeNode = 0;
    contentChanged();

    if (m_showingNodeNumbers)
        showNodeNumbers();
}

void GraphWidget::editNode()
{
    if (!m_activeNode)
    {
        m_parent->statusBarMsg(tr("No active node."));
        return;
    }

    m_editingNode = true;
    m_activeNode->setEditable();
    m_scene->setFocusItem(m_activeNode);
}

void GraphWidget::nodeColor()
{
    if (!m_activeNode)
    {
        m_parent->statusBarMsg(tr("No active node."));
        return;
    }

    QList <Node *> nodeList;
    if (QApplication::keyboardModifiers() & Qt::ControlModifier &&
        QApplication::keyboardModifiers() & Qt::ShiftModifier)
    {
        nodeList = m_activeNode->subtree();
    }
    else
    {
        nodeList.push_back(m_activeNode);
    }

    QColorDialog dialog(this);
    dialog.setWindowTitle(tr("Select node color"));
    dialog.setCurrentColor(m_activeNode->color());
    if (dialog.exec())
    {
        QColor color = dialog.selectedColor();
        foreach(Node *node, nodeList)
        {
            node->setColor(color);
            foreach (Edge * edge, node->edgesToThis(false))
                edge->setColor(color);
        }
    }
}

void GraphWidget::nodeTextColor()
{
    if (!m_activeNode)
    {
        m_parent->statusBarMsg(tr("No active node."));
        return;
    }

    QList <Node *> nodeList;
    if (QApplication::keyboardModifiers() & Qt::ControlModifier &&
        QApplication::keyboardModifiers() & Qt::ShiftModifier)
    {
        nodeList = m_activeNode->subtree();
    }
    else
    {
        nodeList.push_back(m_activeNode);
    }

    QColorDialog dialog(this);
    dialog.setWindowTitle(tr("Select text color"));
    dialog.setCurrentColor(m_activeNode->textColor());
    if (dialog.exec())
    {
        QColor color = dialog.selectedColor();
        foreach(Node *node, nodeList)
            node->setTextColor(color);
    }
}

void GraphWidget::addEdge()
{
    m_parent->statusBarMsg(tr("Add edge: select destination node."));
    m_edgeAdding = true;
}

void GraphWidget::removeEdge()
{
    m_parent->statusBarMsg(tr("Delete edge: select other end-node."));
    m_edgeDeleting = true;
}

void GraphWidget::hintMode()
{
    m_showingNodeNumbers = !m_showingNodeNumbers;
    if (!m_showingNodeNumbers)
    {
        showingAllNodeNumbers(false);
        return;
    }

    m_hintNumber.clear();
    showNodeNumbers();
}

void GraphWidget::showingAllNodeNumbers(const bool &show)
{
    int i =0;
    for (QList<Node *>::const_iterator it = m_nodeList.begin();
         it != m_nodeList.end(); it++, i++)
        dynamic_cast<Node*>(*it)->showNumber(i,show);
}

void GraphWidget::showingNodeNumbersBeginWithNumber(const int &number,
                                                    const bool &show)
{
    int i(0);
    int hit(0);
    for (QList<Node *>::const_iterator it = m_nodeList.begin();
         it != m_nodeList.end(); it++, i++)
    {
        if (i == number)
        {
            hit++;
            dynamic_cast<Node*>(*it)->showNumber(i,show,true);
            m_hintNode = dynamic_cast<Node*>(*it);
            continue;
        }
        if (numberStartsWithNumber(i, number))
        {
            hit++;
            dynamic_cast<Node*>(*it)->showNumber(i,show);
        }
    }
    if (hit==1)
    {
        nodeSelected(m_hintNode);
    }
    else if (hit == 0)
    {
        m_showingNodeNumbers = false;
        showingAllNodeNumbers(false);
    }
}

bool GraphWidget::numberStartsWithNumber(const int &number, const int &prefix)
{
    return (QString::number(number)).startsWith(QString::number(prefix));
}

void GraphWidget::nodeSelected(Node *node)
{
    showingAllNodeNumbers(false);
    m_showingNodeNumbers = false;

    node->setEditable(false);
    m_editingNode = false;

    if (m_edgeAdding)
    {
        addEdge(m_activeNode, node);
        m_edgeAdding = false;
    }
    else if (m_edgeDeleting)
    {
        removeEdge(m_activeNode, node);
        m_edgeDeleting = false;
    }
    else
    {
        setActiveNode(node);
    }
}

void GraphWidget::nodeMoved(QGraphicsSceneMouseEvent *event)
{
    QList <Node *> nodeList;
    if (event->modifiers() & Qt::ControlModifier &&
        event->modifiers() & Qt::ShiftModifier)
    {
        nodeList = m_activeNode->subtree();
    }
    else
    {
        nodeList.push_back(m_activeNode);
    }

    foreach(Node *node, nodeList)
        node->setPos(node->pos() + event->scenePos() - event->lastScenePos());
}

void GraphWidget::nodeLostFocus()
{
    if (m_editingNode)
    {
        m_editingNode = false;
        m_activeNode->setEditable(false);
        m_activeNode->update();
    }
    else if (m_edgeAdding)
    {
        m_edgeAdding = false;
        m_parent->statusBarMsg(tr("Edge adding cancelled."));
    }
    else if (m_edgeDeleting)
    {
        m_edgeDeleting = false;
        m_parent->statusBarMsg(tr("Edge deleting cancelled."));
    }
    else if(m_showingNodeNumbers)
    {
        m_hintNumber.clear();
        showingAllNodeNumbers(false);
        m_showingNodeNumbers = false;
    }
}

void GraphWidget::addEdge(Node *source, Node *destination)
{
    if (destination == m_nodeList.first())
    {
        m_parent->statusBarMsg(
                    tr("Root element cannot be an edge target."));
        return;
    }

    if (source->isConnected(destination))
    {
        m_parent->statusBarMsg(
                    tr("There is already an edge between these two nodes."));
    }
    else
    {
        bool sec(false);
        if (!destination->edgesToThis().empty())
        {
            m_parent->statusBarMsg(
                     tr("The graph is acyclic, edge added as secondary edge."));
            sec = true;
        }
        Edge *edge = new Edge(source, destination);
        edge->setColor(destination->color());
        edge->setWidth(destination->scale()*2 + 1);
        edge->setSecondary(sec);

        m_scene->addItem(edge);
        contentChanged();
    }
}

void GraphWidget::removeEdge(Node *source, Node *destination)
{
    if (!source->isConnected(destination))
    {
        m_parent->statusBarMsg(tr("There no edge between these two nodes."));
    }
    else
    {
        source->deleteEdge(destination);
        contentChanged();
    }
}

void GraphWidget::showNodeNumbers()
{
    if (m_hintNumber.isEmpty())
     {
         showingAllNodeNumbers(true);
         m_nodeList.first()->showNumber(0,true,true);
         m_hintNode = m_nodeList.first();
     }
     else
     {
         showingAllNodeNumbers(false);
         showingNodeNumbersBeginWithNumber(m_hintNumber.toInt(), true);
     }
}

void GraphWidget::removeAllNodes()
{
    foreach(Node *node, m_nodeList) delete node;
    m_nodeList.clear();
    m_activeNode = 0;
    m_hintNode = 0;
}

void GraphWidget::addFirstNode()
{
    Node *node = new Node(this);
    node->setHtml(
                QString("<img src=:/qtmindmap.svg width=50 height=50></img>"));
    m_scene->addItem(node);

    m_nodeList.append(node);

    m_activeNode = m_nodeList.first();
    m_activeNode->setActive();
}

QList<Edge *> GraphWidget::edges() const
{
    QList<Edge *> list;

    foreach(Node * node, m_nodeList)
        list.append(node->edgesFrom(false));

    return list;
}

void GraphWidget::contentChanged(const bool &changed)
{
    m_parent->contentChanged(changed);
}