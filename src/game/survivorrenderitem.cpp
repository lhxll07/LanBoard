#include "survivorrenderitem.h"

#include "survivorcontroller.h"

#include <functional>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QtMath>

namespace {

QColor enemyFillColor(int kind, bool hurtFlash)
{
    if (hurtFlash)
        return QColor("#F7F1E4");

    switch (kind) {
    case 0: return QColor("#5A7284"); // Night crow
    case 1: return QColor("#B78A69"); // Jiangshi
    case 2: return QColor("#F0E5D6"); // Paper effigy
    case 3: return QColor("#8E6C5B"); // Mountain beast
    case 4: return QColor("#8BA06E"); // Tree spirit
    case 5: return QColor("#6F5E53"); // Brute
    default: return QColor("#A47052"); // Boss demon
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

void drawEnemyShape(QPainter *painter,
                    const SurvivorController::RenderEnemy &enemy,
                    const QPointF &center,
                    qreal radius,
                    bool highDetail);

QPainterPath roundedDiamondPath(qreal size)
{
    QPainterPath path;
    path.moveTo(0.0, -size);
    path.quadTo(size * 0.55, -size * 0.42, size, 0.0);
    path.quadTo(size * 0.5, size * 0.58, 0.0, size);
    path.quadTo(-size * 0.5, size * 0.58, -size, 0.0);
    path.quadTo(-size * 0.55, -size * 0.42, 0.0, -size);
    return path;
}

QHash<QString, QPixmap> &spriteCache()
{
    static QHash<QString, QPixmap> cache;
    return cache;
}

const QPixmap &cachedPixmap(const QString &key,
                            int width,
                            int height,
                            const std::function<void(QPainter &)> &paintFn)
{
    QHash<QString, QPixmap> &cache = spriteCache();
    const auto it = cache.constFind(key);
    if (it != cache.constEnd())
        return it.value();

    QPixmap pixmap(width, height);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    paintFn(painter);
    painter.end();

    auto inserted = cache.insert(key, pixmap);
    return inserted.value();
}

quint32 stableCellNoise(int cellX, int cellY)
{
    quint32 hash = static_cast<quint32>(cellX) * 73856093u
        ^ static_cast<quint32>(cellY) * 19349663u
        ^ 0x9E3779B9u;
    hash ^= hash >> 13;
    hash *= 1274126177u;
    return hash ^ (hash >> 16);
}

void drawBackdropDecoration(QPainter *painter,
                            qreal width,
                            qreal height,
                            qreal playerWorldX,
                            qreal playerWorldY,
                            qreal scale)
{
    painter->fillRect(QRectF(0.0, 0.0, width, height), QColor("#E8E1D3"));

    const qreal centerX = width / 2.0;
    const qreal centerY = height / 2.0;
    const qreal leftWorld = playerWorldX - width / (2.0 * scale);
    const qreal rightWorld = playerWorldX + width / (2.0 * scale);
    const qreal topWorld = playerWorldY - height / (2.0 * scale);
    const qreal bottomWorld = playerWorldY + height / (2.0 * scale);

    auto screenX = [centerX, playerWorldX, scale](qreal worldX) {
        return centerX + (worldX - playerWorldX) * scale;
    };
    auto screenY = [centerY, playerWorldY, scale](qreal worldY) {
        return centerY + (worldY - playerWorldY) * scale;
    };

    const qreal minorCell = 0.22;
    const qreal majorCell = 0.66;

    painter->setPen(QPen(QColor("#D8D0C2"), 1.0));
    for (qreal worldX = qFloor(leftWorld / minorCell) * minorCell; worldX <= rightWorld + minorCell; worldX += minorCell)
        painter->drawLine(QPointF(screenX(worldX), 0.0), QPointF(screenX(worldX), height));
    for (qreal worldY = qFloor(topWorld / minorCell) * minorCell; worldY <= bottomWorld + minorCell; worldY += minorCell)
        painter->drawLine(QPointF(0.0, screenY(worldY)), QPointF(width, screenY(worldY)));

    painter->setPen(QPen(QColor("#C5BBAB"), 1.2));
    for (qreal worldX = qFloor(leftWorld / majorCell) * majorCell; worldX <= rightWorld + majorCell; worldX += majorCell)
        painter->drawLine(QPointF(screenX(worldX), 0.0), QPointF(screenX(worldX), height));
    for (qreal worldY = qFloor(topWorld / majorCell) * majorCell; worldY <= bottomWorld + majorCell; worldY += majorCell)
        painter->drawLine(QPointF(0.0, screenY(worldY)), QPointF(width, screenY(worldY)));

    const int cellXStart = qFloor(leftWorld / majorCell) - 1;
    const int cellXEnd = qCeil(rightWorld / majorCell) + 1;
    const int cellYStart = qFloor(topWorld / majorCell) - 1;
    const int cellYEnd = qCeil(bottomWorld / majorCell) + 1;

    for (int cellY = cellYStart; cellY <= cellYEnd; ++cellY) {
        for (int cellX = cellXStart; cellX <= cellXEnd; ++cellX) {
            const quint32 noise = stableCellNoise(cellX, cellY);
            const qreal worldCenterX = (cellX + 0.5) * majorCell;
            const qreal worldCenterY = (cellY + 0.5) * majorCell;
            const QPointF point(screenX(worldCenterX), screenY(worldCenterY));

            if (point.x() < -36.0 || point.x() > width + 36.0 || point.y() < -36.0 || point.y() > height + 36.0)
                continue;

            if (noise % 13u == 0u) {
                const qreal dot = qMax(4.0, scale * 0.016);
                painter->setPen(Qt::NoPen);
                painter->setBrush(QColor::fromRgbF(0.72, 0.67, 0.61, 0.16));
                painter->drawEllipse(point, dot, dot);
            } else if (noise % 7u == 0u) {
                painter->setPen(QPen(QColor::fromRgbF(0.76, 0.71, 0.65, 0.22),
                                     qMax(4.0, scale * 0.016),
                                     Qt::SolidLine,
                                     Qt::RoundCap));
                painter->drawLine(QPointF(point.x() - scale * 0.06, point.y()),
                                  QPointF(point.x() + scale * 0.06, point.y()));
            }
        }
    }
}

void drawJadePickup(QPainter *painter, const QPointF &center, qreal size, int kind)
{
    painter->save();
    painter->translate(center);
    painter->setPen(QPen(QColor("#F8F6F2"), 1.3));
    painter->setBrush(jadeColor(kind));
    painter->drawPath(roundedDiamondPath(size));

    painter->setPen(QPen(QColor::fromRgbF(1.0, 1.0, 1.0, 0.55), 1.0));
    painter->drawLine(QPointF(0.0, -size * 0.56), QPointF(0.0, size * 0.42));
    painter->drawLine(QPointF(-size * 0.34, 0.0), QPointF(size * 0.34, 0.0));
    painter->restore();
}

void drawChestPickup(QPainter *painter, const QPointF &center, qreal width)
{
    const qreal boxWidth = width;
    const qreal boxHeight = width * 0.70;

    painter->save();
    painter->translate(center);
    painter->setPen(QPen(QColor("#6C5234"), 1.8));
    painter->setBrush(QColor("#B9894B"));
    painter->drawRoundedRect(QRectF(-boxWidth / 2.0, -boxHeight / 2.0, boxWidth, boxHeight), 4.0, 4.0);
    painter->setBrush(QColor("#E5C68A"));
    painter->drawRoundedRect(QRectF(-boxWidth / 2.0, -boxHeight / 2.0, boxWidth, boxHeight * 0.34), 3.0, 3.0);
    painter->setPen(QPen(QColor("#F7E4B6"), 1.8));
    painter->drawLine(QPointF(0.0, -boxHeight * 0.46), QPointF(0.0, boxHeight * 0.46));
    painter->drawLine(QPointF(-boxWidth * 0.46, 0.0), QPointF(boxWidth * 0.46, 0.0));
    painter->restore();
}

void drawZoneShape(QPainter *painter, const QPointF &center, qreal radius, bool evolvedZone)
{
    painter->save();
    painter->translate(center);

    if (evolvedZone) {
        painter->setPen(QPen(QColor::fromRgbF(95.0 / 255.0, 132.0 / 255.0, 214.0 / 255.0, 0.70), 2.2));
        painter->setBrush(QColor::fromRgbF(95.0 / 255.0, 132.0 / 255.0, 214.0 / 255.0, 0.10));
        painter->drawEllipse(QPointF(0.0, 0.0), radius, radius);
        painter->drawEllipse(QPointF(0.0, 0.0), radius * 0.55, radius * 0.55);
        painter->setPen(QPen(QColor::fromRgbF(0.95, 0.98, 1.0, 0.36), 1.4));
        painter->drawLine(QPointF(-radius * 0.36, 0.0), QPointF(radius * 0.36, 0.0));
        painter->drawLine(QPointF(0.0, -radius * 0.36), QPointF(0.0, radius * 0.36));
    } else {
        painter->setPen(QPen(QColor::fromRgbF(92.0 / 255.0, 136.0 / 255.0, 208.0 / 255.0, 0.52), 2.0));
        painter->setBrush(QColor::fromRgbF(92.0 / 255.0, 136.0 / 255.0, 208.0 / 255.0, 0.08));
        painter->drawEllipse(QPointF(0.0, 0.0), radius, radius);
        painter->setPen(QPen(QColor::fromRgbF(0.92, 0.96, 1.0, 0.28), 1.2));
        painter->drawEllipse(QPointF(0.0, 0.0), radius * 0.40, radius * 0.40);
    }

    painter->restore();
}

void drawAura(QPainter *painter, const QPointF &center, qreal radius)
{
    painter->save();
    painter->setPen(QPen(QColor::fromRgbF(167.0 / 255.0, 192.0 / 255.0, 122.0 / 255.0, 0.54), 2.0));
    painter->setBrush(QColor::fromRgbF(167.0 / 255.0, 192.0 / 255.0, 122.0 / 255.0, 0.08));
    painter->drawEllipse(center, radius, radius);
    painter->drawEllipse(center, radius * 0.62, radius * 0.62);
    painter->restore();
}

void drawPlayerShape(QPainter *painter, const QPointF &center, qreal scaleFactor)
{
    painter->save();
    painter->translate(center);
    painter->scale(scaleFactor, scaleFactor);

    painter->setPen(QPen(QColor("#2E5451"), 3.0));
    painter->setBrush(QColor("#5C8E89"));
    painter->drawEllipse(QPointF(0.0, 0.0), 17.0, 17.0);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(QColor("#DCE8E2"), 2.2));
    painter->drawEllipse(QPointF(0.0, 0.0), 10.0, 10.0);
    painter->setPen(QPen(QColor("#E26C58"), 2.4, Qt::SolidLine, Qt::RoundCap));
    painter->drawLine(QPointF(-6.0, 0.0), QPointF(6.0, 0.0));
    painter->drawLine(QPointF(0.0, -6.0), QPointF(0.0, 6.0));
    painter->restore();
}

void drawCoinOrbital(QPainter *painter, const QPointF &center, qreal radius)
{
    painter->save();
    painter->translate(center);
    painter->setPen(QPen(QColor("#B38D45"), 1.4));
    painter->setBrush(QColor("#E5BB57"));
    painter->drawEllipse(QPointF(0.0, 0.0), radius, radius);
    painter->setBrush(QColor("#FFF1C8"));
    painter->drawEllipse(QPointF(0.0, 0.0), radius * 0.34, radius * 0.34);
    painter->restore();
}

void drawProjectileShape(QPainter *painter, const QPointF &center, int kind, qreal baseSize)
{
    painter->save();
    painter->translate(center);

    if (kind == 0) {
        painter->setPen(QPen(QColor("#927448"), 1.0));
        painter->setBrush(QColor("#F0D7A5"));
        QPolygonF blade;
        blade << QPointF(-baseSize * 1.12, 0.0)
              << QPointF(baseSize * 0.20, -baseSize * 0.64)
              << QPointF(baseSize * 1.08, 0.0)
              << QPointF(baseSize * 0.20, baseSize * 0.64);
        painter->drawPolygon(blade);
        painter->setPen(QPen(QColor("#FFF7DF"), 1.0));
        painter->drawLine(QPointF(-baseSize * 0.18, 0.0), QPointF(baseSize * 0.72, 0.0));
    } else if (kind == 1 || kind == 3) {
        const QColor fill = kind == 3 ? QColor("#F05F47") : QColor("#F28B5D");
        painter->setPen(QPen(QColor("#7D3528"), 1.4));
        painter->setBrush(fill);
        painter->drawEllipse(QPointF(0.0, 0.0), baseSize * 0.94, baseSize * 0.94);
        painter->setPen(QPen(QColor("#FFD4B2"), 1.2));
        painter->drawLine(QPointF(-baseSize * 0.44, 0.0), QPointF(baseSize * 0.44, 0.0));
        painter->drawLine(QPointF(0.0, -baseSize * 0.44), QPointF(0.0, baseSize * 0.44));
    } else if (kind == 2) {
        painter->setPen(QPen(QColor("#C5A25D"), 2.0, Qt::SolidLine, Qt::RoundCap));
        painter->drawLine(QPointF(-baseSize * 1.4, 0.0), QPointF(baseSize * 1.4, 0.0));
        painter->drawLine(QPointF(0.0, -baseSize * 1.4), QPointF(0.0, baseSize * 1.4));
        painter->setPen(QPen(QColor("#EEDAA4"), 1.2));
        painter->drawEllipse(QPointF(0.0, 0.0), baseSize * 0.42, baseSize * 0.42);
    }

    painter->restore();
}

const QPixmap &playerSprite(bool compactLayout)
{
    const qreal scaleFactor = compactLayout ? 0.42 : 0.46;
    const int spriteSize = compactLayout ? 54 : 58;
    const QString key = QStringLiteral("player_%1").arg(compactLayout ? 1 : 0);
    return cachedPixmap(key, spriteSize, spriteSize, [scaleFactor, spriteSize](QPainter &painter) {
        drawPlayerShape(&painter, QPointF(spriteSize / 2.0, spriteSize / 2.0), scaleFactor);
    });
}

const QPixmap &enemySprite(int kind, int pixelRadius, bool highDetail, bool hurtFlash)
{
    const int radius = qMax(7, pixelRadius);
    const int spriteWidth = qRound(radius * 5.8);
    const int spriteHeight = qRound(radius * 6.2);
    const QString key = QStringLiteral("enemy_%1_%2_%3_%4")
                            .arg(kind)
                            .arg(radius)
                            .arg(highDetail ? 1 : 0)
                            .arg(hurtFlash ? 1 : 0);
    return cachedPixmap(key, spriteWidth, spriteHeight, [kind, radius, highDetail, hurtFlash, spriteWidth, spriteHeight](QPainter &painter) {
        SurvivorController::RenderEnemy enemy;
        enemy.kind = kind;
        enemy.hitFlashMs = hurtFlash ? 1 : 0;
        drawEnemyShape(&painter, enemy, QPointF(spriteWidth / 2.0, spriteHeight * 0.38), radius, highDetail);
    });
}

const QPixmap &projectileSprite(int kind, int baseSize)
{
    const int size = qMax(10, baseSize);
    const int spriteSize = qMax(18, size * 4);
    const QString key = QStringLiteral("projectile_%1_%2").arg(kind).arg(size);
    return cachedPixmap(key, spriteSize, spriteSize, [kind, size, spriteSize](QPainter &painter) {
        drawProjectileShape(&painter, QPointF(spriteSize / 2.0, spriteSize / 2.0), kind, size);
    });
}

const QPixmap &jadeSprite(int kind, int size)
{
    const int spriteSize = qMax(16, size * 4);
    const QString key = QStringLiteral("jade_%1_%2").arg(kind).arg(size);
    return cachedPixmap(key, spriteSize, spriteSize, [kind, size, spriteSize](QPainter &painter) {
        drawJadePickup(&painter, QPointF(spriteSize / 2.0, spriteSize / 2.0), size, kind);
    });
}

const QPixmap &chestSprite(int size)
{
    const int spriteWidth = qMax(22, qRound(size * 2.4));
    const int spriteHeight = qMax(18, qRound(size * 1.9));
    const QString key = QStringLiteral("chest_%1").arg(size);
    return cachedPixmap(key, spriteWidth, spriteHeight, [size, spriteWidth, spriteHeight](QPainter &painter) {
        drawChestPickup(&painter, QPointF(spriteWidth / 2.0, spriteHeight * 0.56), size * 1.4);
    });
}

const QPixmap &orbitalSprite(int size)
{
    const int spriteSize = qMax(14, size * 4);
    const QString key = QStringLiteral("orbital_%1").arg(size);
    return cachedPixmap(key, spriteSize, spriteSize, [size, spriteSize](QPainter &painter) {
        drawCoinOrbital(&painter, QPointF(spriteSize / 2.0, spriteSize / 2.0), size);
    });
}

void drawEnemyShape(QPainter *painter,
                    const SurvivorController::RenderEnemy &enemy,
                    const QPointF &center,
                    qreal radius,
                    bool highDetail)
{
    const QColor fill = enemyFillColor(enemy.kind, enemy.hitFlashMs > 0);
    const QColor outline = enemyOutlineColor(enemy.kind);
    const QColor accent = enemyAccentColor(enemy.kind);

    painter->save();
    painter->translate(center);
    painter->setPen(QPen(outline, qMax(1.6, radius * 0.18)));
    painter->setBrush(fill);

    switch (enemy.kind) {
    case 0:
        painter->drawEllipse(QPointF(0.0, 0.0), radius * 0.78, radius * 0.78);
        break;
    case 1:
        painter->drawEllipse(QPointF(0.0, 0.0), radius * 0.90, radius * 0.90);
        break;
    case 2: {
        QPolygonF diamond;
        diamond << QPointF(0.0, -radius * 0.98)
                << QPointF(radius * 0.98, 0.0)
                << QPointF(0.0, radius * 0.98)
                << QPointF(-radius * 0.98, 0.0);
        painter->drawPolygon(diamond);
        break;
    }
    case 3: {
        QPolygonF hex;
        for (int i = 0; i < 6; ++i) {
            const qreal angle = qDegreesToRadians(60.0 * i - 30.0);
            hex << QPointF(qCos(angle) * radius * 0.98, qSin(angle) * radius * 0.98);
        }
        painter->drawPolygon(hex);
        break;
    }
    case 4:
        painter->drawEllipse(QPointF(0.0, 0.0), radius * 0.92, radius * 0.92);
        break;
    case 5:
        painter->drawRoundedRect(QRectF(-radius * 0.94, -radius * 0.94, radius * 1.88, radius * 1.88),
                                 radius * 0.20,
                                 radius * 0.20);
        break;
    default: {
        QPolygonF octagon;
        for (int i = 0; i < 8; ++i) {
            const qreal angle = qDegreesToRadians(45.0 * i - 22.5);
            octagon << QPointF(qCos(angle) * radius * 1.02, qSin(angle) * radius * 1.02);
        }
        painter->drawPolygon(octagon);
        break;
    }
    }

    painter->setBrush(accent);
    painter->setPen(Qt::NoPen);
    if (enemy.kind == 4) {
        painter->drawEllipse(QPointF(0.0, 0.0), radius * 0.26, radius * 0.26);
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(accent, qMax(1.2, radius * 0.10)));
        painter->drawEllipse(QPointF(0.0, 0.0), radius * 0.58, radius * 0.58);
    } else {
        painter->drawEllipse(QPointF(0.0, 0.0), radius * 0.22, radius * 0.22);
        if (highDetail) {
            painter->setPen(QPen(QColor("#F8F4EC"), qMax(1.0, radius * 0.10), Qt::SolidLine, Qt::RoundCap));
            painter->drawLine(QPointF(-radius * 0.30, 0.0), QPointF(radius * 0.30, 0.0));
        }
    }

    painter->restore();
}

}

