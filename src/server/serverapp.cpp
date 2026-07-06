#include "serverapp.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QRandomGenerator>

#include "../game/survivornetcodec.h"
#include "../network/enetutils.h"
#include "../network/protocolids.h"

namespace {

constexpr quint16 kDefaultPort = 44567;
constexpr int kServiceIntervalMs = 4;
constexpr enet_uint8 kSurvivorFrameChannel = 2;
constexpr enet_uint8 kSurvivorHudChannel = 3;

QString normalizedName(const QString &name)
{
    const QString trimmed = name.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("player") : trimmed;
}

QString normalizedRoomName(const QString &roomName, const QString &hostName, const QString &gameId)
{
    const QString trimmed = roomName.trimmed();
    if (!trimmed.isEmpty())
        return trimmed;
    return QStringLiteral("%1的%2房间").arg(hostName, LanBoard::gameName(gameId));
}

}

ServerApp::ServerApp(QObject *parent)
    : QObject(parent)
{
    LanBoard::Enet::initialize();
    m_serviceTimer.setInterval(kServiceIntervalMs);
    m_serviceTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_serviceTimer, &QTimer::timeout, this, &ServerApp::serviceNetwork);
}

ServerApp::~ServerApp()
{
    m_serviceTimer.stop();

    for (RoomState *room : std::as_const(m_rooms))
        delete room;
    m_rooms.clear();
    m_players.clear();

    if (m_host) {
        enet_host_destroy(m_host);
        m_host = nullptr;
    }

    LanBoard::Enet::deinitialize();
}

bool ServerApp::start(quint16 port)
{
    if (port == 0)
        port = kDefaultPort;

    if (m_host) {
        enet_host_destroy(m_host);
        m_host = nullptr;
    }

    ENetAddress address {};
    address.host = ENET_HOST_ANY;
    address.port = port;
    m_host = enet_host_create(&address, 64, 4, 0, 0);
    if (!m_host)
        return false;

    if (!m_serviceTimer.isActive())
        m_serviceTimer.start();

    qInfo().noquote() << QStringLiteral("LanBoard ENet server listening on port %1").arg(port);
    return true;
}

void ServerApp::serviceNetwork()
{
    if (!m_host)
        return;

    ENetEvent event {};
    while (enet_host_service(m_host, &event, 0) > 0) {
        switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT:
            handleConnect(event.peer);
            break;
        case ENET_EVENT_TYPE_RECEIVE:
            handleReceive(event.peer, event.packet);
            enet_packet_destroy(event.packet);
            break;
        case ENET_EVENT_TYPE_DISCONNECT:
            handleDisconnect(event.peer);
            break;
        case ENET_EVENT_TYPE_NONE:
        default:
            break;
        }
    }
}

void ServerApp::handleConnect(ENetPeer *peer)
{
    if (!peer)
        return;

    const QString host = QStringLiteral("%1:%2")
        .arg(QHostAddress(peer->address.host).toString())
        .arg(peer->address.port);
    qInfo().noquote() << QStringLiteral("Incoming ENet connection from %1").arg(host);
}

void ServerApp::handleDisconnect(ENetPeer *peer)
{
    if (!peer)
        return;

    PlayerSession *session = sessionForPeer(peer);
    if (!session) {
        peer->data = nullptr;
        return;
    }

    const QString roomId = session->roomId;
    RoomState *room = roomForPlayer(session);
    const QString playerName = session->name;
    const bool wasHost = session->isHost;
    const int disconnectedPiece = session->piece;

    if (wasHost && room) {
        QList<ENetPeer *> remainingPeers;
        for (const auto &player : std::as_const(m_players)) {
            if (player.roomId != roomId || player.peer == peer || !player.peer)
                continue;
            remainingPeers.append(player.peer);
        }

        for (ENetPeer *otherPeer : remainingPeers) {
            sendError(otherPeer, QStringLiteral("room_closed"));
            enet_peer_disconnect(otherPeer, 0);
        }

        for (int i = m_players.size() - 1; i >= 0; --i) {
            if (m_players.at(i).roomId == roomId)
                m_players.removeAt(i);
        }

        for (int i = 0; i < m_rooms.size(); ++i) {
            if (m_rooms.at(i)->roomId == roomId) {
                delete m_rooms.at(i);
                m_rooms.removeAt(i);
                break;
            }
        }
    } else {
        for (int i = 0; i < m_players.size(); ++i) {
            if (m_players[i].peer == peer) {
                m_players.removeAt(i);
                break;
            }
        }

        room = roomById(roomId);
        if (room) {
            if (isDouDiZhuRoom(room->gameId)) {
                resetFinishedRoom(room);
            } else if (isSurvivorRoom(room->gameId)
                       && room->gameActive
                       && LanBoard::isActiveSeat(session->seatKind)) {
                concludeRoomGame(room, 0, false);
            } else if (isFlightChessRoom(room->gameId)
                       && room->gameActive
                       && LanBoard::isActiveSeat(session->seatKind)
                       && disconnectedPiece != 0) {
                room->flightChessController->setGameOver(otherPiece(disconnectedPiece));
                concludeRoomGame(room, room->flightChessController->winner(), false);
            } else if (room->gameActive
                       && LanBoard::isActiveSeat(session->seatKind)
                       && disconnectedPiece != 0) {
                room->gameController->setGameOver(otherPiece(disconnectedPiece));
                concludeRoomGame(room, room->gameController->winner(), false);
            }

            normalizeSeats(room);
            broadcastRoomState(room);
            removeRoomIfEmpty(roomId);
        }
    }

    qInfo().noquote() << QStringLiteral("Disconnected: %1").arg(playerName);
    peer->data = nullptr;
}

