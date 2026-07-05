#pragma once

#include <QPointer>
#include <QQuickPaintedItem>

class SurvivorController;

class SurvivorRenderItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QObject *controller READ controller WRITE setController NOTIFY controllerChanged)
    Q_PROPERTY(bool radarMode READ radarMode WRITE setRadarMode NOTIFY radarModeChanged)
    Q_PROPERTY(bool compactLayout READ compactLayout WRITE setCompactLayout NOTIFY compactLayoutChanged)

public:
    explicit SurvivorRenderItem(QQuickItem *parent = nullptr);

    QObject *controller() const;
    void setController(QObject *controllerObject);

    bool radarMode() const { return m_radarMode; }
    void setRadarMode(bool value);

    bool compactLayout() const { return m_compactLayout; }
    void setCompactLayout(bool value);

    void paint(QPainter *painter) override;

signals:
    void controllerChanged();
    void radarModeChanged();
    void compactLayoutChanged();

private:
    void attachControllerSignals();
    void drawArena(QPainter *painter);
    void drawRadar(QPainter *painter);
    qreal arenaScale() const;

    QPointer<SurvivorController> m_controller;
    bool m_radarMode = false;
    bool m_compactLayout = false;
    int m_frameCounter = 0;
};
