#pragma once

#include <QHostAddress>
#include <QList>
#include <QString>

namespace NetworkAddressUtils {

struct LocalIpv4Address
{
    QHostAddress address;
    QHostAddress broadcast;
    int prefixLength = 0;
    int interfaceIndex = 0;
    QString interfaceName;
    bool isVirtual = false;
};

bool isUsableIpv4(const QHostAddress &address);
bool isPrivateIpv4(const QHostAddress &address);
bool isSameSubnet(const QHostAddress &lhs, const QHostAddress &rhs, int prefixLength);
bool isLocalIpv4(const QHostAddress &address);
bool isLikelyVirtualInterface(const QString &name);
int endpointPreference(const QHostAddress &endpoint,
                       const QList<LocalIpv4Address> &localAddresses);
QList<LocalIpv4Address> localIpv4Addresses();
QString bestLocalIpv4();
QString localIpv4ForPeer(const QHostAddress &peer);

} // namespace NetworkAddressUtils