void ServerApp::handleReceive(ENetPeer *peer, ENetPacket *packet)
{
    if (!peer || !packet)
        return;

    const QByteArray payload(reinterpret_cast<const char *>(packet->data),
                             static_cast<qsizetype>(packet->dataLength));
    if (processBinaryPacket(peer, payload))
        return;

    QJsonObject object;
    if (!LanBoard::Enet::decodeJsonPacket(packet, object)) {
        sendError(peer, QStringLiteral("invalid_json"));
        return;
    }

    processMessage(peer, object);
}

void ServerApp::processMessage(ENetPeer *peer, const QJsonObject &msg)
{
    const QString type = msg.value(QStringLiteral("type")).toString();

    if (type == LanBoard::Protocol::Join) {
        handleJoin(peer,
                   msg.value(QStringLiteral("name")).toString(),
                   msg.value(QStringLiteral("gameId")).toString());
    } else if (type == LanBoard::Protocol::ListRooms) {
        handleListRooms(peer);
    } else if (type == LanBoard::Protocol::CreateRoom) {
        handleCreateRoom(peer,
                         msg.value(QStringLiteral("name")).toString(),
                         msg.value(QStringLiteral("roomName")).toString(),
                         msg.value(QStringLiteral("gameId")).toString());
    } else if (type == LanBoard::Protocol::JoinRoom) {
        handleJoinRoom(peer,
                       msg.value(QStringLiteral("name")).toString(),
                       msg.value(QStringLiteral("roomId")).toString());
    } else if (type == LanBoard::Protocol::Ready) {
        handleReady(peer, msg.value(QStringLiteral("ready")).toBool());
    } else if (type == LanBoard::Protocol::StartGame) {
        handleStartGame(peer);
    } else if (type == LanBoard::Protocol::ChangeSeat) {
        handleChangeSeat(peer, msg.value(QStringLiteral("seatType")).toString());
    } else if (type == LanBoard::Protocol::SwitchRoomGame) {
        handleSwitchRoomGame(peer, msg.value(QStringLiteral("gameId")).toString());
    } else if (type == LanBoard::Protocol::PlacePiece) {
        handlePlacePiece(peer,
                         msg.value(QStringLiteral("row")).toInt(-1),
                         msg.value(QStringLiteral("col")).toInt(-1));
    } else if (type == LanBoard::Protocol::FlightRoll) {
        handleFlightRoll(peer);
    } else if (type == LanBoard::Protocol::FlightMove) {
        handleFlightMove(peer, msg.value(QStringLiteral("planeIndex")).toInt(-1));
    } else if (type == LanBoard::Protocol::Surrender) {
        handleSurrender(peer);
    } else if (type == LanBoard::Protocol::GameOver) {
        handleGameOver(peer, msg.value(QStringLiteral("winner")).toInt());
    } else if (type == LanBoard::Protocol::DouDiZhuPlay) {
        handleDouDiZhuPlay(peer, msg.value(QStringLiteral("cards")).toArray());
    } else if (type == LanBoard::Protocol::DouDiZhuPass) {
        handleDouDiZhuPass(peer);
    } else {
        sendError(peer, QStringLiteral("unsupported_message_type"));
    }
}

void ServerApp::handleJoin(ENetPeer *peer, const QString &name, const QString &gameId)
{
    const QString normalized = LanBoard::normalizeGameId(gameId);
    RoomState *targetRoom = nullptr;
    for (RoomState *room : std::as_const(m_rooms)) {
        if (room->gameId == normalized) {
            targetRoom = room;
            break;
        }
    }

    if (!targetRoom) {
        handleCreateRoom(peer, name, QString(), normalized);
        return;
    }

    handleJoinRoom(peer, name, targetRoom->roomId);
}

void ServerApp::handleListRooms(ENetPeer *peer)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("rooms_list");
    msg[QStringLiteral("rooms")] = roomListPayload();
    sendJson(peer, msg);
    enet_peer_disconnect_later(peer, 0);
}

