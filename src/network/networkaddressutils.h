#pragma once

#include <QHostAddress>
#include <QString>

namespace NetworkAddressUtils {

bool isUsableIpv4(const QHostAddress &address);
QString bestLocalIpv4();
QString localIpv4ForPeer(const QHostAddress &peer);

} // namespace NetworkAddressUtils
