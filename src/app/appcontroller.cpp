#include "appcontroller.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QJsonObject>
#include <QJsonDocument>
#include <QSettings>
#include <QTimer>

#include "src/common/roomtypes.h"
#include "src/network/protocolids.h"
#include "src/game/doudizhucontroller.h"
#include "src/game/flightchesscontroller.h"
#include "src/game/gamecontroller.h"
#include "src/game/survivorcontroller.h"
#include "src/lobby/roommanager.h"
#include "src/network/networkmanager.h"

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_roomManager(new RoomManager(this))
    , m_gameController(new GameController(this))
    , m_douDiZhuController(new DouDiZhuController(this))
    , m_flightChessController(new FlightChessController(this))
    , m_survivorController(new SurvivorController(this))
    , m_networkManager(new NetworkManager(this))
{
    loadSettings();
    m_networkManager->setDiscoveryHostName(m_nickname);
    m_networkManager->setDiscoveryGameInProgress(false);

    // Local mode: room gameStarted → start game
    connect(m_roomManager, &RoomManager::gameStarted, this, [this]() {
        startCurrentGameSession();
    });

    // End of game: synchronize result, reset ready state, and return to room.
    connect(m_gameController, &GameController::gameOverChanged, this, [this]() {
        if (!m_gameController->isGameOver())
            return;

        finishCurrentGameSession(m_gameController->winner(), true);
    });

    connect(m_flightChessController, &FlightChessController::gameOverChanged, this, [this]() {
        if (!m_flightChessController->isGameOver())
            return;

        finishCurrentGameSession(m_flightChessController->winner(), false);
    });

    connect(m_survivorController, &SurvivorController::gameOverChanged, this, [this]() {
        if (!m_survivorController->isGameOver()
            || !isCurrentGame(LanBoard::GameControllerKind::Survivor)) {
            return;
        }

        if (!m_isDedicatedServerRoom
            && m_roomManager->isHost()
            && (m_networkManager->isHost() || m_networkManager->isConnected())) {
            m_networkManager->sendGameOverResult(0);
        }
        if (m_networkManager->isHost() || m_networkManager->isConnected())
            finishCurrentGameSession(0, false);
    });

    // Network signals
    connect(m_networkManager, &NetworkManager::joinRequested,
            this, &AppController::onJoinRequested);
    connect(m_networkManager, &NetworkManager::remoteReadyChanged,
            this, &AppController::onRemoteReadyChanged);
    connect(m_networkManager, &NetworkManager::remoteMoveReceived,
            this, &AppController::onRemoteMoveReceived);
    connect(m_networkManager, &NetworkManager::remoteFlightRoll,
            this, &AppController::onRemoteFlightRoll);
    connect(m_networkManager, &NetworkManager::remoteFlightMove,
            this, &AppController::onRemoteFlightMove);
    connect(m_networkManager, &NetworkManager::remoteSurrender,
            this, &AppController::onRemoteSurrender);
    connect(m_networkManager, &NetworkManager::remoteSurvivorInput,
            this, [this](int playerId, qreal horizontal, qreal vertical) {
        if (!isCurrentGame(LanBoard::GameControllerKind::Survivor) || !m_networkManager->isHost())
            return;
        m_survivorController->setRemoteMoveInput(playerId, horizontal, vertical);
    });
    connect(m_networkManager, &NetworkManager::remoteSurvivorChooseLevelUp,
            this, [this](int playerId, const QString &upgradeId) {
        if (!isCurrentGame(LanBoard::GameControllerKind::Survivor) || !m_networkManager->isHost())
            return;
        if (m_survivorController->interactionPlayerId() != playerId)
            return;
        m_survivorController->chooseLevelUp(upgradeId);
    });
    connect(m_networkManager, &NetworkManager::remoteSurvivorCloseChest,
            this, [this](int playerId) {
        if (!isCurrentGame(LanBoard::GameControllerKind::Survivor) || !m_networkManager->isHost())
            return;
        if (m_survivorController->interactionPlayerId() != playerId)
            return;
        m_survivorController->closeChestRewards();
    });
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
    connect(m_networkManager, &NetworkManager::survivorFastPacketReceived,
            this, [this](const QByteArray &payload) {
        if (!isCurrentGame(LanBoard::GameControllerKind::Survivor))
            return;
        m_survivorController->applyFastNetworkPacket(payload);
    });
    connect(m_networkManager, &NetworkManager::survivorHudPacketReceived,
            this, [this](const QByteArray &payload) {
        if (!isCurrentGame(LanBoard::GameControllerKind::Survivor))
            return;
        m_survivorController->applyHudNetworkPacket(payload);
    });
    connect(m_networkManager, &NetworkManager::clientDisconnected,
            this, &AppController::onClientDisconnected);
    connect(m_survivorController, &SurvivorController::localInputChanged,
            this, [this](qreal horizontal, qreal vertical) {
        if (!isCurrentGame(LanBoard::GameControllerKind::Survivor))
            return;
        if (!m_networkManager->isConnected() || m_networkManager->isHost())
            return;
        m_networkManager->sendSurvivorInput(horizontal, vertical);
    });
    connect(m_survivorController, &SurvivorController::levelUpChoiceRequested,
            this, [this](const QString &upgradeId) {
        if (!isCurrentGame(LanBoard::GameControllerKind::Survivor) || !m_networkManager->isConnected())
            return;
        m_networkManager->sendSurvivorChooseLevelUp(upgradeId);
    });
    connect(m_survivorController, &SurvivorController::chestRewardsCloseRequested,
            this, [this]() {
        if (!isCurrentGame(LanBoard::GameControllerKind::Survivor) || !m_networkManager->isConnected())
            return;
        m_networkManager->sendSurvivorCloseChest();
    });
    connect(m_survivorController, &SurvivorController::networkSyncRequested,
            this, [this](bool includeHudDetails) {
        if (!isCurrentGame(LanBoard::GameControllerKind::Survivor) || !m_networkManager->isHost())
            return;

        const LanBoard::RoomSnapshot room = currentRoomSnapshot();
        for (const LanBoard::RoomPlayerState &player : room.activePlayers()) {
            if (player.playerId < 0 || player.playerId == m_networkPlayerId)
                continue;

            m_networkManager->sendSurvivorFastPacketToPlayer(
                player.playerId,
                m_survivorController->buildFastNetworkPacket(player.playerId));
            if (includeHudDetails) {
                m_networkManager->sendSurvivorHudPacketToPlayer(
                    player.playerId,
                    m_survivorController->buildHudNetworkPacket(player.playerId));
            }
        }
    });
    connect(m_networkManager, &NetworkManager::connectionChanged,
            this, [this]() {
        if (!m_isClientMode
            || m_networkManager->isConnected()
            || m_networkManager->connectionPending()) {
            return;
        }

        m_isDedicatedServerRoom = false;
        resetRoomSession(QStringLiteral("gomoku"));
        setModeState(false, false, 0);
        m_activeGuestPlayerId = -1;
        m_networkManager->setDiscoveryGameInProgress(false);
        emit navigationRequested(static_cast<int>(LanBoard::NavigationPage::Room));
    });
    connect(m_networkManager, &NetworkManager::gameOverReceived,
            this, [this](int winner) {
        applyReceivedGameOver(winner);
    });
    connect(m_networkManager, &NetworkManager::flightRollReceived,
            this, [this](int player, int diceValue) {
        if (!isCurrentGame(LanBoard::GameControllerKind::FlightChess)
            || player != m_flightChessController->currentPlayer()) {
            return;
        }
        m_flightChessController->setDiceValue(diceValue);
    });
    connect(m_networkManager, &NetworkManager::flightMoveReceived,
            this, [this](int player, int planeIndex) {
        if (!isCurrentGame(LanBoard::GameControllerKind::FlightChess)
            || player != m_flightChessController->currentPlayer()) {
            return;
        }
        m_flightChessController->movePlane(planeIndex);
    });
    connect(m_networkManager, &NetworkManager::roomStateReceived,
            this, [this](const QJsonObject &state) {
        // Client received room state from host
        if (state.contains(QStringLiteral("gameId")))
            m_roomManager->setGameId(state.value(QStringLiteral("gameId")).toString());
        m_isDedicatedServerRoom = state.value(QStringLiteral("mode")).toString()
            == QStringLiteral("dedicated_server");

        if (state.contains(QStringLiteral("yourPlayerId"))) {
            m_networkPlayerId = state.value(QStringLiteral("yourPlayerId")).toInt(-1);
            m_roomManager->setLocalPlayerId(m_networkPlayerId);
            m_douDiZhuController->setLocalPlayer(m_networkPlayerId);
            emit modeChanged();
        }

        QJsonArray players = state.value(QStringLiteral("players")).toArray();
        m_roomManager->reset();
        for (const auto &p : players) {
            const LanBoard::RoomPlayerState player = LanBoard::RoomPlayerState::fromJsonObject(p.toObject());
            m_roomManager->addPlayer(player.name,
                                     player.isHost,
                                     player.isReady,
                                     player.playerId,
                                     player.seatType());
        }
        syncActiveGuestPlayerId();
        emit roomReady();
    });
}