void ServerApp::handleCreateRoom(ENetPeer *peer, const QString &name, const QString &roomName,
                                 const QString &gameId)
{
    if (!peer)
        return;

    if (sessionForPeer(peer)) {
        sendError(peer, QStringLiteral("already_joined"));
        return;
    }

    RoomState *room = new RoomState();
    room->roomId = createRoomId();
    room->gameId = LanBoard::normalizeGameId(gameId);
    room->roomName = normalizedRoomName(roomName, normalizedName(name), room->gameId);
    room->gameController = std::make_unique<GameController>();
    room->douDiZhuController = std::make_unique<DouDiZhuController>();
    room->flightChessController = std::make_unique<FlightChessController>();
    room->survivorController = std::make_unique<SurvivorController>();
    room->gameActive = false;
    connect(room->survivorController.get(), &SurvivorController::networkSyncRequested,
            this, [this, room](bool includeHudDetails) {
        if (!room || !room->gameActive || !isSurvivorRoom(room->gameId))
            return;
        for (const PlayerSession *player : activePlayersInRoom(room->roomId)) {
            if (!player || !player->peer)
                continue;
            sendRaw(player->peer,
                    room->survivorController->buildFastNetworkPacket(player->playerId),
                    kSurvivorFrameChannel,
                    0);
            if (includeHudDetails) {
                sendRaw(player->peer,
                        room->survivorController->buildHudNetworkPacket(player->playerId),
                        kSurvivorHudChannel,
                        ENET_PACKET_FLAG_RELIABLE);
            }
        }
    });
    connect(room->survivorController.get(), &SurvivorController::gameOverChanged,
            this, [this, room]() {
        if (!room || !room->gameActive || !room->survivorController
            || !room->survivorController->isGameOver()) {
            return;
        }
        concludeRoomGame(room, 0);
    });
    m_rooms.append(room);

    PlayerSession session;
    session.playerId = 0;
    session.piece = initialPieceForGame(room->gameId, true);
    session.name = normalizedName(name);
    session.roomId = room->roomId;
    session.isHost = true;
    session.seatKind = LanBoard::SeatKind::Active;
    session.peer = peer;
    m_players.append(session);
    peer->data = reinterpret_cast<void *>(1);

    resetGame(room);
    broadcastRoomState(room);
}

void ServerApp::handleJoinRoom(ENetPeer *peer, const QString &name, const QString &roomId)
{
    if (!peer)
        return;

    if (sessionForPeer(peer)) {
        sendError(peer, QStringLiteral("already_joined"));
        return;
    }

    RoomState *room = roomById(roomId.trimmed());
    if (!room) {
        sendError(peer, QStringLiteral("room_not_found"));
        return;
    }

    if (room->gameActive) {
        sendError(peer, QStringLiteral("room_in_game"));
        return;
    }

    const auto players = playersInRoom(room->roomId);
    if (players.size() >= roomCapacity()) {
        sendError(peer, QStringLiteral("room_full"));
        return;
    }

    PlayerSession session;
    session.playerId = nextPlayerIdForRoom(room->roomId, room->gameId);
    if (session.playerId < 0) {
        sendError(peer, QStringLiteral("room_full"));
        return;
    }
    session.piece = initialPieceForGame(room->gameId, false);
    session.name = normalizedName(name);
    session.roomId = room->roomId;
    session.isHost = false;
    session.seatKind = activePlayersInRoom(room->roomId).size() < maxPlayers(room->gameId)
        ? LanBoard::SeatKind::Active
        : LanBoard::SeatKind::Spectator;
    session.peer = peer;
    m_players.append(session);
    peer->data = reinterpret_cast<void *>(1);

    normalizeSeats(room);
    broadcastRoomState(room);
}

void ServerApp::handleReady(ENetPeer *peer, bool ready)
{
    PlayerSession *session = sessionForPeer(peer);
    if (!session) {
        sendError(peer, QStringLiteral("not_joined"));
        return;
    }

    RoomState *room = roomForPlayer(session);
    if (!room) {
        sendError(peer, QStringLiteral("room_not_found"));
        return;
    }

    if (!LanBoard::isActiveSeat(session->seatKind)) {
        sendError(peer, QStringLiteral("spectator_cannot_ready"));
        return;
    }

    if (room->gameActive && isGameFinished(room))
        resetFinishedRoom(room);

    session->isReady = ready;
    broadcastRoomState(room);
}

void ServerApp::handleStartGame(ENetPeer *peer)
{
    PlayerSession *session = sessionForPeer(peer);
    if (!session) {
        sendError(peer, QStringLiteral("not_joined"));
        return;
    }

    RoomState *room = roomForPlayer(session);
    if (!room) {
        sendError(peer, QStringLiteral("room_not_found"));
        return;
    }

    if (!session->isHost) {
        sendError(peer, QStringLiteral("only_player_one_can_start"));
        return;
    }

    const auto players = activePlayersInRoom(room->roomId);
    const bool survivorRoom = isSurvivorRoom(room->gameId);
    if ((!survivorRoom && players.size() != maxPlayers(room->gameId))
        || (survivorRoom && players.isEmpty())) {
        sendError(peer, missingPlayersError(room));
        return;
    }

    for (const PlayerSession *player : players) {
        if (!player->isReady) {
            sendError(peer, QStringLiteral("players_not_ready"));
            return;
        }
    }

    resetGame(room);
    room->gameActive = true;

    QJsonObject msg;
    msg[QStringLiteral("type")] = LanBoard::Protocol::GameStart;
    msg[QStringLiteral("gameId")] = room->gameId;
    msg[QStringLiteral("firstPlayer")] = firstPlayerForRoom(room);
    broadcastJsonToRoom(room->roomId, msg);

    if (isSurvivorRoom(room->gameId)) {
        QVariantList activePlayers;
        for (const PlayerSession *player : players) {
            QVariantMap entry;
            entry[QStringLiteral("playerId")] = player->playerId;
            entry[QStringLiteral("name")] = player->name;
            activePlayers.append(entry);
        }
        const int anchorPlayerId = players.isEmpty() ? 0 : players.first()->playerId;
        room->survivorController->configureNetworkSession(activePlayers,
                                                          anchorPlayerId,
                                                          true,
                                                          true);
        room->survivorController->startRun(true);
    } else if (isDouDiZhuRoom(room->gameId)) {
        broadcastDouDiZhuStates(room);
    }
}

