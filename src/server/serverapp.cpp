#include "serverapp.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLoggingCategory>

namespace {

constexpr quint16 kDefaultPort = 44567;
constexpr int kRoomHumanPlayers = 2;
constexpr int kDouDiZhuRobotPlayer = 2;
constexpr int kRobotTurnDelayMs = 3000;

QString normalizedName(const QString &name)
{
    const QString trimmed = name.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("player") : trimmed;
}

QString normalizedGameId(const QString &gameId)
{
    return gameId == QStringLiteral("doudizhu")
        ? QStringLiteral("doudizhu")
        : QStringLiteral("gomoku");
}

QString gameName(const QString &gameId)
{
    return gameId == QStringLiteral("doudizhu")
        ? QStringLiteral("斗地主")
        : QStringLiteral("五子棋");
}

} // namespace

ServerApp::ServerApp(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &ServerApp::onNewConnection);

    connect(&m_gameController, &GameController::gameOverChanged, this, [this]() {
        if (isDouDiZhuRoom())
            return;
        if (!m_gameController.isGameOver())
            return;

        QJsonObject msg;
        msg[QStringLiteral("type")] = QStringLiteral("game_over");
        msg[QStringLiteral("winner")] = m_gameController.winner();
        broadcastJson(msg);

        clearReadyStates();
        broadcastRoomState();
    });
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

    const int disconnectedPiece = session->piece;
    const QString playerName = session->name;

    for (int i = 0; i < m_players.size(); ++i) {
        if (m_players[i].socket == socket) {
            m_players.removeAt(i);
            break;
        }
    }

    if (!isDouDiZhuRoom() && !m_gameController.isGameOver() && disconnectedPiece != 0 && !m_players.isEmpty())
        m_gameController.setGameOver(otherPiece(disconnectedPiece));

    clearReadyStates();
    if (m_players.isEmpty()) {
        resetGame();
        m_gameId = QStringLiteral("gomoku");
        m_nextPlayerId = 1;
    } else if (isDouDiZhuRoom()) {
        resetGame();
    }

    broadcastRoomState();

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
    } else if (type == QStringLiteral("ready")) {
        handleReady(socket, msg.value(QStringLiteral("ready")).toBool());
    } else if (type == QStringLiteral("start_game")) {
        handleStartGame(socket);
    } else if (type == QStringLiteral("place_piece")) {
        handlePlacePiece(socket,
                         msg.value(QStringLiteral("row")).toInt(-1),
                         msg.value(QStringLiteral("col")).toInt(-1));
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
    if (!socket)
        return;

    if (sessionForSocket(socket)) {
        sendError(socket, QStringLiteral("already_joined"));
        return;
    }

    const QString requestedGameId = normalizedGameId(gameId);
    if (m_players.isEmpty()) {
        m_gameId = requestedGameId;
        resetGame();
    } else if (requestedGameId != m_gameId) {
        sendError(socket, QStringLiteral("room_game_mismatch"));
        socket->disconnectFromHost();
        return;
    }

    if (m_players.size() >= maxPlayers()) {
        sendError(socket, QStringLiteral("room_full"));
        socket->disconnectFromHost();
        return;
    }

    const int piece = isDouDiZhuRoom() ? 0 : (m_players.isEmpty() ? 1 : 2);
    const int playerId = isDouDiZhuRoom() ? nextDouDiZhuPlayerId() : m_nextPlayerId++;
    if (playerId < 0) {
        sendError(socket, QStringLiteral("room_full"));
        socket->disconnectFromHost();
        return;
    }

    PlayerSession session;
    session.playerId = playerId;
    session.piece = piece;
    session.name = normalizedName(name);
    session.socket = socket;
    m_players.append(session);

    qInfo().noquote() << QStringLiteral("Joined: %1 (playerId=%2, piece=%3)")
                             .arg(session.name)
                             .arg(playerId)
                             .arg(piece);

    broadcastRoomState();
}

