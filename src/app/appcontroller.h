#pragma once

#include <QObject>
#include "src/lobby/roommanager.h"
#include "src/game/gamecontroller.h"

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(RoomManager *roomManager READ roomManager CONSTANT)
    Q_PROPERTY(GameController *gameController READ gameController CONSTANT)

public:
    explicit AppController(QObject *parent = nullptr);

    RoomManager *roomManager() const { return m_roomManager; }
    GameController *gameController() const { return m_gameController; }

private:
    RoomManager *m_roomManager = nullptr;
    GameController *m_gameController = nullptr;
};
