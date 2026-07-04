#include "serverapp.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLoggingCategory>

namespace {

constexpr quint16 kDefaultPort = 44567;

QString normalizedName(const QString &name)
{
    const QString trimmed = name.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("player") : trimmed;
}

} // namespace

ServerApp::ServerApp(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &ServerApp::onNewConnection);

    connect(&m_gameController, &GameController::gameOverChanged, this, [this]() {
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

    if (!m_gameController.isGameOver() && disconnectedPiece != 0 && !m_players.isEmpty())
        m_gameController.setGameOver(otherPiece(disconnectedPiece));

    clearReadyStates();
    if (m_players.isEmpty())
        resetGame();

    broadcastRoomState();

    qInfo().noquote() << QStringLiteral("Disconnected: %1").arg(playerName);
    socket->deleteLater();
}

void ServerApp::processMessage(QTcpSocket *socket, const QJsonObject &msg)
{
    const QString type = msg.value(QStringLiteral("type")).toString();

    if (type == QStringLiteral("join")) {
        handleJoin(socket, msg.value(QStringLiteral("name")).toString());
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
    } else {
        sendError(socket, QStringLiteral("unsupported_message_type"));
    }
}

void ServerApp::handleJoin(QTcpSocket *socket, const QString &name)
{
    if (!socket)
        return;

    if (sessionForSocket(socket)) {
        sendError(socket, QStringLiteral("already_joined"));
        return;
    }

    if (m_players.size() >= 2) {
        sendError(socket, QStringLiteral("room_full"));
        socket->disconnectFromHost();
        return;
    }

    const int piece = m_players.isEmpty() ? 1 : 2;
    const int playerId = m_nextPlayerId++;

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

    if (m_gameController.isGameOver()) {
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

    if (session->piece != 1) {
        sendError(socket, QStringLiteral("only_player_one_can_start"));
        return;
    }

    if (m_players.size() != 2) {
        sendError(socket, QStringLiteral("need_two_players"));
        return;
    }

    for (const auto &player : m_players) {
        if (!player.isReady) {
            sendError(socket, QStringLiteral("players_not_ready"));
            return;
        }
    }

    resetGame();

    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("game_start");
    msg[QStringLiteral("firstPlayer")] = 1;
    broadcastJson(msg);
}

void ServerApp::handlePlacePiece(QTcpSocket *socket, int row, int col)
{
    PlayerSession *session = sessionForSocket(socket);
    if (!session) {
        sendError(socket, QStringLiteral("not_joined"));
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

    if (!m_gameController.surrender(session->piece))
        sendError(socket, QStringLiteral("invalid_surrender"));
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
        entry[QStringLiteral("isHost")] = player.piece == 1;
        entry[QStringLiteral("isReady")] = player.isReady;
        entry[QStringLiteral("piece")] = player.piece;
        players.append(entry);
    }

    for (const auto &player : m_players) {
        if (!player.socket)
            continue;

        QJsonObject msg;
        msg[QStringLiteral("type")] = QStringLiteral("room_state");
        msg[QStringLiteral("players")] = players;
        msg[QStringLiteral("yourPlayerId")] = player.playerId;
        msg[QStringLiteral("yourPiece")] = player.piece;
        msg[QStringLiteral("mode")] = QStringLiteral("dedicated_server");
        sendJson(player.socket, msg);
    }
}

void ServerApp::clearReadyStates()
{
    for (auto &player : m_players)
        player.isReady = false;
}

void ServerApp::resetGame()
{
    m_gameController.reset();
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
