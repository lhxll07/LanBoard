#pragma once

#include <QObject>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include "../game/gamecontroller.h"
#include "../game/doudizhucontroller.h"

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
        bool isReady = false;
        QPointer<QTcpSocket> socket;
    };

    void onNewConnection();
    void onReadyRead(QTcpSocket *socket);
    void onDisconnected(QTcpSocket *socket);

    void processMessage(QTcpSocket *socket, const QJsonObject &msg);
    void handleJoin(QTcpSocket *socket, const QString &name, const QString &gameId);
    void handleReady(QTcpSocket *socket, bool ready);
    void handleStartGame(QTcpSocket *socket);
    void handlePlacePiece(QTcpSocket *socket, int row, int col);
    void handleSurrender(QTcpSocket *socket);
    void handleDouDiZhuPlay(QTcpSocket *socket, const QJsonArray &cardIds);
    void handleDouDiZhuPass(QTcpSocket *socket);

    void sendJson(QTcpSocket *socket, const QJsonObject &obj);
    void sendError(QTcpSocket *socket, const QString &message);
    void broadcastJson(const QJsonObject &obj, QTcpSocket *exclude = nullptr);
    void broadcastRoomState();
    void broadcastDouDiZhuStates();
    void clearReadyStates();
    void resetGame();
    void scheduleDouDiZhuRobotTurn();

    PlayerSession *sessionForSocket(QTcpSocket *socket);
    const PlayerSession *sessionForSocket(QTcpSocket *socket) const;
    int otherPiece(int piece) const;
    int maxPlayers() const;
    int nextDouDiZhuPlayerId() const;
    bool isDouDiZhuRoom() const;

    QTcpServer m_server;
    QList<PlayerSession> m_players;
    GameController m_gameController;
    DouDiZhuController m_douDiZhuController;
    QString m_gameId = QStringLiteral("gomoku");
    int m_nextPlayerId = 1;
    bool m_douDiZhuRobotTurnPending = false;
    int m_douDiZhuRobotTurnToken = 0;
};