void AppController::startLocalGame(const QString &gameId)
{
    if (LanBoard::normalizeGameId(gameId) == QStringLiteral("survivor")) {
        startSoloSurvivorSession();
        return;
    }

    m_networkManager->disconnectAll();
    m_isDedicatedServerRoom = false;
    setModeState(false, false, 0);
    m_activeGuestPlayerId = -1;
    setLobbyGameId(gameId);
    resetRoomSession(gameId);
    startCurrentGameRuntime();
    navigateToCurrentGame();
}

void AppController::startSoloSurvivorSession()
{
    m_networkManager->disconnectAll();
    m_isDedicatedServerRoom = false;
    setModeState(false, false, 0);
    m_activeGuestPlayerId = -1;
    setLobbyGameId(QStringLiteral("survivor"));
    resetRoomSession(QStringLiteral("survivor"), 0);
    m_roomManager->addPlayer(m_nickname, true, false, 0, QStringLiteral("active"));
    syncActiveGuestPlayerId();
    startCurrentGameRuntime();
    navigateToCurrentGame();
}

void AppController::startRoomAsHost(const QString &gameId)
{
    m_networkManager->disconnectAll();
    m_isDedicatedServerRoom = false;
    resetGameControllers();
    setLobbyGameId(gameId);
    configureRoomGame(gameId);
    m_networkManager->startServer(m_defaultPort);
    if (!m_networkManager->isHost()) {
        setModeState(false, false, 0);
        m_activeGuestPlayerId = -1;
        return;
    }

    setModeState(true, false, 0);
    m_activeGuestPlayerId = -1;

    m_roomManager->reset();
    m_roomManager->setLocalPlayerId(0);
    m_roomManager->addPlayer(m_nickname, true, false, 0);
    syncActiveGuestPlayerId();
    emit roomReady();
}

