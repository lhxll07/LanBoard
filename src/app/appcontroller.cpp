#include "appcontroller.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QSettings>

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_roomManager(new RoomManager(this))
    , m_gameController(new GameController(this))
    , m_douDiZhuController(new DouDiZhuController(this))
    , m_networkManager(new NetworkManager(this))
{
    loadSettings();
    m_networkManager->setDiscoveryHostName(m_nickname);
    m_networkManager->setDiscoveryGameInProgress(false);

    // Local mode: room gameStarted → start game
    connect(m_roomManager, &RoomManager::gameStarted, this, [this]() {
        if (isDouDiZhuRoom()) {
            if (m_networkManager->isHost()) {
                m_networkManager->setDiscoveryGameInProgress(true);
                m_douDiZhuController->startNetworkGame(0);
                m_networkManager->broadcastGameStarted(QStringLiteral("doudizhu"));
                broadcastDouDiZhuStates();
                emit navigationRequested(4);
                return;
            }

            if (m_networkManager->isConnected()) {
                m_networkManager->sendStartGame();
                return;
            }

            m_douDiZhuController->startNewGame();
            emit navigationRequested(4);
            return;
        }

        if (m_networkManager->isHost()) {
            m_networkManager->setDiscoveryGameInProgress(true);
            m_gameController->startNewGame();
            m_activeGuestPlayerId = m_roomManager->firstGuestPlayerId();
            m_networkManager->broadcastGameStarted(QStringLiteral("gomoku"));
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
            m_roomManager->reset();
            m_roomManager->setGameId(QStringLiteral("gomoku"));
            m_roomManager->setLocalPlayerId(-1);
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
    connect(m_networkManager, &NetworkManager::remoteSeatChanged,
            this, &AppController::onRemoteSeatChanged);
    connect(m_networkManager, &NetworkManager::remoteStartGame,
            this, &AppController::onRemoteStartGame);
    connect(m_networkManager, &NetworkManager::remoteDouDiZhuPlay,
            this, &AppController::onRemoteDouDiZhuPlay);
    connect(m_networkManager, &NetworkManager::remoteDouDiZhuPass,
            this, &AppController::onRemoteDouDiZhuPass);
    connect(m_networkManager, &NetworkManager::douDiZhuStateReceived,
            this, [this](const QJsonObject &state) {
        m_douDiZhuController->applyNetworkState(state);
    });
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
        if (state.contains(QStringLiteral("gameId")))
            m_roomManager->setGameId(state.value(QStringLiteral("gameId")).toString());

        if (state.contains(QStringLiteral("yourPlayerId"))) {
            m_networkPlayerId = state.value(QStringLiteral("yourPlayerId")).toInt(-1);
            m_roomManager->setLocalPlayerId(m_networkPlayerId);
            m_douDiZhuController->setLocalPlayer(m_networkPlayerId);
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
                obj.value(QStringLiteral("playerId")).toInt(-1),
                obj.value(QStringLiteral("seatType")).toString(QStringLiteral("active")));
        }
        m_activeGuestPlayerId = m_roomManager->firstGuestPlayerId();
        emit roomReady();
    });
}

