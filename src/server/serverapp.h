#pragma once

#include <QByteArray>
#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QTimer>
#include <memory>

#include <enet/enet.h>

#include "../common/types.h"
#include "../common/roomtypes.h"
#include "../game/gamecontroller.h"
#include "../game/doudizhucontroller.h"
#include "../game/flightchesscontroller.h"
#include "../game/survivorcontroller.h"
#include "../lobby/roommanager.h"

class ServerApp : public QObject
{
    Q_OBJECT

public:
    explicit ServerApp(QObject *parent = nullptr);
    ~ServerApp() override;

    bool start(quint16 port);

private:
    struct PlayerSession {
        int playerId = -1;
        QString roomId;
        ENetPeer *peer = nullptr;
    };

    struct RoomState {
        std::unique_ptr<RoomManager> roomManager;
        std::unique_ptr<GameController> gameController;
        std::unique_ptr<DouDiZhuController> douDiZhuController;
        std::unique_ptr<FlightChessController> flightChessController;
        std::unique_ptr<SurvivorController> survivorController;
    };

    void serviceNetwork();
    void handleConnect(ENetPeer *peer);
    void handleDisconnect(ENetPeer *peer);
    void handleReceive(ENetPeer *peer, ENetPacket *packet);

    void processMessage(ENetPeer *peer, const QJsonObject &msg);
    void handleJoin(ENetPeer *peer, const QString &name, const QString &gameId);
    void handleListRooms(ENetPeer *peer);
    void handleCreateRoom(ENetPeer *peer, const QString &name, const QString &roomName,
                          const QString &gameId);
    void handleJoinRoom(ENetPeer *peer, const QString &name, const QString &roomId);
    void handleReady(ENetPeer *peer, bool ready);
    void handleStartGame(ENetPeer *peer);
    void handleChangeSeat(ENetPeer *peer, const QString &seatType);
    void handleSwitchRoomGame(ENetPeer *peer, const QString &gameId);
    void handlePlacePiece(ENetPeer *peer, int row, int col);
    void handleFlightRoll(ENetPeer *peer);
    void handleFlightMove(ENetPeer *peer, int planeIndex);
    void handleSurrender(ENetPeer *peer);
    void handleSurvivorInput(ENetPeer *peer, qreal horizontal, qreal vertical);
    void handleSurvivorChooseLevelUp(ENetPeer *peer, const QString &upgradeId);
    void handleSurvivorCloseChest(ENetPeer *peer);
    void handleGameOver(ENetPeer *peer, int winner);
    void handleDouDiZhuPlay(ENetPeer *peer, const QJsonArray &cardIds);
    void handleDouDiZhuPass(ENetPeer *peer);

    void sendJson(ENetPeer *peer, const QJsonObject &obj);
    void sendRaw(ENetPeer *peer, const QByteArray &payload, enet_uint8 channel, enet_uint32 flags);
    void sendError(ENetPeer *peer, const QString &message);
    void broadcastJsonToRoom(const QString &roomId, const QJsonObject &obj, ENetPeer *exclude = nullptr);
    bool processBinaryPacket(ENetPeer *peer, const QByteArray &payload);
    void broadcastRoomState(RoomState *room);
    void broadcastDouDiZhuStates(RoomState *room);
    void startRoomGame(RoomState *room, const QList<PlayerSession *> &activePlayers);
    void handlePlayerDisconnectInRoom(RoomState *room,
                                      const PlayerSession &session);
    void resetGame(RoomState *room);
    bool isGameFinished(const RoomState *room) const;
    void concludeRoomGame(RoomState *room, int winner, bool broadcastRoomStateAfterward = true);
    void resetFinishedRoom(RoomState *room);
    void removeRoomIfEmpty(const QString &roomId);
    QJsonArray roomListPayload() const;
    bool resolveSessionRoom(ENetPeer *peer, PlayerSession *&session, RoomState *&room);
    bool ensureControllerKind(ENetPeer *peer,
                              const RoomState *room,
                              LanBoard::GameControllerKind expectedKind);
    bool ensureActiveSeat(ENetPeer *peer,
                          const PlayerSession *session,
                          const char *errorKey = "spectator_cannot_move");
    bool ensureGameStarted(ENetPeer *peer, const RoomState *room);
    bool ensureSurvivorInteractionOwner(ENetPeer *peer,
                                        const RoomState *room,
                                        int playerId);

    PlayerSession *sessionForPeer(ENetPeer *peer);
    const PlayerSession *sessionForPeer(ENetPeer *peer) const;
    RoomState *roomForPlayer(const PlayerSession *session);
    const RoomState *roomForPlayer(const PlayerSession *session) const;
    RoomState *roomById(const QString &roomId);
    const RoomState *roomById(const QString &roomId) const;
    QList<PlayerSession *> playersInRoom(const QString &roomId);
    QList<const PlayerSession *> playersInRoom(const QString &roomId) const;
    QList<PlayerSession *> activePlayersInRoom(const QString &roomId);
    QList<const PlayerSession *> activePlayersInRoom(const QString &roomId) const;
    QString hostNameForRoom(const QString &roomId) const;
    QString createRoomId() const;
    int otherPiece(int piece) const;
    int roomCapacity() const;

    ENetHost *m_host = nullptr;
    QTimer m_serviceTimer;
    QList<PlayerSession> m_players;
    QList<RoomState *> m_rooms;
};
