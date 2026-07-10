#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>
#include <QUdpSocket>

#include "src/network/networkaddressutils.h"
#include "src/network/roomdiscoveryservice.h"

namespace {

QJsonObject announcement(const QString &roomUid,
                         const QString &hostIp,
                         quint16 port,
                         const QString &hostName = QStringLiteral("Host"))
{
    QJsonObject message;
    message[QStringLiteral("roomUid")] = roomUid;
    message[QStringLiteral("hostIp")] = hostIp;
    message[QStringLiteral("port")] = static_cast<int>(port);
    message[QStringLiteral("hostName")] = hostName;
    message[QStringLiteral("playerCount")] = 1;
    message[QStringLiteral("roomCapacity")] = 2;
    message[QStringLiteral("maxPlayers")] = 2;
    message[QStringLiteral("gameId")] = QStringLiteral("gomoku");
    message[QStringLiteral("gameName")] = QStringLiteral("Gomoku");
    message[QStringLiteral("inGame")] = false;
    message[QStringLiteral("isFull")] = false;
    return message;
}

QVariantMap onlyRoom(const RoomDiscoveryService &service)
{
    const QVariantList rooms = service.discoveredRooms();
    if (rooms.size() != 1)
        return {};
    return rooms.constFirst().toMap();
}

QVariantMap roomByUid(const RoomDiscoveryService &service, const QString &roomUid)
{
    for (const QVariant &value : service.discoveredRooms()) {
        const QVariantMap room = value.toMap();
        if (room.value(QStringLiteral("roomUid")).toString() == roomUid)
            return room;
    }
    return {};
}

} // namespace

class NetworkDiscoveryTest : public QObject
{
    Q_OBJECT

private slots:
    void classifiesIpv4Addresses();
    void prefersPhysicalSameSubnetEndpoint();
    void aggregatesMultipleEndpointsByRoomUid();
    void ignoresOwnRoomUid();
    void rejectsAnnouncementWithoutRoomUid();
    void prunesEndpointsIndividually();
    void receivesAnnouncementFromUdpSocket();
};

void NetworkDiscoveryTest::classifiesIpv4Addresses()
{
    QVERIFY(NetworkAddressUtils::isPrivateIpv4(QHostAddress(QStringLiteral("10.1.2.3"))));
    QVERIFY(NetworkAddressUtils::isPrivateIpv4(QHostAddress(QStringLiteral("172.16.0.1"))));
    QVERIFY(NetworkAddressUtils::isPrivateIpv4(QHostAddress(QStringLiteral("172.31.255.254"))));
    QVERIFY(NetworkAddressUtils::isPrivateIpv4(QHostAddress(QStringLiteral("192.168.1.1"))));
    QVERIFY(!NetworkAddressUtils::isPrivateIpv4(QHostAddress(QStringLiteral("172.15.255.254"))));
    QVERIFY(!NetworkAddressUtils::isPrivateIpv4(QHostAddress(QStringLiteral("172.32.0.1"))));

    QVERIFY(NetworkAddressUtils::isUsableIpv4(QHostAddress(QStringLiteral("192.168.1.8"))));
    QVERIFY(!NetworkAddressUtils::isUsableIpv4(QHostAddress(QStringLiteral("127.0.0.1"))));
    QVERIFY(!NetworkAddressUtils::isUsableIpv4(QHostAddress(QStringLiteral("169.254.1.2"))));
    QVERIFY(!NetworkAddressUtils::isUsableIpv4(QHostAddress(QStringLiteral("224.0.0.1"))));
    QVERIFY(!NetworkAddressUtils::isUsableIpv4(QHostAddress(QStringLiteral("240.0.0.1"))));
    QVERIFY(!NetworkAddressUtils::isUsableIpv4(QHostAddress(QStringLiteral("0.1.2.3"))));
}

void NetworkDiscoveryTest::prefersPhysicalSameSubnetEndpoint()
{
    NetworkAddressUtils::LocalIpv4Address physical;
    physical.address = QHostAddress(QStringLiteral("192.168.10.20"));
    physical.broadcast = QHostAddress(QStringLiteral("192.168.10.255"));
    physical.prefixLength = 24;
    physical.interfaceName = QStringLiteral("Wi-Fi");

    NetworkAddressUtils::LocalIpv4Address vpn;
    vpn.address = QHostAddress(QStringLiteral("10.8.0.2"));
    vpn.broadcast = QHostAddress(QStringLiteral("10.8.0.255"));
    vpn.prefixLength = 24;
    vpn.interfaceName = QStringLiteral("VPN Adapter");
    vpn.isVirtual = true;

    const QList<NetworkAddressUtils::LocalIpv4Address> locals { physical, vpn };
    const int lanScore = NetworkAddressUtils::endpointPreference(
        QHostAddress(QStringLiteral("192.168.10.30")), locals);
    const int vpnScore = NetworkAddressUtils::endpointPreference(
        QHostAddress(QStringLiteral("10.8.0.3")), locals);
    QVERIFY(lanScore > vpnScore);
}

