#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

#include "src/lobby/roommanager.h"

namespace {

void addHost(RoomManager &room, const QString &gameId = QStringLiteral("gomoku"))
{
    room.setGameId(gameId);
    QCOMPARE(room.tryAddRoomPlayer(QStringLiteral("Host"), true, 0),
             RoomManager::ActionError::None);
    room.setLocalPlayerId(0);
}

void setAllActivePlayersReady(RoomManager &room)
{
    for (const LanBoard::RoomPlayerState &player : room.snapshot().activePlayers())
        QVERIFY(room.setPlayerReadyById(player.playerId, true));
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

class RoomManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void validatesPlayerAdmission();
    void assignsGomokuSeatsAndPieces();
    void enforcesSeatChanges();
    void enforcesStartPermissionsAndReadiness();
    void appliesGameSpecificStartRules_data();
    void appliesGameSpecificStartRules();
    void normalizesSeatsWhenSwitchingGames();
    void promotesSpectatorWhenActiveGuestLeaves();
    void concludesGameAndClearsReadyStates();
    void preservesSnapshotAndRoomMessageFields();
};

void RoomManagerTest::validatesPlayerAdmission()
{
    RoomManager room;
    addHost(room);

    QCOMPARE(room.tryAddRoomPlayer(QStringLiteral("Duplicate"), false, 0),
             RoomManager::ActionError::PlayerAlreadyExists);
    QCOMPARE(room.tryAddRoomPlayer(QStringLiteral("Negative"), false, -1),
             RoomManager::ActionError::InvalidPlayerId);
    QCOMPARE(room.tryAddRoomPlayer(QStringLiteral("Too large"), false, room.roomCapacity()),
             RoomManager::ActionError::InvalidPlayerId);

    for (int playerId = 1; playerId < room.roomCapacity(); ++playerId) {
        QCOMPARE(room.allocatePlayerId(), playerId);
        QCOMPARE(room.tryAddRoomPlayer(QStringLiteral("Player %1").arg(playerId),
                                       false,
                                       playerId),
                 RoomManager::ActionError::None);
    }

    QCOMPARE(room.snapshot().players.size(), room.roomCapacity());
    QCOMPARE(room.allocatePlayerId(), -1);
    QCOMPARE(room.activePlayerCount(), 2);
}

void RoomManagerTest::assignsGomokuSeatsAndPieces()
{
    RoomManager room;
    addHost(room);
    QCOMPARE(room.tryAddRoomPlayer(QStringLiteral("Guest 1"), false, 1),
             RoomManager::ActionError::None);
    QCOMPARE(room.tryAddRoomPlayer(QStringLiteral("Guest 2"), false, 2),
             RoomManager::ActionError::None);

    const LanBoard::RoomSnapshot snapshot = room.snapshot();
    const LanBoard::RoomPlayerState *host = playerById(snapshot, 0);
    const LanBoard::RoomPlayerState *guest1 = playerById(snapshot, 1);
    const LanBoard::RoomPlayerState *guest2 = playerById(snapshot, 2);

    QVERIFY(host);
    QVERIFY(guest1);
    QVERIFY(guest2);
    QVERIFY(host->isActive());
    QVERIFY(guest1->isActive());
    QVERIFY(!guest2->isActive());
    QCOMPARE(host->piece, 1);
    QCOMPARE(guest1->piece, 2);
    QCOMPARE(guest2->piece, 0);
}

void RoomManagerTest::enforcesSeatChanges()
{
    RoomManager room;
    addHost(room);
    QCOMPARE(room.tryAddRoomPlayer(QStringLiteral("Guest 1"), false, 1),
             RoomManager::ActionError::None);
    QCOMPARE(room.tryAddRoomPlayer(QStringLiteral("Guest 2"), false, 2),
             RoomManager::ActionError::None);

    QCOMPARE(room.tryChangeSeat(0, QStringLiteral("spectator")),
             RoomManager::ActionError::HostLockedActive);
    QCOMPARE(room.tryChangeSeat(2, QStringLiteral("active")),
             RoomManager::ActionError::ActiveSeatFull);

    QVERIFY(room.setPlayerReadyById(1, true));
    QCOMPARE(room.tryChangeSeat(1, QStringLiteral("spectator")),
             RoomManager::ActionError::None);
    QVERIFY(!playerById(room.snapshot(), 1)->isReady);
    QCOMPARE(room.tryChangeSeat(2, QStringLiteral("active")),
             RoomManager::ActionError::None);
    QVERIFY(room.isPlayerActive(2));

    room.setGameInProgress(true);
    QCOMPARE(room.tryChangeSeat(2, QStringLiteral("spectator")),
             RoomManager::ActionError::GameInProgress);
}

void RoomManagerTest::enforcesStartPermissionsAndReadiness()
{
    RoomManager room;
    addHost(room);
    QCOMPARE(room.tryAddRoomPlayer(QStringLiteral("Guest"), false, 1),
             RoomManager::ActionError::None);

    QCOMPARE(room.tryStartGame(1), RoomManager::ActionError::OnlyHostCanStart);
    QCOMPARE(room.tryStartGame(0), RoomManager::ActionError::PlayersNotReady);

    setAllActivePlayersReady(room);
    QVERIFY(room.canStart());

    room.setLocalPlayerId(1);
    QVERIFY(!room.canStart());
    room.setLocalPlayerId(0);

    QCOMPARE(room.tryStartGame(0), RoomManager::ActionError::None);
    QVERIFY(room.gameInProgress());
    QVERIFY(!room.canStart());
    QCOMPARE(room.tryStartGame(0), RoomManager::ActionError::GameInProgress);
    QCOMPARE(room.trySwitchGame(0, QStringLiteral("doudizhu")),
             RoomManager::ActionError::GameInProgress);
}

void RoomManagerTest::appliesGameSpecificStartRules_data()
{
    QTest::addColumn<QString>("gameId");
    QTest::addColumn<int>("playerCount");
    QTest::addColumn<RoomManager::ActionError>("expectedError");

    QTest::newRow("gomoku-missing-player")
        << QStringLiteral("gomoku") << 1 << RoomManager::ActionError::MissingPlayers;
    QTest::newRow("gomoku-ready")
        << QStringLiteral("gomoku") << 2 << RoomManager::ActionError::None;
    QTest::newRow("doudizhu-missing-player")
        << QStringLiteral("doudizhu") << 2 << RoomManager::ActionError::MissingPlayers;
    QTest::newRow("doudizhu-ready")
        << QStringLiteral("doudizhu") << 3 << RoomManager::ActionError::None;
    QTest::newRow("flightchess-missing-player")
        << QStringLiteral("flightchess") << 1 << RoomManager::ActionError::MissingPlayers;
    QTest::newRow("flightchess-ready")
        << QStringLiteral("flightchess") << 2 << RoomManager::ActionError::None;
    QTest::newRow("survivor-solo")
        << QStringLiteral("survivor") << 1 << RoomManager::ActionError::None;
    QTest::newRow("survivor-four-player")
        << QStringLiteral("survivor") << 4 << RoomManager::ActionError::None;
}

void RoomManagerTest::appliesGameSpecificStartRules()
{
    QFETCH(QString, gameId);
    QFETCH(int, playerCount);
    QFETCH(RoomManager::ActionError, expectedError);

    RoomManager room;
    addHost(room, gameId);
    for (int playerId = 1; playerId < playerCount; ++playerId) {
        QCOMPARE(room.tryAddRoomPlayer(QStringLiteral("Guest %1").arg(playerId),
                                       false,
                                       playerId),
                 RoomManager::ActionError::None);
    }
    setAllActivePlayersReady(room);

    QCOMPARE(room.tryStartGame(0), expectedError);
}

void RoomManagerTest::normalizesSeatsWhenSwitchingGames()
{
    RoomManager room;
    addHost(room, QStringLiteral("survivor"));
    for (int playerId = 1; playerId <= 3; ++playerId) {
        QCOMPARE(room.tryAddRoomPlayer(QStringLiteral("Guest %1").arg(playerId),
                                       false,
                                       playerId),
                 RoomManager::ActionError::None);
    }
    setAllActivePlayersReady(room);
    QCOMPARE(room.activePlayerCount(), 4);

    QCOMPARE(room.trySwitchGame(1, QStringLiteral("gomoku")),
             RoomManager::ActionError::OnlyHostCanSwitchGame);
    QCOMPARE(room.trySwitchGame(0, QStringLiteral("gomoku")),
             RoomManager::ActionError::None);
    QCOMPARE(room.activePlayerCount(), 2);
    QCOMPARE(room.playerPiece(0), 1);
    QCOMPARE(room.playerPiece(1), 2);
    QCOMPARE(room.playerPiece(2), 0);
    QCOMPARE(room.playerPiece(3), 0);
    for (const LanBoard::RoomPlayerState &player : room.snapshot().players)
        QVERIFY(!player.isReady);

    QCOMPARE(room.trySwitchGame(0, QStringLiteral("doudizhu")),
             RoomManager::ActionError::None);
    QCOMPARE(room.activePlayerCount(), 3);
    QCOMPARE(room.maxPlayers(), 3);
}

void RoomManagerTest::promotesSpectatorWhenActiveGuestLeaves()
{
    RoomManager room;
    addHost(room);
    QCOMPARE(room.tryAddRoomPlayer(QStringLiteral("Guest 1"), false, 1),
             RoomManager::ActionError::None);
    QCOMPARE(room.tryAddRoomPlayer(QStringLiteral("Guest 2"), false, 2),
             RoomManager::ActionError::None);
    QVERIFY(!room.isPlayerActive(2));

    QVERIFY(room.removePlayerById(1));
    QVERIFY(room.isPlayerActive(2));
    QCOMPARE(room.playerPiece(2), 2);
    QCOMPARE(room.firstGuestPlayerId(), 2);
}

void RoomManagerTest::concludesGameAndClearsReadyStates()
{
    RoomManager room;
    addHost(room);
    QCOMPARE(room.tryAddRoomPlayer(QStringLiteral("Guest"), false, 1),
             RoomManager::ActionError::None);
    setAllActivePlayersReady(room);
    QCOMPARE(room.tryStartGame(0), RoomManager::ActionError::None);

    room.concludeGame();

    QVERIFY(!room.gameInProgress());
    QVERIFY(!room.canStart());
    for (const LanBoard::RoomPlayerState &player : room.snapshot().activePlayers())
        QVERIFY(!player.isReady);
}

void RoomManagerTest::preservesSnapshotAndRoomMessageFields()
{
    LanBoard::RoomSnapshot source;
    source.roomId = QStringLiteral("room-42");
    source.roomName = QStringLiteral("Regression Room");
    source.gameId = QStringLiteral("flightchess");
    source.localPlayerId = 3;
    source.gameInProgress = true;
    source.players = {
        {0, QStringLiteral("Host"), true, true, LanBoard::SeatKind::Active, 1},
        {3, QStringLiteral("Local"), false, false, LanBoard::SeatKind::Active, 2},
        {5, QStringLiteral("Viewer"), false, false, LanBoard::SeatKind::Spectator, 0}
    };

    RoomManager room;
    room.setSnapshot(source);

    const LanBoard::RoomSnapshot restored = room.snapshot();
    QCOMPARE(restored.roomId, source.roomId);
    QCOMPARE(restored.roomName, source.roomName);
    QCOMPARE(restored.gameId, source.gameId);
    QCOMPARE(restored.localPlayerId, source.localPlayerId);
    QCOMPARE(restored.gameInProgress, source.gameInProgress);
    QCOMPARE(restored.players.size(), source.players.size());
    QCOMPARE(room.localPlayerIndex(), 1);
    QVERIFY(!room.isHost());

    const QJsonObject message = room.roomStateMessageForPlayer(3,
                                                                QStringLiteral("dedicated_server"));
    QCOMPARE(message.value(QStringLiteral("roomId")).toString(), source.roomId);
    QCOMPARE(message.value(QStringLiteral("gameId")).toString(), source.gameId);
    QCOMPARE(message.value(QStringLiteral("yourPlayerId")).toInt(), 3);
    QCOMPARE(message.value(QStringLiteral("yourPiece")).toInt(), 2);
    QCOMPARE(message.value(QStringLiteral("mode")).toString(),
             QStringLiteral("dedicated_server"));
    QCOMPARE(message.value(QStringLiteral("players")).toArray().size(), source.players.size());
}

QTEST_GUILESS_MAIN(RoomManagerTest)

#include "roommanagertest.moc"
