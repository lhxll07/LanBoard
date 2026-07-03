#include "roommanager.h"

#include <QJsonArray>
#include <QJsonObject>

RoomManager::RoomManager(QObject *parent)
    : QObject(parent)
{
}

void RoomManager::addPlayer(const QString &name, bool host, bool ready, int playerId)
{
    if (playerId < 0) {
        playerId = m_players.isEmpty() ? 0 : m_nextGeneratedPlayerId++;
    }

    const int existingIndex = indexOfPlayerId(playerId);
    if (existingIndex >= 0) {
        m_players[existingIndex] = {playerId, name, host, ready};
    } else {
        m_players.append({playerId, name, host, ready});
    }

    emitStateChanged();
}

void RoomManager::addTestPlayer(const QString &name)
{
    addPlayer(name, false, true);
}

void RoomManager::toggleReady()
{
    const int index = localPlayerIndex();
    if (index < 0)
        return;
    m_players[index].isReady = !m_players[index].isReady;
    emitStateChanged();
}

void RoomManager::startGame()
{
    if (!canStart())
        return;
    emit gameStarted();
}

void RoomManager::reset()
{
    m_players.clear();
    emitStateChanged();
}

QVariantList RoomManager::playerList() const
{
    QVariantList list;
    for (const auto &p : m_players) {
        QVariantMap map;
        map[QStringLiteral("playerId")] = p.playerId;
        map[QStringLiteral("name")] = p.name;
        map[QStringLiteral("isHost")] = p.isHost;
        map[QStringLiteral("isReady")] = p.isReady;
        list.append(map);
    }
    return list;
}

bool RoomManager::isHost() const
{
    const int index = localPlayerIndex();
    return index >= 0 && m_players[index].isHost;
}

bool RoomManager::canStart() const
{
    if (m_players.size() != 2)
        return false;
    if (!isHost())
        return false;
    for (const auto &p : m_players) {
        if (!p.isReady)
            return false;
    }
    return true;
}

int RoomManager::localPlayerIndex() const
{
    return indexOfPlayerId(m_localPlayerId);
}

void RoomManager::setLocalPlayerId(int playerId)
{
    if (m_localPlayerId == playerId)
        return;

    m_localPlayerId = playerId;
    emit hostChanged();
    emit localPlayerIndexChanged();
}

bool RoomManager::setPlayerReadyById(int playerId, bool ready)
{
    const int index = indexOfPlayerId(playerId);
    if (index < 0 || m_players[index].isReady == ready)
        return false;

    m_players[index].isReady = ready;
    emitStateChanged();
    return true;
}

bool RoomManager::clearReadyStates()
{
    bool changed = false;
    for (auto &player : m_players) {
        if (!player.isReady)
            continue;
        player.isReady = false;
        changed = true;
    }

    if (changed)
        emitStateChanged();

    return changed;
}

bool RoomManager::removePlayerById(int playerId)
{
    const int index = indexOfPlayerId(playerId);
    if (index < 0)
        return false;

    m_players.removeAt(index);
    emitStateChanged();
    return true;
}

int RoomManager::firstGuestPlayerId() const
{
    for (const auto &player : m_players) {
        if (!player.isHost)
            return player.playerId;
    }
    return -1;
}

int RoomManager::indexOfPlayerId(int playerId) const
{
    for (int i = 0; i < m_players.size(); ++i) {
        if (m_players[i].playerId == playerId)
            return i;
    }
    return -1;
}

void RoomManager::emitStateChanged()
{
    emit playerListChanged();
    emit hostChanged();
    emit localPlayerIndexChanged();
}