void ServerApp::handleChangeSeat(ENetPeer *peer, const QString &seatType)
{
    PlayerSession *session = sessionForPeer(peer);
    if (!session) {
        sendError(peer, QStringLiteral("not_joined"));
        return;
    }

    RoomState *room = roomForPlayer(session);
    if (!room) {
        sendError(peer, QStringLiteral("room_not_found"));
        return;
    }

    if (session->isHost) {
        sendError(peer, QStringLiteral("host_locked_active"));
        return;
    }

    if (room->gameActive) {
        sendError(peer, QStringLiteral("game_in_progress"));
        return;
    }

    const LanBoard::SeatKind normalizedSeatKind = LanBoard::normalizedSeatKind(seatType);
    if (normalizedSeatKind == LanBoard::SeatKind::Active
        && !LanBoard::isActiveSeat(session->seatKind)
        && activePlayersInRoom(room->roomId).size() >= maxPlayers(room->gameId)) {
        sendError(peer, QStringLiteral("active_seat_full"));
        return;
    }

    if (session->seatKind == normalizedSeatKind) {
        broadcastRoomState(room);
        return;
    }

    session->seatKind = normalizedSeatKind;
    session->isReady = false;
    normalizeSeats(room, false);
    broadcastRoomState(room);
}

void ServerApp::handleSwitchRoomGame(ENetPeer *peer, const QString &gameId)
{
    PlayerSession *session = sessionForPeer(peer);
    if (!session) {
        sendError(peer, QStringLiteral("not_joined"));
        return;
    }

    RoomState *room = roomForPlayer(session);
    if (!room) {
        sendError(peer, QStringLiteral("room_not_found"));
        return;
    }

    if (!session->isHost) {
        sendError(peer, QStringLiteral("only_host_can_switch_game"));
        return;
    }

    if (room->gameActive) {
        sendError(peer, QStringLiteral("game_in_progress"));
        return;
    }

    const QString normalized = LanBoard::normalizeGameId(gameId);
    if (room->gameId == normalized) {
        broadcastRoomState(room);
        return;
    }

    room->gameId = normalized;
    room->gameActive = false;
    resetGame(room);
    clearReadyStates(room);
    normalizeSeats(room);
    broadcastRoomState(room);
}

void ServerApp::handlePlacePiece(ENetPeer *peer, int row, int col)
{
    PlayerSession *session = sessionForPeer(peer);
    if (!session) {
        sendError(peer, QStringLiteral("not_joined"));
        return;
    }

    RoomState *room = roomForPlayer(session);
    if (!room) {
        sendError(peer, QStringLiteral("room_not_found"));
        return;
    }

    if (isDouDiZhuRoom(room->gameId) || isFlightChessRoom(room->gameId)) {
        sendError(peer, QStringLiteral("wrong_game"));
        return;
    }

    if (!LanBoard::isActiveSeat(session->seatKind)) {
        sendError(peer, QStringLiteral("spectator_cannot_move"));
        return;
    }

    if (activePlayersInRoom(room->roomId).size() != 2) {
        sendError(peer, QStringLiteral("need_two_players"));
        return;
    }

    if (!room->gameController->placePiece(row, col, session->piece)) {
        sendError(peer, QStringLiteral("invalid_move"));
        return;
    }

    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("move");
    msg[QStringLiteral("player")] = session->piece;
    msg[QStringLiteral("row")] = row;
    msg[QStringLiteral("col")] = col;
    broadcastJsonToRoom(room->roomId, msg);

    if (room->gameController->isGameOver())
        concludeRoomGame(room, room->gameController->winner());
}

void ServerApp::handleFlightRoll(ENetPeer *peer)
{
    PlayerSession *session = sessionForPeer(peer);
    if (!session) {
        sendError(peer, QStringLiteral("not_joined"));
        return;
    }

    RoomState *room = roomForPlayer(session);
    if (!room) {
        sendError(peer, QStringLiteral("room_not_found"));
        return;
    }

    if (!isFlightChessRoom(room->gameId)) {
        sendError(peer, QStringLiteral("wrong_game"));
        return;
    }

    if (!LanBoard::isActiveSeat(session->seatKind)) {
        sendError(peer, QStringLiteral("spectator_cannot_move"));
        return;
    }

    if (room->flightChessController->currentPlayer() != session->piece
        || room->flightChessController->hasRolled()) {
        sendError(peer, QStringLiteral("invalid_move"));
        return;
    }

    const int diceValue = room->flightChessController->rollDice();
    if (diceValue <= 0) {
        sendError(peer, QStringLiteral("invalid_move"));
        return;
    }

    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("flight_roll_result");
    msg[QStringLiteral("player")] = session->piece;
    msg[QStringLiteral("diceValue")] = diceValue;
    broadcastJsonToRoom(room->roomId, msg);
}

void ServerApp::handleFlightMove(ENetPeer *peer, int planeIndex)
{
    PlayerSession *session = sessionForPeer(peer);
    if (!session) {
        sendError(peer, QStringLiteral("not_joined"));
        return;
    }

    RoomState *room = roomForPlayer(session);
    if (!room) {
        sendError(peer, QStringLiteral("room_not_found"));
        return;
    }

    if (!isFlightChessRoom(room->gameId)) {
        sendError(peer, QStringLiteral("wrong_game"));
        return;
    }

    if (!LanBoard::isActiveSeat(session->seatKind)) {
        sendError(peer, QStringLiteral("spectator_cannot_move"));
        return;
    }

    if (room->flightChessController->currentPlayer() != session->piece
        || !room->flightChessController->movePlane(planeIndex)) {
        sendError(peer, QStringLiteral("invalid_move"));
        return;
    }

    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("flight_move_result");
    msg[QStringLiteral("player")] = session->piece;
    msg[QStringLiteral("planeIndex")] = planeIndex;
    broadcastJsonToRoom(room->roomId, msg);

    if (room->flightChessController->isGameOver())
        concludeRoomGame(room, room->flightChessController->winner());
}

