#include <QCoreApplication>
#include <QJsonObject>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUdpSocket>

#include "src/app/appcontroller.h"
#include "src/network/roomdiscoveryservice.h"

namespace {

quint16 availableUdpPort()
{
    QUdpSocket socket;
    if (!socket.bind(QHostAddress::LocalHost, 0))
        return 0;
    return socket.localPort();
}

const LanBoard::RoomPlayerState *playerById(const LanBoard::RoomSnapshot &snapshot, int playerId)
{
    for (const LanBoard::RoomPlayerState &player : snapshot.players) {
        if (player.playerId == playerId)
            return &player;
    }
    return nullptr;
}

} // namespace

class AppControllerRegressionTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void requestedSeatChangeKeepsRequestedPlayerActive();
    void pendingConnectionIsNotHandledAsDisconnect();
    void intentionalTransitionDoesNotNavigateThroughRoom();
    void defaultLanRoomStartsOnConfiguredPort();
    void occupiedConfiguredLanPortFallsBackToDefault();
    void hostedGameStartsLocally();
    void discoversHostedRoomAndExchangesMessage();
    void surrenderIsNotLimitedToCurrentTurn();
    void remoteGuestCanSurrenderOnHostTurn();
    void finalGomokuMoveArrivesBeforeGameOver();
    void flightChessFinishIsDeferred();
    void localFlightChessCanRestartAndReturnHome();

private:
    QTemporaryDir m_settingsDirectory;
};

void AppControllerRegressionTest::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    QCoreApplication::setOrganizationName(QStringLiteral("LanBoardTests"));
    QCoreApplication::setApplicationName(QStringLiteral("AppControllerRegressionTests"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,
                       QSettings::UserScope,
                       m_settingsDirectory.path());
}

void AppControllerRegressionTest::requestedSeatChangeKeepsRequestedPlayerActive()
{
    AppController controller;
    const quint16 port = availableUdpPort();
    QVERIFY(port != 0);
    QVERIFY(controller.updateDefaultPort(port));
    controller.startRoomAsHost(QStringLiteral("flightchess"));
    QVERIFY(controller.networkManager()->isHost());

    RoomManager *room = controller.roomManager();
    QCOMPARE(room->tryAddRoomPlayer(QStringLiteral("Guest 1"), false, 1),
             RoomManager::ActionError::None);
    QCOMPARE(room->tryAddRoomPlayer(QStringLiteral("Guest 2"), false, 2),
             RoomManager::ActionError::None);

    QVERIFY(QMetaObject::invokeMethod(&controller,
                                      "onRemoteSeatChanged",
                                      Qt::DirectConnection,
                                      Q_ARG(int, 1),
                                      Q_ARG(QString, QStringLiteral("spectator"))));
    QVERIFY(QMetaObject::invokeMethod(&controller,
                                      "onRemoteSeatChanged",
                                      Qt::DirectConnection,
                                      Q_ARG(int, 2),
                                      Q_ARG(QString, QStringLiteral("active"))));

    const LanBoard::RoomSnapshot snapshot = room->snapshot();
    const LanBoard::RoomPlayerState *guest1 = playerById(snapshot, 1);
    const LanBoard::RoomPlayerState *guest2 = playerById(snapshot, 2);
    QVERIFY(guest1);
    QVERIFY(guest2);
    QVERIFY(!guest1->isActive());
    QVERIFY(guest2->isActive());

    controller.networkManager()->disconnectAll();
}

