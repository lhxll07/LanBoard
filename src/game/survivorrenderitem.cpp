#include "survivorrenderitem.h"

#include "survivorcontroller.h"

#include <QColor>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGNode>
#include <QSGVertexColorMaterial>
#include <cstring>
#include <QtMath>

namespace {

using Vertex = QSGGeometry::ColoredPoint2D;

Vertex makeVertex(qreal x, qreal y, const QColor &color)
{
    Vertex vertex;
    vertex.set(static_cast<float>(x),
               static_cast<float>(y),
               static_cast<uchar>(color.red()),
               static_cast<uchar>(color.green()),
               static_cast<uchar>(color.blue()),
               static_cast<uchar>(color.alpha()));
    return vertex;
}

void appendLine(QVector<Vertex> &vertices,
                const QPointF &from,
                const QPointF &to,
                const QColor &color)
{
    vertices.append(makeVertex(from.x(), from.y(), color));
    vertices.append(makeVertex(to.x(), to.y(), color));
}

void appendTriangle(QVector<Vertex> &vertices,
                    const QPointF &a,
                    const QPointF &b,
                    const QPointF &c,
                    const QColor &color)
{
    vertices.append(makeVertex(a.x(), a.y(), color));
    vertices.append(makeVertex(b.x(), b.y(), color));
    vertices.append(makeVertex(c.x(), c.y(), color));
}

void appendRect(QVector<Vertex> &vertices,
                const QRectF &rect,
                const QColor &color)
{
    const QPointF topLeft = rect.topLeft();
    const QPointF topRight = rect.topRight();
    const QPointF bottomLeft = rect.bottomLeft();
    const QPointF bottomRight = rect.bottomRight();
    appendTriangle(vertices, topLeft, bottomLeft, topRight, color);
    appendTriangle(vertices, topRight, bottomLeft, bottomRight, color);
}

void appendRectOutline(QVector<Vertex> &vertices,
                       const QRectF &rect,
                       const QColor &color)
{
    const QPointF topLeft = rect.topLeft();
    const QPointF topRight = rect.topRight();
    const QPointF bottomLeft = rect.bottomLeft();
    const QPointF bottomRight = rect.bottomRight();
    appendLine(vertices, topLeft, topRight, color);
    appendLine(vertices, topRight, bottomRight, color);
    appendLine(vertices, bottomRight, bottomLeft, color);
    appendLine(vertices, bottomLeft, topLeft, color);
}

void appendRegularPolygon(QVector<Vertex> &vertices,
                          const QPointF &center,
                          qreal radius,
                          int sides,
                          const QColor &color,
                          qreal rotationDeg = 0.0)
{
    if (sides < 3 || radius <= 0.0)
        return;

    const qreal step = 360.0 / sides;
    QPointF previousPoint;
    for (int index = 0; index <= sides; ++index) {
        const qreal angle = qDegreesToRadians(rotationDeg + step * index);
        const QPointF point(center.x() + qCos(angle) * radius,
                            center.y() + qSin(angle) * radius);
        if (index > 0)
            appendTriangle(vertices, center, previousPoint, point, color);
        previousPoint = point;
    }
}

void appendRegularPolygonOutline(QVector<Vertex> &vertices,
                                 const QPointF &center,
                                 qreal radius,
                                 int sides,
                                 const QColor &color,
                                 qreal rotationDeg = 0.0)
{
    if (sides < 3 || radius <= 0.0)
        return;

    const qreal step = 360.0 / sides;
    QPointF firstPoint;
    QPointF previousPoint;
    for (int index = 0; index < sides; ++index) {
        const qreal angle = qDegreesToRadians(rotationDeg + step * index);
        const QPointF point(center.x() + qCos(angle) * radius,
                            center.y() + qSin(angle) * radius);
        if (index == 0)
            firstPoint = point;
        else
            appendLine(vertices, previousPoint, point, color);
        previousPoint = point;
    }
    appendLine(vertices, previousPoint, firstPoint, color);
}

void appendDiamond(QVector<Vertex> &vertices,
                   const QPointF &center,
                   qreal radius,
                   const QColor &color)
{
    const QPointF top(center.x(), center.y() - radius);
    const QPointF right(center.x() + radius, center.y());
    const QPointF bottom(center.x(), center.y() + radius);
    const QPointF left(center.x() - radius, center.y());
    appendTriangle(vertices, top, left, right, color);
    appendTriangle(vertices, right, left, bottom, color);
}

void appendDiamondOutline(QVector<Vertex> &vertices,
                          const QPointF &center,
                          qreal radius,
                          const QColor &color)
{
    const QPointF top(center.x(), center.y() - radius);
    const QPointF right(center.x() + radius, center.y());
    const QPointF bottom(center.x(), center.y() + radius);
    const QPointF left(center.x() - radius, center.y());
    appendLine(vertices, top, right, color);
    appendLine(vertices, right, bottom, color);
    appendLine(vertices, bottom, left, color);
    appendLine(vertices, left, top, color);
}

QColor withAlpha(const QColor &color, int alpha)
{
    QColor copy = color;
    copy.setAlpha(alpha);
    return copy;
}

void appendLayeredPolygon(QVector<Vertex> &fillVertices,
                          QVector<Vertex> &lineVertices,
                          const QPointF &center,
                          qreal radius,
                          int sides,
                          qreal rotationDeg,
                          const QColor &baseColor,
                          const QColor &innerColor,
                          const QColor &outlineColor,
                          const QColor &shadowColor)
{
    if (radius <= 0.0)
        return;

    appendRegularPolygon(fillVertices,
                         QPointF(center.x(), center.y() + radius * 0.16),
                         radius * 1.03,
                         sides,
                         shadowColor,
                         rotationDeg);
    appendRegularPolygon(fillVertices, center, radius, sides, baseColor, rotationDeg);
    appendRegularPolygon(fillVertices,
                         QPointF(center.x() - radius * 0.14, center.y() - radius * 0.18),
                         radius * 0.58,
                         qMax(4, sides - 2),
                         innerColor,
                         rotationDeg);
    appendRegularPolygonOutline(lineVertices, center, radius, sides, outlineColor, rotationDeg);
}

void appendLayeredDiamond(QVector<Vertex> &fillVertices,
                          QVector<Vertex> &lineVertices,
                          const QPointF &center,
                          qreal radius,
                          const QColor &baseColor,
                          const QColor &innerColor,
                          const QColor &outlineColor,
                          const QColor &shadowColor)
{
    if (radius <= 0.0)
        return;

    appendDiamond(fillVertices,
                  QPointF(center.x(), center.y() + radius * 0.16),
                  radius * 1.04,
                  shadowColor);
    appendDiamond(fillVertices, center, radius, baseColor);
    appendDiamond(fillVertices,
                  QPointF(center.x() - radius * 0.12, center.y() - radius * 0.14),
                  radius * 0.54,
                  innerColor);
    appendDiamondOutline(lineVertices, center, radius, outlineColor);
}

QColor enemyFillColor(int kind)
{
    switch (kind) {
    case 0: return QColor("#5A7284");
    case 1: return QColor("#B78A69");
    case 2: return QColor("#F0E5D6");
    case 3: return QColor("#8E6C5B");
    case 4: return QColor("#8BA06E");
    case 5: return QColor("#6F5E53");
    default: return QColor("#A47052");
    }
}

QColor enemyOutlineColor(int kind)
{
    switch (kind) {
    case 2: return QColor("#403730");
    default: return QColor("#2C2621");
    }
}

QColor enemyAccentColor(int kind)
{
    switch (kind) {
    case 0: return QColor("#D05549");
    case 1: return QColor("#EBDDC8");
    case 2: return QColor("#C94D47");
    case 3: return QColor("#D6B37C");
    case 4: return QColor("#CDE094");
    case 5: return QColor("#E4C080");
    default: return QColor("#E0B36C");
    }
}

QColor jadeColor(int kind)
{
    switch (kind) {
    case 2: return QColor("#D8504D");
    case 1: return QColor("#72C96A");
    default: return QColor("#4BA6E7");
    }
}

bool circlesOverlap(const QVector2D &lhs, const QVector2D &rhs, qreal combinedRadius)
{
    return (lhs - rhs).lengthSquared() <= combinedRadius * combinedRadius;
}

QPointF clampToRadar(qreal dx, qreal dy, qreal range, bool *faded)
{
    const qreal distanceSquared = dx * dx + dy * dy;
    if (distanceSquared <= range * range) {
        *faded = false;
        return QPointF(dx, dy);
    }

    *faded = true;
    const qreal distance = qSqrt(distanceSquared);
    const qreal ratio = range / qMax(distance, 0.0001);
    return QPointF(dx * ratio, dy * ratio);
}

struct GeometryLayerNode : public QSGGeometryNode
{
    GeometryLayerNode(QSGGeometry::DrawingMode mode)
        : geometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0)
    {
        geometry.setDrawingMode(mode);
        geometry.setLineWidth(1.0f);
        setGeometry(&geometry);
        setFlag(QSGNode::OwnsGeometry, false);
        setMaterial(&material);
        setFlag(QSGNode::OwnsMaterial, false);
    }