void ServerApp::handleSurrender(ENetPeer *peer)
{
    PlayerSession *session = sessionForPeer(peer);
    if (!session) {
        sendError(peer, QStringLiteral("not_joined"));
        return;
    }

    RoomState *room = roomForPlayer(session);
    if (!room) {
        sendError(peer, QStringLiteral("room_not_found"));
        return;
    }

    if (isDouDiZhuRoom(room->gameId)) {
        sendError(peer, QStringLiteral("wrong_game"));
        return;
    }

    if (!LanBoard::isActiveSeat(session->seatKind)) {
        sendError(peer, QStringLiteral("spectator_cannot_surrender"));
        return;
    }

    if (isFlightChessRoom(room->gameId)) {
        room->flightChessController->setGameOver(otherPiece(session->piece));
        concludeRoomGame(room, room->flightChessController->winner());
    } else {
        if (!room->gameController->surrender(session->piece)) {
            sendError(peer, QStringLiteral("invalid_surrender"));
            return;
        }
        concludeRoomGame(room, room->gameController->winner());
    }
}

void ServerApp::handleSurvivorInput(ENetPeer *peer, qreal horizontal, qreal vertical)
{
    PlayerSession *session = sessionForPeer(peer);
    if (!session) {
        sendError(peer, QStringLiteral("not_joined"));
        return;
    }

    RoomState *room = roomForPlayer(session);
    if (!room) {
        sendError(peer, QStringLiteral("room_not_found"));
        return;
    }

    if (!isSurvivorRoom(room->gameId)) {
        sendError(peer, QStringLiteral("wrong_game"));
        return;
    }

    if (!room->gameActive) {
        sendError(peer, QStringLiteral("game_not_started"));
        return;
    }

    if (!LanBoard::isActiveSeat(session->seatKind)) {
        sendError(peer, QStringLiteral("spectator_cannot_move"));
        return;
    }

    if (room->survivorController)
        room->survivorController->setRemoteMoveInput(session->playerId, horizontal, vertical);
}

void ServerApp::handleSurvivorChooseLevelUp(ENetPeer *peer, const QString &upgradeId)
{
    PlayerSession *session = sessionForPeer(peer);
    if (!session) {
        sendError(peer, QStringLiteral("not_joined"));
        return;
    }

    RoomState *room = roomForPlayer(session);
    if (!room) {
        sendError(peer, QStringLiteral("room_not_found"));
        return;
    }

    if (!isSurvivorRoom(room->gameId)) {
        sendError(peer, QStringLiteral("wrong_game"));
        return;
    }

    if (!room->gameActive) {
        sendError(peer, QStringLiteral("game_not_started"));
        return;
    }

    if (!LanBoard::isActiveSeat(session->seatKind)) {
        sendError(peer, QStringLiteral("spectator_cannot_move"));
        return;
    }

    if (!room->survivorController)
        return;
    if (room->survivorController->interactionPlayerId() != session->playerId) {
        sendError(peer, QStringLiteral("not_your_upgrade"));
        return;
    }
    room->survivorController->chooseLevelUp(upgradeId);
}

void ServerApp::handleSurvivorCloseChest(ENetPeer *peer)
{
    PlayerSession *session = sessionForPeer(peer);
    if (!session) {
        sendError(peer, QStringLiteral("not_joined"));
        return;
    }

    RoomState *room = roomForPlayer(session);
    if (!room) {
        sendError(peer, QStringLiteral("room_not_found"));
        return;
    }

    if (!isSurvivorRoom(room->gameId)) {
        sendError(peer, QStringLiteral("wrong_game"));
        return;
    }

    if (!room->gameActive) {
        sendError(peer, QStringLiteral("game_not_started"));
        return;
    }

    if (!LanBoard::isActiveSeat(session->seatKind)) {
        sendError(peer, QStringLiteral("spectator_cannot_move"));
        return;
    }

    if (!room->survivorController)
        return;
    if (room->survivorController->interactionPlayerId() != session->playerId) {
        sendError(peer, QStringLiteral("not_your_upgrade"));
        return;
    }
    room->survivorController->closeChestRewards();
}

void ServerApp::handleGameOver(ENetPeer *peer, int winner)
{
    Q_UNUSED(winner);
    sendError(peer, QStringLiteral("survivor_server_authoritative"));
}

void ServerApp::handleDouDiZhuPlay(ENetPeer *peer, const QJsonArray &cardIds)
{
    PlayerSession *session = sessionForPeer(peer);
    if (!session) {
        sendError(peer, QStringLiteral("not_joined"));
        return;
    }

    RoomState *room = roomForPlayer(session);
    if (!room) {
        sendError(peer, QStringLiteral("room_not_found"));
        return;
    }

    if (!isDouDiZhuRoom(room->gameId)) {
        sendError(peer, QStringLiteral("wrong_game"));
        return;
    }

    if (!LanBoard::isActiveSeat(session->seatKind)) {
        sendError(peer, QStringLiteral("spectator_cannot_play"));
        return;
    }

    QVariantList ids;
    for (const auto &id : cardIds)
        ids.append(id.toInt());

    if (!room->douDiZhuController->playCardsForPlayer(session->playerId, ids)) {
        sendError(peer, QStringLiteral("invalid_ddz_play"));
        broadcastDouDiZhuStates(room);
        return;
    }

    broadcastDouDiZhuStates(room);
}

