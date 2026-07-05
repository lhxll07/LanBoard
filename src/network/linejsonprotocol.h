#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QStringList>

namespace LineJsonProtocol {

QByteArray encode(const QJsonObject &message);
QList<QJsonObject> takeMessages(QByteArray *buffer, QStringList *errors = nullptr);

} // namespace LineJsonProtocol