void ServerApp::handleReady(QTcpSocket *socket, bool ready)
{
    PlayerSession *session = sessionForSocket(socket);
    if (!session) {
        sendError(socket, QStringLiteral("not_joined"));
        return;
    }

    if ((isDouDiZhuRoom() && m_douDiZhuController.isGameOver())
        || (!isDouDiZhuRoom() && m_gameController.isGameOver())) {
        resetGame();
        clearReadyStates();
    }

    session->isReady = ready;
    broadcastRoomState();
}

void ServerApp::handleStartGame(QTcpSocket *socket)
{
    PlayerSession *session = sessionForSocket(socket);
    if (!session) {
        sendError(socket, QStringLiteral("not_joined"));
        return;
    }

    if ((!isDouDiZhuRoom() && session->piece != 1)
        || (isDouDiZhuRoom() && session->playerId != 0)) {
        sendError(socket, QStringLiteral("only_player_one_can_start"));
        return;
    }

    if (m_players.size() != maxPlayers()) {
        sendError(socket, QStringLiteral("need_two_players"));
        return;
    }

    for (const auto &player : m_players) {
        const bool hostPlayer = isDouDiZhuRoom()
            ? player.playerId == 0
            : player.piece == 1;
        if (!hostPlayer && !player.isReady) {
            sendError(socket, QStringLiteral("players_not_ready"));
            return;
        }
    }

    resetGame();

    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("game_start");
    msg[QStringLiteral("gameId")] = m_gameId;
    msg[QStringLiteral("firstPlayer")] = isDouDiZhuRoom() ? 0 : 1;
    broadcastJson(msg);

    if (isDouDiZhuRoom()) {
        broadcastDouDiZhuStates();
        scheduleDouDiZhuRobotTurn();
    }
}

void ServerApp::handlePlacePiece(QTcpSocket *socket, int row, int col)
{
    PlayerSession *session = sessionForSocket(socket);
    if (!session) {
        sendError(socket, QStringLiteral("not_joined"));
        return;
    }

    if (isDouDiZhuRoom()) {
        sendError(socket, QStringLiteral("wrong_game"));
        return;
    }

    if (m_players.size() != 2) {
        sendError(socket, QStringLiteral("need_two_players"));
        return;
    }

    if (!m_gameController.placePiece(row, col, session->piece)) {
        sendError(socket, QStringLiteral("invalid_move"));
        return;
    }

    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("move");
    msg[QStringLiteral("player")] = session->piece;
    msg[QStringLiteral("row")] = row;
    msg[QStringLiteral("col")] = col;
    broadcastJson(msg);
}

void ServerApp::handleSurrender(QTcpSocket *socket)
{
    PlayerSession *session = sessionForSocket(socket);
    if (!session) {
        sendError(socket, QStringLiteral("not_joined"));
        return;
    }

    if (isDouDiZhuRoom()) {
        sendError(socket, QStringLiteral("wrong_game"));
        return;
    }

    if (!m_gameController.surrender(session->piece))
        sendError(socket, QStringLiteral("invalid_surrender"));
}

void ServerApp::handleDouDiZhuPlay(QTcpSocket *socket, const QJsonArray &cardIds)
{
    PlayerSession *session = sessionForSocket(socket);
    if (!session) {
        sendError(socket, QStringLiteral("not_joined"));
        return;
    }

    if (!isDouDiZhuRoom()) {
        sendError(socket, QStringLiteral("wrong_game"));
        return;
    }

    QVariantList ids;
    for (const auto &id : cardIds)
        ids.append(id.toInt());

    if (!m_douDiZhuController.playCardsForPlayer(session->playerId, ids)) {
        sendError(socket, QStringLiteral("invalid_ddz_play"));
        broadcastDouDiZhuStates();
        return;
    }

    broadcastDouDiZhuStates();
    scheduleDouDiZhuRobotTurn();
}