void ServerApp::handleDouDiZhuPass(ENetPeer *peer)
{
    PlayerSession *session = sessionForPeer(peer);
    if (!session) {
        sendError(peer, QStringLiteral("not_joined"));
        return;
    }

    RoomState *room = roomForPlayer(session);
    if (!room) {
        sendError(peer, QStringLiteral("room_not_found"));
        return;
    }

    if (!isDouDiZhuRoom(room->gameId)) {
        sendError(peer, QStringLiteral("wrong_game"));
        return;
    }

    if (!LanBoard::isActiveSeat(session->seatKind)) {
        sendError(peer, QStringLiteral("spectator_cannot_pass"));
        return;
    }

    if (!room->douDiZhuController->passForPlayer(session->playerId)) {
        sendError(peer, QStringLiteral("invalid_ddz_pass"));
        broadcastDouDiZhuStates(room);
        return;
    }

    broadcastDouDiZhuStates(room);
}

void ServerApp::sendJson(ENetPeer *peer, const QJsonObject &obj)
{
    if (!LanBoard::Enet::sendJson(peer, obj) || !m_host)
        return;
    enet_host_flush(m_host);
}

void ServerApp::sendRaw(ENetPeer *peer, const QByteArray &payload, enet_uint8 channel, enet_uint32 flags)
{
    if (!LanBoard::Enet::sendRaw(peer, channel, payload, flags) || !m_host)
        return;
    enet_host_flush(m_host);
}

void ServerApp::sendError(ENetPeer *peer, const QString &message)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = LanBoard::Protocol::Error;
    msg[QStringLiteral("message")] = message;
    sendJson(peer, msg);
}

bool ServerApp::processBinaryPacket(ENetPeer *peer, const QByteArray &payload)
{
    qreal horizontal = 0.0;
    qreal vertical = 0.0;
    QString upgradeId;

    switch (LanBoard::Survivor::NetCodec::packetKind(payload)) {
    case LanBoard::Survivor::NetCodec::PacketKind::Input:
        if (!LanBoard::Survivor::NetCodec::decodeInputPacket(payload, horizontal, vertical))
            return false;
        handleSurvivorInput(peer, horizontal, vertical);
        return true;
    case LanBoard::Survivor::NetCodec::PacketKind::ChooseLevelUp:
        if (!LanBoard::Survivor::NetCodec::decodeChooseLevelUpPacket(payload, upgradeId))
            return false;
        handleSurvivorChooseLevelUp(peer, upgradeId);
        return true;
    case LanBoard::Survivor::NetCodec::PacketKind::CloseChest:
        if (!LanBoard::Survivor::NetCodec::decodeCloseChestPacket(payload))
            return false;
        handleSurvivorCloseChest(peer);
        return true;
    case LanBoard::Survivor::NetCodec::PacketKind::FastState:
    case LanBoard::Survivor::NetCodec::PacketKind::HudState:
        sendError(peer, QStringLiteral("survivor_server_authoritative"));
        return true;
    default:
        return false;
    }
}

void ServerApp::broadcastJsonToRoom(const QString &roomId, const QJsonObject &obj, ENetPeer *exclude)
{
    for (const auto &player : std::as_const(m_players)) {
        if (player.roomId != roomId || !player.peer || player.peer == exclude)
            continue;
        LanBoard::Enet::sendJson(player.peer, obj);
    }

    if (m_host)
        enet_host_flush(m_host);
}

void ServerApp::broadcastRoomState(RoomState *room)
{
    if (!room)
        return;

    const auto players = playersInRoom(room->roomId);
    QJsonArray playerArray;
    for (const PlayerSession *player : players) {
        QJsonObject entry;
        entry[QStringLiteral("playerId")] = player->playerId;
        entry[QStringLiteral("name")] = player->name;
        entry[QStringLiteral("isHost")] = player->isHost;
        entry[QStringLiteral("isReady")] = player->isReady;
        entry[QStringLiteral("seatType")] = LanBoard::seatTypeString(player->seatKind);
        entry[QStringLiteral("piece")] = player->piece;
        playerArray.append(entry);
    }

    for (const PlayerSession *player : players) {
        if (!player->peer)
            continue;

        QJsonObject msg;
        msg[QStringLiteral("type")] = LanBoard::Protocol::RoomState;
        msg[QStringLiteral("roomId")] = room->roomId;
        msg[QStringLiteral("roomName")] = room->roomName;
        msg[QStringLiteral("gameId")] = room->gameId;
        msg[QStringLiteral("gameName")] = LanBoard::gameName(room->gameId);
        msg[QStringLiteral("maxPlayers")] = maxPlayers(room->gameId);
        msg[QStringLiteral("roomCapacity")] = roomCapacity();
        msg[QStringLiteral("players")] = playerArray;
        msg[QStringLiteral("yourPlayerId")] = player->playerId;
        msg[QStringLiteral("yourPiece")] = player->piece;
        msg[QStringLiteral("mode")] = QStringLiteral("dedicated_server");
        sendJson(player->peer, msg);
    }
}

