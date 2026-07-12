#include "serverapp.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>

#include "../game/survivornetcodec.h"
#include "../network/enetutils.h"
#include "../network/protocolids.h"

namespace {

constexpr quint16 kDefaultPort = 44567;
constexpr int kServiceIntervalMs = 4;
constexpr enet_uint8 kSurvivorFrameChannel = 2;
constexpr enet_uint8 kSurvivorHudChannel = 3;
constexpr enet_uint8 kDormDefensePositionChannel = 1;

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

    const PlayerSession disconnectedSession = *session;
    const QString roomId = session->roomId;
    RoomState *room = roomForPlayer(session);
    const QString playerName = room && room->roomManager
        ? room->roomManager->playerName(session->playerId)
        : QStringLiteral("player");
    const bool wasHost = room && room->roomManager
        && room->roomManager->isPlayerHost(session->playerId);

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
            if (m_rooms.at(i)->roomManager
                && m_rooms.at(i)->roomManager->roomId() == roomId) {
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
            handlePlayerDisconnectInRoom(room, disconnectedSession);
            room->roomManager->removePlayerById(disconnectedSession.playerId);
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
    } else if (type == LanBoard::Protocol::DormDefenseRole) {
        handleDormDefenseRole(peer, msg.value(QStringLiteral("role")).toString());
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
    } else if (type == LanBoard::Protocol::DormDefenseAction) {
        handleDormDefenseAction(peer, msg);
    } else {
        sendError(peer, QStringLiteral("unsupported_message_type"));
    }
}

