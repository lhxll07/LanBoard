#include "roommanager.h"

#include "src/common/types.h"

RoomManager::RoomManager(QObject *parent)
    : QObject(parent)
{
}

void RoomManager::addPlayer(const QString &name, bool host, bool ready, int playerId,
                            const QString &seatType, int piece)
{
    if (playerId < 0)
        playerId = m_players.isEmpty() ? 0 : m_nextGeneratedPlayerId++;

    const int existingIndex = indexOfPlayerId(playerId);
    const LanBoard::RoomPlayerState player {
        playerId,
        name,
        host,
        ready,
        LanBoard::normalizedSeatKind(seatType),
        piece,
        QStringLiteral("defender")
    };
    if (existingIndex >= 0)
        m_players[existingIndex] = player;
    else
        m_players.append(player);

    emitStateChanged();
}

void RoomManager::setSnapshot(const LanBoard::RoomSnapshot &snapshot)
{
    const bool gameChangedState = m_gameId != snapshot.gameId;
    const bool hostOrLocalChanged = m_localPlayerId != snapshot.localPlayerId
        || snapshot.localPlayerIsHost() != isHost();

    m_roomId = snapshot.roomId;
    m_roomName = snapshot.roomName;
    m_gameId = LanBoard::normalizeGameId(snapshot.gameId);
    m_gameInProgress = snapshot.gameInProgress;
    m_localPlayerId = snapshot.localPlayerId;
    m_players = snapshot.players;

    if (gameChangedState)
        emit gameChanged();
    if (hostOrLocalChanged) {
        emit hostChanged();
        emit localPlayerIndexChanged();
    }
    emit playerListChanged();
}

void RoomManager::setRoomIdentity(const QString &roomId, const QString &roomName)
{
    m_roomId = roomId.trimmed();
    m_roomName = roomName.trimmed();
}

void RoomManager::setGameInProgress(bool inProgress)
{
    if (m_gameInProgress == inProgress)
        return;

    m_gameInProgress = inProgress;
    emit playerListChanged();
}

RoomManager::ActionError RoomManager::tryAddRoomPlayer(const QString &name, bool host, int playerId)
{
    if (playerId < 0 || playerId >= roomCapacity())
        return ActionError::InvalidPlayerId;
    if (indexOfPlayerId(playerId) >= 0)
        return ActionError::PlayerAlreadyExists;
    if (m_players.size() >= roomCapacity())
        return ActionError::RoomFull;

    const QString seatType = host
        ? QStringLiteral("active")
        : LanBoard::seatTypeString(activeGuestCount() < LanBoard::activeGuestLimitForGame(m_gameId)
                ? LanBoard::SeatKind::Active
                : LanBoard::SeatKind::Spectator);
    m_players.append(LanBoard::RoomPlayerState {
        playerId,
        name,
        host,
        false,
        LanBoard::normalizedSeatKind(seatType),
        0,
        QStringLiteral("defender")
    });
    normalizeSeats();
    emitStateChanged();
    return ActionError::None;
}

RoomManager::ActionError RoomManager::tryChangeSeat(int playerId, const QString &seatType)
{
    const int index = indexOfPlayerId(playerId);
    if (index < 0)
        return ActionError::PlayerNotFound;
    if (m_players[index].isHost)
        return ActionError::HostLockedActive;
    if (m_gameInProgress)
        return ActionError::GameInProgress;

    const LanBoard::SeatKind normalizedSeatKind = LanBoard::normalizedSeatKind(seatType);
    if (normalizedSeatKind == LanBoard::SeatKind::Active
        && !m_players[index].isActive()
        && activePlayerCount() >= maxPlayers()) {
        return ActionError::ActiveSeatFull;
    }

    if (m_players[index].seatKind == normalizedSeatKind)
        return ActionError::None;

    m_players[index].seatKind = normalizedSeatKind;
    if (!m_players[index].isActive())
        m_players[index].isReady = false;
    normalizeSeats(false);
    emitStateChanged();
    return ActionError::None;
}

RoomManager::ActionError RoomManager::tryChangeDormDefenseRole(int playerId, const QString &role)
{
    if (!LanBoard::isDormDefenseGame(m_gameId))
        return ActionError::InvalidDormDefenseRole;

    const int index = indexOfPlayerId(playerId);
    if (index < 0)
        return ActionError::PlayerNotFound;
    if (m_gameInProgress)
        return ActionError::GameInProgress;

    const QString trimmedRole = role.trimmed().toLower();
    if (trimmedRole != QStringLiteral("defender") && trimmedRole != QStringLiteral("ghost"))
        return ActionError::InvalidDormDefenseRole;
    if (trimmedRole == QStringLiteral("ghost") && !m_players[index].isActive())
        return ActionError::DormDefenseGhostRequiresActiveSeat;

    if (trimmedRole == QStringLiteral("ghost")) {
        for (int i = 0; i < m_players.size(); ++i) {
            if (i == index || !m_players[i].isActive())
                continue;
            if (LanBoard::normalizedDormDefenseRole(m_players[i].dormDefenseRole)
                == QStringLiteral("ghost")) {
                return ActionError::DormDefenseGhostAlreadyTaken;
            }
        }
    }

    const QString normalizedRole = LanBoard::normalizedDormDefenseRole(trimmedRole);
    if (m_players[index].dormDefenseRole == normalizedRole)
        return ActionError::None;

    m_players[index].dormDefenseRole = normalizedRole;
    normalizeDormDefenseRoles();
    emitStateChanged();
    return ActionError::None;
}

