#include "networkaddressutils.h"

#include <QAbstractSocket>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QSet>

#include <limits>

namespace {

bool ipv4Value(const QHostAddress &address, quint32 &value)
{
    bool ok = false;
    value = address.toIPv4Address(&ok);
    return ok && address.protocol() == QAbstractSocket::IPv4Protocol;
}

int localAddressPreference(const NetworkAddressUtils::LocalIpv4Address &entry)
{
    int score = 0;
    if (NetworkAddressUtils::isPrivateIpv4(entry.address))
        score += 200;
    if (!entry.broadcast.isNull())
        score += 40;
    if (entry.isVirtual)
        score -= 150;
    else
        score += 50;

    const QString name = entry.interfaceName.toLower();
    if (name.contains(QStringLiteral("wi-fi"))
        || name.contains(QStringLiteral("wifi"))
        || name.contains(QStringLiteral("wlan"))) {
        score += 20;
    }
    return score;
}

} // namespace

namespace NetworkAddressUtils {

bool isUsableIpv4(const QHostAddress &address)
{
    quint32 value = 0;
    if (!ipv4Value(address, value))
        return false;
    if (value == 0 || value == 0xffffffffU)
        return false;
    if ((value & 0xff000000U) == 0x00000000U)
        return false;
    if ((value & 0xff000000U) == 0x7f000000U)
        return false;
    if ((value & 0xffff0000U) == 0xa9fe0000U)
        return false;
    if ((value & 0xf0000000U) == 0xe0000000U)
        return false;
    if ((value & 0xf0000000U) == 0xf0000000U)
        return false;
    return true;
}

bool isPrivateIpv4(const QHostAddress &address)
{
    quint32 value = 0;
    if (!ipv4Value(address, value))
        return false;
    return (value & 0xff000000U) == 0x0a000000U
        || (value & 0xfff00000U) == 0xac100000U
        || (value & 0xffff0000U) == 0xc0a80000U;
}

bool isSameSubnet(const QHostAddress &lhs, const QHostAddress &rhs, int prefixLength)
{
    quint32 lhsValue = 0;
    quint32 rhsValue = 0;
    if (!ipv4Value(lhs, lhsValue) || !ipv4Value(rhs, rhsValue))
        return false;

    const int normalizedPrefix = qBound(0, prefixLength, 32);
    if (normalizedPrefix == 0)
        return true;
    const quint32 mask = normalizedPrefix == 32
        ? 0xffffffffU
        : (0xffffffffU << (32 - normalizedPrefix));
    return (lhsValue & mask) == (rhsValue & mask);
}

bool isLikelyVirtualInterface(const QString &name)
{
    const QString normalized = name.toLower();
    static const QStringList markers {
        QStringLiteral("vpn"),
        QStringLiteral("virtual"),
        QStringLiteral("vbox"),
        QStringLiteral("vmware"),
        QStringLiteral("hyper-v"),
        QStringLiteral("docker"),
        QStringLiteral("wsl"),
        QStringLiteral("tunnel"),
        QStringLiteral("tun"),
        QStringLiteral("tap")
    };
    for (const QString &marker : markers) {
        if (normalized.contains(marker))
            return true;
    }
    return false;
}

QList<LocalIpv4Address> localIpv4Addresses()
{
    QList<LocalIpv4Address> result;
    QSet<QString> seenAddresses;

    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp)
            || !(flags & QNetworkInterface::IsRunning)
            || (flags & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        const QString interfaceName = iface.humanReadableName() + QLatin1Char(' ') + iface.name();
        for (const QNetworkAddressEntry &addressEntry : iface.addressEntries()) {
            if (!isUsableIpv4(addressEntry.ip()))
                continue;

            const QString key = addressEntry.ip().toString();
            if (seenAddresses.contains(key))
                continue;
            seenAddresses.insert(key);

            LocalIpv4Address entry;
            entry.address = addressEntry.ip();
            entry.broadcast = addressEntry.broadcast();
            entry.prefixLength = addressEntry.prefixLength() >= 0
                ? qBound(0, addressEntry.prefixLength(), 32)
                : 32;
            entry.interfaceIndex = iface.index();
            entry.interfaceName = interfaceName;
            entry.isVirtual = isLikelyVirtualInterface(interfaceName);
            result.append(entry);
        }
    }

    return result;
}

bool isLocalIpv4(const QHostAddress &address)
{
    for (const LocalIpv4Address &local : localIpv4Addresses()) {
        if (local.address == address)
            return true;
    }
    return false;
}

int endpointPreference(const QHostAddress &endpoint,
                       const QList<LocalIpv4Address> &localAddresses)
{
    if (!isUsableIpv4(endpoint))
        return std::numeric_limits<int>::min();

    int bestScore = isPrivateIpv4(endpoint) ? 200 : 0;
    for (const LocalIpv4Address &local : localAddresses) {
        if (!isUsableIpv4(local.address))
            continue;

        int score = isPrivateIpv4(endpoint) ? 200 : 0;
        if (isSameSubnet(endpoint, local.address, local.prefixLength))
            score += 1000;
        if (local.isVirtual)
            score -= 200;
        else
            score += 50;
        if (!local.broadcast.isNull())
            score += 10;
        bestScore = qMax(bestScore, score);
    }
    return bestScore;
}

QString bestLocalIpv4()
{
    const QList<LocalIpv4Address> addresses = localIpv4Addresses();
    const LocalIpv4Address *best = nullptr;
    int bestScore = std::numeric_limits<int>::min();
    for (const LocalIpv4Address &entry : addresses) {
        const int score = localAddressPreference(entry);
        if (score > bestScore
            || (score == bestScore && best && entry.address.toString() < best->address.toString())) {
            best = &entry;
            bestScore = score;
        }
    }
    return best ? best->address.toString() : QStringLiteral("127.0.0.1");
}

QString localIpv4ForPeer(const QHostAddress &peer)
{
    const QList<LocalIpv4Address> addresses = localIpv4Addresses();
    const LocalIpv4Address *best = nullptr;
    int bestScore = std::numeric_limits<int>::min();
    for (const LocalIpv4Address &entry : addresses) {
        int score = localAddressPreference(entry);
        if (isSameSubnet(peer, entry.address, entry.prefixLength))
            score += 1000;
        if (score > bestScore) {
            best = &entry;
            bestScore = score;
        }
    }
    return best ? best->address.toString() : bestLocalIpv4();
}

} // namespace NetworkAddressUtils
