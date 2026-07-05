#include "serverapp.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRandomGenerator>

namespace {

constexpr quint16 kDefaultPort = 44567;

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
    connect(&m_server, &QTcpServer::newConnection, this, &ServerApp::onNewConnection);
}

bool ServerApp::start(quint16 port)
{
    if (port == 0)
        port = kDefaultPort;

    if (!m_server.listen(QHostAddress::AnyIPv4, port))
        return false;

    qInfo().noquote() << QStringLiteral("LanBoard server listening on port %1").arg(port);
    return true;
}

void ServerApp::onNewConnection()
{
    while (m_server.hasPendingConnections()) {
        QTcpSocket *socket = m_server.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            onReadyRead(socket);
        });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
            onDisconnected(socket);
        });

        qInfo().noquote() << QStringLiteral("Incoming connection from %1")
                                 .arg(socket->peerAddress().toString());
    }
}

void ServerApp::onReadyRead(QTcpSocket *socket)
{
    if (!socket)
        return;

    QByteArray buffer = socket->property("readBuffer").toByteArray();
    buffer.append(socket->readAll());

    while (true) {
        const int newlineIndex = buffer.indexOf('\n');
        if (newlineIndex < 0)
            break;

        const QByteArray line = buffer.left(newlineIndex).trimmed();
        buffer.remove(0, newlineIndex + 1);

        if (line.isEmpty())
            continue;

        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(line, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            sendError(socket, QStringLiteral("invalid_json"));
            continue;
        }

        processMessage(socket, document.object());
    }

    socket->setProperty("readBuffer", buffer);
}