RoomManager::ActionError RoomManager::trySwitchGame(int playerId, const QString &gameId)
{
    const int index = indexOfPlayerId(playerId);
    if (index < 0)
        return ActionError::PlayerNotFound;
    if (!m_players[index].isHost)
        return ActionError::OnlyHostCanSwitchGame;
    if (m_gameInProgress)
        return ActionError::GameInProgress;

    const QString normalized = LanBoard::normalizeGameId(gameId);
    if (m_gameId == normalized)
        return ActionError::None;

    m_gameId = normalized;
    m_gameInProgress = false;
    clearReadyStates();
    normalizeSeats();
    emit gameChanged();
    emitStateChanged();
    return ActionError::None;
}

RoomManager::ActionError RoomManager::tryStartGame(int playerId)
{
    const int index = indexOfPlayerId(playerId);
    if (index < 0)
        return ActionError::PlayerNotFound;
    if (!m_players[index].isHost)
        return ActionError::OnlyHostCanStart;
    if (m_gameInProgress)
        return ActionError::GameInProgress;
    if (!LanBoard::hasEnoughActivePlayersToStart(m_gameId, activePlayerCount()))
        return ActionError::MissingPlayers;
    if (LanBoard::requiresReadyForStartForGame(m_gameId) && !snapshot().allActivePlayersReady())
        return ActionError::PlayersNotReady;

    m_gameInProgress = true;
    emit playerListChanged();
    return ActionError::None;
}

void RoomManager::addTestPlayer(const QString &name)
{
    const int playerId = allocatePlayerId();
    if (playerId < 0)
        return;
    tryAddRoomPlayer(name, false, playerId);
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
    if (tryStartGame(m_localPlayerId) != ActionError::None)
        return;
    emit gameStarted();
}

void RoomManager::concludeGame()
{
    bool changed = m_gameInProgress;
    m_gameInProgress = false;
    for (auto &player : m_players) {
        if (!player.isActive() || !player.isReady)
            ;
        else {
            player.isReady = false;
            changed = true;
        }

        if (player.dormDefenseRole != QStringLiteral("defender")) {
            player.dormDefenseRole = QStringLiteral("defender");
            changed = true;
        }
    }

    if (changed)
        emitStateChanged();
}

void RoomManager::reset()
{
    m_players.clear();
    m_roomId.clear();
    m_roomName.clear();
    m_gameInProgress = false;
    emitStateChanged();
}