void NetworkDiscoveryTest::aggregatesMultipleEndpointsByRoomUid()
{
    RoomDiscoveryService service;
    service.processAnnouncement(announcement(QStringLiteral("room-a"),
                                             QStringLiteral("192.168.1.20"), 44567),
                                QHostAddress(QStringLiteral("192.168.1.20")), 100);
    service.processAnnouncement(announcement(QStringLiteral("room-a"),
                                             QStringLiteral("10.8.0.20"), 44567),
                                QHostAddress(QStringLiteral("10.8.0.20")), 200);

    const QVariantMap room = onlyRoom(service);
    QVERIFY(!room.isEmpty());
    QCOMPARE(room.value(QStringLiteral("roomUid")).toString(), QStringLiteral("room-a"));
    QCOMPARE(room.value(QStringLiteral("endpointCount")).toInt(), 2);
}

void NetworkDiscoveryTest::ignoresOwnRoomUid()
{
    RoomDiscoveryService service;
    service.processAnnouncement(announcement(service.publishedRoomUid(),
                                             QStringLiteral("192.168.1.20"), 44567),
                                QHostAddress(QStringLiteral("192.168.1.20")), 100);
    QVERIFY(service.discoveredRooms().isEmpty());
}

void NetworkDiscoveryTest::rejectsAnnouncementWithoutRoomUid()
{
    RoomDiscoveryService service;
    QJsonObject missingUid = announcement(QStringLiteral("room-a"),
                                          QStringLiteral("192.168.1.20"), 44567);
    missingUid.remove(QStringLiteral("roomUid"));
    service.processAnnouncement(missingUid,
                                QHostAddress(QStringLiteral("192.168.1.20")), 100);
    QVERIFY(service.discoveredRooms().isEmpty());

    service.processAnnouncement(announcement(QStringLiteral("   "),
                                             QStringLiteral("192.168.1.20"), 44567),
                                QHostAddress(QStringLiteral("192.168.1.20")), 200);
    QVERIFY(service.discoveredRooms().isEmpty());
}

void NetworkDiscoveryTest::prunesEndpointsIndividually()
{
    RoomDiscoveryService service;
    service.processAnnouncement(announcement(QStringLiteral("room-a"),
                                             QStringLiteral("192.168.1.20"), 44567),
                                QHostAddress(QStringLiteral("192.168.1.20")), 0);
    service.processAnnouncement(announcement(QStringLiteral("room-a"),
                                             QStringLiteral("10.8.0.20"), 44567),
                                QHostAddress(QStringLiteral("10.8.0.20")), 1000);

    service.pruneExpired(RoomDiscoveryService::DiscoveryStaleMs + 1);
    QCOMPARE(onlyRoom(service).value(QStringLiteral("endpointCount")).toInt(), 1);

    service.pruneExpired(RoomDiscoveryService::DiscoveryStaleMs + 1001);
    QVERIFY(service.discoveredRooms().isEmpty());
}

void NetworkDiscoveryTest::receivesAnnouncementFromUdpSocket()
{
    RoomDiscoveryService service;
    service.start();

    QJsonObject message = announcement(QStringLiteral("udp-room"),
                                       QStringLiteral("192.168.50.20"), 44567);
    message[QStringLiteral("type")] = QStringLiteral("room_announce");
    message[QStringLiteral("app")] = QStringLiteral("LanBoard");
    message[QStringLiteral("version")] = 1;

    QUdpSocket sender;
    const QByteArray payload = QJsonDocument(message).toJson(QJsonDocument::Compact);
    QCOMPARE(sender.writeDatagram(payload, QHostAddress::LocalHost, 44568),
             static_cast<qint64>(payload.size()));

    QTRY_VERIFY_WITH_TIMEOUT(!roomByUid(service, QStringLiteral("udp-room")).isEmpty(), 2000);
    service.stop();
}

QTEST_GUILESS_MAIN(NetworkDiscoveryTest)

#include "networkdiscoverytest.moc"