void ServerApp::onDisconnected(QTcpSocket *socket)
{
    if (!socket)
        return;

    PlayerSession *session = sessionForSocket(socket);
    if (!session) {
        socket->deleteLater();
        return;
    }

    const QString roomId = session->roomId;
    RoomState *room = roomForPlayer(session);
    const QString playerName = session->name;
    const bool wasHost = session->isHost;
    const int disconnectedPiece = session->piece;

    if (wasHost && room) {
        QList<QTcpSocket *> remainingSockets;
        for (const auto &player : std::as_const(m_players)) {
            if (player.roomId != roomId || player.socket == socket || !player.socket)
                continue;
            remainingSockets.append(player.socket);
        }

        for (QTcpSocket *otherSocket : remainingSockets) {
            sendError(otherSocket, QStringLiteral("room_closed"));
            otherSocket->disconnectFromHost();
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
            if (m_players[i].socket == socket) {
                m_players.removeAt(i);
                break;
            }
        }

        room = roomById(roomId);
        if (room) {
            if (isDouDiZhuRoom(room->gameId)) {
                resetFinishedRoom(room);
            } else if (isFlightChessRoom(room->gameId)
                       && room->gameActive
                       && session->seatType == QStringLiteral("active")
                       && disconnectedPiece != 0) {
                room->flightChessController->setGameOver(otherPiece(disconnectedPiece));
                concludeRoomGame(room, room->flightChessController->winner(), false);
            } else if (room->gameActive
                       && session->seatType == QStringLiteral("active")
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
    socket->deleteLater();
}

void ServerApp::processMessage(QTcpSocket *socket, const QJsonObject &msg)
{
    const QString type = msg.value(QStringLiteral("type")).toString();

    if (type == QStringLiteral("join")) {
        handleJoin(socket,
                   msg.value(QStringLiteral("name")).toString(),
                   msg.value(QStringLiteral("gameId")).toString());
    } else if (type == QStringLiteral("list_rooms")) {
        handleListRooms(socket);
    } else if (type == QStringLiteral("create_room")) {
        handleCreateRoom(socket,
                         msg.value(QStringLiteral("name")).toString(),
                         msg.value(QStringLiteral("roomName")).toString(),
                         msg.value(QStringLiteral("gameId")).toString());
    } else if (type == QStringLiteral("join_room")) {
        handleJoinRoom(socket,
                       msg.value(QStringLiteral("name")).toString(),
                       msg.value(QStringLiteral("roomId")).toString());
    } else if (type == QStringLiteral("ready")) {
        handleReady(socket, msg.value(QStringLiteral("ready")).toBool());
    } else if (type == QStringLiteral("start_game")) {
        handleStartGame(socket);
    } else if (type == QStringLiteral("change_seat")) {
        handleChangeSeat(socket, msg.value(QStringLiteral("seatType")).toString());
    } else if (type == QStringLiteral("switch_room_game")) {
        handleSwitchRoomGame(socket, msg.value(QStringLiteral("gameId")).toString());
    } else if (type == QStringLiteral("place_piece")) {
        handlePlacePiece(socket,
                         msg.value(QStringLiteral("row")).toInt(-1),
                         msg.value(QStringLiteral("col")).toInt(-1));
    } else if (type == QStringLiteral("flight_roll")) {
        handleFlightRoll(socket);
    } else if (type == QStringLiteral("flight_move")) {
        handleFlightMove(socket, msg.value(QStringLiteral("planeIndex")).toInt(-1));
    } else if (type == QStringLiteral("surrender")) {
        handleSurrender(socket);
    } else if (type == QStringLiteral("ddz_play")) {
        handleDouDiZhuPlay(socket, msg.value(QStringLiteral("cards")).toArray());
    } else if (type == QStringLiteral("ddz_pass")) {
        handleDouDiZhuPass(socket);
    } else {
        sendError(socket, QStringLiteral("unsupported_message_type"));
    }
}

void ServerApp::handleJoin(QTcpSocket *socket, const QString &name, const QString &gameId)
{
    // Legacy fallback: first player creates a room, later players join the first room with same game.
    const QString normalized = LanBoard::normalizeGameId(gameId);
    RoomState *targetRoom = nullptr;
    for (RoomState *room : std::as_const(m_rooms)) {
        if (room->gameId == normalized) {
            targetRoom = room;
            break;
        }
    }

    if (!targetRoom) {
        handleCreateRoom(socket, name, QString(), normalized);
        return;
    }

    handleJoinRoom(socket, name, targetRoom->roomId);
}

void ServerApp::handleListRooms(QTcpSocket *socket)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("rooms_list");
    msg[QStringLiteral("rooms")] = roomListPayload();
    sendJson(socket, msg);
}

void ServerApp::handleCreateRoom(QTcpSocket *socket, const QString &name, const QString &roomName,
                                 const QString &gameId)
{
    if (!socket)
        return;

    if (sessionForSocket(socket)) {
        sendError(socket, QStringLiteral("already_joined"));
        return;
    }

    RoomState *room = new RoomState();
    room->roomId = createRoomId();
    room->gameId = LanBoard::normalizeGameId(gameId);
    room->roomName = normalizedRoomName(roomName, normalizedName(name), room->gameId);
    room->gameController = std::make_unique<GameController>();
    room->douDiZhuController = std::make_unique<DouDiZhuController>();
    room->flightChessController = std::make_unique<FlightChessController>();
    room->gameActive = false;
    m_rooms.append(room);

    PlayerSession session;
    session.playerId = 0;
    session.piece = initialPieceForGame(room->gameId, true);
    session.name = normalizedName(name);
    session.roomId = room->roomId;
    session.isHost = true;
    session.seatType = QStringLiteral("active");
    session.socket = socket;
    m_players.append(session);

    resetGame(room);
    broadcastRoomState(room);
}

void ServerApp::handleJoinRoom(QTcpSocket *socket, const QString &name, const QString &roomId)
{
    if (!socket)
        return;

    if (sessionForSocket(socket)) {
        sendError(socket, QStringLiteral("already_joined"));
        return;
    }

    RoomState *room = roomById(roomId.trimmed());
    if (!room) {
        sendError(socket, QStringLiteral("room_not_found"));
        return;
    }

    if (room->gameActive) {
        sendError(socket, QStringLiteral("room_in_game"));
        return;
    }

    const auto players = playersInRoom(room->roomId);
    if (players.size() >= roomCapacity()) {
        sendError(socket, QStringLiteral("room_full"));
        return;
    }

    PlayerSession session;
    session.playerId = nextPlayerIdForRoom(room->roomId, room->gameId);
    if (session.playerId < 0) {
        sendError(socket, QStringLiteral("room_full"));
        return;
    }
    session.piece = initialPieceForGame(room->gameId, false);
    session.name = normalizedName(name);
    session.roomId = room->roomId;
    session.isHost = false;
    session.seatType = activePlayersInRoom(room->roomId).size() < maxPlayers(room->gameId)
        ? QStringLiteral("active")
        : QStringLiteral("spectator");
    session.socket = socket;
    m_players.append(session);

    normalizeSeats(room);
    broadcastRoomState(room);
}

void ServerApp::handleReady(QTcpSocket *socket, bool ready)
{
    PlayerSession *session = sessionForSocket(socket);
    if (!session) {
        sendError(socket, QStringLiteral("not_joined"));
        return;
    }

    RoomState *room = roomForPlayer(session);
    if (!room) {
        sendError(socket, QStringLiteral("room_not_found"));
        return;
    }

    if (session->seatType != QStringLiteral("active")) {
        sendError(socket, QStringLiteral("spectator_cannot_ready"));
        return;
    }

    if (room->gameActive) {
        if (isGameFinished(room))
            resetFinishedRoom(room);
    }

    session->isReady = ready;
    broadcastRoomState(room);
}

void ServerApp::handleStartGame(QTcpSocket *socket)
{
    PlayerSession *session = sessionForSocket(socket);
    if (!session) {
        sendError(socket, QStringLiteral("not_joined"));
        return;
    }

    RoomState *room = roomForPlayer(session);
    if (!room) {
        sendError(socket, QStringLiteral("room_not_found"));
        return;
    }

    if (!session->isHost) {
        sendError(socket, QStringLiteral("only_player_one_can_start"));
        return;
    }

    const auto players = activePlayersInRoom(room->roomId);
    if (players.size() != maxPlayers(room->gameId)) {
        sendError(socket, missingPlayersError(room));
        return;
    }

    for (const PlayerSession *player : players) {
        if (!player->isReady) {
            sendError(socket, QStringLiteral("players_not_ready"));
            return;
        }
    }

    resetGame(room);
    room->gameActive = true;

    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("game_start");
    msg[QStringLiteral("gameId")] = room->gameId;
    msg[QStringLiteral("firstPlayer")] = firstPlayerForRoom(room);
    broadcastJsonToRoom(room->roomId, msg);

    if (isDouDiZhuRoom(room->gameId))
        broadcastDouDiZhuStates(room);
}

void ServerApp::handleChangeSeat(QTcpSocket *socket, const QString &seatType)
{
    PlayerSession *session = sessionForSocket(socket);
    if (!session) {
        sendError(socket, QStringLiteral("not_joined"));
        return;
    }

    RoomState *room = roomForPlayer(session);
    if (!room) {
        sendError(socket, QStringLiteral("room_not_found"));
        return;
    }

    if (session->isHost) {
        sendError(socket, QStringLiteral("host_locked_active"));
        return;
    }

    if (room->gameActive) {
        sendError(socket, QStringLiteral("game_in_progress"));
        return;
    }

    const QString normalizedSeatType = seatType == QStringLiteral("spectator")
        ? QStringLiteral("spectator")
        : QStringLiteral("active");
    if (normalizedSeatType == QStringLiteral("active")
        && session->seatType != QStringLiteral("active")
        && activePlayersInRoom(room->roomId).size() >= maxPlayers(room->gameId)) {
        sendError(socket, QStringLiteral("active_seat_full"));
        return;
    }

    if (session->seatType == normalizedSeatType) {
        broadcastRoomState(room);
        return;
    }

    session->seatType = normalizedSeatType;
    session->isReady = false;
    normalizeSeats(room, false);
    broadcastRoomState(room);
}

void ServerApp::handleSwitchRoomGame(QTcpSocket *socket, const QString &gameId)
{
    PlayerSession *session = sessionForSocket(socket);
    if (!session) {
        sendError(socket, QStringLiteral("not_joined"));
        return;
    }

    RoomState *room = roomForPlayer(session);
    if (!room) {
        sendError(socket, QStringLiteral("room_not_found"));
        return;
    }

    if (!session->isHost) {
        sendError(socket, QStringLiteral("only_host_can_switch_game"));
        return;
    }

    if (room->gameActive) {
        sendError(socket, QStringLiteral("game_in_progress"));
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

void ServerApp::handlePlacePiece(QTcpSocket *socket, int row, int col)
{
    PlayerSession *session = sessionForSocket(socket);
    if (!session) {
        sendError(socket, QStringLiteral("not_joined"));
        return;
    }

    RoomState *room = roomForPlayer(session);
    if (!room) {
        sendError(socket, QStringLiteral("room_not_found"));
        return;
    }

    if (isDouDiZhuRoom(room->gameId) || isFlightChessRoom(room->gameId)) {
        sendError(socket, QStringLiteral("wrong_game"));
        return;
    }

    if (session->seatType != QStringLiteral("active")) {
        sendError(socket, QStringLiteral("spectator_cannot_move"));
        return;
    }

    if (activePlayersInRoom(room->roomId).size() != 2) {
        sendError(socket, QStringLiteral("need_two_players"));
        return;
    }

    if (!room->gameController->placePiece(row, col, session->piece)) {
        sendError(socket, QStringLiteral("invalid_move"));
        return;
    }

    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("move");
    msg[QStringLiteral("player")] = session->piece;
    msg[QStringLiteral("row")] = row;
    msg[QStringLiteral("col")] = col;
    broadcastJsonToRoom(room->roomId, msg);

    if (room->gameController->isGameOver()) {
        concludeRoomGame(room, room->gameController->winner());
    }
}

void ServerApp::handleFlightRoll(QTcpSocket *socket)
{
    PlayerSession *session = sessionForSocket(socket);
    if (!session) {
        sendError(socket, QStringLiteral("not_joined"));
        return;
    }

    RoomState *room = roomForPlayer(session);
    if (!room) {
        sendError(socket, QStringLiteral("room_not_found"));
        return;
    }

    if (!isFlightChessRoom(room->gameId)) {
        sendError(socket, QStringLiteral("wrong_game"));
        return;
    }

    if (!room->gameActive) {
        sendError(socket, QStringLiteral("game_not_started"));
        return;
    }

    if (session->seatType != QStringLiteral("active")) {
        sendError(socket, QStringLiteral("spectator_cannot_move"));
        return;
    }

    if (room->flightChessController->currentPlayer() != session->piece
        || room->flightChessController->hasRolled()) {
        sendError(socket, QStringLiteral("not_your_turn"));
        return;
    }

    const int diceValue = room->flightChessController->rollDice();
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("flight_roll_result");
    msg[QStringLiteral("player")] = session->piece;
    msg[QStringLiteral("diceValue")] = diceValue;
    broadcastJsonToRoom(room->roomId, msg);
}

void ServerApp::handleFlightMove(QTcpSocket *socket, int planeIndex)
{
    PlayerSession *session = sessionForSocket(socket);
    if (!session) {
        sendError(socket, QStringLiteral("not_joined"));
        return;
    }

    RoomState *room = roomForPlayer(session);
    if (!room) {
        sendError(socket, QStringLiteral("room_not_found"));
        return;
    }

    if (!isFlightChessRoom(room->gameId)) {
        sendError(socket, QStringLiteral("wrong_game"));
        return;
    }

    if (!room->gameActive) {
        sendError(socket, QStringLiteral("game_not_started"));
        return;
    }

    if (session->seatType != QStringLiteral("active")) {
        sendError(socket, QStringLiteral("spectator_cannot_move"));
        return;
    }

    if (room->flightChessController->currentPlayer() != session->piece) {
        sendError(socket, QStringLiteral("not_your_turn"));
        return;
    }

    if (!room->flightChessController->movePlane(planeIndex)) {
        sendError(socket, QStringLiteral("invalid_move"));
        return;
    }

    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("flight_move_result");
    msg[QStringLiteral("player")] = session->piece;
    msg[QStringLiteral("planeIndex")] = planeIndex;
    broadcastJsonToRoom(room->roomId, msg);

    if (room->flightChessController->isGameOver()) {
        concludeRoomGame(room, room->flightChessController->winner());
    }
}

void ServerApp::handleSurrender(QTcpSocket *socket)
{
    PlayerSession *session = sessionForSocket(socket);
    if (!session) {
        sendError(socket, QStringLiteral("not_joined"));
        return;
    }

    RoomState *room = roomForPlayer(session);
    if (!room) {
        sendError(socket, QStringLiteral("room_not_found"));
        return;
    }

    if (isDouDiZhuRoom(room->gameId)) {
        sendError(socket, QStringLiteral("wrong_game"));
        return;
    }

    if (session->seatType != QStringLiteral("active")) {
        sendError(socket, QStringLiteral("spectator_cannot_surrender"));
        return;
    }

    if (isFlightChessRoom(room->gameId)) {
        room->flightChessController->setGameOver(otherPiece(session->piece));
        concludeRoomGame(room, room->flightChessController->winner());
    } else {
        if (!room->gameController->surrender(session->piece)) {
            sendError(socket, QStringLiteral("invalid_surrender"));
            return;
        }
        concludeRoomGame(room, room->gameController->winner());
    }
}

void ServerApp::handleDouDiZhuPlay(QTcpSocket *socket, const QJsonArray &cardIds)
{
    PlayerSession *session = sessionForSocket(socket);
    if (!session) {
        sendError(socket, QStringLiteral("not_joined"));
        return;
    }

    RoomState *room = roomForPlayer(session);
    if (!room) {
        sendError(socket, QStringLiteral("room_not_found"));
        return;
    }

    if (!isDouDiZhuRoom(room->gameId)) {
        sendError(socket, QStringLiteral("wrong_game"));
        return;
    }

    if (session->seatType != QStringLiteral("active")) {
        sendError(socket, QStringLiteral("spectator_cannot_play"));
        return;
    }

    QVariantList ids;
    for (const auto &id : cardIds)
        ids.append(id.toInt());

    if (!room->douDiZhuController->playCardsForPlayer(session->playerId, ids)) {
        sendError(socket, QStringLiteral("invalid_ddz_play"));
        broadcastDouDiZhuStates(room);
        return;
    }

    broadcastDouDiZhuStates(room);
}

void ServerApp::handleDouDiZhuPass(QTcpSocket *socket)
{
    PlayerSession *session = sessionForSocket(socket);
    if (!session) {
        sendError(socket, QStringLiteral("not_joined"));
        return;
    }

    RoomState *room = roomForPlayer(session);
    if (!room) {
        sendError(socket, QStringLiteral("room_not_found"));
        return;
    }

    if (!isDouDiZhuRoom(room->gameId)) {
        sendError(socket, QStringLiteral("wrong_game"));
        return;
    }

    if (session->seatType != QStringLiteral("active")) {
        sendError(socket, QStringLiteral("spectator_cannot_pass"));
        return;
    }

    if (!room->douDiZhuController->passForPlayer(session->playerId)) {
        sendError(socket, QStringLiteral("invalid_ddz_pass"));
        broadcastDouDiZhuStates(room);
        return;
    }

    broadcastDouDiZhuStates(room);
}

void ServerApp::sendJson(QTcpSocket *socket, const QJsonObject &obj)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState)
        return;

    QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    data.append('\n');
    socket->write(data);
    socket->flush();
}

void ServerApp::sendError(QTcpSocket *socket, const QString &message)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("error");
    msg[QStringLiteral("message")] = message;
    sendJson(socket, msg);
}

void ServerApp::broadcastJsonToRoom(const QString &roomId, const QJsonObject &obj, QTcpSocket *exclude)
{
    for (const auto &player : std::as_const(m_players)) {
        if (player.roomId != roomId || !player.socket || player.socket == exclude)
            continue;
        sendJson(player.socket, obj);
    }
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
        entry[QStringLiteral("seatType")] = player->seatType;
        entry[QStringLiteral("piece")] = player->piece;
        playerArray.append(entry);
    }

    for (const PlayerSession *player : players) {
        if (!player->socket)
            continue;

        QJsonObject msg;
        msg[QStringLiteral("type")] = QStringLiteral("room_state");
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
        sendJson(player->socket, msg);
    }
}

