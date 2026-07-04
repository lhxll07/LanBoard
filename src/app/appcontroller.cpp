#include "appcontroller.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QSettings>

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_roomManager(new RoomManager(this))
    , m_gameController(new GameController(this))
    , m_networkManager(new NetworkManager(this))
{
    loadSettings();
    m_networkManager->setDiscoveryHostName(m_nickname);
    m_networkManager->setDiscoveryGameInProgress(false);

    // Local mode: room gameStarted → start game
    connect(m_roomManager, &RoomManager::gameStarted, this, [this]() {
        if (m_networkManager->isHost()) {
            m_networkManager->setDiscoveryGameInProgress(true);
            m_gameController->startNewGame();
            m_activeGuestPlayerId = m_roomManager->firstGuestPlayerId();
            m_networkManager->broadcastGameStarted();
            emit navigationRequested(2); // go to game page
            return;
        }

        if (m_networkManager->isConnected()) {
            m_networkManager->sendStartGame();
            return;
        }

        m_networkManager->setDiscoveryGameInProgress(true);
        m_gameController->startNewGame();
        emit navigationRequested(2); // go to game page
    });

    // End of game: synchronize result, reset ready state, and return to room.
    connect(m_gameController, &GameController::gameOverChanged, this, [this]() {
        if (!m_gameController->isGameOver())
            return;

        if (m_networkManager->isHost()) {
            m_networkManager->setDiscoveryGameInProgress(false);
            m_networkManager->broadcastGameOver(m_gameController->winner());
            m_roomManager->clearReadyStates();
            broadcastCurrentRoomState();
        } else if (!m_networkManager->isConnected()) {
            m_networkManager->setDiscoveryGameInProgress(false);
            m_roomManager->clearReadyStates();
        }

        emit navigationRequested(1);
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
    connect(m_networkManager, &NetworkManager::clientDisconnected,
            this, &AppController::onClientDisconnected);
    connect(m_networkManager, &NetworkManager::connectionChanged,
            this, [this]() {
        if (!m_isClientMode || m_networkManager->isConnected())
            return;

        m_roomManager->reset();
        m_roomManager->setLocalPlayerId(-1);
        m_isClientMode = false;
        m_networkPlayerId = 0;
        m_activeGuestPlayerId = -1;
        m_networkManager->setDiscoveryGameInProgress(false);
        emit modeChanged();
    });
    connect(m_networkManager, &NetworkManager::gameOverReceived,
            this, [this](int winner) {
        // Client received game_over from host
        m_gameController->setGameOver(winner);
    });
    connect(m_networkManager, &NetworkManager::roomStateReceived,
            this, [this](const QJsonObject &state) {
        // Client received room state from host
        if (state.contains(QStringLiteral("yourPlayerId"))) {
            m_networkPlayerId = state.value(QStringLiteral("yourPlayerId")).toInt(-1);
            m_roomManager->setLocalPlayerId(m_networkPlayerId);
            emit modeChanged();
        }

        QJsonArray players = state.value(QStringLiteral("players")).toArray();
        m_roomManager->reset();
        for (const auto &p : players) {
            QJsonObject obj = p.toObject();
            m_roomManager->addPlayer(
                obj.value(QStringLiteral("name")).toString(),
                obj.value(QStringLiteral("isHost")).toBool(),
                obj.value(QStringLiteral("isReady")).toBool(),
                obj.value(QStringLiteral("playerId")).toInt(-1));
        }
        emit roomReady();
    });
}

void AppController::startLocalMode()
{
    m_isHostMode = false;
    m_isClientMode = false;
    m_networkPlayerId = 0;
    m_activeGuestPlayerId = -1;
    emit modeChanged();
    m_networkManager->disconnectAll();
    m_networkManager->setDiscoveryGameInProgress(false);
    m_gameController->reset();
    m_roomManager->reset();
    m_roomManager->setLocalPlayerId(0);
    m_roomManager->addPlayer(m_nickname, true, false, 0);
    emit roomReady();
}

void AppController::startRoomAsHost()
{
    m_networkManager->disconnectAll();
    m_networkManager->setDiscoveryGameInProgress(false);
    m_gameController->reset();
    m_networkManager->startServer(m_defaultPort);
    if (!m_networkManager->isHost()) {
        m_isHostMode = false;
        m_isClientMode = false;
        m_networkPlayerId = 0;
        m_activeGuestPlayerId = -1;
        emit modeChanged();
        return;
    }

    m_isHostMode = true;
    m_isClientMode = false;
    m_networkPlayerId = 0;
    m_activeGuestPlayerId = -1;
    emit modeChanged();

    m_roomManager->reset();
    m_roomManager->setLocalPlayerId(0);
    m_roomManager->addPlayer(m_nickname, true, false, 0);
    emit roomReady();
}

void AppController::joinRoom(const QString &ip, int port, const QString &playerName)
{
    if (port < 1 || port > 65535)
        return;

    const QString trimmedIp = ip.trimmed();
    if (trimmedIp.isEmpty())
        return;

    m_networkManager->disconnectAll();
    m_networkManager->setDiscoveryGameInProgress(false);
    m_gameController->reset();

    m_isHostMode = false;
    m_isClientMode = true;
    m_networkPlayerId = -1;
    m_activeGuestPlayerId = -1;
    emit modeChanged();

    m_roomManager->reset();
    m_roomManager->setLocalPlayerId(-1);

    const QString resolvedName = playerName.trimmed().isEmpty() ? m_nickname : playerName.trimmed();
    m_recentJoinIp = trimmedIp;
    m_recentJoinPort = static_cast<quint16>(port);
    saveSettings();
    emit settingsChanged();

    m_networkManager->connectToServer(trimmedIp, m_recentJoinPort, resolvedName);
}

void AppController::leaveRoom()
{
    m_networkManager->disconnectAll();
    m_networkManager->setDiscoveryGameInProgress(false);
    m_gameController->reset();
    m_roomManager->reset();
    m_roomManager->setLocalPlayerId(-1);

    m_isHostMode = false;
    m_isClientMode = false;
    m_networkPlayerId = 0;
    m_activeGuestPlayerId = -1;
    emit modeChanged();
}

void AppController::openOnlinePage()
{
    emit navigationRequested(3);
}

void AppController::joinOnlineServer()
{
    joinRoom(m_onlineServerHost, m_onlineServerPort, m_nickname);
}

void AppController::toggleLocalReady()
{
    m_roomManager->toggleReady();
    if (m_networkManager->isHost()) {
        broadcastCurrentRoomState();
    }
}

bool AppController::updateNickname(const QString &nickname)
{
    const QString trimmed = nickname.trimmed();
    if (trimmed.isEmpty())
        return false;
    if (trimmed == m_nickname)
        return true;

    m_nickname = trimmed;
    m_networkManager->setDiscoveryHostName(m_nickname);
    saveSettings();
    emit settingsChanged();
    return true;
}

bool AppController::updateDefaultPort(int port)
{
    if (port < 1 || port > 65535)
        return false;

    const quint16 normalizedPort = static_cast<quint16>(port);
    if (normalizedPort == m_defaultPort)
        return true;

    m_defaultPort = normalizedPort;
    saveSettings();
    emit settingsChanged();
    return true;
}

bool AppController::updateOnlineServerEndpoint(const QString &host, int port)
{
    const QString trimmedHost = host.trimmed();
    if (trimmedHost.isEmpty() || port < 1 || port > 65535)
        return false;

    const quint16 normalizedPort = static_cast<quint16>(port);
    if (trimmedHost == m_onlineServerHost && normalizedPort == m_onlineServerPort)
        return true;

    m_onlineServerHost = trimmedHost;
    m_onlineServerPort = normalizedPort;
    saveSettings();
    emit settingsChanged();
    return true;
}

void AppController::onJoinRequested(const QString &name, QTcpSocket *socket)
{
    // Host side: a new player connected
    int playerId = socket->property("playerId").toInt();

    // Add player to room
    m_roomManager->addPlayer(name, false, false, playerId);

    // Send current room state to the new player
    QJsonObject stateMsg;
    stateMsg[QStringLiteral("type")] = QStringLiteral("room_state");
    stateMsg[QStringLiteral("players")] = currentRoomState();
    stateMsg[QStringLiteral("yourPlayerId")] = playerId;

    QByteArray data = QJsonDocument(stateMsg).toJson(QJsonDocument::Compact);
    data.append('\n');
    socket->write(data);
    socket->flush();

    // Broadcast updated room state to all clients
    broadcastCurrentRoomState();
}

void AppController::onRemoteReadyChanged(int playerId, bool ready)
{
    if (!m_networkManager->isHost())
        return;
    if (m_roomManager->setPlayerReadyById(playerId, ready))
        broadcastCurrentRoomState();
}

void AppController::onRemoteMoveReceived(int playerId, int row, int col)
{
    if (m_networkManager->isHost()) {
        if (playerId != m_activeGuestPlayerId)
            return;
        if (m_gameController->placePiece(row, col, 2)) {
            m_networkManager->broadcastMove(2, row, col);
        }
        return;
    }

    m_gameController->placePiece(row, col, playerId);
}

void AppController::onRemoteSurrender(int playerId)
{
    if (m_networkManager->isHost() && playerId == m_activeGuestPlayerId)
        m_gameController->surrender(2);
}

void AppController::onRemoteStartGame()
{
    if (m_networkManager->isHost())
        return;

    // Client received game_start from host
    m_networkManager->setDiscoveryGameInProgress(true);
    m_gameController->startNewGame();
    emit navigationRequested(2); // go to game page
}

void AppController::onClientDisconnected(int playerId)
{
    if (!m_networkManager->isHost())
        return;
    if (!m_roomManager->removePlayerById(playerId))
        return;

    if (m_activeGuestPlayerId == playerId) {
        m_activeGuestPlayerId = -1;
        if (!m_gameController->isGameOver())
            m_gameController->setGameOver(1);
    }

    broadcastCurrentRoomState();
}

void AppController::broadcastCurrentRoomState()
{
    m_networkManager->broadcastRoomState(currentRoomState());
}

void AppController::loadSettings()
{
    QSettings settings;
    m_nickname = settings.value(QStringLiteral("profile/nickname"),
                                QStringLiteral("lhx")).toString().trimmed();
    if (m_nickname.isEmpty())
        m_nickname = QStringLiteral("lhx");

    const int port = settings.value(QStringLiteral("network/defaultPort"), 44567).toInt();
    if (port >= 1 && port <= 65535)
        m_defaultPort = static_cast<quint16>(port);
    else
        m_defaultPort = 44567;

    m_recentJoinIp = settings.value(QStringLiteral("network/recentJoinIp")).toString().trimmed();

    const int recentPort = settings.value(QStringLiteral("network/recentJoinPort"), m_defaultPort).toInt();
    if (recentPort >= 1 && recentPort <= 65535)
        m_recentJoinPort = static_cast<quint16>(recentPort);
    else
        m_recentJoinPort = m_defaultPort;

    const QString onlineHost = settings.value(QStringLiteral("network/onlineServerHost"),
                                              m_onlineServerHost).toString().trimmed();
    if (!onlineHost.isEmpty())
        m_onlineServerHost = onlineHost;

    const int onlinePort = settings.value(QStringLiteral("network/onlineServerPort"),
                                          m_onlineServerPort).toInt();
    if (onlinePort >= 1 && onlinePort <= 65535)
        m_onlineServerPort = static_cast<quint16>(onlinePort);
}

void AppController::saveSettings() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("profile/nickname"), m_nickname);
    settings.setValue(QStringLiteral("network/defaultPort"), m_defaultPort);
    settings.setValue(QStringLiteral("network/recentJoinIp"), m_recentJoinIp);
    settings.setValue(QStringLiteral("network/recentJoinPort"), m_recentJoinPort);
    settings.setValue(QStringLiteral("network/onlineServerHost"), m_onlineServerHost);
    settings.setValue(QStringLiteral("network/onlineServerPort"), m_onlineServerPort);
}

QJsonArray AppController::currentRoomState() const
{
    QJsonArray players;
    const auto list = m_roomManager->playerList();
    for (const auto &player : list) {
        const QVariantMap map = player.toMap();
        QJsonObject obj;
        obj[QStringLiteral("playerId")] = map.value(QStringLiteral("playerId")).toInt();
        obj[QStringLiteral("name")] = map.value(QStringLiteral("name")).toString();
        obj[QStringLiteral("isHost")] = map.value(QStringLiteral("isHost")).toBool();
        obj[QStringLiteral("isReady")] = map.value(QStringLiteral("isReady")).toBool();
        players.append(obj);
    }
    return players;
}