void ServerApp::handleJoin(ENetPeer *peer, const QString &name, const QString &gameId)
{
    const QString normalized = LanBoard::normalizeGameId(gameId);
    RoomState *targetRoom = nullptr;
    for (RoomState *room : std::as_const(m_rooms)) {
        if (room->roomManager && room->roomManager->gameId() == normalized) {
            targetRoom = room;
            break;
        }
    }

    if (!targetRoom) {
        handleCreateRoom(peer, name, QString(), normalized);
        return;
    }

    handleJoinRoom(peer, name, targetRoom->roomManager->roomId());
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
    room->roomManager = std::make_unique<RoomManager>();
    room->gameController = std::make_unique<GameController>();
    room->douDiZhuController = std::make_unique<DouDiZhuController>();
    room->dormDefenseController = std::make_unique<DormDefenseController>();
    room->flightChessController = std::make_unique<FlightChessController>();
    room->survivorController = std::make_unique<SurvivorController>();
    room->roomManager->setRoomIdentity(createRoomId(),
                                       normalizedRoomName(roomName,
                                                          normalizedName(name),
                                                          LanBoard::normalizeGameId(gameId)));
    room->roomManager->setGameId(gameId);
    room->roomManager->setLocalPlayerId(0);
    connect(room->survivorController.get(), &SurvivorController::networkSyncRequested,
            this, [this, room](bool includeHudDetails) {
        if (!room || !room->roomManager || !room->roomManager->gameInProgress()
            || LanBoard::controllerKindForGame(room->roomManager->gameId())
                != LanBoard::GameControllerKind::Survivor) {
            return;
        }
        for (const PlayerSession *player : activePlayersInRoom(room->roomManager->roomId())) {
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
        if (!room || !room->roomManager || !room->roomManager->gameInProgress() || !room->survivorController
            || !room->survivorController->isGameOver()) {
            return;
        }
        concludeRoomGame(room, 0);
    });
    auto syncDormDefenseState = [this, room]() {
        if (!room || !room->roomManager || !room->dormDefenseController
            || !room->roomManager->gameInProgress()
            || LanBoard::controllerKindForGame(room->roomManager->gameId())
                != LanBoard::GameControllerKind::DormDefense) {
            return;
        }
        broadcastDormDefenseStates(room);
    };
    connect(room->dormDefenseController.get(), &DormDefenseController::boardChanged,
            this, syncDormDefenseState);
    connect(room->dormDefenseController.get(), &DormDefenseController::resourcesChanged,
            this, syncDormDefenseState);
    connect(room->dormDefenseController.get(), &DormDefenseController::statusChanged,
            this, syncDormDefenseState);
    connect(room->dormDefenseController.get(), &DormDefenseController::ghostStateChanged,
            this, [this, room]() { broadcastDormDefenseGhostPosition(room); });
    connect(room->dormDefenseController.get(), &DormDefenseController::turretVolleyChanged,
            this, syncDormDefenseState);
    connect(room->dormDefenseController.get(), &DormDefenseController::roleChanged,
            this, syncDormDefenseState);
    connect(room->dormDefenseController.get(), &DormDefenseController::gameOverChanged,
            this, [this, room]() {
        if (!room || !room->roomManager || !room->dormDefenseController
            || !room->roomManager->gameInProgress()
            || !room->dormDefenseController->isGameOver()) {
            return;
        }
        concludeRoomGame(room, room->dormDefenseController->winner());
    });
    m_rooms.append(room);

    PlayerSession session;
    session.playerId = 0;
    session.roomId = room->roomManager->roomId();
    session.peer = peer;
    room->roomManager->tryAddRoomPlayer(normalizedName(name), true, session.playerId);
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

    if (room->roomManager->gameInProgress()) {
        sendError(peer, QStringLiteral("room_in_game"));
        return;
    }

    const int playerId = room->roomManager->allocatePlayerId();
    if (playerId < 0) {
        sendError(peer, QStringLiteral("room_full"));
        return;
    }

    PlayerSession session;
    session.playerId = playerId;
    session.roomId = room->roomManager->roomId();
    session.peer = peer;
    const RoomManager::ActionError addError = room->roomManager->tryAddRoomPlayer(
        normalizedName(name), false, session.playerId);
    if (addError != RoomManager::ActionError::None) {
        sendError(peer, room->roomManager->actionErrorKey(addError));
        return;
    }
    m_players.append(session);
    peer->data = reinterpret_cast<void *>(1);

    broadcastRoomState(room);
}

void ServerApp::handleReady(ENetPeer *peer, bool ready)
{
    PlayerSession *session = nullptr;
    RoomState *room = nullptr;
    if (!resolveSessionRoom(peer, session, room))
        return;

    if (!room->roomManager->isPlayerActive(session->playerId)) {
        sendError(peer, QStringLiteral("spectator_cannot_ready"));
        return;
    }

    if (room->roomManager->gameInProgress() && isGameFinished(room))
        resetFinishedRoom(room);

    if (room->roomManager->setPlayerReadyById(session->playerId, ready))
        broadcastRoomState(room);
}

void ServerApp::handleStartGame(ENetPeer *peer)
{
    PlayerSession *session = nullptr;
    RoomState *room = nullptr;
    if (!resolveSessionRoom(peer, session, room))
        return;

    const RoomManager::ActionError startError = room->roomManager->tryStartGame(session->playerId);
    if (startError != RoomManager::ActionError::None) {
        sendError(peer, room->roomManager->actionErrorKey(startError));
        return;
    }

    resetGame(room);
    room->roomManager->setGameInProgress(true);

    QJsonObject msg;
    msg[QStringLiteral("type")] = LanBoard::Protocol::GameStart;
    msg[QStringLiteral("gameId")] = room->roomManager->gameId();
    msg[QStringLiteral("firstPlayer")] = LanBoard::firstPlayerForGame(room->roomManager->gameId());
    broadcastJsonToRoom(room->roomManager->roomId(), msg);
    const auto players = activePlayersInRoom(room->roomManager->roomId());
    startRoomGame(room, players);
}

void ServerApp::handleChangeSeat(ENetPeer *peer, const QString &seatType)
{
    PlayerSession *session = nullptr;
    RoomState *room = nullptr;
    if (!resolveSessionRoom(peer, session, room))
        return;

    const RoomManager::ActionError seatError = room->roomManager->tryChangeSeat(session->playerId,
                                                                                 seatType);
    if (seatError != RoomManager::ActionError::None) {
        sendError(peer, room->roomManager->actionErrorKey(seatError));
        return;
    }
    broadcastRoomState(room);
}

void ServerApp::handleDormDefenseRole(ENetPeer *peer, const QString &role)
{
    PlayerSession *session = nullptr;
    RoomState *room = nullptr;
    if (!resolveSessionRoom(peer, session, room)
        || !ensureControllerKind(peer, room, LanBoard::GameControllerKind::DormDefense)) {
        return;
    }

    const RoomManager::ActionError roleError = room->roomManager->tryChangeDormDefenseRole(
        session->playerId, role);
    if (roleError != RoomManager::ActionError::None) {
        sendError(peer, room->roomManager->actionErrorKey(roleError));
        return;
    }

    broadcastRoomState(room);
}

void ServerApp::handleSwitchRoomGame(ENetPeer *peer, const QString &gameId)
{
    PlayerSession *session = nullptr;
    RoomState *room = nullptr;
    if (!resolveSessionRoom(peer, session, room))
        return;

    const RoomManager::ActionError switchError = room->roomManager->trySwitchGame(session->playerId,
                                                                                   gameId);
    if (switchError != RoomManager::ActionError::None) {
        sendError(peer, room->roomManager->actionErrorKey(switchError));
        return;
    }
    resetGame(room);
    broadcastRoomState(room);
}

void ServerApp::handlePlacePiece(ENetPeer *peer, int row, int col)
{
    PlayerSession *session = nullptr;
    RoomState *room = nullptr;
    if (!resolveSessionRoom(peer, session, room)
        || !ensureControllerKind(peer, room, LanBoard::GameControllerKind::Gomoku)
        || !ensureActiveSeat(peer, session)) {
        return;
    }

    if (activePlayersInRoom(room->roomManager->roomId()).size() != 2) {
        sendError(peer, QStringLiteral("need_two_players"));
        return;
    }

    const int piece = room->roomManager->playerPiece(session->playerId);
    if (!room->gameController->placePiece(row, col, piece)) {
        sendError(peer, QStringLiteral("invalid_move"));
        return;
    }

    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("move");
    msg[QStringLiteral("player")] = piece;
    msg[QStringLiteral("row")] = row;
    msg[QStringLiteral("col")] = col;
    broadcastJsonToRoom(room->roomManager->roomId(), msg);

    if (room->gameController->isGameOver())
        concludeRoomGame(room, room->gameController->winner());
}

void ServerApp::handleFlightRoll(ENetPeer *peer)
{
    PlayerSession *session = nullptr;
    RoomState *room = nullptr;
    if (!resolveSessionRoom(peer, session, room)
        || !ensureControllerKind(peer, room, LanBoard::GameControllerKind::FlightChess)
        || !ensureActiveSeat(peer, session)) {
        return;
    }

    const int piece = room->roomManager->playerPiece(session->playerId);
    if (room->flightChessController->currentPlayer() != piece
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
    msg[QStringLiteral("player")] = piece;
    msg[QStringLiteral("diceValue")] = diceValue;
    broadcastJsonToRoom(room->roomManager->roomId(), msg);
}

void ServerApp::handleFlightMove(ENetPeer *peer, int planeIndex)
{
    PlayerSession *session = nullptr;
    RoomState *room = nullptr;
    if (!resolveSessionRoom(peer, session, room)
        || !ensureControllerKind(peer, room, LanBoard::GameControllerKind::FlightChess)
        || !ensureActiveSeat(peer, session)) {
        return;
    }

    const int piece = room->roomManager->playerPiece(session->playerId);
    if (room->flightChessController->currentPlayer() != piece
        || !room->flightChessController->movePlane(planeIndex)) {
        sendError(peer, QStringLiteral("invalid_move"));
        return;
    }

    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("flight_move_result");
    msg[QStringLiteral("player")] = piece;
    msg[QStringLiteral("planeIndex")] = planeIndex;
    broadcastJsonToRoom(room->roomManager->roomId(), msg);

    if (room->flightChessController->isGameOver())
        concludeRoomGame(room, room->flightChessController->winner());
}

void ServerApp::handleSurrender(ENetPeer *peer)
{
    PlayerSession *session = nullptr;
    RoomState *room = nullptr;
    if (!resolveSessionRoom(peer, session, room))
        return;

    const LanBoard::GameControllerKind controllerKind = LanBoard::controllerKindForGame(
        room->roomManager->gameId());
    if (controllerKind != LanBoard::GameControllerKind::Gomoku
        && controllerKind != LanBoard::GameControllerKind::FlightChess) {
        sendError(peer, QStringLiteral("wrong_game"));
        return;
    }

    if (!ensureActiveSeat(peer, session, "spectator_cannot_surrender"))
        return;

    const int piece = room->roomManager->playerPiece(session->playerId);
    if (controllerKind == LanBoard::GameControllerKind::FlightChess) {
        room->flightChessController->setGameOver(otherPiece(piece));
        concludeRoomGame(room, room->flightChessController->winner());
    } else {
        if (!room->gameController->surrender(piece)) {
            sendError(peer, QStringLiteral("invalid_surrender"));
            return;
        }
        concludeRoomGame(room, room->gameController->winner());
    }
}

void ServerApp::handleSurvivorInput(ENetPeer *peer, qreal horizontal, qreal vertical)
{
    PlayerSession *session = nullptr;
    RoomState *room = nullptr;
    if (!resolveSessionRoom(peer, session, room)
        || !ensureControllerKind(peer, room, LanBoard::GameControllerKind::Survivor)
        || !ensureGameStarted(peer, room)
        || !ensureActiveSeat(peer, session)) {
        return;
    }

    if (room->survivorController)
        room->survivorController->setRemoteMoveInput(session->playerId, horizontal, vertical);
}

void ServerApp::handleSurvivorChooseLevelUp(ENetPeer *peer, const QString &upgradeId)
{
    PlayerSession *session = nullptr;
    RoomState *room = nullptr;
    if (!resolveSessionRoom(peer, session, room)
        || !ensureControllerKind(peer, room, LanBoard::GameControllerKind::Survivor)
        || !ensureGameStarted(peer, room)
        || !ensureActiveSeat(peer, session)
        || !ensureSurvivorInteractionOwner(peer, room, session->playerId)) {
        return;
    }

    if (!room->survivorController->chooseLevelUp(upgradeId))
        room->survivorController->forceNetworkResync(true);
}

void ServerApp::handleSurvivorCloseChest(ENetPeer *peer)
{
    PlayerSession *session = nullptr;
    RoomState *room = nullptr;
    if (!resolveSessionRoom(peer, session, room)
        || !ensureControllerKind(peer, room, LanBoard::GameControllerKind::Survivor)
        || !ensureGameStarted(peer, room)
        || !ensureActiveSeat(peer, session)
        || !ensureSurvivorInteractionOwner(peer, room, session->playerId)) {
        return;
    }

    if (!room->survivorController->closeChestRewards())
        room->survivorController->forceNetworkResync(true);
}

void ServerApp::handleGameOver(ENetPeer *peer, int winner)
{
    PlayerSession *session = nullptr;
    RoomState *room = nullptr;
    if (!resolveSessionRoom(peer, session, room))
        return;

    if (!room || !room->roomManager || !room->roomManager->isPlayerHost(session->playerId)) {
        sendError(peer, QStringLiteral("only_host_can_end_game"));
        return;
    }

    if (!room->roomManager->gameInProgress()) {
        broadcastRoomState(room);
        return;
    }

    if (winner == 0) {
        room->roomManager->concludeGame();
        resetGame(room);
        broadcastRoomState(room);
        return;
    }

    if (LanBoard::controllerKindForGame(room->roomManager->gameId())
        == LanBoard::GameControllerKind::DormDefense) {
        concludeRoomGame(room, winner);
        return;
    }

    sendError(peer, QStringLiteral("survivor_server_authoritative"));
}

void ServerApp::handleDouDiZhuPlay(ENetPeer *peer, const QJsonArray &cardIds)
{
    PlayerSession *session = nullptr;
    RoomState *room = nullptr;
    if (!resolveSessionRoom(peer, session, room)
        || !ensureControllerKind(peer, room, LanBoard::GameControllerKind::DouDiZhu)
        || !ensureActiveSeat(peer, session, "spectator_cannot_play")) {
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
    PlayerSession *session = nullptr;
    RoomState *room = nullptr;
    if (!resolveSessionRoom(peer, session, room)
        || !ensureControllerKind(peer, room, LanBoard::GameControllerKind::DouDiZhu)
        || !ensureActiveSeat(peer, session, "spectator_cannot_pass")) {
        return;
    }

    if (!room->douDiZhuController->passForPlayer(session->playerId)) {
        sendError(peer, QStringLiteral("invalid_ddz_pass"));
        broadcastDouDiZhuStates(room);
        return;
    }

    broadcastDouDiZhuStates(room);
}

void ServerApp::handleDormDefenseAction(ENetPeer *peer, const QJsonObject &action)
{
    PlayerSession *session = nullptr;
    RoomState *room = nullptr;
    if (!resolveSessionRoom(peer, session, room)
        || !ensureControllerKind(peer, room, LanBoard::GameControllerKind::DormDefense)
        || !ensureGameStarted(peer, room)
        || !ensureActiveSeat(peer, session)) {
        return;
    }

    room->dormDefenseController->applyNetworkAction(session->playerId, action);
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

    for (const PlayerSession *player : playersInRoom(room->roomManager->roomId())) {
        if (!player->peer)
            continue;
        sendJson(player->peer,
                 room->roomManager->roomStateMessageForPlayer(player->playerId,
                                                              QStringLiteral("dedicated_server")));
    }
}

void ServerApp::broadcastDouDiZhuStates(RoomState *room)
{
    if (!room
        || LanBoard::controllerKindForGame(room->roomManager->gameId())
            != LanBoard::GameControllerKind::DouDiZhu) {
        return;
    }

    for (const PlayerSession *player : activePlayersInRoom(room->roomManager->roomId())) {
        if (!player->peer)
            continue;

        QJsonObject msg = room->douDiZhuController->stateForPlayer(player->playerId);
        msg[QStringLiteral("type")] = LanBoard::Protocol::DouDiZhuState;
        sendJson(player->peer, msg);
    }

    if (room->douDiZhuController->isGameOver())
        concludeRoomGame(room, room->douDiZhuController->winner());
}

void ServerApp::broadcastDormDefenseStates(RoomState *room)
{
    if (!room
        || LanBoard::controllerKindForGame(room->roomManager->gameId())
            != LanBoard::GameControllerKind::DormDefense
        || !room->dormDefenseController) {
        return;
    }

    for (const PlayerSession *player : playersInRoom(room->roomManager->roomId())) {
        if (!player || !player->peer)
            continue;
        sendJson(player->peer, dormDefenseStateForPlayer(room, player->playerId));
    }
}

void ServerApp::broadcastDormDefenseGhostPosition(RoomState *room)
{
    if (!room || !room->roomManager || !room->dormDefenseController)
        return;

    QJsonObject msg;
    msg[QStringLiteral("type")] = LanBoard::Protocol::DormDefenseGhostPosition;
    msg[QStringLiteral("row")] = room->dormDefenseController->ghostCenterRow();
    msg[QStringLiteral("column")] = room->dormDefenseController->ghostCenterColumn();
    const QByteArray payload = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    for (const PlayerSession *player : playersInRoom(room->roomManager->roomId())) {
        if (player && player->peer)
            sendRaw(player->peer, payload, kDormDefensePositionChannel, 0);
    }
}

void ServerApp::startRoomGame(RoomState *room, const QList<PlayerSession *> &activePlayers)
{
    if (!room)
        return;

    switch (LanBoard::controllerKindForGame(room->roomManager->gameId())) {
    case LanBoard::GameControllerKind::Survivor: {
        QVariantList survivorPlayers;
        survivorPlayers.reserve(activePlayers.size());
        for (const PlayerSession *player : activePlayers) {
            QVariantMap entry;
            entry[QStringLiteral("playerId")] = player->playerId;
            entry[QStringLiteral("name")] = room->roomManager->playerName(player->playerId);
            survivorPlayers.append(entry);
        }
        const int anchorPlayerId = activePlayers.isEmpty() ? 0 : activePlayers.first()->playerId;
        room->survivorController->configureNetworkSession(survivorPlayers,
                                                          anchorPlayerId,
                                                          true,
                                                          true);
        room->survivorController->startRun(true);
        return;
    }
    case LanBoard::GameControllerKind::DormDefense: {
        room->dormDefenseController->configureNetworkSession(
            room->roomManager->snapshot().activePlayerVariantList(),
            -1,
            true,
            true);
        room->dormDefenseController->startNewGame();
        broadcastDormDefenseStates(room);
        return;
    }
    case LanBoard::GameControllerKind::DouDiZhu:
        broadcastDouDiZhuStates(room);
        return;
    case LanBoard::GameControllerKind::FlightChess:
    case LanBoard::GameControllerKind::Gomoku:
    default:
        return;
    }
}

void ServerApp::handlePlayerDisconnectInRoom(RoomState *room,
                                             const PlayerSession &session)
{
    if (!room) {
        return;
    }

    const bool activeSeat = room->roomManager->isPlayerActive(session.playerId);
    const int disconnectedPiece = room->roomManager->playerPiece(session.playerId);
    switch (LanBoard::controllerKindForGame(room->roomManager->gameId())) {
    case LanBoard::GameControllerKind::DouDiZhu:
        resetFinishedRoom(room);
        return;
    case LanBoard::GameControllerKind::DormDefense:
        if (room->roomManager->gameInProgress() && room->dormDefenseController) {
            room->dormDefenseController->configureNetworkSession(
                room->roomManager->snapshot().activePlayerVariantList(),
                -1,
                true,
                true);
            broadcastDormDefenseStates(room);
        }
        return;
    case LanBoard::GameControllerKind::Survivor:
        if (room->roomManager->gameInProgress() && activeSeat)
            concludeRoomGame(room, 0, false);
        return;
    case LanBoard::GameControllerKind::FlightChess:
        if (room->roomManager->gameInProgress() && activeSeat && disconnectedPiece != 0) {
            room->flightChessController->setGameOver(otherPiece(disconnectedPiece));
            concludeRoomGame(room, room->flightChessController->winner(), false);
        }
        return;
    case LanBoard::GameControllerKind::Gomoku:
    default:
        if (room->roomManager->gameInProgress() && activeSeat && disconnectedPiece != 0) {
            room->gameController->setGameOver(otherPiece(disconnectedPiece));
            concludeRoomGame(room, room->gameController->winner(), false);
        }
        return;
    }
}

void ServerApp::resetGame(RoomState *room)
{
    if (!room)
        return;

    room->gameController->reset();
    room->douDiZhuController->startNetworkGame(0);
    room->dormDefenseController->reset();
    room->flightChessController->reset();
    if (room->survivorController)
        room->survivorController->stopRun();
}

bool ServerApp::isGameFinished(const RoomState *room) const
{
    if (!room)
        return false;

    switch (LanBoard::controllerKindForGame(room->roomManager->gameId())) {
    case LanBoard::GameControllerKind::DouDiZhu:
        return room->douDiZhuController->isGameOver();
    case LanBoard::GameControllerKind::DormDefense:
        return room->dormDefenseController && room->dormDefenseController->isGameOver();
    case LanBoard::GameControllerKind::Survivor:
        return room->survivorController && room->survivorController->isGameOver();
    case LanBoard::GameControllerKind::FlightChess:
        return room->flightChessController->isGameOver();
    case LanBoard::GameControllerKind::Gomoku:
    default:
        return room->gameController->isGameOver();
    }
}

void ServerApp::concludeRoomGame(RoomState *room, int winner, bool broadcastRoomStateAfterward)
{
    if (!room)
        return;

    if (LanBoard::controllerKindForGame(room->roomManager->gameId()) == LanBoard::GameControllerKind::Survivor
        && room->survivorController) {
        if (!room->survivorController->isGameOver())
            room->survivorController->finalizeGameOver(winner);

        for (const PlayerSession *player : activePlayersInRoom(room->roomManager->roomId())) {
            if (!player || !player->peer)
                continue;
            sendRaw(player->peer,
                    room->survivorController->buildFastNetworkPacket(player->playerId),
                    kSurvivorFrameChannel,
                    ENET_PACKET_FLAG_RELIABLE);
            sendRaw(player->peer,
                    room->survivorController->buildHudNetworkPacket(player->playerId),
                    kSurvivorHudChannel,
                    ENET_PACKET_FLAG_RELIABLE);
        }
    }
    if (LanBoard::controllerKindForGame(room->roomManager->gameId())
            == LanBoard::GameControllerKind::DormDefense
        && room->dormDefenseController) {
        broadcastDormDefenseStates(room);
    }

    QJsonObject msg;
    msg[QStringLiteral("type")] = LanBoard::Protocol::GameOver;
    msg[QStringLiteral("winner")] = winner;
    broadcastJsonToRoom(room->roomManager->roomId(), msg);

    room->roomManager->concludeGame();
    if (broadcastRoomStateAfterward)
        broadcastRoomState(room);
}

void ServerApp::resetFinishedRoom(RoomState *room)
{
    if (!room)
        return;

    room->roomManager->concludeGame();
    resetGame(room);
}

void ServerApp::removeRoomIfEmpty(const QString &roomId)
{
    for (const auto &player : std::as_const(m_players)) {
        if (player.roomId == roomId)
            return;
    }

    for (int i = 0; i < m_rooms.size(); ++i) {
        if (m_rooms.at(i)->roomManager && m_rooms.at(i)->roomManager->roomId() == roomId) {
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
        const QString roomId = room->roomManager->roomId();
        const int playerCount = playersInRoom(roomId).size();
        QJsonObject entry;
        entry[QStringLiteral("roomId")] = roomId;
        entry[QStringLiteral("roomName")] = room->roomManager->roomName();
        entry[QStringLiteral("hostName")] = hostNameForRoom(roomId);
        entry[QStringLiteral("gameId")] = room->roomManager->gameId();
        entry[QStringLiteral("gameName")] = room->roomManager->gameName();
        entry[QStringLiteral("playerCount")] = playerCount;
        entry[QStringLiteral("roomCapacity")] = room->roomManager->roomCapacity();
        entry[QStringLiteral("maxPlayers")] = room->roomManager->maxPlayers();
        entry[QStringLiteral("inGame")] = room->roomManager->gameInProgress();
        entry[QStringLiteral("isFull")] = playerCount >= room->roomManager->roomCapacity();
        rooms.append(entry);
    }
    return rooms;
}

QString ServerApp::dormDefenseRoleForPlayer(const RoomState *room, int playerId) const
{
    if (!room || !room->roomManager)
        return QStringLiteral("spectator");
    return room->roomManager->snapshot().dormDefenseRoleForPlayer(playerId);
}

QJsonObject ServerApp::dormDefenseStateForPlayer(const RoomState *room, int playerId) const
{
    QJsonObject state = room->dormDefenseController->buildNetworkStateForPlayer(playerId);
    state[QStringLiteral("type")] = LanBoard::Protocol::DormDefenseState;
    return state;
}

bool ServerApp::resolveSessionRoom(ENetPeer *peer, PlayerSession *&session, RoomState *&room)
{
    session = sessionForPeer(peer);
    if (!session) {
        sendError(peer, QStringLiteral("not_joined"));
        room = nullptr;
        return false;
    }

    room = roomForPlayer(session);
    if (room)
        return true;

    sendError(peer, QStringLiteral("room_not_found"));
    return false;
}

bool ServerApp::ensureControllerKind(ENetPeer *peer,
                                     const RoomState *room,
                                     LanBoard::GameControllerKind expectedKind)
{
    if (room && room->roomManager
        && LanBoard::controllerKindForGame(room->roomManager->gameId()) == expectedKind) {
        return true;
    }

    sendError(peer, QStringLiteral("wrong_game"));
    return false;
}

bool ServerApp::ensureActiveSeat(ENetPeer *peer,
                                 const PlayerSession *session,
                                 const char *errorKey)
{
    if (session) {
        const RoomState *room = roomById(session->roomId);
        if (room && room->roomManager && room->roomManager->isPlayerActive(session->playerId))
            return true;
    }

    sendError(peer, QString::fromLatin1(errorKey));
    return false;
}

bool ServerApp::ensureGameStarted(ENetPeer *peer, const RoomState *room)
{
    if (room && room->roomManager && room->roomManager->gameInProgress())
        return true;

    sendError(peer, QStringLiteral("game_not_started"));
    return false;
}

bool ServerApp::ensureSurvivorInteractionOwner(ENetPeer *peer,
                                               const RoomState *room,
                                               int playerId)
{
    if (!room || !room->survivorController)
        return false;
    if (room->survivorController->interactionPlayerId() == playerId)
        return true;

    sendError(peer, QStringLiteral("not_your_upgrade"));
    return false;
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
        if (room->roomManager && room->roomManager->roomId() == roomId)
            return room;
    }
    return nullptr;
}

const ServerApp::RoomState *ServerApp::roomById(const QString &roomId) const
{
    for (const RoomState *room : std::as_const(m_rooms)) {
        if (room->roomManager && room->roomManager->roomId() == roomId)
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
    RoomState *room = roomById(roomId);
    for (auto &player : m_players) {
        if (player.roomId == roomId
            && room && room->roomManager
            && room->roomManager->isPlayerActive(player.playerId)) {
            roomPlayers.append(&player);
        }
    }
    return roomPlayers;
}

QList<const ServerApp::PlayerSession *> ServerApp::activePlayersInRoom(const QString &roomId) const
{
    QList<const PlayerSession *> roomPlayers;
    const RoomState *room = roomById(roomId);
    for (const auto &player : m_players) {
        if (player.roomId == roomId
            && room && room->roomManager
            && room->roomManager->isPlayerActive(player.playerId)) {
            roomPlayers.append(&player);
        }
    }
    return roomPlayers;
}

QString ServerApp::hostNameForRoom(const QString &roomId) const
{
    const RoomState *room = roomById(roomId);
    if (!room || !room->roomManager)
        return QStringLiteral("host");

    const LanBoard::RoomSnapshot snapshot = room->roomManager->snapshot();
    for (const auto &player : snapshot.players) {
        if (player.isHost)
            return player.name;
    }
    return QStringLiteral("host");
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
