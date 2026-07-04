#pragma once

#include <QObject>
#include <QJsonObject>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class DouDiZhuController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList playerHand READ playerHand NOTIFY stateChanged)
    Q_PROPERTY(QVariantList landlordCards READ landlordCards NOTIFY stateChanged)
    Q_PROPERTY(int currentPlayer READ currentPlayer NOTIFY stateChanged)
    Q_PROPERTY(int localPlayer READ localPlayer NOTIFY stateChanged)
    Q_PROPERTY(int landlord READ landlord NOTIFY stateChanged)
    Q_PROPERTY(int leftOpponentPlayer READ leftOpponentPlayer NOTIFY stateChanged)
    Q_PROPERTY(int rightOpponentPlayer READ rightOpponentPlayer NOTIFY stateChanged)
    Q_PROPERTY(int leftOpponentCount READ leftOpponentCount NOTIFY stateChanged)
    Q_PROPERTY(int rightOpponentCount READ rightOpponentCount NOTIFY stateChanged)
    Q_PROPERTY(bool gameOver READ isGameOver NOTIFY stateChanged)
    Q_PROPERTY(int winner READ winner NOTIFY stateChanged)
    Q_PROPERTY(bool canPass READ canPass NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QString turnText READ turnText NOTIFY stateChanged)
    Q_PROPERTY(QString lastPlayText READ lastPlayText NOTIFY stateChanged)
    Q_PROPERTY(QVariantList lastPlayedCards READ lastPlayedCards NOTIFY stateChanged)
    Q_PROPERTY(QString resultText READ resultText NOTIFY stateChanged)

public:
    explicit DouDiZhuController(QObject *parent = nullptr);

    QVariantList playerHand() const;
    QVariantList landlordCards() const;
    int currentPlayer() const { return m_currentPlayer; }
    int localPlayer() const { return m_localPlayer; }
    int landlord() const { return m_landlord; }
    int leftOpponentPlayer() const { return (m_localPlayer + 1) % 3; }
    int rightOpponentPlayer() const { return (m_localPlayer + 2) % 3; }
    int leftOpponentCount() const { return m_cardCounts[leftOpponentPlayer()]; }
    int rightOpponentCount() const { return m_cardCounts[rightOpponentPlayer()]; }
    bool isGameOver() const { return m_gameOver; }
    int winner() const { return m_winner; }
    bool canPass() const;
    QString statusText() const { return m_statusText; }
    QString turnText() const;
    QString lastPlayText() const;
    QVariantList lastPlayedCards() const;
    QString resultText() const;

    Q_INVOKABLE void startNewGame();
    Q_INVOKABLE void startNetworkGame(int localPlayer = 0);
    Q_INVOKABLE void setLocalPlayer(int player);
    Q_INVOKABLE void applyNetworkState(const QJsonObject &state);
    Q_INVOKABLE bool playCards(const QVariantList &cardIds);
    Q_INVOKABLE bool playCardsForPlayer(int player, const QVariantList &cardIds);
    Q_INVOKABLE bool passTurn();
    Q_INVOKABLE bool passForPlayer(int player);
    QJsonObject stateForPlayer(int player) const;

signals:
    void stateChanged();

private:
    struct Card {
        int id = -1;
        int rank = 0;
        int suit = 0;
    };

    enum class HandType {
        Invalid,
        Single,
        Pair,
        Triple,
        TripleSingle,
        TriplePair,
        Straight,
        PairStraight,
        Plane,
        PlaneSingles,
        PlanePairs,
        Bomb,
        Rocket
    };

    struct HandAnalysis {
        HandType type = HandType::Invalid;
        int mainRank = 0;
        int length = 0;
        int tripleCount = 0;
        QVector<Card> cards;

        bool isValid() const { return type != HandType::Invalid; }
    };

    QVariantMap cardToMap(const Card &card) const;
    QVariantList cardsToVariantList(const QVector<Card> &cards) const;

    void buildDeck(QVector<Card> *deck) const;
    void sortHand(QVector<Card> *hand) const;
    QVector<Card> selectedCardsFromIds(int player, const QVariantList &cardIds) const;
    HandAnalysis analyzeHand(const QVector<Card> &cards) const;
    bool canBeat(const HandAnalysis &candidate, const HandAnalysis &base) const;

    void processAiTurns();
    QVector<Card> chooseAiPlay(int player) const;
    QVector<Card> chooseLead(int player) const;
    QVector<Card> chooseFollow(int player) const;
    QVector<Card> takeCardsByRank(const QVector<Card> &hand, int rank, int count) const;
    QVector<Card> takeSequence(const QVector<Card> &hand, int rankCount, int cardsPerRank, int minMainRank) const;
    QVector<Card> takePlaneWithWings(const QVector<Card> &hand, int planeCount, int wingSize, int minMainRank) const;
    QVector<Card> takeSmallestBomb(const QVector<Card> &hand, int minRank) const;
    QVector<Card> takeRocket(const QVector<Card> &hand) const;

    void playInternal(int player, const HandAnalysis &analysis);
    void passInternal(int player);
    void finishGame(int winner);
    void updateCardCounts();

    QString playerName(int player) const;
    QString handTypeName(HandType type) const;
    QString cardRankText(int rank) const;
    QString cardSuitText(int suit) const;
    QString cardsText(const QVector<Card> &cards) const;
    bool isConsecutive(const QVector<int> &ranks, int minLength) const;

    QVector<Card> m_hands[3];
    QVector<Card> m_landlordCards;
    int m_cardCounts[3] = {};
    int m_currentPlayer = 0;
    int m_localPlayer = 0;
    int m_landlord = 0;
    int m_lastPlayer = -1;
    int m_passCount = 0;
    int m_winner = -1;
    bool m_gameOver = false;
    bool m_autoAi = true;
    bool m_aiTurnPending = false;
    int m_aiTurnToken = 0;
    QString m_statusText;
    HandAnalysis m_lastPlay;
};