void ServerApp::handleDouDiZhuPass(QTcpSocket *socket)
{
    PlayerSession *session = sessionForSocket(socket);
    if (!session) {
        sendError(socket, QStringLiteral("not_joined"));
        return;
    }

    if (!isDouDiZhuRoom()) {
        sendError(socket, QStringLiteral("wrong_game"));
        return;
    }

    if (!m_douDiZhuController.passForPlayer(session->playerId)) {
        sendError(socket, QStringLiteral("invalid_ddz_pass"));
        broadcastDouDiZhuStates();
        return;
    }

    broadcastDouDiZhuStates();
    scheduleDouDiZhuRobotTurn();
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

void ServerApp::broadcastJson(const QJsonObject &obj, QTcpSocket *exclude)
{
    for (const auto &player : m_players) {
        if (player.socket && player.socket != exclude)
            sendJson(player.socket, obj);
    }
}

void ServerApp::broadcastRoomState()
{
    QJsonArray players;
    for (const auto &player : m_players) {
        QJsonObject entry;
        entry[QStringLiteral("playerId")] = player.playerId;
        entry[QStringLiteral("name")] = player.name;
        entry[QStringLiteral("isHost")] = isDouDiZhuRoom()
            ? player.playerId == 0
            : player.piece == 1;
        entry[QStringLiteral("isReady")] = player.isReady;
        entry[QStringLiteral("piece")] = player.piece;
        players.append(entry);
    }

    for (const auto &player : m_players) {
        if (!player.socket)
            continue;

        QJsonObject msg;
        msg[QStringLiteral("type")] = QStringLiteral("room_state");
        msg[QStringLiteral("gameId")] = m_gameId;
        msg[QStringLiteral("gameName")] = gameName(m_gameId);
        msg[QStringLiteral("maxPlayers")] = maxPlayers();
        msg[QStringLiteral("players")] = players;
        msg[QStringLiteral("yourPlayerId")] = player.playerId;
        msg[QStringLiteral("yourPiece")] = player.piece;
        msg[QStringLiteral("mode")] = QStringLiteral("dedicated_server");
        sendJson(player.socket, msg);
    }
}

void ServerApp::broadcastDouDiZhuStates()
{
    if (!isDouDiZhuRoom())
        return;

    for (const auto &player : m_players) {
        if (!player.socket)
            continue;

        QJsonObject msg = m_douDiZhuController.stateForPlayer(player.playerId);
        msg[QStringLiteral("type")] = QStringLiteral("ddz_state");
        sendJson(player.socket, msg);
    }

    if (m_douDiZhuController.isGameOver()) {
        clearReadyStates();
        broadcastRoomState();
    }
}

void ServerApp::clearReadyStates()
{
    for (auto &player : m_players)
        player.isReady = false;
}

void ServerApp::resetGame()
{
    ++m_douDiZhuRobotTurnToken;
    m_douDiZhuRobotTurnPending = false;

    if (isDouDiZhuRoom()) {
        m_douDiZhuController.startNetworkGame(0);
    } else {
        m_gameController.reset();
    }
}

void ServerApp::scheduleDouDiZhuRobotTurn()
{
    if (!isDouDiZhuRoom()
        || m_douDiZhuController.isGameOver()
        || m_douDiZhuController.currentPlayer() != kDouDiZhuRobotPlayer
        || m_douDiZhuRobotTurnPending) {
        return;
    }

    m_douDiZhuRobotTurnPending = true;
    const int token = ++m_douDiZhuRobotTurnToken;

    QTimer::singleShot(kRobotTurnDelayMs, this, [this, token]() {
        if (token != m_douDiZhuRobotTurnToken || !isDouDiZhuRoom())
            return;

        m_douDiZhuRobotTurnPending = false;
        if (!m_douDiZhuController.playAiTurnForPlayer(kDouDiZhuRobotPlayer))
            return;

        broadcastDouDiZhuStates();
        scheduleDouDiZhuRobotTurn();
    });
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

int ServerApp::otherPiece(int piece) const
{
    return piece == 1 ? 2 : 1;
}

int ServerApp::maxPlayers() const
{
    return kRoomHumanPlayers;
}

int ServerApp::nextDouDiZhuPlayerId() const
{
    for (int candidate = 0; candidate < maxPlayers(); ++candidate) {
        bool used = false;
        for (const auto &player : m_players) {
            if (player.playerId == candidate) {
                used = true;
                break;
            }
        }
        if (!used)
            return candidate;
    }
    return -1;
}

bool ServerApp::isDouDiZhuRoom() const
{
    return m_gameId == QStringLiteral("doudizhu");
}