void AppController::joinRoom(const QString &ip, int port, const QString &playerName, const QString &gameId)
{
    if (port < 1 || port > 65535)
        return;

    const QString trimmedIp = ip.trimmed();
    if (trimmedIp.isEmpty())
        return;

    const QString normalizedGameId = LanBoard::normalizeGameId(gameId);
    setLobbyGameId(normalizedGameId);
    m_networkManager->disconnectAll();
    m_isDedicatedServerRoom = false;
    resetRoomSession(normalizedGameId);
    setModeState(false, true, -1);
    m_activeGuestPlayerId = -1;

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
    m_isDedicatedServerRoom = false;
    resetRoomSession(QStringLiteral("gomoku"));
    setModeState(false, false, 0);
    m_activeGuestPlayerId = -1;
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

void AppController::refreshOnlineRooms()
{
    m_networkManager->requestOnlineRooms(m_onlineServerHost, m_onlineServerPort);
}

void AppController::createOnlineRoom(const QString &gameId, const QString &roomName)
{
    const QString normalizedGameId = LanBoard::normalizeGameId(gameId);
    setLobbyGameId(normalizedGameId);

    m_networkManager->disconnectAll();
    m_isDedicatedServerRoom = false;
    resetRoomSession(normalizedGameId);
    setModeState(false, true, -1);
    m_activeGuestPlayerId = -1;
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
    m_isDedicatedServerRoom = false;
    resetRoomSession(currentGameId());
    setModeState(false, true, -1);
    m_activeGuestPlayerId = -1;
    m_networkManager->joinOnlineRoom(m_onlineServerHost,
                                     m_onlineServerPort,
                                     m_nickname,
                                     roomId.trimmed());
}

void AppController::openLobbyForGame(const QString &gameId)
{
    setLobbyGameId(gameId);
    emit navigationRequested(static_cast<int>(LanBoard::NavigationPage::Room));
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
    const QString normalizedGameId = LanBoard::normalizeGameId(gameId);
    if (m_roomManager->gameId() == normalizedGameId || !m_roomManager->isHost())
        return;

    if (m_networkManager->isHost()) {
        resetGameControllers();
        configureRoomGame(normalizedGameId);
        normalizeRoomSeatsForCurrentGame();
        m_roomManager->clearReadyStates();
        syncActiveGuestPlayerId();
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

bool AppController::copyText(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return false;

    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard)
        return false;

    clipboard->setText(trimmed);
    return true;
}

void AppController::onJoinRequested(const QString &name, int playerId)
{
    // Host side: a new player connected
    if (currentRoomSnapshot().players.size() >= roomCapacity()
        || playerId >= roomCapacity()) {
        m_networkManager->sendRoomStateToPlayer(playerId, QJsonObject {
            {QStringLiteral("type"), LanBoard::Protocol::Error},
            {QStringLiteral("message"), QStringLiteral("房间已满")}
        });
        return;
    }

    // Add player to room
    const QString seatType = LanBoard::seatTypeString(
        m_roomManager->activeGuestCount()
            < LanBoard::activeGuestLimitForGame(m_roomManager->gameId())
            ? LanBoard::SeatKind::Active
            : LanBoard::SeatKind::Spectator);
    m_roomManager->addPlayer(name, false, false, playerId, seatType);
    syncActiveGuestPlayerId();

    // Send current room state to the new player
    QJsonObject stateMsg;
    stateMsg[QStringLiteral("type")] = LanBoard::Protocol::RoomState;
    stateMsg[QStringLiteral("gameId")] = m_roomManager->gameId();
    stateMsg[QStringLiteral("gameName")] = m_roomManager->gameName();
    stateMsg[QStringLiteral("maxPlayers")] = m_roomManager->maxPlayers();
    stateMsg[QStringLiteral("roomCapacity")] = roomCapacity();
    stateMsg[QStringLiteral("players")] = currentRoomState();
    stateMsg[QStringLiteral("yourPlayerId")] = playerId;
    m_networkManager->sendRoomStateToPlayer(playerId, stateMsg);

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
    if (currentControllerKind() != LanBoard::GameControllerKind::Gomoku)
        return;

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

void AppController::onRemoteFlightRoll(int playerId)
{
    if (!m_networkManager->isHost()
        || !isCurrentGame(LanBoard::GameControllerKind::FlightChess)) {
        return;
    }
    if (playerId != m_activeGuestPlayerId)
        return;
    if (m_flightChessController->currentPlayer() != 2 || m_flightChessController->hasRolled())
        return;

    const int diceValue = m_flightChessController->rollDice();
    if (diceValue > 0)
        m_networkManager->broadcastFlightRoll(2, diceValue);
}

void AppController::onRemoteFlightMove(int playerId, int planeIndex)
{
    if (!m_networkManager->isHost()
        || !isCurrentGame(LanBoard::GameControllerKind::FlightChess)) {
        return;
    }
    if (playerId != m_activeGuestPlayerId)
        return;
    if (m_flightChessController->currentPlayer() != 2)
        return;

    if (m_flightChessController->movePlane(planeIndex))
        m_networkManager->broadcastFlightMove(2, planeIndex);
}

void AppController::onRemoteSurrender(int playerId)
{
    if (!m_networkManager->isHost())
        return;

    if (currentControllerKind() == LanBoard::GameControllerKind::FlightChess
        && playerId == m_activeGuestPlayerId) {
        m_flightChessController->setGameOver(1);
        return;
    }

    if (currentControllerKind() == LanBoard::GameControllerKind::Gomoku
        && playerId == m_activeGuestPlayerId) {
        m_gameController->surrender(2);
    }
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
        if (m_roomManager->activeGuestCount()
            >= LanBoard::activeGuestLimitForGame(m_roomManager->gameId())) {
            return;
        }
    }

    if (!m_roomManager->setPlayerSeatById(playerId, normalizedSeatType))
        return;

    syncActiveGuestPlayerId();
    broadcastCurrentRoomState();
}

void AppController::onRemoteStartGame(const QString &gameId)
{
    if (m_networkManager->isHost())
        return;

    const QString targetGameId = gameId.isEmpty() ? currentGameId() : gameId;
    configureRoomGame(targetGameId);
    startCurrentGameRuntime();
    m_networkManager->setDiscoveryGameInProgress(true);
    navigateToCurrentGame();
}

void AppController::onRemoteDouDiZhuPlay(int playerId, const QJsonArray &cardIds)
{
    if (!m_networkManager->isHost()
        || !isCurrentGame(LanBoard::GameControllerKind::DouDiZhu)) {
        return;
    }

    QVariantList ids;
    for (const auto &id : cardIds)
        ids.append(id.toInt());

    m_douDiZhuController->playCardsForPlayer(playerId, ids);
    broadcastDouDiZhuStates();
}

void AppController::onRemoteDouDiZhuPass(int playerId)
{
    if (!m_networkManager->isHost()
        || !isCurrentGame(LanBoard::GameControllerKind::DouDiZhu)) {
        return;
    }

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
    syncActiveGuestPlayerId();
    if (wasActiveGuest)
        handleActiveGuestDisconnectInCurrentGame();

    broadcastCurrentRoomState();
}

void AppController::setModeState(bool hostMode, bool clientMode, int playerId)
{
    if (m_isHostMode == hostMode
        && m_isClientMode == clientMode
        && m_networkPlayerId == playerId) {
        return;
    }

    m_isHostMode = hostMode;
    m_isClientMode = clientMode;
    m_networkPlayerId = playerId;
    emit modeChanged();
}

void AppController::syncActiveGuestPlayerId()
{
    m_activeGuestPlayerId = m_roomManager->firstGuestPlayerId();
}

void AppController::applyReceivedGameOver(int winner)
{
    switch (currentControllerKind()) {
    case LanBoard::GameControllerKind::Survivor:
        finishCurrentGameSession(winner, false);
        return;
    case LanBoard::GameControllerKind::FlightChess:
        m_flightChessController->setGameOver(winner);
        return;
    case LanBoard::GameControllerKind::Gomoku:
        m_gameController->setGameOver(winner);
        return;
    case LanBoard::GameControllerKind::DouDiZhu:
    default:
        return;
    }
}

void AppController::handleActiveGuestDisconnectInCurrentGame()
{
    switch (currentControllerKind()) {
    case LanBoard::GameControllerKind::FlightChess:
        if (!m_flightChessController->isGameOver())
            m_flightChessController->setGameOver(1);
        return;
    case LanBoard::GameControllerKind::Gomoku:
        if (!m_gameController->isGameOver())
            m_gameController->setGameOver(1);
        return;
    case LanBoard::GameControllerKind::DouDiZhu:
    case LanBoard::GameControllerKind::Survivor:
    default:
        return;
    }
}

void AppController::resetGameControllers()
{
    m_networkManager->setDiscoveryGameInProgress(false);
    m_gameController->reset();
    m_douDiZhuController->reset();
    m_flightChessController->reset();
    m_survivorController->configureNetworkSession({}, 0, false, true);
    m_survivorController->stopRun();
}

void AppController::resetRoomSession(const QString &gameId, int localPlayerId)
{
    resetGameControllers();
    m_roomManager->reset();
    configureRoomGame(gameId);
    m_roomManager->setLocalPlayerId(localPlayerId);
    syncActiveGuestPlayerId();
}

void AppController::startCurrentGameSession()
{
    const bool hostMode = m_networkManager->isHost();
    if (!hostMode && m_networkManager->isConnected()) {
        m_networkManager->sendStartGame();
        return;
    }

    if (hostMode || !isCurrentGame(LanBoard::GameControllerKind::DouDiZhu)) {
        m_networkManager->setDiscoveryGameInProgress(true);
    }

    startCurrentGameRuntime(true);
    if (hostMode) {
        syncActiveGuestPlayerId();
        m_networkManager->broadcastGameStarted(currentGameId());
        if (isCurrentGame(LanBoard::GameControllerKind::DouDiZhu))
            broadcastDouDiZhuStates();
    }

    navigateToCurrentGame();
}

void AppController::finishCurrentGameSession(int winner, bool resetOfflineRoom)
{
    if (m_networkManager->isHost()) {
        m_networkManager->setDiscoveryGameInProgress(false);
        QTimer::singleShot(0, this, [this, winner]() {
            if (!m_networkManager->isHost())
                return;
            m_networkManager->broadcastGameOver(winner);
            m_roomManager->clearReadyStates();
            broadcastCurrentRoomState();
        });
    } else if (!m_networkManager->isConnected()) {
        m_networkManager->setDiscoveryGameInProgress(false);
        if (resetOfflineRoom) {
            m_roomManager->reset();
            configureRoomGame(QStringLiteral("gomoku"));
            m_roomManager->setLocalPlayerId(-1);
        } else {
            m_roomManager->clearReadyStates();
        }
    }

    emit navigationRequested(static_cast<int>(LanBoard::NavigationPage::Room));
}

void AppController::startCurrentGameRuntime(bool waitForRemoteState)
{
    switch (currentControllerKind()) {
    case LanBoard::GameControllerKind::Survivor: {
        const bool networked = m_networkManager->isConnected() || m_networkManager->isHost();
        const bool authoritative = !networked
            || (m_roomManager->isHost() && !m_isDedicatedServerRoom);
        const LanBoard::RoomSnapshot room = currentRoomSnapshot();
        const QVariantList activePlayers = room.activePlayerVariantList();
        const int localPlayerId = networked
            ? m_networkPlayerId
            : (room.localPlayerId >= 0 ? room.localPlayerId : 0);
        m_survivorController->configureNetworkSession(activePlayers,
                                                     localPlayerId,
                                                     networked,
                                                     authoritative);
        m_survivorController->startRun(networked);
        return;
    }
    case LanBoard::GameControllerKind::DouDiZhu:
        if (waitForRemoteState && m_networkManager->isConnected() && !m_networkManager->isHost())
            return;

        if (m_networkManager->isHost()) {
            m_douDiZhuController->startNetworkGame(0);
        } else {
            m_douDiZhuController->startNewGame();
        }
        return;
    case LanBoard::GameControllerKind::FlightChess:
        m_flightChessController->startNewGame();
        return;
    case LanBoard::GameControllerKind::Gomoku:
    default:
        m_gameController->startNewGame();
        return;
    }
}

GameControllerBase *AppController::activeController() const
{
    switch (currentControllerKind()) {
    case LanBoard::GameControllerKind::Gomoku:     return m_gameController;
    case LanBoard::GameControllerKind::DouDiZhu:   return m_douDiZhuController;
    case LanBoard::GameControllerKind::FlightChess: return m_flightChessController;
    case LanBoard::GameControllerKind::Survivor:   return m_survivorController;
    }
    return nullptr;
}

void AppController::broadcastCurrentRoomState()
{
    m_networkManager->broadcastRoomState(currentRoomState());
}

void AppController::broadcastDouDiZhuStates()
{
    if (!m_networkManager->isHost())
        return;

    const LanBoard::RoomSnapshot room = currentRoomSnapshot();
    for (const LanBoard::RoomPlayerState &player : room.activePlayers()) {
        if (player.playerId <= 0)
            continue;
        m_networkManager->sendDouDiZhuState(player.playerId,
                                            m_douDiZhuController->stateForPlayer(player.playerId));
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
    return currentRoomSnapshot().playerJsonArray();
}

LanBoard::RoomSnapshot AppController::currentRoomSnapshot() const
{
    return m_roomManager->snapshot();
}

QString AppController::currentGameId() const
{
    return LanBoard::normalizeGameId(m_roomManager->gameId());
}

int AppController::currentGamePage() const
{
    return LanBoard::navigationPageForGame(currentGameId());
}

void AppController::setLobbyGameId(const QString &gameId)
{
    const QString normalizedGameId = LanBoard::normalizeGameId(gameId);
    if (m_lobbyGameId == normalizedGameId)
        return;

    m_lobbyGameId = normalizedGameId;
    emit lobbyGameChanged();
}

bool AppController::isCurrentGame(LanBoard::GameControllerKind kind) const
{
    return currentControllerKind() == kind;
}

LanBoard::GameControllerKind AppController::currentControllerKind() const
{
    return LanBoard::controllerKindForGame(m_roomManager->gameId());
}

void AppController::navigateToCurrentGame()
{
    emit navigationRequested(currentGamePage());
}

void AppController::configureRoomGame(const QString &gameId)
{
    m_roomManager->setGameId(LanBoard::normalizeGameId(gameId));
    m_networkManager->setDiscoveryRoomInfo(m_roomManager->gameId(),
                                           m_roomManager->gameName(),
                                           roomCapacity(),
                                           m_roomManager->maxPlayers());
}

void AppController::normalizeRoomSeatsForCurrentGame()
{
    const LanBoard::RoomSnapshot room = currentRoomSnapshot();
    int activeGuests = 0;
    for (const LanBoard::RoomPlayerState &player : room.players) {
        if (player.isHost) {
            m_roomManager->setPlayerSeatById(player.playerId, QStringLiteral("active"));
            continue;
        }

        const QString targetSeatType = activeGuests
            < LanBoard::activeGuestLimitForGame(m_roomManager->gameId())
            ? QStringLiteral("active")
            : QStringLiteral("spectator");
        m_roomManager->setPlayerSeatById(player.playerId, targetSeatType);
        if (targetSeatType == QStringLiteral("active"))
            ++activeGuests;
    }
}

int AppController::roomCapacity() const
{
    return m_roomManager->roomCapacity();
}
