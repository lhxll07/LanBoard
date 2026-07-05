#include "networkaddressutils.h"

#include <QAbstractSocket>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>

#include <limits>

namespace {

int ipPreferenceScore(const QNetworkInterface &iface, const QHostAddress &address)
{
    int score = 0;
    const QString humanName = (iface.humanReadableName() + QLatin1Char(' ') + iface.name()).toLower();
    const QString ip = address.toString();

    if (ip.startsWith(QStringLiteral("192.168.")) || ip.startsWith(QStringLiteral("10.")))
        score += 40;
    else if (ip.startsWith(QStringLiteral("172.")))
        score += 30;
    else
        score += 10;

    if (humanName.contains(QStringLiteral("wlan"))
        || humanName.contains(QStringLiteral("wi-fi"))
        || humanName.contains(QStringLiteral("wifi"))) {
        score += 30;
    }

    if (humanName.contains(QStringLiteral("rmnet"))
        || humanName.contains(QStringLiteral("cell"))
        || humanName.contains(QStringLiteral("mobile"))
        || humanName.contains(QStringLiteral("tun"))
        || humanName.contains(QStringLiteral("tap"))
        || humanName.contains(QStringLiteral("vpn"))
        || humanName.contains(QStringLiteral("virtual"))) {
        score -= 20;
    }

    return score;
}

} // namespace

namespace NetworkAddressUtils {

bool isUsableIpv4(const QHostAddress &address)
{
    if (address.protocol() != QAbstractSocket::IPv4Protocol)
        return false;

    if (address == QHostAddress::LocalHost)
        return false;

    return !address.toString().startsWith(QStringLiteral("169.254."));
}

QString bestLocalIpv4()
{
    QHostAddress bestAddress;
    int bestScore = std::numeric_limits<int>::min();

    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning)
            || (flags & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            const QHostAddress address = entry.ip();
            if (!isUsableIpv4(address))
                continue;

            const int score = ipPreferenceScore(iface, address);
            if (score <= bestScore)
                continue;

            bestScore = score;
            bestAddress = address;
        }
    }

    return bestAddress.isNull() ? QStringLiteral("127.0.0.1") : bestAddress.toString();
}

QString localIpv4ForPeer(const QHostAddress &peer)
{
    QHostAddress fallbackAddress;
    int fallbackScore = std::numeric_limits<int>::min();

    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning)
            || (flags & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            const QHostAddress ip = entry.ip();
            if (!isUsableIpv4(ip))
                continue;

            if (peer.isInSubnet(ip, entry.prefixLength()))
                return ip.toString();

            const int score = ipPreferenceScore(iface, ip);
            if (score <= fallbackScore)
                continue;

            fallbackScore = score;
            fallbackAddress = ip;
        }
    }

    return fallbackAddress.isNull() ? bestLocalIpv4() : fallbackAddress.toString();
}

} // namespace NetworkAddressUtils