void AppController::startLocalMode()
{
    configureRoomGame(QStringLiteral("gomoku"));
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

void AppController::startGomokuLocalGame()
{
    configureRoomGame(QStringLiteral("gomoku"));
    m_isHostMode = false;
    m_isClientMode = false;
    m_networkPlayerId = 0;
    m_activeGuestPlayerId = -1;
    emit modeChanged();

    m_networkManager->disconnectAll();
    m_networkManager->setDiscoveryGameInProgress(false);
    m_roomManager->reset();
    m_roomManager->setLocalPlayerId(-1);
    m_gameController->startNewGame();
    emit navigationRequested(2);
}

void AppController::startDouDiZhuLocalMode()
{
    m_networkManager->disconnectAll();
    m_networkManager->setDiscoveryGameInProgress(false);
    m_roomManager->reset();
    m_roomManager->setGameId(QStringLiteral("gomoku"));
    m_roomManager->setLocalPlayerId(-1);
    m_douDiZhuController->startNewGame();

    m_isHostMode = false;
    m_isClientMode = false;
    m_networkPlayerId = 0;
    m_activeGuestPlayerId = -1;
    emit modeChanged();
    emit navigationRequested(4);
}

void AppController::startRoomAsHost()
{
    configureRoomGame(QStringLiteral("gomoku"));
    m_networkManager->disconnectAll();
    m_networkManager->setDiscoveryGameInProgress(false);
    m_gameController->reset();
    m_networkManager->setDiscoveryRoomInfo(m_roomManager->gameId(),
                                           m_roomManager->gameName(),
                                           roomCapacity());
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

void AppController::startDouDiZhuRoomAsHost()
{
    configureRoomGame(QStringLiteral("doudizhu"));
    m_networkManager->disconnectAll();
    m_networkManager->setDiscoveryGameInProgress(false);
    m_gameController->reset();
    m_douDiZhuController->setLocalPlayer(0);
    m_networkManager->setDiscoveryRoomInfo(m_roomManager->gameId(),
                                           m_roomManager->gameName(),
                                           roomCapacity());
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

void AppController::joinRoom(const QString &ip, int port, const QString &playerName, const QString &gameId)
{
    if (port < 1 || port > 65535)
        return;

    const QString trimmedIp = ip.trimmed();
    if (trimmedIp.isEmpty())
        return;

    const QString normalizedGameId = gameId == QStringLiteral("doudizhu")
        ? QStringLiteral("doudizhu")
        : QStringLiteral("gomoku");
    configureRoomGame(normalizedGameId);

    m_networkManager->disconnectAll();
    m_networkManager->setDiscoveryGameInProgress(false);
    m_gameController->reset();
    m_douDiZhuController->setLocalPlayer(0);

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

    m_networkManager->connectToServer(trimmedIp, m_recentJoinPort, resolvedName, normalizedGameId);
}

void AppController::leaveRoom()
{
    m_networkManager->disconnectAll();
    m_networkManager->setDiscoveryGameInProgress(false);
    m_gameController->reset();
    m_douDiZhuController->startNewGame();
    m_roomManager->reset();
    m_roomManager->setGameId(QStringLiteral("gomoku"));
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

void AppController::openDouDiZhuPage()
{
    startDouDiZhuLocalMode();
}

bool AppController::playDouDiZhuCards(const QVariantList &cardIds)
{
    if (m_networkManager->isHost()) {
        const bool ok = m_douDiZhuController->playCardsForPlayer(m_networkPlayerId, cardIds);
        broadcastDouDiZhuStates();
        return ok;
    }

    if (m_networkManager->isConnected()) {
        if (m_douDiZhuController->currentPlayer() != m_networkPlayerId)
            return false;
        m_networkManager->sendDouDiZhuPlay(cardIds);
        return true;
    }

    return m_douDiZhuController->playCards(cardIds);
}

bool AppController::passDouDiZhuTurn()
{
    if (m_networkManager->isHost()) {
        const bool ok = m_douDiZhuController->passForPlayer(m_networkPlayerId);
        broadcastDouDiZhuStates();
        return ok;
    }

    if (m_networkManager->isConnected()) {
        if (!m_douDiZhuController->canPass())
            return false;
        m_networkManager->sendDouDiZhuPass();
        return true;
    }

    return m_douDiZhuController->passTurn();
}

void AppController::restartDouDiZhuGame()
{
    if (m_networkManager->isHost()) {
        m_douDiZhuController->startNetworkGame(0);
        m_networkManager->setDiscoveryGameInProgress(true);
        broadcastDouDiZhuStates();
        return;
    }

    if (m_networkManager->isConnected())
        return;

    m_douDiZhuController->startNewGame();
}

void AppController::joinOnlineServer(const QString &gameId)
{
    joinRoom(m_onlineServerHost, m_onlineServerPort, m_nickname, gameId);
}

void AppController::refreshOnlineRooms()
{
    m_networkManager->requestOnlineRooms(m_onlineServerHost, m_onlineServerPort);
}

void AppController::createOnlineRoom(const QString &gameId, const QString &roomName)
{
    const QString normalizedGameId = gameId == QStringLiteral("doudizhu")
        ? QStringLiteral("doudizhu")
        : QStringLiteral("gomoku");

    configureRoomGame(normalizedGameId);
    m_networkManager->disconnectAll();
    m_networkManager->setDiscoveryGameInProgress(false);
    m_gameController->reset();
    m_douDiZhuController->setLocalPlayer(0);

    m_isHostMode = false;
    m_isClientMode = true;
    m_networkPlayerId = -1;
    m_activeGuestPlayerId = -1;
    emit modeChanged();

    m_roomManager->reset();
    m_roomManager->setLocalPlayerId(-1);
    m_networkManager->createOnlineRoom(m_onlineServerHost,
                                       m_onlineServerPort,
                                       m_nickname,
                                       normalizedGameId,
                                       roomName);
}

void AppController::joinOnlineRoom(const QString &roomId)
{
    if (roomId.trimmed().isEmpty())
        return;

    m_networkManager->disconnectAll();
    m_networkManager->setDiscoveryGameInProgress(false);
    m_gameController->reset();
    m_douDiZhuController->setLocalPlayer(0);

    m_isHostMode = false;
    m_isClientMode = true;
    m_networkPlayerId = -1;
    m_activeGuestPlayerId = -1;
    emit modeChanged();

    m_roomManager->reset();
    m_roomManager->setLocalPlayerId(-1);
    m_networkManager->joinOnlineRoom(m_onlineServerHost,
                                     m_onlineServerPort,
                                     m_nickname,
                                     roomId.trimmed());
}

void AppController::toggleLocalReady()
{
    m_roomManager->toggleReady();
    if (m_networkManager->isHost()) {
        broadcastCurrentRoomState();
    }
}

void AppController::switchRoomGame(const QString &gameId)
{
    const QString normalizedGameId = gameId == QStringLiteral("doudizhu")
        ? QStringLiteral("doudizhu")
        : QStringLiteral("gomoku");
    if (m_roomManager->gameId() == normalizedGameId || !m_roomManager->isHost())
        return;

    if (m_networkManager->isHost()) {
        m_networkManager->setDiscoveryGameInProgress(false);
        m_gameController->reset();
        m_douDiZhuController->startNewGame();
        m_douDiZhuController->setLocalPlayer(0);

        configureRoomGame(normalizedGameId);
        normalizeRoomSeatsForCurrentGame();
        m_roomManager->clearReadyStates();
        m_activeGuestPlayerId = m_roomManager->firstGuestPlayerId();
        broadcastCurrentRoomState();
        return;
    }

    if (m_networkManager->isConnected())
        m_networkManager->sendSwitchRoomGame(normalizedGameId);
}

void AppController::requestSeatChange(const QString &seatType)
{
    const QString normalizedSeatType = seatType == QStringLiteral("spectator")
        ? QStringLiteral("spectator")
        : QStringLiteral("active");
    if (m_roomManager->isHost())
        return;
    if (m_networkManager->isConnected()) {
        m_networkManager->sendChangeSeat(normalizedSeatType);
        return;
    }

    if (!m_networkManager->isHost())
        return;

    onRemoteSeatChanged(m_networkPlayerId, normalizedSeatType);
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

    if (m_roomManager->playerList().size() >= roomCapacity()
        || playerId >= roomCapacity()) {
        QJsonObject errorMsg;
        errorMsg[QStringLiteral("type")] = QStringLiteral("error");
        errorMsg[QStringLiteral("message")] = QStringLiteral("房间已满");
        QByteArray data = QJsonDocument(errorMsg).toJson(QJsonDocument::Compact);
        data.append('\n');
        socket->write(data);
        socket->flush();
        socket->disconnectFromHost();
        return;
    }

    // Add player to room
    const QString seatType = m_roomManager->activeGuestCount() < activeGuestLimit()
        ? QStringLiteral("active")
        : QStringLiteral("spectator");
    m_roomManager->addPlayer(name, false, false, playerId, seatType);
    m_activeGuestPlayerId = m_roomManager->firstGuestPlayerId();

    // Send current room state to the new player
    QJsonObject stateMsg;
    stateMsg[QStringLiteral("type")] = QStringLiteral("room_state");
    stateMsg[QStringLiteral("gameId")] = m_roomManager->gameId();
    stateMsg[QStringLiteral("gameName")] = m_roomManager->gameName();
    stateMsg[QStringLiteral("maxPlayers")] = m_roomManager->maxPlayers();
    stateMsg[QStringLiteral("roomCapacity")] = roomCapacity();
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

void AppController::onRemoteSeatChanged(int playerId, const QString &seatType)
{
    if (!m_networkManager->isHost())
        return;
    if (playerId <= 0)
        return;

    const QString normalizedSeatType = seatType == QStringLiteral("spectator")
        ? QStringLiteral("spectator")
        : QStringLiteral("active");
    if (normalizedSeatType == QStringLiteral("active")) {
        if (m_roomManager->activeGuestCount() >= activeGuestLimit())
            return;
    }

    if (!m_roomManager->setPlayerSeatById(playerId, normalizedSeatType))
        return;

    if (normalizedSeatType == QStringLiteral("active"))
        normalizeRoomSeatsForCurrentGame();
    m_activeGuestPlayerId = m_roomManager->firstGuestPlayerId();
    broadcastCurrentRoomState();
}

void AppController::onRemoteStartGame(const QString &gameId)
{
    if (m_networkManager->isHost())
        return;

    if (gameId == QStringLiteral("doudizhu") || isDouDiZhuRoom()) {
        m_roomManager->setGameId(QStringLiteral("doudizhu"));
        m_networkManager->setDiscoveryGameInProgress(true);
        emit navigationRequested(4);
        return;
    }

    // Client received game_start from host
    m_networkManager->setDiscoveryGameInProgress(true);
    m_gameController->startNewGame();
    emit navigationRequested(2); // go to game page
}

void AppController::onRemoteDouDiZhuPlay(int playerId, const QJsonArray &cardIds)
{
    if (!m_networkManager->isHost() || !isDouDiZhuRoom())
        return;

    QVariantList ids;
    for (const auto &id : cardIds)
        ids.append(id.toInt());

    m_douDiZhuController->playCardsForPlayer(playerId, ids);
    broadcastDouDiZhuStates();
}

void AppController::onRemoteDouDiZhuPass(int playerId)
{
    if (!m_networkManager->isHost() || !isDouDiZhuRoom())
        return;

    m_douDiZhuController->passForPlayer(playerId);
    broadcastDouDiZhuStates();
}

void AppController::onClientDisconnected(int playerId)
{
    if (!m_networkManager->isHost())
        return;
    const bool wasActiveGuest = m_roomManager->isPlayerActive(playerId)
        && playerId == m_activeGuestPlayerId;
    if (!m_roomManager->removePlayerById(playerId))
        return;

    normalizeRoomSeatsForCurrentGame();
    m_activeGuestPlayerId = m_roomManager->firstGuestPlayerId();
    if (wasActiveGuest) {
        if (!m_gameController->isGameOver())
            m_gameController->setGameOver(1);
    }

    broadcastCurrentRoomState();
}

void AppController::broadcastCurrentRoomState()
{
    m_networkManager->broadcastRoomState(currentRoomState());
}

void AppController::broadcastDouDiZhuStates()
{
    if (!m_networkManager->isHost())
        return;

    const auto players = m_roomManager->playerList();
    for (const auto &playerValue : players) {
        const QVariantMap player = playerValue.toMap();
        const int playerId = player.value(QStringLiteral("playerId"), -1).toInt();
        if (playerId <= 0 || player.value(QStringLiteral("seatType")).toString() != QStringLiteral("active"))
            continue;
        m_networkManager->sendDouDiZhuState(playerId, m_douDiZhuController->stateForPlayer(playerId));
    }

    if (m_douDiZhuController->isGameOver()) {
        m_networkManager->setDiscoveryGameInProgress(false);
        if (m_roomManager->clearReadyStates())
            broadcastCurrentRoomState();
    }
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
        const QString seatType = map.value(QStringLiteral("seatType")).toString().isEmpty()
            ? QStringLiteral("active")
            : map.value(QStringLiteral("seatType")).toString();
        obj[QStringLiteral("seatType")] = seatType;
        players.append(obj);
    }
    return players;
}

bool AppController::isDouDiZhuRoom() const
{
    return m_roomManager->gameId() == QStringLiteral("doudizhu");
}

void AppController::configureRoomGame(const QString &gameId)
{
    m_roomManager->setGameId(gameId);
    m_networkManager->setDiscoveryRoomInfo(m_roomManager->gameId(),
                                           m_roomManager->gameName(),
                                           roomCapacity());
}

void AppController::normalizeRoomSeatsForCurrentGame()
{
    const auto players = m_roomManager->playerList();
    int activeGuests = 0;
    for (const auto &playerValue : players) {
        const QVariantMap player = playerValue.toMap();
        const int playerId = player.value(QStringLiteral("playerId"), -1).toInt();
        const bool isHost = player.value(QStringLiteral("isHost")).toBool();
        if (isHost) {
            m_roomManager->setPlayerSeatById(playerId, QStringLiteral("active"));
            continue;
        }

        const QString targetSeatType = activeGuests < activeGuestLimit()
            ? QStringLiteral("active")
            : QStringLiteral("spectator");
        m_roomManager->setPlayerSeatById(playerId, targetSeatType);
        if (targetSeatType == QStringLiteral("active"))
            ++activeGuests;
    }
}

bool AppController::isGameInProgress() const
{
    return false;
}

int AppController::roomCapacity() const
{
    return m_roomManager->roomCapacity();
}

int AppController::activeGuestLimit() const
{
    return qMax(0, m_roomManager->maxPlayers() - 1);
}