    void updateVertices(const QVector<Vertex> &vertices)
    {
        geometry.allocate(vertices.size());
        if (!vertices.isEmpty()) {
            std::memcpy(geometry.vertexData(),
                        vertices.constData(),
                        static_cast<size_t>(vertices.size()) * sizeof(Vertex));
        }
        markDirty(QSGNode::DirtyGeometry);
    }

    QSGGeometry geometry;
    QSGVertexColorMaterial material;
};

struct SceneRootNode : public QSGNode
{
    SceneRootNode()
    {
        fillNode = new GeometryLayerNode(QSGGeometry::DrawTriangles);
        lineNode = new GeometryLayerNode(QSGGeometry::DrawLines);
        appendChildNode(fillNode);
        appendChildNode(lineNode);
    }

    GeometryLayerNode *fillNode = nullptr;
    GeometryLayerNode *lineNode = nullptr;
};

}

SurvivorRenderItem::SurvivorRenderItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

QObject *SurvivorRenderItem::controller() const
{
    return m_controller;
}

void SurvivorRenderItem::setController(QObject *controllerObject)
{
    SurvivorController *controller = qobject_cast<SurvivorController *>(controllerObject);
    if (m_controller == controller)
        return;

    if (m_controller)
        disconnect(m_controller, nullptr, this, nullptr);

    m_controller = controller;
    m_frameCounter = 0;
    attachControllerSignals();
    update();
    emit controllerChanged();
}