void ServerApp::broadcastDouDiZhuStates(RoomState *room)
{
    if (!room || !isDouDiZhuRoom(room->gameId))
        return;

    for (const PlayerSession *player : activePlayersInRoom(room->roomId)) {
        if (!player->socket)
            continue;

        QJsonObject msg = room->douDiZhuController->stateForPlayer(player->playerId);
        msg[QStringLiteral("type")] = QStringLiteral("ddz_state");
        sendJson(player->socket, msg);
    }

    if (room->douDiZhuController->isGameOver()) {
        concludeRoomGame(room, room->douDiZhuController->winner());
    }
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
    msg[QStringLiteral("type")] = QStringLiteral("game_over");
    msg[QStringLiteral("winner")] = winner;
    broadcastJsonToRoom(room->roomId, msg);

    room->gameActive = false;
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

ServerApp::PlayerSession *ServerApp::sessionForSocket(QTcpSocket *socket)
{
    for (auto &player : m_players) {
        if (player.socket == socket)
            return &player;
    }
    return nullptr;
}

const ServerApp::PlayerSession *ServerApp::sessionForSocket(QTcpSocket *socket) const
{
    for (const auto &player : m_players) {
        if (player.socket == socket)
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
        if (player.roomId == roomId && player.seatType == QStringLiteral("active"))
            roomPlayers.append(&player);
    }
    return roomPlayers;
}

QList<const ServerApp::PlayerSession *> ServerApp::activePlayersInRoom(const QString &roomId) const
{
    QList<const PlayerSession *> roomPlayers;
    for (const auto &player : m_players) {
        if (player.roomId == roomId && player.seatType == QStringLiteral("active"))
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
            player->seatType = QStringLiteral("active");
            player->piece = initialPieceForGame(room->gameId, true);
            continue;
        }
        if (player->seatType == QStringLiteral("active"))
            ++activeGuests;
    }

    for (int i = players.size() - 1; i >= 0 && activeGuests > activeGuestLimit(room->gameId); --i) {
        PlayerSession *player = players.at(i);
        if (player->isHost || player->seatType != QStringLiteral("active"))
            continue;
        player->seatType = QStringLiteral("spectator");
        player->isReady = false;
        --activeGuests;
    }

    if (fillMissingActiveSeats) {
        for (PlayerSession *player : players) {
            if (activeGuests >= activeGuestLimit(room->gameId))
                break;
            if (player->isHost || player->seatType != QStringLiteral("spectator"))
                continue;
            player->seatType = QStringLiteral("active");
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
        } else if (player->seatType == QStringLiteral("active") && !whiteAssigned) {
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
