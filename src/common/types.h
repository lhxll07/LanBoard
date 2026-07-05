#pragma once

#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace LanBoard {

enum class NavigationPage {
    Home = 0,
    Room = 1,
    Gomoku = 2,
    Online = 3,
    DouDiZhu = 4,
    FlightChess = 5,
    Survivor = 6
};

struct GameDefinition {
    const char *id = "gomoku";
    const char *title = "\xe4\xba\x94\xe5\xad\x90\xe6\xa3\x8b";
    const char *subtitle = "\xe5\x8f\x8c\xe4\xba\xba\xe5\xaf\xb9\xe5\xbc\x88\xef\xbc\x8c\xe9\x80\x82\xe5\x90\x88\xe5\xbf\xab\xe9\x80\x9f\xe5\xbc\x80\xe5\xa7\x8b\xe3\x80\x82";
    int maxPlayers = 2;
    NavigationPage navigationPage = NavigationPage::Gomoku;
};

inline const GameDefinition &gomokuDefinition()
{
    static const GameDefinition definition {
        "gomoku",
        "\xe4\xba\x94\xe5\xad\x90\xe6\xa3\x8b",
        "\xe5\x8f\x8c\xe4\xba\xba\xe5\xaf\xb9\xe5\xbc\x88\xef\xbc\x8c\xe9\x80\x82\xe5\x90\x88\xe5\xbf\xab\xe9\x80\x9f\xe5\xbc\x80\xe5\xa7\x8b\xe3\x80\x82",
        2,
        NavigationPage::Gomoku
    };
    return definition;
}

inline const GameDefinition &douDiZhuDefinition()
{
    static const GameDefinition definition {
        "doudizhu",
        "\xe6\x96\x97\xe5\x9c\xb0\xe4\xb8\xbb",
        "\xe4\xb8\x89\xe4\xba\xba\xe6\x88\xbf\xef\xbc\x8c\xe5\x85\xa8\xe9\x83\xa8\xe5\x87\x86\xe5\xa4\x87\xe5\x90\x8e\xe7\x94\xb1\xe6\x88\xbf\xe4\xb8\xbb\xe5\xbc\x80\xe5\xb1\x80\xe3\x80\x82",
        3,
        NavigationPage::DouDiZhu
    };
    return definition;
}

inline const GameDefinition &flightChessDefinition()
{
    static const GameDefinition definition {
        "flightchess",
        "\xe9\xa3\x9e\xe8\xa1\x8c\xe6\xa3\x8b",
        "\xe5\x8f\x8c\xe4\xba\xba\xe9\xa3\x9e\xe8\xa1\x8c\xe6\xa3\x8b\xef\xbc\x8c\xe6\x94\xaf\xe6\x8c\x81\xe5\x9b\xb4\xe8\xa7\x82\xe5\x92\x8c\xe5\x88\x87\xe6\x8d\xa2\xe6\x97\x81\xe8\xa7\x82\xe4\xbd\x8d\xe3\x80\x82",
        2,
        NavigationPage::FlightChess
    };
    return definition;
}

inline const GameDefinition &survivorDefinition()
{
    static const GameDefinition definition {
        "survivor",
        "Survivor",
        "\xe5\xa4\x9a\xe4\xba\xba\xe8\x87\xaa\xe5\x8a\xa8\xe6\x94\xbb\xe5\x87\xbb\xe7\x94\x9f\xe5\xad\x98\xe5\x8e\x9f\xe5\x9e\x8b\xef\xbc\x8cMVP \xe9\x98\xb6\xe6\xae\xb5\xe4\xbc\x98\xe5\x85\x88\xe8\xb7\x91\xe9\x80\x9a\xe6\x88\xbf\xe9\x97\xb4\xe4\xb8\x8e\xe5\x8d\x95\xe6\x9c\xba\xe5\x8e\x9f\xe5\x9e\x8b\xe3\x80\x82",
        4,
        NavigationPage::Survivor
    };
    return definition;
}

inline const GameDefinition &definitionForGame(const QString &gameId)
{
    if (gameId == QStringLiteral("doudizhu"))
        return douDiZhuDefinition();
    if (gameId == QStringLiteral("flightchess"))
        return flightChessDefinition();
    if (gameId == QStringLiteral("survivor"))
        return survivorDefinition();
    return gomokuDefinition();
}

inline QString normalizeGameId(const QString &gameId)
{
    return QString::fromLatin1(definitionForGame(gameId).id);
}

inline QString gameName(const QString &gameId)
{
    return QString::fromUtf8(definitionForGame(gameId).title);
}

inline int maxPlayersForGame(const QString &gameId)
{
    return definitionForGame(gameId).maxPlayers;
}

inline int navigationPageForGame(const QString &gameId)
{
    return static_cast<int>(definitionForGame(gameId).navigationPage);
}

inline bool isDouDiZhuGame(const QString &gameId)
{
    return normalizeGameId(gameId) == QStringLiteral("doudizhu");
}

inline bool isFlightChessGame(const QString &gameId)
{
    return normalizeGameId(gameId) == QStringLiteral("flightchess");
}

inline QVariantMap gameVariantMap(const QString &gameId)
{
    const GameDefinition &definition = definitionForGame(gameId);
    QVariantMap map;
    map[QStringLiteral("gameId")] = QString::fromLatin1(definition.id);
    map[QStringLiteral("title")] = QString::fromUtf8(definition.title);
    map[QStringLiteral("subtitle")] = QString::fromUtf8(definition.subtitle);
    map[QStringLiteral("maxPlayers")] = definition.maxPlayers;
    map[QStringLiteral("page")] = static_cast<int>(definition.navigationPage);
    return map;
}

inline QVariantList availableGames()
{
    return {
        gameVariantMap(QStringLiteral("gomoku")),
        gameVariantMap(QStringLiteral("doudizhu")),
        gameVariantMap(QStringLiteral("flightchess")),
        gameVariantMap(QStringLiteral("survivor"))
    };
}

}  // namespace LanBoard