void AppControllerRegressionTest::pendingConnectionIsNotHandledAsDisconnect()
{
    AppController controller;
    controller.startLocalGame(QStringLiteral("doudizhu"));
    QVERIFY(!controller.douDiZhuController()->playerHand().isEmpty());

    const quint16 port = availableUdpPort();
    QVERIFY(port != 0);
    controller.joinRoom(QStringLiteral("127.0.0.1"),
                        port,
                        QStringLiteral("Client"),
                        QStringLiteral("doudizhu"));

    QVERIFY(controller.isClientMode());
    QVERIFY(controller.networkManager()->connectionPending());
    QVERIFY(controller.douDiZhuController()->playerHand().isEmpty());

    QSignalSpy navigationSpy(&controller, &AppController::navigationRequested);
    controller.networkManager()->disconnectAll();

    QVERIFY(!controller.isClientMode());
    QCOMPARE(controller.roomManager()->gameId(), QStringLiteral("gomoku"));
    QVERIFY(controller.douDiZhuController()->playerHand().isEmpty());
    QVERIFY(!navigationSpy.isEmpty());
    QCOMPARE(navigationSpy.last().at(0).toInt(),
             static_cast<int>(LanBoard::NavigationPage::Room));
}

void AppControllerRegressionTest::intentionalTransitionDoesNotNavigateThroughRoom()
{
    AppController controller;
    const quint16 port = availableUdpPort();
    QVERIFY(port != 0);
    controller.joinRoom(QStringLiteral("127.0.0.1"),
                        port,
                        QStringLiteral("Client"),
                        QStringLiteral("gomoku"));
    QVERIFY(controller.networkManager()->connectionPending());

    QSignalSpy navigationSpy(&controller, &AppController::navigationRequested);
    controller.startLocalGame(QStringLiteral("gomoku"));

    QCOMPARE(navigationSpy.count(), 1);
    QCOMPARE(navigationSpy.constFirst().at(0).toInt(),
             static_cast<int>(LanBoard::NavigationPage::Gomoku));
}

void AppControllerRegressionTest::defaultLanRoomStartsOnConfiguredPort()
{
    QTemporaryDir localSettingsDirectory;
    QVERIFY(localSettingsDirectory.isValid());
    QSettings::setPath(QSettings::IniFormat,
                       QSettings::UserScope,
                       localSettingsDirectory.path());

    AppController controller;
    QCOMPARE(controller.defaultPort(), LanBoard::DefaultLanPort);

    controller.startRoomAsHost(QStringLiteral("gomoku"));

    QVERIFY(controller.networkManager()->isHost());
    QVERIFY(controller.networkManager()->serverPort() >= LanBoard::DefaultLanPort);
    QVERIFY(controller.networkManager()->serverPort() < LanBoard::DefaultLanPort + 8);
    controller.networkManager()->disconnectAll();
}

void AppControllerRegressionTest::occupiedConfiguredLanPortFallsBackToDefault()
{
    QTemporaryDir localSettingsDirectory;
    QVERIFY(localSettingsDirectory.isValid());
    QSettings::setPath(QSettings::IniFormat,
                       QSettings::UserScope,
                       localSettingsDirectory.path());

    QUdpSocket blocker;
    QVERIFY(blocker.bind(QHostAddress::AnyIPv4, 0));
    QVERIFY(blocker.localPort() != LanBoard::DefaultLanPort);

    QSettings settings;
    settings.setValue(QStringLiteral("network/defaultPort"), blocker.localPort());

    AppController controller;
    QCOMPARE(controller.defaultPort(), blocker.localPort());

    controller.startRoomAsHost(QStringLiteral("gomoku"));

    QVERIFY(controller.networkManager()->isHost());
    QCOMPARE(controller.defaultPort(), controller.networkManager()->serverPort());
    QVERIFY(controller.defaultPort() >= LanBoard::DefaultLanPort);
    controller.networkManager()->disconnectAll();
}

void AppControllerRegressionTest::hostedGameStartsLocally()
{
    AppController controller;
    const quint16 port = availableUdpPort();
    QVERIFY(port != 0);
    QVERIFY(controller.updateDefaultPort(port));
    controller.startRoomAsHost(QStringLiteral("gomoku"));
    QVERIFY(controller.networkManager()->isHost());
    QVERIFY(controller.networkManager()->isConnected());

    RoomManager *room = controller.roomManager();
    QCOMPARE(room->tryAddRoomPlayer(QStringLiteral("Guest"), false, 1),
             RoomManager::ActionError::None);
    QVERIFY(room->setPlayerReadyById(0, true));
    QVERIFY(room->setPlayerReadyById(1, true));
    QVERIFY(room->canStart());

    QSignalSpy navigationSpy(&controller, &AppController::navigationRequested);
    room->startGame();

    QCOMPARE(navigationSpy.count(), 1);
    QCOMPARE(navigationSpy.constFirst().at(0).toInt(),
             static_cast<int>(LanBoard::NavigationPage::Gomoku));
    QVERIFY(room->gameInProgress());

    controller.networkManager()->disconnectAll();
}

