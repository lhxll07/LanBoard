#include "appcontroller.h"

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_roomManager(new RoomManager(this))
    , m_gameController(new GameController(this))
{
    // Connect room → game transition
    connect(m_roomManager, &RoomManager::gameStarted, this, [this]() {
        m_gameController->startNewGame();
    });
}
