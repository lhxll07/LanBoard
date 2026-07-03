#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QList>
#include <QByteArray>

class NetworkManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isHost READ isHost NOTIFY connectionChanged)
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionChanged)
    Q_PROPERTY(int clientCount READ clientCount NOTIFY clientCountChanged)
    Q_PROPERTY(quint16 serverPort READ serverPort NOTIFY connectionChanged)
    Q_PROPERTY(QString connectedIp READ connectedIp NOTIFY connectionChanged)
    Q_PROPERTY(quint16 connectedPort READ connectedPort NOTIFY connectionChanged)
    Q_PROPERTY(QString localIp READ localIp CONSTANT)

public:
    explicit NetworkManager(QObject *parent = nullptr);

    Q_INVOKABLE void startServer(quint16 port = 44567);
    Q_INVOKABLE void connectToServer(const QString &ip, quint16 port = 44567,
                                     const QString &playerName = QString());
    Q_INVOKABLE void disconnectAll();

    Q_INVOKABLE void sendReady(bool ready);
    Q_INVOKABLE void sendPlacePiece(int row, int col);
    Q_INVOKABLE void sendSurrender();

    bool isHost() const { return m_isHost; }
    bool isConnected() const;
    int clientCount() const { return m_clients.size(); }
    quint16 serverPort() const { return m_serverPort; }
    QString connectedIp() const { return m_connectedIp; }
    quint16 connectedPort() const { return m_connectedPort; }
    QString localIp() const;

signals:
    void connectionChanged();
    void clientCountChanged();
    void serverStarted(quint16 port);
    void errorOccurred(QString message);

    // Received from remote (for AppController to process)
    void joinRequested(QString name, QTcpSocket *socket);
    void remoteReadyChanged(int playerId, bool ready);
    void remoteMoveReceived(int playerId, int row, int col);
    void remoteSurrender(int playerId);
    void remoteStartGame();
    void gameOverReceived(int winner);
    void roomStateReceived(QJsonObject state);
    void clientDisconnected(int playerId);

public slots:
    // Called by AppController to broadcast state changes
    void broadcastRoomState(const QJsonArray &players);
    void broadcastGameStarted();
    void broadcastMove(int player, int row, int col);
    void broadcastGameOver(int winner);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onClientDisconnected();
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);

private:
    void sendJson(QTcpSocket *socket, const QJsonObject &obj);
    void broadcastJson(const QJsonObject &obj, QTcpSocket *exclude = nullptr);
    void processMessage(QTcpSocket *sender, const QJsonObject &msg);

    QTcpServer *m_server = nullptr;
    QTcpSocket *m_socket = nullptr;       // client's connection to server
    QList<QTcpSocket *> m_clients;        // server's connected clients
    bool m_isHost = false;
    quint16 m_serverPort = 0;
    QString m_connectedIp;
    quint16 m_connectedPort = 0;

    // Track which player ID corresponds to which socket
    int m_nextPlayerId = 1;
};