void AppControllerRegressionTest::discoversHostedRoomAndExchangesMessage()
{
    AppController host;
    const quint16 port = availableUdpPort();
    QVERIFY(port != 0);
    QVERIFY(host.updateDefaultPort(port));
    QVERIFY(host.updateNickname(QStringLiteral("Discovery Host")));
    host.startRoomAsHost(QStringLiteral("gomoku"));
    QVERIFY(host.networkManager()->isHost());

    RoomDiscoveryService discovery;
    discovery.start();

    QVariantMap discoveredRoom;
    auto findHostedRoom = [&]() {
        for (const QVariant &value : discovery.discoveredRooms()) {
            const QVariantMap room = value.toMap();
            if (room.value(QStringLiteral("port")).toUInt()
                != host.networkManager()->serverPort()) {
                continue;
            }
            if (room.value(QStringLiteral("hostName")).toString()
                != QStringLiteral("Discovery Host")) {
                continue;
            }
            discoveredRoom = room;
            return true;
        }
        return false;
    };
    QTRY_VERIFY_WITH_TIMEOUT(findHostedRoom(), 7000);
    QVERIFY(!discoveredRoom.value(QStringLiteral("roomUid")).toString().isEmpty());
    QVERIFY(discoveredRoom.value(QStringLiteral("endpointCount")).toInt() >= 1);

    NetworkManager client;
    client.connectToServer(discoveredRoom.value(QStringLiteral("hostIp")).toString(),
                           static_cast<quint16>(discoveredRoom.value(QStringLiteral("port")).toUInt()),
                           QStringLiteral("Discovery Guest"),
                           QStringLiteral("gomoku"));
    QTRY_VERIFY_WITH_TIMEOUT(client.isConnected(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(host.roomManager()->activePlayerCount(), 2, 5000);

    client.sendReady(true);
    QTRY_VERIFY_WITH_TIMEOUT([&]() {
        const LanBoard::RoomSnapshot snapshot = host.roomManager()->snapshot();
        const LanBoard::RoomPlayerState *guest = playerById(snapshot, 1);
        return guest && guest->isReady;
    }(), 5000);

    client.disconnectAll();
    discovery.stop();
    host.networkManager()->disconnectAll();
}

void AppControllerRegressionTest::surrenderIsNotLimitedToCurrentTurn()
{
    GameController game;
    QVERIFY(game.placePiece(0, 0, 1));
    QCOMPARE(game.currentPlayer(), 2);
    QVERIFY(game.surrender(1));
    QVERIFY(game.isGameOver());
    QCOMPARE(game.winner(), 2);

    game.reset();
    QCOMPARE(game.currentPlayer(), 1);
    QVERIFY(game.surrender(2));
    QVERIFY(game.isGameOver());
    QCOMPARE(game.winner(), 1);
}

void AppControllerRegressionTest::remoteGuestCanSurrenderOnHostTurn()
{
    AppController controller;
    const quint16 port = availableUdpPort();
    QVERIFY(port != 0);
    QVERIFY(controller.updateDefaultPort(port));
    controller.startRoomAsHost(QStringLiteral("gomoku"));
    QVERIFY(QMetaObject::invokeMethod(&controller,
                                      "onJoinRequested",
                                      Qt::DirectConnection,
                                      Q_ARG(QString, QStringLiteral("Guest")),
                                      Q_ARG(int, 1)));
    QCOMPARE(controller.gameController()->currentPlayer(), 1);

    QVERIFY(QMetaObject::invokeMethod(&controller,
                                      "onRemoteSurrender",
                                      Qt::DirectConnection,
                                      Q_ARG(int, 1)));
    QVERIFY(controller.gameController()->isGameOver());
    QCOMPARE(controller.gameController()->winner(), 1);

    controller.networkManager()->disconnectAll();
}

void AppControllerRegressionTest::finalGomokuMoveArrivesBeforeGameOver()
{
    AppController host;
    const quint16 port = availableUdpPort();
    QVERIFY(port != 0);
    QVERIFY(host.updateDefaultPort(port));
    host.startRoomAsHost(QStringLiteral("gomoku"));
    QVERIFY(host.networkManager()->isHost());

    NetworkManager client;
    QSignalSpy roomStateSpy(&client, &NetworkManager::roomStateReceived);
    client.connectToServer(QStringLiteral("127.0.0.1"),
                           host.networkManager()->serverPort(),
                           QStringLiteral("Guest"),
                           QStringLiteral("gomoku"));
    QTRY_VERIFY_WITH_TIMEOUT(client.isConnected(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!roomStateSpy.isEmpty(), 5000);

    GameController *game = host.gameController();
    QVERIFY(game->placePiece(0, 0, 1));
    QVERIFY(game->placePiece(1, 0, 2));
    QVERIFY(game->placePiece(0, 1, 1));
    QVERIFY(game->placePiece(2, 0, 2));
    QVERIFY(game->placePiece(0, 2, 1));
    QVERIFY(game->placePiece(3, 0, 2));
    QVERIFY(game->placePiece(5, 5, 1));
    QVERIFY(game->placePiece(4, 0, 2));
    QVERIFY(game->placePiece(5, 6, 1));

    QStringList receivedOrder;
    connect(&client, &NetworkManager::remoteMoveReceived, this,
            [&receivedOrder](int, int, int) { receivedOrder.append(QStringLiteral("move")); });
    connect(&client, &NetworkManager::gameOverReceived, this,
            [&receivedOrder](int) { receivedOrder.append(QStringLiteral("game_over")); });

    client.sendPlacePiece(5, 0);
    QTRY_VERIFY_WITH_TIMEOUT(receivedOrder.size() >= 2, 5000);
    QCOMPARE(receivedOrder.at(0), QStringLiteral("move"));
    QCOMPARE(receivedOrder.at(1), QStringLiteral("game_over"));

    client.disconnectAll();
    host.networkManager()->disconnectAll();
}

void AppControllerRegressionTest::flightChessFinishIsDeferred()
{
    AppController controller;
    const quint16 port = availableUdpPort();
    QVERIFY(port != 0);
    QVERIFY(controller.updateDefaultPort(port));
    controller.startRoomAsHost(QStringLiteral("flightchess"));
    QVERIFY(controller.networkManager()->isHost());

    QSignalSpy navigationSpy(&controller, &AppController::navigationRequested);
    controller.flightChessController()->setGameOver(1);
    QCOMPARE(navigationSpy.count(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(navigationSpy.count(), 1, 1000);

    controller.networkManager()->disconnectAll();
}

void AppControllerRegressionTest::localFlightChessCanRestartAndReturnHome()
{
    AppController controller;
    controller.startLocalGame(QStringLiteral("flightchess"));
    controller.flightChessController()->setGameOver(1);
    QVERIFY(controller.flightChessController()->isGameOver());

    controller.restartFlightChessGame();
    QVERIFY(!controller.flightChessController()->isGameOver());

    QSignalSpy navigationSpy(&controller, &AppController::navigationRequested);
    controller.returnFromFlightChessGame();
    QCOMPARE(navigationSpy.count(), 1);
    QCOMPARE(navigationSpy.takeFirst().at(0).toInt(),
             static_cast<int>(LanBoard::NavigationPage::Home));
}

QTEST_GUILESS_MAIN(AppControllerRegressionTest)

#include "appcontrollerregressiontest.moc"