void RoomManager::setGameId(const QString &gameId)
{
    const QString normalized = LanBoard::normalizeGameId(gameId);
    if (m_gameId == normalized)
        return;

    m_gameId = normalized;
    normalizeSeats();
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

int RoomManager::roomCapacity() const
{
    return LanBoard::roomCapacityForGame(m_gameId);
}

int RoomManager::activePlayerCount() const
{
    return snapshot().activePlayerCount();
}

int RoomManager::allocatePlayerId() const
{
    for (int candidate = 0; candidate < roomCapacity(); ++candidate) {
        if (indexOfPlayerId(candidate) < 0)
            return candidate;
    }
    return -1;
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
    normalizeSeats(false);
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
    normalizeSeats();
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

bool RoomManager::isPlayerHost(int playerId) const
{
    const int index = indexOfPlayerId(playerId);
    return index >= 0 && m_players[index].isHost;
}

int RoomManager::playerPiece(int playerId) const
{
    const int index = indexOfPlayerId(playerId);
    return index >= 0 ? m_players[index].piece : 0;
}

QString RoomManager::playerName(int playerId) const
{
    const int index = indexOfPlayerId(playerId);
    return index >= 0 ? m_players[index].name : QString();
}

QJsonObject RoomManager::roomStateMessageForPlayer(int playerId, const QString &mode) const
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("room_state");
    msg[QStringLiteral("roomId")] = m_roomId;
    msg[QStringLiteral("roomName")] = m_roomName;
    msg[QStringLiteral("gameId")] = m_gameId;
    msg[QStringLiteral("gameName")] = gameName();
    msg[QStringLiteral("maxPlayers")] = maxPlayers();
    msg[QStringLiteral("roomCapacity")] = roomCapacity();
    msg[QStringLiteral("gameInProgress")] = m_gameInProgress;
    msg[QStringLiteral("players")] = snapshot().playerJsonArray();
    msg[QStringLiteral("yourPlayerId")] = playerId;
    msg[QStringLiteral("yourPiece")] = playerPiece(playerId);
    if (!mode.isEmpty())
        msg[QStringLiteral("mode")] = mode;
    return msg;
}

LanBoard::RoomSnapshot RoomManager::snapshot() const
{
    LanBoard::RoomSnapshot room;
    room.roomId = m_roomId;
    room.roomName = m_roomName;
    room.gameId = m_gameId;
    room.gameName = LanBoard::gameName(m_gameId);
    room.maxPlayers = maxPlayers();
    room.roomCapacity = roomCapacity();
    room.localPlayerId = m_localPlayerId;
    room.gameInProgress = m_gameInProgress;
    room.players = m_players;
    return room;
}

QString RoomManager::actionErrorKey(ActionError error) const
{
    switch (error) {
    case ActionError::None:
        return QString();
    case ActionError::PlayerNotFound:
        return QStringLiteral("player_not_found");
    case ActionError::PlayerAlreadyExists:
        return QStringLiteral("player_already_exists");
    case ActionError::RoomFull:
        return QStringLiteral("room_full");
    case ActionError::InvalidPlayerId:
        return QStringLiteral("invalid_player_id");
    case ActionError::OnlyHostCanStart:
        return QStringLiteral("only_player_one_can_start");
    case ActionError::OnlyHostCanSwitchGame:
        return QStringLiteral("only_host_can_switch_game");
    case ActionError::HostLockedActive:
        return QStringLiteral("host_locked_active");
    case ActionError::ActiveSeatFull:
        return QStringLiteral("active_seat_full");
    case ActionError::MissingPlayers:
        return LanBoard::missingPlayersErrorForGame(m_gameId);
    case ActionError::PlayersNotReady:
        return QStringLiteral("players_not_ready");
    case ActionError::GameInProgress:
        return QStringLiteral("game_in_progress");
    case ActionError::InvalidDormDefenseRole:
        return QStringLiteral("dormdefense_invalid_role");
    case ActionError::DormDefenseGhostRequiresActiveSeat:
        return QStringLiteral("dormdefense_ghost_requires_active_seat");
    case ActionError::DormDefenseGhostAlreadyTaken:
        return QStringLiteral("dormdefense_ghost_taken");
    case ActionError::DormDefenseGhostRequired:
        return QStringLiteral("dormdefense_need_exactly_one_ghost");
    }

    return QStringLiteral("unknown_room_error");
}

int RoomManager::indexOfPlayerId(int playerId) const
{
    for (int i = 0; i < m_players.size(); ++i) {
        if (m_players[i].playerId == playerId)
            return i;
    }
    return -1;
}

void RoomManager::normalizeSeats(bool fillMissingActiveSeats)
{
    int activeGuests = 0;
    for (auto &player : m_players) {
        if (player.isHost) {
            player.seatKind = LanBoard::SeatKind::Active;
            continue;
        }
        if (player.isActive())
            ++activeGuests;
    }

    for (int i = m_players.size() - 1;
         i >= 0 && activeGuests > LanBoard::activeGuestLimitForGame(m_gameId);
         --i) {
        auto &player = m_players[i];
        if (player.isHost || !player.isActive())
            continue;
        player.seatKind = LanBoard::SeatKind::Spectator;
        player.isReady = false;
        --activeGuests;
    }

    if (fillMissingActiveSeats) {
        for (auto &player : m_players) {
            if (activeGuests >= LanBoard::activeGuestLimitForGame(m_gameId))
                break;
            if (player.isHost || player.seatKind != LanBoard::SeatKind::Spectator)
                continue;
            player.seatKind = LanBoard::SeatKind::Active;
            ++activeGuests;
        }
    }

    bool whiteAssigned = false;
    for (auto &player : m_players) {
        if (player.isHost) {
            player.piece = LanBoard::usesBoardPiecesForGame(m_gameId) ? 1 : 0;
            continue;
        }

        if (!LanBoard::usesBoardPiecesForGame(m_gameId)) {
            player.piece = 0;
        } else if (player.isActive() && !whiteAssigned) {
            player.piece = 2;
            whiteAssigned = true;
        } else {
            player.piece = 0;
        }
    }

    normalizeDormDefenseRoles();
}

void RoomManager::normalizeDormDefenseRoles()
{
    bool ghostAssigned = false;
    for (auto &player : m_players) {
        if (!LanBoard::isDormDefenseGame(m_gameId) || !player.isActive()) {
            player.dormDefenseRole = QStringLiteral("defender");
            continue;
        }

        const QString role = LanBoard::normalizedDormDefenseRole(player.dormDefenseRole);
        if (role == QStringLiteral("ghost") && !ghostAssigned) {
            player.dormDefenseRole = QStringLiteral("ghost");
            ghostAssigned = true;
        } else {
            player.dormDefenseRole = QStringLiteral("defender");
        }
    }
}

void RoomManager::emitStateChanged()
{
    emit playerListChanged();
    emit hostChanged();
    emit localPlayerIndexChanged();
}
