#include "roommanager.h"

#include "src/common/types.h"

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
    const LanBoard::RoomPlayerState player {
        playerId,
        name,
        host,
        ready,
        LanBoard::normalizedSeatKind(seatType)
    };
    if (existingIndex >= 0) {
        m_players[existingIndex] = player;
    } else {
        m_players.append(player);
    }

    emitStateChanged();
}

void RoomManager::addTestPlayer(const QString &name)
{
    addPlayer(name,
              false,
              activePlayerCount() < maxPlayers(),
              -1,
              LanBoard::seatTypeString(activePlayerCount() < maxPlayers()
                  ? LanBoard::SeatKind::Active
                  : LanBoard::SeatKind::Spectator));
}

void RoomManager::toggleReady()
{
    const int index = localPlayerIndex();
    if (index < 0 || !m_players[index].isActive())
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
    return snapshot().playerVariantList();
}

bool RoomManager::isHost() const
{
    const int index = localPlayerIndex();
    return index >= 0 && m_players[index].isHost;
}

bool RoomManager::canStart() const
{
    return snapshot().canStartForLocalHost();
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
    return snapshot().activePlayerCount();
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
        || !m_players[index].isActive()
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

    const LanBoard::SeatKind normalizedSeatKind = LanBoard::normalizedSeatKind(seatType);
    if (m_players[index].seatKind == normalizedSeatKind)
        return false;

    m_players[index].seatKind = normalizedSeatKind;
    if (!m_players[index].isActive())
        m_players[index].isReady = false;
    emitStateChanged();
    return true;
}

bool RoomManager::clearReadyStates()
{
    bool changed = false;
    for (auto &player : m_players) {
        if (!player.isActive() || !player.isReady)
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
    return snapshot().firstGuestPlayerId();
}

int RoomManager::activeGuestCount() const
{
    return snapshot().activeGuestCount();
}

bool RoomManager::isPlayerActive(int playerId) const
{
    const int index = indexOfPlayerId(playerId);
    return index >= 0 && m_players[index].isActive();
}

int RoomManager::indexOfPlayerId(int playerId) const
{
    for (int i = 0; i < m_players.size(); ++i) {
        if (m_players[i].playerId == playerId)
            return i;
    }
    return -1;
}

LanBoard::RoomSnapshot RoomManager::snapshot() const
{
    LanBoard::RoomSnapshot room;
    room.gameId = m_gameId;
    room.gameName = LanBoard::gameName(m_gameId);
    room.maxPlayers = maxPlayers();
    room.roomCapacity = roomCapacity();
    room.localPlayerId = m_localPlayerId;
    room.players = m_players;
    return room;
}

void RoomManager::emitStateChanged()
{
    emit playerListChanged();
    emit hostChanged();
    emit localPlayerIndexChanged();
}