SurvivorRenderItem::SurvivorRenderItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
    setAntialiasing(true);
    setOpaquePainting(true);
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

void SurvivorRenderItem::paint(QPainter *painter)
{
    if (!m_controller)
        return;

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    if (m_radarMode)
        drawRadar(painter);
    else
        drawArena(painter);
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

void SurvivorRenderItem::drawArena(QPainter *painter)
{
    const qreal playerWorldX = m_controller->playerX();
    const qreal playerWorldY = m_controller->playerY();
    const auto &currentSnapshot = m_controller->renderSnapshot();
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

    drawBackdropDecoration(painter, width(), height(), playerWorldX, playerWorldY, scale);

    painter->setPen(Qt::NoPen);
    for (int i = 0; i < currentSnapshot.pickups.size(); ++i) {
        const SurvivorController::RenderPickup &pickup = currentSnapshot.pickups.at(i);
        const QPointF point(screenX(pickup.x), screenY(pickup.y));
        if (point.x() < -drawMargin || point.x() > width() + drawMargin
            || point.y() < -drawMargin || point.y() > height() + drawMargin) {
            continue;
        }

        if (pickup.kind == 3) {
            const int chestSize = qRound(qMax(14.0, pickup.radius * scale * 1.5));
            const QPixmap &sprite = chestSprite(chestSize);
            painter->drawPixmap(QPointF(point.x() - sprite.width() / 2.0,
                                        point.y() - sprite.height() * 0.56),
                                sprite);
        } else {
            const int gemSize = qRound(qMax(pickup.kind == 2 ? 7.2 : (pickup.kind == 1 ? 5.6 : 4.4),
                                            pickup.radius * scale * 1.18));
            const QPixmap &sprite = jadeSprite(pickup.kind, gemSize);
            painter->drawPixmap(QPointF(point.x() - sprite.width() / 2.0,
                                        point.y() - sprite.height() / 2.0),
                                sprite);
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
        drawZoneShape(painter, point, radius, zone.kind == 1);
    }

    const qreal auraRadius = m_controller->auraRadius();
    if (auraRadius > 0.001)
        drawAura(painter, QPointF(px, py), qMax(20.0, auraRadius * scale));

    for (int i = 0; i < currentSnapshot.orbitals.size(); ++i) {
        const SurvivorController::RenderOrbital &orbital = currentSnapshot.orbitals.at(i);
        const QPointF point(screenX(orbital.x), screenY(orbital.y));
        if (point.x() < -drawMargin || point.x() > width() + drawMargin
            || point.y() < -drawMargin || point.y() > height() + drawMargin) {
            continue;
        }
        const int coinSize = qRound(qMax(4.0, orbital.radius * scale));
        const QPixmap &sprite = orbitalSprite(coinSize);
        painter->drawPixmap(QPointF(point.x() - sprite.width() / 2.0,
                                    point.y() - sprite.height() / 2.0),
                            sprite);
    }

    int projectileIndex = 0;
    for (int i = 0; i < currentSnapshot.projectiles.size(); ++i) {
        const SurvivorController::RenderProjectile &projectile = currentSnapshot.projectiles.at(i);
        const QPointF point(screenX(projectile.x), screenY(projectile.y));
        if (point.x() < -drawMargin || point.x() > width() + drawMargin
            || point.y() < -drawMargin || point.y() > height() + drawMargin) {
            ++projectileIndex;
            continue;
        }
        const int projectileSize = qRound(qMax(projectile.kind == 2 ? 5.0 : 4.8, projectile.radius * scale * 1.2));
        const QPixmap &sprite = projectileSprite(projectile.kind, projectileSize);
        if (projectile.kind == 2) {
            painter->save();
            painter->translate(point);
            painter->rotate(projectileIndex * 18.0 + m_controller->survivalTimeSec() * 8.0);
            painter->drawPixmap(QPointF(-sprite.width() / 2.0, -sprite.height() / 2.0), sprite);
            painter->restore();
        } else {
            painter->drawPixmap(QPointF(point.x() - sprite.width() / 2.0,
                                        point.y() - sprite.height() / 2.0),
                                sprite);
        }
        ++projectileIndex;
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

        const bool highDetail = drawRadius >= 11.5 || enemy.elite || enemy.chestCarrier;
        const QPixmap &sprite = enemySprite(enemy.kind, qRound(drawRadius), highDetail, enemy.hitFlashMs > 0);
        painter->drawPixmap(QPointF(point.x() - sprite.width() / 2.0,
                                    point.y() - sprite.height() * 0.38),
                            sprite);

        if (enemy.elite || enemy.chestCarrier) {
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(enemy.chestCarrier ? QColor("#E6B678") : QColor("#D69A6D"), 2.0));
            painter->drawEllipse(point, drawRadius + 3.0, drawRadius + 3.0);
        }

        if (enemy.maxHp > 0 && enemy.hp < enemy.maxHp) {
            const qreal hpWidth = drawRadius * 1.9;
            painter->fillRect(QRectF(point.x() - hpWidth / 2.0, point.y() - drawRadius - 13.0, hpWidth, 4.5),
                              QColor::fromRgbF(16.0 / 255.0, 24.0 / 255.0, 20.0 / 255.0, 0.82));
            painter->fillRect(QRectF(point.x() - hpWidth / 2.0, point.y() - drawRadius - 13.0,
                                     hpWidth * qMax(0.08, enemy.hp / qMax(1.0, static_cast<qreal>(enemy.maxHp))), 4.5),
                              enemy.chestCarrier ? QColor("#E7B774") : QColor("#D76456"));
        }
    }

    const QPixmap &player = playerSprite(m_compactLayout);
    painter->drawPixmap(QPointF(px - player.width() / 2.0,
                                py - player.height() / 2.0),
                        player);

    static QFont eliteFont(QStringLiteral("STSong"));
    static QFont normalFont(QStringLiteral("STSong"));
    static bool fontInitialized = false;
    if (!fontInitialized) {
        eliteFont.setPixelSize(18);
        eliteFont.setWeight(QFont::DemiBold);
        normalFont.setPixelSize(14);
        normalFont.setWeight(QFont::DemiBold);
        fontInitialized = true;
    }

    for (int i = 0; i < currentSnapshot.damageNumbers.size(); ++i) {
        const SurvivorController::RenderDamageNumber &number = currentSnapshot.damageNumbers.at(i);
        const QPointF point(screenX(number.x), screenY(number.y));
        if (point.x() < -drawMargin || point.x() > width() + drawMargin
            || point.y() < -drawMargin || point.y() > height() + drawMargin) {
            continue;
        }

        const qreal lifeRatio = qMax(0.0, static_cast<qreal>(number.lifeMs) / qMax(1.0, static_cast<qreal>(number.totalLifeMs)));
        const qreal textAlpha = 0.35 + lifeRatio * 0.65;
        const QString text = QString::number(number.amount);
        painter->setFont(number.elite ? eliteFont : normalFont);
        painter->setPen(QColor::fromRgbF(14.0 / 255.0, 18.0 / 255.0, 17.0 / 255.0, textAlpha * 0.72));
        painter->drawText(QPointF(point.x() + 1.0, point.y() + 1.0), QStringLiteral("-") + text);
        painter->setPen(number.elite
                            ? QColor::fromRgbF(239.0 / 255.0, 214.0 / 255.0, 144.0 / 255.0, textAlpha)
                            : QColor::fromRgbF(244.0 / 255.0, 235.0 / 255.0, 216.0 / 255.0, textAlpha));
        painter->drawText(QPointF(point.x(), point.y()), QStringLiteral("-") + text);
    }
}

void SurvivorRenderItem::drawRadar(QPainter *painter)
{
    painter->fillRect(boundingRect(), Qt::transparent);

    const qreal centerX = width() / 2.0;
    const qreal centerY = height() / 2.0;
    const qreal radarRadius = width() / 2.0 - 2.0;
    const qreal worldRange = m_controller->radarRange();
    const qreal scale = radarRadius / worldRange;
    const qreal playerX = m_controller->playerX();
    const qreal playerY = m_controller->playerY();

    auto clampToRadar = [worldRange](qreal dx, qreal dy, bool *faded) {
        const qreal length = qSqrt(dx * dx + dy * dy);
        if (length <= worldRange) {
            *faded = false;
            return QPointF(dx, dy);
        }
        *faded = true;
        const qreal ratio = worldRange / qMax(length, 0.0001);
        return QPointF(dx * ratio, dy * ratio);
    };

    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor("#211915"));
    painter->drawEllipse(QPointF(centerX, centerY), radarRadius, radarRadius);

    painter->setPen(QPen(QColor("#7A6037"), 1.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(QPointF(centerX, centerY), radarRadius - 1.0, radarRadius - 1.0);
    painter->drawEllipse(QPointF(centerX, centerY), radarRadius * 0.66, radarRadius * 0.66);
    painter->setPen(QPen(QColor("#4D3B22"), 1.0));
    painter->drawLine(QPointF(centerX, 0.0), QPointF(centerX, height()));
    painter->drawLine(QPointF(0.0, centerY), QPointF(width(), centerY));

    const auto &currentSnapshot = m_controller->renderSnapshot();
    const int pickupStep = qMax(1, static_cast<int>(qCeil(currentSnapshot.pickups.size() / 28.0)));
    painter->setPen(Qt::NoPen);
    for (int i = 0; i < currentSnapshot.pickups.size(); i += pickupStep) {
        const SurvivorController::RenderPickup &pickup = currentSnapshot.pickups.at(i);
        bool faded = false;
        const QPointF point = clampToRadar(pickup.x - playerX, pickup.y - playerY, &faded);
        painter->setBrush(pickup.kind == 3
                              ? (faded ? QColor::fromRgbF(240.0 / 255.0, 205.0 / 255.0, 132.0 / 255.0, 0.38) : QColor("#F0CD84"))
                              : (faded ? QColor(jadeColor(pickup.kind).red(), jadeColor(pickup.kind).green(), jadeColor(pickup.kind).blue(), 110)
                                       : jadeColor(pickup.kind)));
        painter->drawEllipse(QPointF(centerX + point.x() * scale, centerY + point.y() * scale),
                             pickup.kind == 3 ? 2.7 : 2.3,
                             pickup.kind == 3 ? 2.7 : 2.3);
    }

    const int enemyStep = qMax(1, static_cast<int>(qCeil(currentSnapshot.enemies.size() / 56.0)));
    for (int i = 0; i < currentSnapshot.enemies.size(); i += enemyStep) {
        const SurvivorController::RenderEnemy &enemy = currentSnapshot.enemies.at(i);
        bool faded = false;
        const QPointF point = clampToRadar(enemy.x - playerX, enemy.y - playerY, &faded);
        painter->setBrush(enemy.elite
                              ? (faded ? QColor::fromRgbF(214.0 / 255.0, 154.0 / 255.0, 109.0 / 255.0, 0.42) : QColor("#D69A6D"))
                              : (faded ? QColor::fromRgbF(208.0 / 255.0, 89.0 / 255.0, 74.0 / 255.0, 0.35) : QColor("#D0594A")));
        const qreal radius = enemy.elite ? 3.2 : 2.5;
        painter->drawEllipse(QPointF(centerX + point.x() * scale, centerY + point.y() * scale), radius, radius);
    }

    painter->setBrush(QColor("#F2E7D2"));
    painter->drawEllipse(QPointF(centerX, centerY), 4.0, 4.0);
}
