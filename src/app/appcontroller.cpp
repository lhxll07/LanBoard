#include "appcontroller.h"
#include <QJsonObject>
#include <QJsonDocument>

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_roomManager(new RoomManager(this))
    , m_gameController(new GameController(this))
    , m_networkManager(new NetworkManager(this))
{
    // Local mode: room gameStarted → start game
    connect(m_roomManager, &RoomManager::gameStarted, this, [this]() {
        m_gameController->startNewGame();
        if (m_networkManager->isHost()) {
            m_networkManager->broadcastGameStarted(1);
        }
        emit navigationRequested(2); // go to game page
    });

    // Host: broadcast when game ends (via own move or remote)
    connect(m_gameController, &GameController::gameOverChanged, this, [this]() {
        if (m_gameController->isGameOver() && m_networkManager->isHost()) {
            m_networkManager->broadcastGameOver(m_gameController->winner());
        }
    });

    // Network signals
    connect(m_networkManager, &NetworkManager::joinRequested,
            this, &AppController::onJoinRequested);
    connect(m_networkManager, &NetworkManager::remoteReadyChanged,
            this, &AppController::onRemoteReadyChanged);
    connect(m_networkManager, &NetworkManager::remoteMoveReceived,
            this, &AppController::onRemoteMoveReceived);
    connect(m_networkManager, &NetworkManager::remoteSurrender,
            this, &AppController::onRemoteSurrender);
    connect(m_networkManager, &NetworkManager::remoteStartGame,
            this, &AppController::onRemoteStartGame);
    connect(m_networkManager, &NetworkManager::gameOverReceived,
            this, [this](int winner) {
        // Client received game_over from host
        m_gameController->setGameOver(winner);
    });
    connect(m_networkManager, &NetworkManager::roomStateReceived,
            this, [this](const QJsonObject &state) {
        // Client received room state from host
        QJsonArray players = state.value(QStringLiteral("players")).toArray();
        m_roomManager->reset();
        for (const auto &p : players) {
            QJsonObject obj = p.toObject();
            m_roomManager->addPlayer(
                obj.value(QStringLiteral("name")).toString(),
                obj.value(QStringLiteral("isHost")).toBool(),
                obj.value(QStringLiteral("isReady")).toBool());
        }
        emit roomReady();
    });
}

void AppController::startRoomAsHost()
{
    m_isHostMode = true;
    m_isClientMode = false;
    m_networkPlayerId = 0;
    emit modeChanged();

    // Reset room
    m_roomManager->reset();
    m_roomManager->addPlayer(QStringLiteral("lhx"), true);

    m_networkManager->startServer(44567);
    emit roomReady();
}

void AppController::joinRoom(const QString &ip, const QString &playerName)
{
    m_isHostMode = false;
    m_isClientMode = true;
    m_networkPlayerId = 1; // will be overwritten by room_state
    emit modeChanged();

    // Reset room
    m_roomManager->reset();

    m_networkManager->connectToServer(ip, 44567, playerName);
}

void AppController::onJoinRequested(const QString &name, QTcpSocket *socket)
{
    // Host side: a new player connected
    int playerId = socket->property("playerId").toInt();

    // Add player to room
    m_roomManager->addPlayer(name, false, false);

    // Send current room state to the new player
    QJsonArray players;
    auto list = m_roomManager->playerList();
    for (const auto &p : list) {
        QJsonObject obj;
        QVariantMap map = p.toMap();
        obj[QStringLiteral("name")] = map.value(QStringLiteral("name")).toString();
        obj[QStringLiteral("isHost")] = map.value(QStringLiteral("isHost")).toBool();
        obj[QStringLiteral("isReady")] = map.value(QStringLiteral("isReady")).toBool();
        players.append(obj);
    }

    QJsonObject stateMsg;
    stateMsg[QStringLiteral("type")] = QStringLiteral("room_state");
    stateMsg[QStringLiteral("players")] = players;
    stateMsg[QStringLiteral("yourPlayerId")] = playerId;

    QByteArray data = QJsonDocument(stateMsg).toJson(QJsonDocument::Compact);
    data.append('\n');
    socket->write(data);
    socket->flush();

    // Broadcast updated room state to all clients
    m_networkManager->broadcastRoomState(players);
}

void AppController::onRemoteReadyChanged(int playerId, bool ready)
{
    // Host received ready state change from a client.
    // Update the host's RoomManager and broadcast to all clients.
    int updateIndex = playerId; // first client = playerId 1 = index 1 in RoomManager

    auto list = m_roomManager->playerList();
    m_roomManager->reset();
    for (int i = 0; i < list.size(); ++i) {
        QVariantMap map = list[i].toMap();
        bool isReady = (i == updateIndex) ? ready
                       : map[QStringLiteral("isReady")].toBool();
        m_roomManager->addPlayer(
            map[QStringLiteral("name")].toString(),
            map[QStringLiteral("isHost")].toBool(),
            isReady);
    }

    broadcastCurrentRoomState();
}

void AppController::onRemoteMoveReceived(int playerId, int row, int col)
{
    // Host received a move from a client, validate and broadcast
    m_gameController->placePiece(row, col);

    if (m_gameController->isGameOver()) {
        m_networkManager->broadcastGameOver(m_gameController->winner());
    } else {
        m_networkManager->broadcastMove(playerId, row, col);
    }
}

void AppController::onRemoteSurrender(int playerId)
{
    m_gameController->surrender();
    m_networkManager->broadcastGameOver(m_gameController->winner());
}

void AppController::onRemoteStartGame()
{
    // Client received game_start from host
    m_gameController->startNewGame();
    emit navigationRequested(2); // go to game page
}

void AppController::broadcastCurrentRoomState()
{
    QJsonArray players;
    auto list = m_roomManager->playerList();
    for (const auto &p : list) {
        QJsonObject obj;
        QVariantMap map = p.toMap();
        obj[QStringLiteral("name")] = map[QStringLiteral("name")].toString();
        obj[QStringLiteral("isHost")] = map[QStringLiteral("isHost")].toBool();
        obj[QStringLiteral("isReady")] = map[QStringLiteral("isReady")].toBool();
        players.append(obj);
    }
    m_networkManager->broadcastRoomState(players);
}
