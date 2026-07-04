#pragma once

#include <QObject>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <memory>

#include "../game/gamecontroller.h"
#include "../game/doudizhucontroller.h"
#include "../game/flightchesscontroller.h"

class ServerApp : public QObject
{
    Q_OBJECT

public:
    explicit ServerApp(QObject *parent = nullptr);

    bool start(quint16 port);

private:
    struct PlayerSession {
        int playerId = -1;
        int piece = 0;
        QString name;
        QString roomId;
        bool isHost = false;
        bool isReady = false;
        QString seatType = QStringLiteral("active");
        QPointer<QTcpSocket> socket;
    };

    struct RoomState {
        QString roomId;
        QString roomName;
        QString gameId = QStringLiteral("gomoku");
        bool gameActive = false;
        std::unique_ptr<GameController> gameController;
        std::unique_ptr<DouDiZhuController> douDiZhuController;
        std::unique_ptr<FlightChessController> flightChessController;
    };

    void onNewConnection();
    void onReadyRead(QTcpSocket *socket);
    void onDisconnected(QTcpSocket *socket);

    void processMessage(QTcpSocket *socket, const QJsonObject &msg);
    void handleJoin(QTcpSocket *socket, const QString &name, const QString &gameId);
    void handleListRooms(QTcpSocket *socket);
    void handleCreateRoom(QTcpSocket *socket, const QString &name, const QString &roomName,
                          const QString &gameId);
    void handleJoinRoom(QTcpSocket *socket, const QString &name, const QString &roomId);
    void handleReady(QTcpSocket *socket, bool ready);
    void handleStartGame(QTcpSocket *socket);
    void handleChangeSeat(QTcpSocket *socket, const QString &seatType);
    void handleSwitchRoomGame(QTcpSocket *socket, const QString &gameId);
    void handlePlacePiece(QTcpSocket *socket, int row, int col);
    void handleFlightRoll(QTcpSocket *socket);
    void handleFlightMove(QTcpSocket *socket, int planeIndex);
    void handleSurrender(QTcpSocket *socket);
    void handleDouDiZhuPlay(QTcpSocket *socket, const QJsonArray &cardIds);
    void handleDouDiZhuPass(QTcpSocket *socket);

    void sendJson(QTcpSocket *socket, const QJsonObject &obj);
    void sendError(QTcpSocket *socket, const QString &message);
    void broadcastJsonToRoom(const QString &roomId, const QJsonObject &obj, QTcpSocket *exclude = nullptr);
    void broadcastRoomState(RoomState *room);
    void broadcastDouDiZhuStates(RoomState *room);
    void clearReadyStates(RoomState *room);
    void resetGame(RoomState *room);
    void removeRoomIfEmpty(const QString &roomId);
    QJsonArray roomListPayload() const;

    PlayerSession *sessionForSocket(QTcpSocket *socket);
    const PlayerSession *sessionForSocket(QTcpSocket *socket) const;
    RoomState *roomForPlayer(const PlayerSession *session);
    const RoomState *roomForPlayer(const PlayerSession *session) const;
    RoomState *roomById(const QString &roomId);
    const RoomState *roomById(const QString &roomId) const;
    QList<PlayerSession *> playersInRoom(const QString &roomId);
    QList<const PlayerSession *> playersInRoom(const QString &roomId) const;
    QList<PlayerSession *> activePlayersInRoom(const QString &roomId);
    QList<const PlayerSession *> activePlayersInRoom(const QString &roomId) const;
    QString hostNameForRoom(const QString &roomId) const;
    int nextPlayerIdForRoom(const QString &roomId, const QString &gameId) const;
    QString createRoomId() const;
    int otherPiece(int piece) const;
    int roomCapacity() const;
    int activeGuestLimit(const QString &gameId) const;
    void normalizeSeats(RoomState *room, bool fillMissingActiveSeats = true);
    int maxPlayers(const QString &gameId) const;
    bool isDouDiZhuRoom(const QString &gameId) const;
    bool isFlightChessRoom(const QString &gameId) const;

    QTcpServer m_server;
    QList<PlayerSession> m_players;
    QList<RoomState *> m_rooms;
};