void SurvivorRenderItem::setRadarMode(bool value)
{
    if (m_radarMode == value)
        return;
    m_radarMode = value;
    update();
    emit radarModeChanged();
}

void SurvivorRenderItem::setCompactLayout(bool value)
{
    if (m_compactLayout == value)
        return;
    m_compactLayout = value;
    update();
    emit compactLayoutChanged();
}

QSGNode *SurvivorRenderItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    if (!m_controller)
        return oldNode;

    SceneRootNode *root = static_cast<SceneRootNode *>(oldNode);
    if (!root)
        root = new SceneRootNode();

    QVector<Vertex> fillVertices;
    QVector<Vertex> lineVertices;

    const auto &currentSnapshot = m_controller->renderSnapshot();

    if (m_radarMode) {
        const qreal centerX = width() / 2.0;
        const qreal centerY = height() / 2.0;
        const qreal radarRadius = qMax(0.0, width() / 2.0 - 2.0);
        const qreal worldRange = m_controller->radarRange();
        const qreal scale = radarRadius / qMax(worldRange, 0.0001);
        const qreal playerX = m_controller->playerX();
        const qreal playerY = m_controller->playerY();

        fillVertices.reserve(256);
        lineVertices.reserve(320);

        appendRegularPolygon(fillVertices, QPointF(centerX, centerY + 2.0), radarRadius * 1.02, 28, QColor(9, 14, 12, 72));
        appendRegularPolygon(fillVertices, QPointF(centerX, centerY), radarRadius, 28, QColor("#16211D"));
        appendRegularPolygon(fillVertices, QPointF(centerX, centerY), radarRadius * 0.84, 28, QColor(37, 56, 49, 58));
        appendRegularPolygonOutline(lineVertices, QPointF(centerX, centerY), radarRadius - 1.0, 28, QColor("#7A6037"));
        appendRegularPolygonOutline(lineVertices, QPointF(centerX, centerY), radarRadius * 0.66, 28, QColor(122, 96, 55, 150));
        appendRegularPolygonOutline(lineVertices, QPointF(centerX, centerY), radarRadius * 0.34, 20, QColor(86, 72, 46, 110));
        appendLine(lineVertices, QPointF(centerX, 0.0), QPointF(centerX, height()), QColor(77, 59, 34, 120));
        appendLine(lineVertices, QPointF(0.0, centerY), QPointF(width(), centerY), QColor(77, 59, 34, 120));

        const int pickupStep = qMax(1, static_cast<int>(qCeil(currentSnapshot.pickups.size() / 28.0)));
        for (int i = 0; i < currentSnapshot.pickups.size(); i += pickupStep) {
            const SurvivorController::RenderPickup &pickup = currentSnapshot.pickups.at(i);
            bool faded = false;
            const QPointF radarPoint = clampToRadar(pickup.x - playerX, pickup.y - playerY, worldRange, &faded);
            const QPointF point(centerX + radarPoint.x() * scale,
                                centerY + radarPoint.y() * scale);
            const QColor color = pickup.kind == 3
                ? (faded ? QColor(240, 205, 132, 110) : QColor("#F0CD84"))
                : (faded ? QColor(jadeColor(pickup.kind).red(),
                                  jadeColor(pickup.kind).green(),
                                  jadeColor(pickup.kind).blue(),
                                  110)
                         : jadeColor(pickup.kind));
            appendLayeredPolygon(fillVertices,
                                 lineVertices,
                                 point,
                                 pickup.kind == 3 ? 2.8 : 2.4,
                                 4,
                                 45.0,
                                 color,
                                 withAlpha(QColor("#FFF7E2"), faded ? 90 : 160),
                                 withAlpha(QColor("#F9F1DB"), faded ? 90 : 150),
                                 faded ? QColor(12, 18, 16, 24) : QColor(12, 18, 16, 48));
        }

        const int enemyStep = qMax(1, static_cast<int>(qCeil(currentSnapshot.enemies.size() / 56.0)));
        for (int i = 0; i < currentSnapshot.enemies.size(); i += enemyStep) {
            const SurvivorController::RenderEnemy &enemy = currentSnapshot.enemies.at(i);
            bool faded = false;
            const QPointF radarPoint = clampToRadar(enemy.x - playerX, enemy.y - playerY, worldRange, &faded);
            const QPointF point(centerX + radarPoint.x() * scale,
                                centerY + radarPoint.y() * scale);
            const QColor color = enemy.elite
                ? (faded ? QColor(214, 154, 109, 110) : QColor("#D69A6D"))
                : (faded ? QColor(208, 89, 74, 90) : QColor("#D0594A"));
            appendLayeredPolygon(fillVertices,
                                 lineVertices,
                                 point,
                                 enemy.elite ? 3.2 : 2.5,
                                 enemy.elite ? 6 : 5,
                                 -18.0,
                                 color,
                                 withAlpha(QColor("#F3E8D4"), faded ? 70 : 120),
                                 withAlpha(QColor("#2D261F"), faded ? 80 : 130),
                                 faded ? QColor(10, 16, 14, 24) : QColor(10, 16, 14, 42));
        }

        for (int i = 0; i < currentSnapshot.players.size(); ++i) {
            const SurvivorController::RenderPlayer &player = currentSnapshot.players.at(i);
            if (player.local || !player.alive)
                continue;
            bool faded = false;
            const QPointF radarPoint = clampToRadar(player.x - playerX, player.y - playerY, worldRange, &faded);
            const QPointF point(centerX + radarPoint.x() * scale,
                                centerY + radarPoint.y() * scale);
            const QColor baseColor = player.colorIndex % 3 == 1
                ? QColor("#8DBFB6")
                : (player.colorIndex % 3 == 2 ? QColor("#D9A86B") : QColor("#6F9E98"));
            appendLayeredPolygon(fillVertices,
                                 lineVertices,
                                 point,
                                 3.1,
                                 6,
                                 0.0,
                                 faded ? withAlpha(baseColor, 110) : baseColor,
                                 withAlpha(QColor("#E9F1EC"), faded ? 80 : 150),
                                 withAlpha(QColor("#2B544F"), faded ? 90 : 150),
                                 faded ? QColor(10, 16, 14, 24) : QColor(10, 16, 14, 42));
        }

        appendLayeredPolygon(fillVertices,
                             lineVertices,
                             QPointF(centerX, centerY),
                             4.1,
                             16,
                             0.0,
                             QColor("#6F9E98"),
                             QColor("#E9F1EC"),
                             QColor("#2B544F"),
                             QColor(10, 18, 16, 56));
    } else {
        const qreal playerWorldX = m_controller->playerX();
        const qreal playerWorldY = m_controller->playerY();
        const qreal scale = arenaScale();
        const qreal px = width() / 2.0;
        const qreal py = height() / 2.0;
        const qreal drawMargin = 36.0;

        auto screenX = [px, playerWorldX, scale](qreal worldX) {
            return px + (worldX - playerWorldX) * scale;
        };
        auto screenY = [py, playerWorldY, scale](qreal worldY) {
            return py + (worldY - playerWorldY) * scale;
        };

        fillVertices.reserve(2048 + currentSnapshot.enemies.size() * 48 + currentSnapshot.projectiles.size() * 12);
        lineVertices.reserve(1024 + currentSnapshot.enemies.size() * 32);

        appendRect(fillVertices, QRectF(0.0, 0.0, width(), height()), QColor("#D8D1C3"));

        const qreal leftWorld = playerWorldX - width() / (2.0 * scale);
        const qreal rightWorld = playerWorldX + width() / (2.0 * scale);
        const qreal topWorld = playerWorldY - height() / (2.0 * scale);
        const qreal bottomWorld = playerWorldY + height() / (2.0 * scale);
        const qreal minorCell = 0.22;
        const qreal majorCell = 0.66;

        for (qreal worldX = qFloor(leftWorld / minorCell) * minorCell; worldX <= rightWorld + minorCell; worldX += minorCell) {
            appendLine(lineVertices,
                       QPointF(screenX(worldX), 0.0),
                       QPointF(screenX(worldX), height()),
                       QColor(174, 165, 153, 78));
        }
        for (qreal worldY = qFloor(topWorld / minorCell) * minorCell; worldY <= bottomWorld + minorCell; worldY += minorCell) {
            appendLine(lineVertices,
                       QPointF(0.0, screenY(worldY)),
                       QPointF(width(), screenY(worldY)),
                       QColor(174, 165, 153, 78));
        }
        for (qreal worldX = qFloor(leftWorld / majorCell) * majorCell; worldX <= rightWorld + majorCell; worldX += majorCell) {
            appendLine(lineVertices,
                       QPointF(screenX(worldX), 0.0),
                       QPointF(screenX(worldX), height()),
                       QColor(147, 136, 122, 118));
        }
        for (qreal worldY = qFloor(topWorld / majorCell) * majorCell; worldY <= bottomWorld + majorCell; worldY += majorCell) {
            appendLine(lineVertices,
                       QPointF(0.0, screenY(worldY)),
                       QPointF(width(), screenY(worldY)),
                       QColor(147, 136, 122, 118));
        }

        for (qreal worldX = qFloor(leftWorld / (majorCell * 2.0)) * (majorCell * 2.0);
             worldX <= rightWorld + majorCell * 2.0;
             worldX += majorCell * 2.0) {
            for (qreal worldY = qFloor(topWorld / (majorCell * 2.0)) * (majorCell * 2.0);
                 worldY <= bottomWorld + majorCell * 2.0;
                 worldY += majorCell * 2.0) {
                appendRegularPolygon(fillVertices,
                                     QPointF(screenX(worldX), screenY(worldY)),
                                     4.8,
                                     4,
                                     QColor(255, 250, 238, 14),
                                     45.0);
            }
        }

        for (int i = 0; i < currentSnapshot.pickups.size(); ++i) {
            const SurvivorController::RenderPickup &pickup = currentSnapshot.pickups.at(i);
            const QPointF point(screenX(pickup.x), screenY(pickup.y));
            if (point.x() < -drawMargin || point.x() > width() + drawMargin
                || point.y() < -drawMargin || point.y() > height() + drawMargin) {
                continue;
            }

            if (pickup.kind == 3) {
                appendRect(fillVertices,
                           QRectF(point.x() - 8.0, point.y() - 4.8, 16.0, 12.4),
                           QColor(16, 20, 18, 44));
                appendRect(fillVertices,
                           QRectF(point.x() - 8.0, point.y() - 6.0, 16.0, 12.0),
                           QColor("#B9894B"));
                appendRect(fillVertices,
                           QRectF(point.x() - 8.0, point.y() - 6.0, 16.0, 4.2),
                           QColor("#F1D6A1"));
                appendRect(fillVertices,
                           QRectF(point.x() - 1.2, point.y() - 6.0, 2.4, 12.0),
                           QColor("#F9EDC8"));
                appendRectOutline(lineVertices,
                                  QRectF(point.x() - 8.0, point.y() - 6.0, 16.0, 12.0),
                                  QColor("#7B5931"));
            } else {
                const qreal size = qMax(pickup.kind == 2 ? 7.2 : (pickup.kind == 1 ? 5.6 : 4.4),
                                        pickup.radius * scale * 1.18);
                appendLayeredDiamond(fillVertices,
                                     lineVertices,
                                     point,
                                     size,
                                     jadeColor(pickup.kind),
                                     withAlpha(QColor("#FAF6EE"), 170),
                                     QColor("#F8F6F2"),
                                     QColor(12, 18, 16, 42));
            }
        }

        for (int i = 0; i < currentSnapshot.zones.size(); ++i) {
            const SurvivorController::RenderZone &zone = currentSnapshot.zones.at(i);
            const QPointF point(screenX(zone.x), screenY(zone.y));
            const qreal radius = qMax(13.0, zone.radius * scale);
            if (point.x() + radius < -drawMargin || point.x() - radius > width() + drawMargin
                || point.y() + radius < -drawMargin || point.y() - radius > height() + drawMargin) {
                continue;
            }

            const QColor fill = zone.kind == 1
                ? QColor(66, 110, 170, 18)
                : QColor(62, 104, 156, 14);
            const QColor outline = zone.kind == 1
                ? QColor(89, 134, 196, 110)
                : QColor(82, 122, 178, 84);
            appendRegularPolygon(fillVertices, point, radius * 1.04, 22, withAlpha(QColor(10, 18, 24), 18));
            appendRegularPolygon(fillVertices, point, radius, 20, fill);
            appendRegularPolygon(fillVertices,
                                 QPointF(point.x() - radius * 0.08, point.y() - radius * 0.08),
                                 radius * (zone.kind == 1 ? 0.72 : 0.64),
                                 18,
                                 zone.kind == 1 ? QColor(118, 154, 204, 24) : QColor(108, 146, 196, 18));
            appendRegularPolygonOutline(lineVertices, point, radius, 18, outline);
            appendRegularPolygonOutline(lineVertices, point, radius * (zone.kind == 1 ? 0.74 : 0.56), 16,
                                        zone.kind == 1
                                            ? QColor(168, 204, 236, 64)
                                            : QColor(146, 186, 226, 48));
            appendRegularPolygonOutline(lineVertices, point, radius * (zone.kind == 1 ? 0.32 : 0.26), 12,
                                        zone.kind == 1
                                            ? QColor(214, 230, 248, 52)
                                            : QColor(200, 220, 242, 40));
        }

        const qreal auraRadius = m_controller->auraRadius();
        if (auraRadius > 0.001) {
            const qreal radius = qMax(20.0, auraRadius * scale);
            appendRegularPolygon(fillVertices, QPointF(px, py), radius * 1.03, 24, QColor(14, 20, 12, 16));
            appendRegularPolygon(fillVertices, QPointF(px, py), radius, 22, QColor(103, 128, 82, 10));
            appendRegularPolygon(fillVertices, QPointF(px, py), radius * 0.72, 20, QColor(128, 156, 104, 12));
            appendRegularPolygonOutline(lineVertices, QPointF(px, py), radius, 22, QColor(136, 168, 112, 72));
            appendRegularPolygonOutline(lineVertices, QPointF(px, py), radius * 0.74, 20, QColor(160, 188, 132, 48));
            appendRegularPolygonOutline(lineVertices, QPointF(px, py), radius * 0.42, 16, QColor(182, 206, 154, 36));
        }

        for (int i = 0; i < currentSnapshot.orbitals.size(); ++i) {
            const SurvivorController::RenderOrbital &orbital = currentSnapshot.orbitals.at(i);
            const QPointF point(screenX(orbital.x), screenY(orbital.y));
            if (point.x() < -drawMargin || point.x() > width() + drawMargin
                || point.y() < -drawMargin || point.y() > height() + drawMargin) {
                continue;
            }

            const qreal radius = qMax(4.0, orbital.radius * scale);
            appendLayeredPolygon(fillVertices,
                                 lineVertices,
                                 point,
                                 radius,
                                 12,
                                 0.0,
                                 QColor("#D6A74B"),
                                 QColor("#FFF0C4"),
                                 QColor("#8E6D31"),
                                 QColor(14, 16, 12, 42));
        }

        for (int i = 0; i < currentSnapshot.projectiles.size(); ++i) {
            const SurvivorController::RenderProjectile &projectile = currentSnapshot.projectiles.at(i);
            const QPointF point(screenX(projectile.x), screenY(projectile.y));
            if (point.x() < -drawMargin || point.x() > width() + drawMargin
                || point.y() < -drawMargin || point.y() > height() + drawMargin) {
                continue;
            }

            const qreal size = qMax(projectile.kind == 2 ? 5.0 : 4.8,
                                    projectile.radius * scale * 1.2);
            if (projectile.kind == 0) {
                appendLayeredDiamond(fillVertices,
                                     lineVertices,
                                     point,
                                     size,
                                     QColor("#E7CCA0"),
                                     QColor("#FFF4D4"),
                                     QColor("#8B6C41"),
                                     QColor(12, 16, 14, 34));
            } else if (projectile.kind == 2) {
                appendLine(lineVertices,
                           QPointF(point.x() - size * 1.4, point.y() + 1.0),
                           QPointF(point.x() + size * 1.4, point.y() + 1.0),
                           QColor(14, 18, 16, 48));
                appendLine(lineVertices,
                           QPointF(point.x() + 1.0, point.y() - size * 1.4),
                           QPointF(point.x() + 1.0, point.y() + size * 1.4),
                           QColor(14, 18, 16, 48));
                appendLine(lineVertices,
                           QPointF(point.x() - size * 1.4, point.y()),
                           QPointF(point.x() + size * 1.4, point.y()),
                           QColor("#C5A25D"));
                appendLine(lineVertices,
                           QPointF(point.x(), point.y() - size * 1.4),
                           QPointF(point.x(), point.y() + size * 1.4),
                           QColor("#C5A25D"));
                appendRegularPolygon(fillVertices, point, size * 0.54, 10, QColor("#F2E0AD"));
                appendRegularPolygon(fillVertices,
                                     QPointF(point.x() - size * 0.08, point.y() - size * 0.08),
                                     size * 0.26,
                                     8,
                                     QColor("#FFF5D6"));
            } else {
                const QColor fill = projectile.kind == 3 ? QColor("#F05F47") : QColor("#F28B5D");
                appendLayeredPolygon(fillVertices,
                                     lineVertices,
                                     point,
                                     size * 0.94,
                                     12,
                                     0.0,
                                     fill,
                                     projectile.kind == 3 ? QColor("#FFD2B0") : QColor("#FFE1C4"),
                                     QColor("#7D3528"),
                                     QColor(18, 12, 10, 40));
            }
        }

        for (int i = 0; i < currentSnapshot.enemies.size(); ++i) {
            const SurvivorController::RenderEnemy &enemy = currentSnapshot.enemies.at(i);
            const QPointF point(screenX(enemy.x), screenY(enemy.y));
            const qreal radius = qMax(7.0, enemy.radius * scale);
            const qreal drawRadius = radius * (enemy.kind == 0 ? 1.02 : 1.0);
            if (point.x() + drawRadius * 2.4 < -drawMargin || point.x() - drawRadius * 2.4 > width() + drawMargin
                || point.y() + drawRadius * 2.8 < -drawMargin || point.y() - drawRadius * 2.0 > height() + drawMargin) {
                continue;
            }

            const QColor fill = enemy.hitFlashMs > 0 ? QColor("#F7F1E4") : enemyFillColor(enemy.kind);
            const QColor outline = enemyOutlineColor(enemy.kind);
            const QColor accent = enemyAccentColor(enemy.kind);
            const QColor shadow = QColor(16, 18, 16, enemy.elite ? 62 : 44);

            switch (enemy.kind) {
            case 0:
            case 1:
            case 4:
                appendLayeredPolygon(fillVertices,
                                     lineVertices,
                                     point,
                                     drawRadius * (enemy.kind == 0 ? 0.78 : (enemy.kind == 1 ? 0.90 : 0.92)),
                                     enemy.kind == 4 ? 12 : 10,
                                     0.0,
                                     fill,
                                     withAlpha(QColor("#FFF4E1"), enemy.kind == 4 ? 120 : 96),
                                     outline,
                                     shadow);
                break;
            case 2:
                appendLayeredDiamond(fillVertices,
                                     lineVertices,
                                     point,
                                     drawRadius * 0.98,
                                     fill,
                                     QColor("#FFF7E7"),
                                     outline,
                                     shadow);
                break;
            case 3:
                appendLayeredPolygon(fillVertices,
                                     lineVertices,
                                     point,
                                     drawRadius * 0.98,
                                     6,
                                     -30.0,
                                     fill,
                                     QColor("#F0D7B7"),
                                     outline,
                                     shadow);
                break;
            case 5:
                appendRect(fillVertices,
                           QRectF(point.x() - drawRadius * 0.94,
                                  point.y() - drawRadius * 0.78,
                                  drawRadius * 1.88,
                                  drawRadius * 1.88),
                           shadow);
                appendRect(fillVertices,
                           QRectF(point.x() - drawRadius * 0.94,
                                  point.y() - drawRadius * 0.94,
                                  drawRadius * 1.88,
                                  drawRadius * 1.88),
                           fill);
                appendRect(fillVertices,
                           QRectF(point.x() - drawRadius * 0.68,
                                  point.y() - drawRadius * 0.80,
                                  drawRadius * 0.98,
                                  drawRadius * 0.98),
                           QColor("#D8B98F"));
                appendLine(lineVertices,
                           QPointF(point.x() - drawRadius * 0.94, point.y() - drawRadius * 0.94),
                           QPointF(point.x() + drawRadius * 0.94, point.y() - drawRadius * 0.94),
                           outline);
                appendLine(lineVertices,
                           QPointF(point.x() + drawRadius * 0.94, point.y() - drawRadius * 0.94),
                           QPointF(point.x() + drawRadius * 0.94, point.y() + drawRadius * 0.94),
                           outline);
                appendLine(lineVertices,
                           QPointF(point.x() + drawRadius * 0.94, point.y() + drawRadius * 0.94),
                           QPointF(point.x() - drawRadius * 0.94, point.y() + drawRadius * 0.94),
                           outline);
                appendLine(lineVertices,
                           QPointF(point.x() - drawRadius * 0.94, point.y() + drawRadius * 0.94),
                           QPointF(point.x() - drawRadius * 0.94, point.y() - drawRadius * 0.94),
                           outline);
                break;
            default:
                appendLayeredPolygon(fillVertices,
                                     lineVertices,
                                     point,
                                     drawRadius * 1.02,
                                     8,
                                     -22.5,
                                     fill,
                                     QColor("#E5C79B"),
                                     outline,
                                     shadow);
                break;
            }

            appendRegularPolygon(fillVertices,
                                 QPointF(point.x() - drawRadius * 0.08, point.y() - drawRadius * 0.10),
                                 drawRadius * 0.22,
                                 8,
                                 accent);
            if (enemy.kind == 4)
                appendRegularPolygonOutline(lineVertices, point, drawRadius * 0.58, 12, QColor(accent.red(), accent.green(), accent.blue(), 150));

            if (enemy.elite || enemy.chestCarrier) {
                appendRegularPolygonOutline(lineVertices,
                                            point,
                                            drawRadius + 3.0,
                                            16,
                                            enemy.chestCarrier ? QColor("#E6B678") : QColor("#D69A6D"));
                appendRegularPolygonOutline(lineVertices,
                                            point,
                                            drawRadius + 5.4,
                                            18,
                                            enemy.chestCarrier ? QColor(230, 182, 120, 72) : QColor(214, 154, 109, 72));
            }

            if (enemy.maxHp > 0 && enemy.hp < enemy.maxHp) {
                const qreal hpWidth = drawRadius * 1.9;
                appendRect(fillVertices,
                           QRectF(point.x() - hpWidth / 2.0,
                                  point.y() - drawRadius - 13.0,
                                  hpWidth,
                                  4.5),
                           QColor(16, 24, 20, 210));
                appendRect(fillVertices,
                           QRectF(point.x() - hpWidth / 2.0,
                                  point.y() - drawRadius - 13.0,
                                  hpWidth * qMax(0.08, enemy.hp / qMax(1.0, static_cast<qreal>(enemy.maxHp))),
                                  4.5),
                           enemy.chestCarrier ? QColor("#E7B774") : QColor("#D76456"));
            }
        }

        for (int i = 0; i < currentSnapshot.players.size(); ++i) {
            const SurvivorController::RenderPlayer &player = currentSnapshot.players.at(i);
            const QPointF point(screenX(player.x), screenY(player.y));
            const qreal playerRadius = player.local
                ? (m_compactLayout ? 17.5 : 19.0)
                : (m_compactLayout ? 14.0 : 15.0);
            const QColor baseColor = player.local
                ? QColor("#4E827B")
                : (player.colorIndex % 3 == 1
                    ? QColor("#739C95")
                    : (player.colorIndex % 3 == 2 ? QColor("#A6845D") : QColor("#5F8D86")));
            const QColor outlineColor = player.local ? QColor("#244E49") : QColor("#304E49");
            const QColor accentColor = player.alive ? QColor("#D96D58") : QColor("#766A63");

            appendRegularPolygon(fillVertices,
                                 QPointF(point.x(), point.y() + 3.0),
                                 playerRadius * 1.04,
                                 14,
                                 QColor(10, 16, 14, player.local ? 72 : 54));
            appendRegularPolygon(fillVertices, point, playerRadius, 14, baseColor);
            appendRegularPolygon(fillVertices,
                                 QPointF(point.x() - 2.0, point.y() - 3.0),
                                 playerRadius * 0.56,
                                 12,
                                 QColor("#E7F1EC"));
            appendRegularPolygonOutline(lineVertices, point, playerRadius, 14, outlineColor);
            appendRegularPolygonOutline(lineVertices,
                                        point,
                                        playerRadius * 0.62,
                                        12,
                                        QColor(219, 236, 230, player.local ? 164 : 130));
            appendLine(lineVertices,
                       QPointF(point.x() - 6.0, point.y()),
                       QPointF(point.x() + 6.0, point.y()),
                       accentColor);
            appendLine(lineVertices,
                       QPointF(point.x(), point.y() - 6.0),
                       QPointF(point.x(), point.y() + 6.0),
                       accentColor);
            appendRegularPolygon(fillVertices,
                                 QPointF(point.x() + playerRadius * 0.52, point.y()),
                                 playerRadius * 0.16,
                                 6,
                                 accentColor);
        }
    }

    root->fillNode->updateVertices(fillVertices);
    root->lineNode->updateVertices(lineVertices);
    return root;
}

void SurvivorRenderItem::attachControllerSignals()
{
    if (!m_controller)
        return;

    connect(m_controller, &SurvivorController::frameChanged, this, [this]() {
        ++m_frameCounter;
        if (!m_radarMode || m_frameCounter % 3 == 0)
            update();
    });

    connect(m_controller, &SurvivorController::runningChanged, this, [this]() { update(); });
}

qreal SurvivorRenderItem::arenaScale() const
{
    return qMax(width() * (m_compactLayout ? 0.64 : 0.56), 220.0);
}
