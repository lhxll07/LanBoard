#include "roommanager.h"

#include "src/common/types.h"

namespace {

QString normalizedSeatTypeValue(const QString &seatType)
{
    return seatType == QStringLiteral("spectator")
        ? QStringLiteral("spectator")
        : QStringLiteral("active");
}

bool isActiveSeat(const QString &seatType)
{
    return seatType == QStringLiteral("active");
}

}

RoomManager::RoomManager(QObject *parent)
    : QObject(parent)
{
}

void RoomManager::addPlayer(const QString &name, bool host, bool ready, int playerId,
                            const QString &seatType)
{
    if (playerId < 0) {
        playerId = m_players.isEmpty() ? 0 : m_nextGeneratedPlayerId++;
    }

    const int existingIndex = indexOfPlayerId(playerId);
    if (existingIndex >= 0) {
        m_players[existingIndex] = {playerId, name, host, ready, normalizedSeatTypeValue(seatType)};
    } else {
        m_players.append({playerId, name, host, ready, normalizedSeatTypeValue(seatType)});
    }

    emitStateChanged();
}

void RoomManager::addTestPlayer(const QString &name)
{
    addPlayer(name,
              false,
              activePlayerCount() < maxPlayers(),
              -1,
              activePlayerCount() < maxPlayers()
                  ? QStringLiteral("active")
                  : QStringLiteral("spectator"));
}

void RoomManager::toggleReady()
{
    const int index = localPlayerIndex();
    if (index < 0 || !isActiveSeat(m_players[index].seatType))
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

void RoomManager::setGameId(const QString &gameId)
{
    const QString normalized = LanBoard::normalizeGameId(gameId);
    if (m_gameId == normalized)
        return;

    m_gameId = normalized;
    emit gameChanged();
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
        map[QStringLiteral("seatType")] = p.seatType;
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
    if (activePlayerCount() != maxPlayers())
        return false;
    if (!isHost())
        return false;
    for (const auto &p : m_players) {
        if (!isActiveSeat(p.seatType))
            continue;
        if (!p.isReady)
            return false;
    }
    return true;
}

int RoomManager::localPlayerIndex() const
{
    return indexOfPlayerId(m_localPlayerId);
}

QString RoomManager::gameName() const
{
    return LanBoard::gameName(m_gameId);
}

int RoomManager::maxPlayers() const
{
    return LanBoard::maxPlayersForGame(m_gameId);
}

int RoomManager::activePlayerCount() const
{
    int count = 0;
    for (const auto &player : m_players) {
        if (isActiveSeat(player.seatType))
            ++count;
    }
    return count;
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
    if (index < 0
        || !isActiveSeat(m_players[index].seatType)
        || m_players[index].isReady == ready) {
        return false;
    }

    m_players[index].isReady = ready;
    emitStateChanged();
    return true;
}

bool RoomManager::setPlayerSeatById(int playerId, const QString &seatType)
{
    const int index = indexOfPlayerId(playerId);
    if (index < 0)
        return false;

    const QString normalizedSeatType = normalizedSeatTypeValue(seatType);
    if (m_players[index].seatType == normalizedSeatType)
        return false;

    m_players[index].seatType = normalizedSeatType;
    if (normalizedSeatType == QStringLiteral("spectator"))
        m_players[index].isReady = false;
    emitStateChanged();
    return true;
}

bool RoomManager::clearReadyStates()
{
    bool changed = false;
    for (auto &player : m_players) {
        if (!isActiveSeat(player.seatType) || !player.isReady)
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
        if (!player.isHost && isActiveSeat(player.seatType))
            return player.playerId;
    }
    return -1;
}

int RoomManager::activeGuestCount() const
{
    int count = 0;
    for (const auto &player : m_players) {
        if (!player.isHost && isActiveSeat(player.seatType))
            ++count;
    }
    return count;
}

bool RoomManager::isPlayerActive(int playerId) const
{
    const int index = indexOfPlayerId(playerId);
    return index >= 0 && isActiveSeat(m_players[index].seatType);
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