void ServerApp::broadcastDouDiZhuStates(RoomState *room)
{
    if (!room || !isDouDiZhuRoom(room->gameId))
        return;

    for (const PlayerSession *player : activePlayersInRoom(room->roomId)) {
        if (!player->peer)
            continue;

        QJsonObject msg = room->douDiZhuController->stateForPlayer(player->playerId);
        msg[QStringLiteral("type")] = LanBoard::Protocol::DouDiZhuState;
        sendJson(player->peer, msg);
    }

    if (room->douDiZhuController->isGameOver())
        concludeRoomGame(room, room->douDiZhuController->winner());
}

void ServerApp::clearReadyStates(RoomState *room)
{
    if (!room)
        return;

    for (PlayerSession *player : playersInRoom(room->roomId))
        player->isReady = false;
}

void ServerApp::resetGame(RoomState *room)
{
    if (!room)
        return;

    if (isDouDiZhuRoom(room->gameId)) {
        room->douDiZhuController->startNetworkGame(0);
    } else if (isSurvivorRoom(room->gameId)) {
        if (room->survivorController)
            room->survivorController->stopRun();
    } else if (isFlightChessRoom(room->gameId)) {
        room->flightChessController->startNewGame();
    } else {
        room->gameController->reset();
    }
}

bool ServerApp::isGameFinished(const RoomState *room) const
{
    if (!room)
        return false;

    if (isDouDiZhuRoom(room->gameId))
        return room->douDiZhuController->isGameOver();
    if (isSurvivorRoom(room->gameId))
        return room->survivorController && room->survivorController->isGameOver();
    if (isFlightChessRoom(room->gameId))
        return room->flightChessController->isGameOver();
    return room->gameController->isGameOver();
}

int ServerApp::firstPlayerForRoom(const RoomState *room) const
{
    return room && isDouDiZhuRoom(room->gameId) ? 0 : 1;
}

QString ServerApp::missingPlayersError(const RoomState *room) const
{
    if (room && isSurvivorRoom(room->gameId))
        return QStringLiteral("need_active_players");
    return room && isDouDiZhuRoom(room->gameId)
        ? QStringLiteral("need_three_players")
        : QStringLiteral("need_two_players");
}

int ServerApp::initialPieceForGame(const QString &gameId, bool isHost) const
{
    if (isDouDiZhuRoom(gameId))
        return 0;
    return isHost ? 1 : 2;
}

void ServerApp::concludeRoomGame(RoomState *room, int winner, bool broadcastRoomStateAfterward)
{
    if (!room)
        return;

    QJsonObject msg;
    msg[QStringLiteral("type")] = LanBoard::Protocol::GameOver;
    msg[QStringLiteral("winner")] = winner;
    broadcastJsonToRoom(room->roomId, msg);

    room->gameActive = false;
    if (isSurvivorRoom(room->gameId) && room->survivorController)
        room->survivorController->stopRun();
    clearReadyStates(room);
    if (broadcastRoomStateAfterward)
        broadcastRoomState(room);
}

void ServerApp::resetFinishedRoom(RoomState *room)
{
    if (!room)
        return;

    room->gameActive = false;
    resetGame(room);
    clearReadyStates(room);
}

void ServerApp::removeRoomIfEmpty(const QString &roomId)
{
    for (const auto &player : std::as_const(m_players)) {
        if (player.roomId == roomId)
            return;
    }

    for (int i = 0; i < m_rooms.size(); ++i) {
        if (m_rooms.at(i)->roomId == roomId) {
            delete m_rooms.at(i);
            m_rooms.removeAt(i);
            return;
        }
    }
}

QJsonArray ServerApp::roomListPayload() const
{
    QJsonArray rooms;
    for (const RoomState *room : std::as_const(m_rooms)) {
        const int playerCount = playersInRoom(room->roomId).size();
        QJsonObject entry;
        entry[QStringLiteral("roomId")] = room->roomId;
        entry[QStringLiteral("roomName")] = room->roomName;
        entry[QStringLiteral("hostName")] = hostNameForRoom(room->roomId);
        entry[QStringLiteral("gameId")] = room->gameId;
        entry[QStringLiteral("gameName")] = LanBoard::gameName(room->gameId);
        entry[QStringLiteral("playerCount")] = playerCount;
        entry[QStringLiteral("roomCapacity")] = roomCapacity();
        entry[QStringLiteral("maxPlayers")] = maxPlayers(room->gameId);
        entry[QStringLiteral("inGame")] = room->gameActive;
        entry[QStringLiteral("isFull")] = playerCount >= roomCapacity();
        rooms.append(entry);
    }
    return rooms;
}

ServerApp::PlayerSession *ServerApp::sessionForPeer(ENetPeer *peer)
{
    for (auto &player : m_players) {
        if (player.peer == peer)
            return &player;
    }
    return nullptr;
}

const ServerApp::PlayerSession *ServerApp::sessionForPeer(ENetPeer *peer) const
{
    for (const auto &player : m_players) {
        if (player.peer == peer)
            return &player;
    }
    return nullptr;
}

ServerApp::RoomState *ServerApp::roomForPlayer(const PlayerSession *session)
{
    if (!session)
        return nullptr;
    return roomById(session->roomId);
}

