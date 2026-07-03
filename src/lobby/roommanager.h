#pragma once

#include <QObject>
#include <QVariantList>
#include <QVector>

class RoomManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList playerList READ playerList NOTIFY playerListChanged)
    Q_PROPERTY(bool isHost READ isHost NOTIFY hostChanged)
    Q_PROPERTY(bool canStart READ canStart NOTIFY playerListChanged)
    Q_PROPERTY(int localPlayerIndex READ localPlayerIndex CONSTANT)

public:
    explicit RoomManager(QObject *parent = nullptr);

    Q_INVOKABLE void addPlayer(const QString &name, bool host = false);
    Q_INVOKABLE void toggleReady();
    Q_INVOKABLE void startGame();

    QVariantList playerList() const;
    bool isHost() const;
    bool canStart() const;
    int localPlayerIndex() const { return 0; }

signals:
    void playerListChanged();
    void hostChanged();
    void gameStarted();

private:
    struct Player {
        QString name;
        bool isHost = false;
        bool isReady = false;
    };

    QVector<Player> m_players;
};
