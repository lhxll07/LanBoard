#include "roommanager.h"

#include <QJsonArray>
#include <QJsonObject>

RoomManager::RoomManager(QObject *parent)
    : QObject(parent)
{
    // Start with local player as host
    addPlayer(QStringLiteral("lhx"), true);
}

void RoomManager::addPlayer(const QString &name, bool host)
{
    m_players.append({name, host, false});
    emit playerListChanged();
    if (host)
        emit hostChanged();
}

void RoomManager::toggleReady()
{
    if (m_players.isEmpty())
        return;
    m_players[0].isReady = !m_players[0].isReady;
    emit playerListChanged();
}

void RoomManager::startGame()
{
    if (!canStart())
        return;
    emit gameStarted();
}

QVariantList RoomManager::playerList() const
{
    QVariantList list;
    for (const auto &p : m_players) {
        QVariantMap map;
        map[QStringLiteral("name")] = p.name;
        map[QStringLiteral("isHost")] = p.isHost;
        map[QStringLiteral("isReady")] = p.isReady;
        list.append(map);
    }
    return list;
}

bool RoomManager::isHost() const
{
    return !m_players.isEmpty() && m_players[0].isHost;
}

bool RoomManager::canStart() const
{
    // Host can start when at least 2 players are ready (including host)
    if (m_players.size() < 2)
        return false;
    if (!m_players[0].isHost)
        return false;
    int readyCount = 0;
    for (const auto &p : m_players) {
        if (p.isReady)
            ++readyCount;
    }
    return readyCount >= 2 && readyCount == m_players.size();
}