const ServerApp::RoomState *ServerApp::roomForPlayer(const PlayerSession *session) const
{
    if (!session)
        return nullptr;
    return roomById(session->roomId);
}

ServerApp::RoomState *ServerApp::roomById(const QString &roomId)
{
    for (RoomState *room : std::as_const(m_rooms)) {
        if (room->roomId == roomId)
            return room;
    }
    return nullptr;
}

const ServerApp::RoomState *ServerApp::roomById(const QString &roomId) const
{
    for (const RoomState *room : std::as_const(m_rooms)) {
        if (room->roomId == roomId)
            return room;
    }
    return nullptr;
}

QList<ServerApp::PlayerSession *> ServerApp::playersInRoom(const QString &roomId)
{
    QList<PlayerSession *> roomPlayers;
    for (auto &player : m_players) {
        if (player.roomId == roomId)
            roomPlayers.append(&player);
    }
    return roomPlayers;
}

QList<const ServerApp::PlayerSession *> ServerApp::playersInRoom(const QString &roomId) const
{
    QList<const PlayerSession *> roomPlayers;
    for (const auto &player : m_players) {
        if (player.roomId == roomId)
            roomPlayers.append(&player);
    }
    return roomPlayers;
}

QList<ServerApp::PlayerSession *> ServerApp::activePlayersInRoom(const QString &roomId)
{
    QList<PlayerSession *> roomPlayers;
    for (auto &player : m_players) {
        if (player.roomId == roomId && LanBoard::isActiveSeat(player.seatKind))
            roomPlayers.append(&player);
    }
    return roomPlayers;
}

QList<const ServerApp::PlayerSession *> ServerApp::activePlayersInRoom(const QString &roomId) const
{
    QList<const PlayerSession *> roomPlayers;
    for (const auto &player : m_players) {
        if (player.roomId == roomId && LanBoard::isActiveSeat(player.seatKind))
            roomPlayers.append(&player);
    }
    return roomPlayers;
}

QString ServerApp::hostNameForRoom(const QString &roomId) const
{
    for (const auto &player : m_players) {
        if (player.roomId == roomId && player.isHost)
            return player.name;
    }
    return QStringLiteral("host");
}

int ServerApp::nextPlayerIdForRoom(const QString &roomId, const QString &gameId) const
{
    Q_UNUSED(gameId);
    for (int candidate = 0; candidate < roomCapacity(); ++candidate) {
        bool used = false;
        for (const auto &player : m_players) {
            if (player.roomId == roomId && player.playerId == candidate) {
                used = true;
                break;
            }
        }
        if (!used)
            return candidate;
    }
    return -1;
}

QString ServerApp::createRoomId() const
{
    while (true) {
        const QString roomId = QString::number(QRandomGenerator::global()->bounded(100000, 999999));
        if (!roomById(roomId))
            return roomId;
    }
}

int ServerApp::otherPiece(int piece) const
{
    return piece == 1 ? 2 : 1;
}

int ServerApp::roomCapacity() const
{
    return 8;
}

int ServerApp::activeGuestLimit(const QString &gameId) const
{
    return qMax(0, maxPlayers(gameId) - 1);
}

void ServerApp::normalizeSeats(RoomState *room, bool fillMissingActiveSeats)
{
    if (!room)
        return;

    auto players = playersInRoom(room->roomId);
    int activeGuests = 0;
    for (PlayerSession *player : players) {
        if (player->isHost) {
            player->seatKind = LanBoard::SeatKind::Active;
            player->piece = initialPieceForGame(room->gameId, true);
            continue;
        }
        if (LanBoard::isActiveSeat(player->seatKind))
            ++activeGuests;
    }

    for (int i = players.size() - 1; i >= 0 && activeGuests > activeGuestLimit(room->gameId); --i) {
        PlayerSession *player = players.at(i);
        if (player->isHost || !LanBoard::isActiveSeat(player->seatKind))
            continue;
        player->seatKind = LanBoard::SeatKind::Spectator;
        player->isReady = false;
        --activeGuests;
    }

    if (fillMissingActiveSeats) {
        for (PlayerSession *player : players) {
            if (activeGuests >= activeGuestLimit(room->gameId))
                break;
            if (player->isHost || player->seatKind != LanBoard::SeatKind::Spectator)
                continue;
            player->seatKind = LanBoard::SeatKind::Active;
            ++activeGuests;
        }
    }

    bool whiteAssigned = false;
    for (PlayerSession *player : players) {
        if (player->isHost) {
            player->piece = initialPieceForGame(room->gameId, true);
            continue;
        }

        if (isDouDiZhuRoom(room->gameId)) {
            player->piece = 0;
        } else if (LanBoard::isActiveSeat(player->seatKind) && !whiteAssigned) {
            player->piece = 2;
            whiteAssigned = true;
        } else {
            player->piece = 0;
        }
    }
}

int ServerApp::maxPlayers(const QString &gameId) const
{
    return LanBoard::maxPlayersForGame(gameId);
}

bool ServerApp::isDouDiZhuRoom(const QString &gameId) const
{
    return LanBoard::isDouDiZhuGame(gameId);
}

bool ServerApp::isFlightChessRoom(const QString &gameId) const
{
    return LanBoard::isFlightChessGame(gameId);
}

bool ServerApp::isSurvivorRoom(const QString &gameId) const
{
    return LanBoard::normalizeGameId(gameId) == QStringLiteral("survivor");
}
