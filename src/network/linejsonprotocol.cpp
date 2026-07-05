#include "linejsonprotocol.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace LineJsonProtocol {

QByteArray encode(const QJsonObject &message)
{
    QByteArray data = QJsonDocument(message).toJson(QJsonDocument::Compact);
    data.append('\n');
    return data;
}

QList<QJsonObject> takeMessages(QByteArray *buffer, QStringList *errors)
{
    QList<QJsonObject> messages;
    if (!buffer)
        return messages;

    while (true) {
        const int newlineIndex = buffer->indexOf('\n');
        if (newlineIndex < 0)
            break;

        const QByteArray line = buffer->left(newlineIndex).trimmed();
        buffer->remove(0, newlineIndex + 1);

        if (line.isEmpty())
            continue;

        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(line, &error);
        if (error.error != QJsonParseError::NoError) {
            if (errors)
                errors->append(error.errorString());
            continue;
        }

        if (!document.isObject()) {
            if (errors)
                errors->append(QStringLiteral("JSON message is not an object"));
            continue;
        }

        messages.append(document.object());
    }

    return messages;
}

} // namespace LineJsonProtocol
