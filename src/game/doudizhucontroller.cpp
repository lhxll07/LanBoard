#include "doudizhucontroller.h"

#include <QMap>
#include <QRandomGenerator>
#include <QSet>
#include <QStringList>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>

#include <algorithm>
#include <random>

namespace {

QVector<int> sortedRanksFromCounts(const QMap<int, int> &counts)
{
    QVector<int> ranks;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it)
        ranks.append(it.key());
    std::sort(ranks.begin(), ranks.end());
    return ranks;
}

}

DouDiZhuController::DouDiZhuController(QObject *parent)
    : QObject(parent)
{
    startNewGame();
}

QVariantList DouDiZhuController::playerHand() const
{
    return cardsToVariantList(m_hands[m_localPlayer]);
}

QVariantList DouDiZhuController::landlordCards() const
{
    return cardsToVariantList(m_landlordCards);
}

bool DouDiZhuController::canPass() const
{
    return !m_gameOver
        && m_currentPlayer == m_localPlayer
        && m_lastPlayer >= 0
        && m_lastPlayer != m_currentPlayer
        && m_lastPlay.isValid();
}

QString DouDiZhuController::turnText() const
{
    if (m_gameOver)
        return resultText();

    if (m_currentPlayer == m_localPlayer)
        return QStringLiteral("\u8f6e\u5230\u4f60\u51fa\u724c");

    return QStringLiteral("%1\u6b63\u5728\u51fa\u724c").arg(playerName(m_currentPlayer));
}

QString DouDiZhuController::lastPlayText() const
{
    if (!m_lastPlay.isValid())
        return QStringLiteral("\u6682\u65e0\u4e0a\u624b\uff0c\u53ef\u4ee5\u4efb\u610f\u51fa\u724c");

    return QStringLiteral("%1\uff1a%2  %3")
        .arg(playerName(m_lastPlayer), handTypeName(m_lastPlay.type), cardsText(m_lastPlay.cards));
}

QVariantList DouDiZhuController::lastPlayedCards() const
{
    if (!m_lastPlay.isValid())
        return {};

    return cardsToVariantList(m_lastPlay.cards);
}

QString DouDiZhuController::resultText() const
{
    if (!m_gameOver)
        return QString();

    if (m_winner == 0)
        return QStringLiteral("\u4f60\u8d62\u4e86");

    return QStringLiteral("\u519c\u6c11\u80dc\u5229");
}

void DouDiZhuController::startNewGame()
{
    ++m_aiTurnToken;
    m_aiTurnPending = false;
    m_autoAi = true;
    m_localPlayer = 0;

    QVector<Card> deck;
    buildDeck(&deck);

    std::mt19937 generator(QRandomGenerator::global()->generate());
    std::shuffle(deck.begin(), deck.end(), generator);

    for (auto &hand : m_hands)
        hand.clear();
    m_landlordCards.clear();

    for (int i = 0; i < 51; ++i)
        m_hands[i % 3].append(deck.at(i));

    for (int i = 51; i < deck.size(); ++i)
        m_landlordCards.append(deck.at(i));

    m_landlord = 0;
    for (const auto &card : m_landlordCards)
        m_hands[m_landlord].append(card);

    for (auto &hand : m_hands)
        sortHand(&hand);
    sortHand(&m_landlordCards);
    updateCardCounts();

    m_currentPlayer = m_landlord;
    m_lastPlayer = -1;
    m_passCount = 0;
    m_winner = -1;
    m_gameOver = false;
    m_lastPlay = HandAnalysis();
    m_statusText = QStringLiteral("\u4f60\u662f\u5730\u4e3b\uff0c\u5148\u51fa\u724c");

    emit stateChanged();
}

void DouDiZhuController::startNetworkGame(int localPlayer)
{
    startNewGame();
    ++m_aiTurnToken;
    m_aiTurnPending = false;
    m_autoAi = false;
    setLocalPlayer(localPlayer);
    m_statusText = QStringLiteral("联机斗地主开始，房主为地主");
    emit stateChanged();
}

void DouDiZhuController::setLocalPlayer(int player)
{
    if (player < 0 || player > 2)
        return;
    if (m_localPlayer == player)
        return;

    m_localPlayer = player;
    emit stateChanged();
}

bool DouDiZhuController::playCards(const QVariantList &cardIds)
{
    if (m_gameOver || m_currentPlayer != m_localPlayer)
        return false;

    const QVector<Card> cards = selectedCardsFromIds(m_localPlayer, cardIds);
    const HandAnalysis analysis = analyzeHand(cards);
    if (!analysis.isValid()) {
        m_statusText = QStringLiteral("\u8fd9\u7ec4\u724c\u578b\u4e0d\u5408\u6cd5");
        emit stateChanged();
        return false;
    }

    if (!canBeat(analysis, m_lastPlay)) {
        m_statusText = QStringLiteral("\u538b\u4e0d\u8fc7\u4e0a\u4e00\u624b");
        emit stateChanged();
        return false;
    }

    playInternal(m_localPlayer, analysis);
    if (m_autoAi && !m_gameOver)
        processAiTurns();

    emit stateChanged();
    return true;
}

bool DouDiZhuController::playCardsForPlayer(int player, const QVariantList &cardIds)
{
    if (m_gameOver || player < 0 || player > 2 || m_currentPlayer != player)
        return false;

    const QVector<Card> cards = selectedCardsFromIds(player, cardIds);
    const HandAnalysis analysis = analyzeHand(cards);
    if (!analysis.isValid()) {
        m_statusText = QStringLiteral("这组牌型不合法");
        emit stateChanged();
        return false;
    }

    if (!canBeat(analysis, m_lastPlay)) {
        m_statusText = QStringLiteral("压不过上一手");
        emit stateChanged();
        return false;
    }

    playInternal(player, analysis);
    emit stateChanged();
    return true;
}

bool DouDiZhuController::passTurn()
{
    if (m_gameOver || m_currentPlayer != m_localPlayer)
        return false;

    if (!canPass()) {
        m_statusText = QStringLiteral("\u5f53\u524d\u4e0d\u80fd\u4e0d\u8981");
        emit stateChanged();
        return false;
    }

    passInternal(m_localPlayer);
    if (m_autoAi && !m_gameOver)
        processAiTurns();

    emit stateChanged();
    return true;
}

bool DouDiZhuController::passForPlayer(int player)
{
    if (m_gameOver || player < 0 || player > 2 || m_currentPlayer != player)
        return false;
    if (m_lastPlayer < 0 || m_lastPlayer == player || !m_lastPlay.isValid()) {
        m_statusText = QStringLiteral("当前不能不要");
        emit stateChanged();
        return false;
    }

    passInternal(player);
    emit stateChanged();
    return true;
}

QJsonObject DouDiZhuController::stateForPlayer(int player) const
{
    auto cardToJson = [](const Card &card) {
        QJsonObject obj;
        obj[QStringLiteral("id")] = card.id;
        obj[QStringLiteral("rank")] = card.rank;
        obj[QStringLiteral("suit")] = card.suit;
        return obj;
    };

    auto cardsToJson = [&cardToJson](const QVector<Card> &cards) {
        QJsonArray array;
        for (const auto &card : cards)
            array.append(cardToJson(card));
        return array;
    };

    QJsonArray counts;
    for (int i = 0; i < 3; ++i)
        counts.append(m_hands[i].size());

    QJsonObject lastPlay;
    lastPlay[QStringLiteral("type")] = static_cast<int>(m_lastPlay.type);
    lastPlay[QStringLiteral("mainRank")] = m_lastPlay.mainRank;
    lastPlay[QStringLiteral("length")] = m_lastPlay.length;
    lastPlay[QStringLiteral("tripleCount")] = m_lastPlay.tripleCount;
    lastPlay[QStringLiteral("cards")] = cardsToJson(m_lastPlay.cards);

    QJsonObject state;
    state[QStringLiteral("localPlayer")] = player;
    state[QStringLiteral("landlord")] = m_landlord;
    state[QStringLiteral("currentPlayer")] = m_currentPlayer;
    state[QStringLiteral("lastPlayer")] = m_lastPlayer;
    state[QStringLiteral("passCount")] = m_passCount;
    state[QStringLiteral("winner")] = m_winner;
    state[QStringLiteral("gameOver")] = m_gameOver;
    state[QStringLiteral("statusText")] = m_statusText;
    state[QStringLiteral("hand")] = cardsToJson(player >= 0 && player < 3 ? m_hands[player] : QVector<Card>());
    state[QStringLiteral("landlordCards")] = cardsToJson(m_landlordCards);
    state[QStringLiteral("cardCounts")] = counts;
    state[QStringLiteral("lastPlay")] = lastPlay;
    return state;
}

void DouDiZhuController::applyNetworkState(const QJsonObject &state)
{
    ++m_aiTurnToken;
    m_aiTurnPending = false;

    auto cardFromJson = [](const QJsonObject &obj) {
        return Card{
            obj.value(QStringLiteral("id")).toInt(-1),
            obj.value(QStringLiteral("rank")).toInt(),
            obj.value(QStringLiteral("suit")).toInt()
        };
    };

    auto cardsFromJson = [&cardFromJson](const QJsonArray &array) {
        QVector<Card> cards;
        for (const auto &value : array) {
            if (value.isObject())
                cards.append(cardFromJson(value.toObject()));
        }
        return cards;
    };

    m_autoAi = false;
    const int player = state.value(QStringLiteral("localPlayer")).toInt(m_localPlayer);
    if (player >= 0 && player <= 2)
        m_localPlayer = player;

    for (auto &hand : m_hands)
        hand.clear();
    m_hands[m_localPlayer] = cardsFromJson(state.value(QStringLiteral("hand")).toArray());
    sortHand(&m_hands[m_localPlayer]);

    m_landlordCards = cardsFromJson(state.value(QStringLiteral("landlordCards")).toArray());
    sortHand(&m_landlordCards);

    const QJsonArray counts = state.value(QStringLiteral("cardCounts")).toArray();
    for (int i = 0; i < 3; ++i)
        m_cardCounts[i] = i < counts.size() ? counts.at(i).toInt() : m_hands[i].size();

    m_currentPlayer = state.value(QStringLiteral("currentPlayer")).toInt(0);
    m_landlord = state.value(QStringLiteral("landlord")).toInt(0);
    m_lastPlayer = state.value(QStringLiteral("lastPlayer")).toInt(-1);
    m_passCount = state.value(QStringLiteral("passCount")).toInt(0);
    m_winner = state.value(QStringLiteral("winner")).toInt(-1);
    m_gameOver = state.value(QStringLiteral("gameOver")).toBool(false);
    m_statusText = state.value(QStringLiteral("statusText")).toString();

    const QJsonObject last = state.value(QStringLiteral("lastPlay")).toObject();
    m_lastPlay = HandAnalysis();
    m_lastPlay.type = static_cast<HandType>(last.value(QStringLiteral("type")).toInt(0));
    m_lastPlay.mainRank = last.value(QStringLiteral("mainRank")).toInt(0);
    m_lastPlay.length = last.value(QStringLiteral("length")).toInt(0);
    m_lastPlay.tripleCount = last.value(QStringLiteral("tripleCount")).toInt(0);
    m_lastPlay.cards = cardsFromJson(last.value(QStringLiteral("cards")).toArray());

    emit stateChanged();
}

QVariantMap DouDiZhuController::cardToMap(const Card &card) const
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), card.id);
    map.insert(QStringLiteral("rank"), card.rank);
    map.insert(QStringLiteral("rankText"), cardRankText(card.rank));
    map.insert(QStringLiteral("suitText"), cardSuitText(card.suit));
    map.insert(QStringLiteral("displayText"),
               card.rank >= 16 ? cardRankText(card.rank)
                               : cardSuitText(card.suit) + cardRankText(card.rank));
    map.insert(QStringLiteral("red"), card.suit == 1 || card.suit == 3 || card.rank == 17);
    return map;
}

QVariantList DouDiZhuController::cardsToVariantList(const QVector<Card> &cards) const
{
    QVariantList list;
    for (const auto &card : cards)
        list.append(cardToMap(card));
    return list;
}

void DouDiZhuController::buildDeck(QVector<Card> *deck) const
{
    deck->clear();
    int id = 0;
    for (int rank = 3; rank <= 15; ++rank) {
        for (int suit = 0; suit < 4; ++suit)
            deck->append(Card{id++, rank, suit});
    }
    deck->append(Card{id++, 16, 4});
    deck->append(Card{id++, 17, 4});
}

void DouDiZhuController::sortHand(QVector<Card> *hand) const
{
    std::sort(hand->begin(), hand->end(), [](const Card &a, const Card &b) {
        if (a.rank != b.rank)
            return a.rank < b.rank;
        return a.suit < b.suit;
    });
}

QVector<DouDiZhuController::Card> DouDiZhuController::selectedCardsFromIds(int player, const QVariantList &cardIds) const
{
    if (player < 0 || player > 2)
        return {};

    QSet<int> ids;
    for (const auto &value : cardIds)
        ids.insert(value.toInt());

    QVector<Card> cards;
    for (const auto &card : m_hands[player]) {
        if (ids.contains(card.id))
            cards.append(card);
    }

    return cards.size() == ids.size() ? cards : QVector<Card>();
}

DouDiZhuController::HandAnalysis DouDiZhuController::analyzeHand(const QVector<Card> &cards) const
{
    HandAnalysis result;
    if (cards.isEmpty())
        return result;

    result.cards = cards;
    result.length = cards.size();

    QMap<int, int> counts;
    for (const auto &card : cards)
        ++counts[card.rank];

    const QVector<int> ranks = sortedRanksFromCounts(counts);
    const int n = cards.size();

    if (n == 1) {
        result.type = HandType::Single;
        result.mainRank = cards.first().rank;
        return result;
    }

    if (n == 2) {
        if (counts.contains(16) && counts.contains(17)) {
            result.type = HandType::Rocket;
            result.mainRank = 17;
            return result;
        }
        if (counts.size() == 1) {
            result.type = HandType::Pair;
            result.mainRank = ranks.first();
            return result;
        }
        return result;
    }

    if (n == 3 && counts.size() == 1) {
        result.type = HandType::Triple;
        result.mainRank = ranks.first();
        return result;
    }

    if (n == 4) {
        for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
            if (it.value() == 4) {
                result.type = HandType::Bomb;
                result.mainRank = it.key();
                return result;
            }
            if (it.value() == 3) {
                result.type = HandType::TripleSingle;
                result.mainRank = it.key();
                return result;
            }
        }
    }

    if (n == 5) {
        bool hasTriple = false;
        bool hasPair = false;
        int tripleRank = 0;
        for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
            hasTriple = hasTriple || it.value() == 3;
            hasPair = hasPair || it.value() == 2;
            if (it.value() == 3)
                tripleRank = it.key();
        }
        if (hasTriple && hasPair) {
            result.type = HandType::TriplePair;
            result.mainRank = tripleRank;
            return result;
        }
    }

    bool allSingles = true;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it)
        allSingles = allSingles && it.value() == 1;
    if (allSingles && isConsecutive(ranks, 5)) {
        result.type = HandType::Straight;
        result.mainRank = ranks.last();
        return result;
    }

    bool allPairs = n >= 6 && n % 2 == 0;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it)
        allPairs = allPairs && it.value() == 2;
    if (allPairs && isConsecutive(ranks, 3)) {
        result.type = HandType::PairStraight;
        result.mainRank = ranks.last();
        return result;
    }

    QVector<int> tripleRanks;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
        if (it.value() == 3)
            tripleRanks.append(it.key());
    }
    std::sort(tripleRanks.begin(), tripleRanks.end());

    if (tripleRanks.size() >= 2 && isConsecutive(tripleRanks, 2)) {
        if (n == tripleRanks.size() * 3 && counts.size() == tripleRanks.size()) {
            result.type = HandType::Plane;
            result.mainRank = tripleRanks.last();
            result.tripleCount = tripleRanks.size();
            return result;
        }

        if (n == tripleRanks.size() * 4) {
            int wings = 0;
            bool singleWings = true;
            for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
                if (tripleRanks.contains(it.key()))
                    continue;
                singleWings = singleWings && it.value() == 1;
                wings += it.value();
            }
            if (singleWings && wings == tripleRanks.size()) {
                result.type = HandType::PlaneSingles;
                result.mainRank = tripleRanks.last();
                result.tripleCount = tripleRanks.size();
                return result;
            }
        }

        if (n == tripleRanks.size() * 5) {
            int pairWings = 0;
            bool validPairWings = true;
            for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
                if (tripleRanks.contains(it.key()))
                    continue;
                validPairWings = validPairWings && it.value() == 2;
                if (it.value() == 2)
                    ++pairWings;
            }
            if (validPairWings && pairWings == tripleRanks.size()) {
                result.type = HandType::PlanePairs;
                result.mainRank = tripleRanks.last();
                result.tripleCount = tripleRanks.size();
                return result;
            }
        }
    }

    return result;
}

bool DouDiZhuController::canBeat(const HandAnalysis &candidate, const HandAnalysis &base) const
{
    if (!candidate.isValid())
        return false;
    if (!base.isValid())
        return true;
    if (candidate.type == HandType::Rocket)
        return true;
    if (base.type == HandType::Rocket)
        return false;
    if (candidate.type == HandType::Bomb) {
        if (base.type != HandType::Bomb)
            return true;
        return candidate.mainRank > base.mainRank;
    }
    if (base.type == HandType::Bomb)
        return false;
    if (candidate.type != base.type)
        return false;
    if (candidate.length != base.length)
        return false;
    if (candidate.tripleCount != base.tripleCount)
        return false;
    return candidate.mainRank > base.mainRank;
}

void DouDiZhuController::processAiTurns()
{
    if (!m_autoAi || m_gameOver || m_currentPlayer == m_localPlayer || m_aiTurnPending)
        return;

    m_aiTurnPending = true;
    const int token = ++m_aiTurnToken;

    QTimer::singleShot(3000, this, [this, token]() {
        if (token != m_aiTurnToken)
            return;

        m_aiTurnPending = false;
        if (!m_autoAi || m_gameOver || m_currentPlayer == m_localPlayer)
            return;

        const QVector<Card> cards = chooseAiPlay(m_currentPlayer);
        if (cards.isEmpty()) {
            passInternal(m_currentPlayer);
        } else {
            const HandAnalysis analysis = analyzeHand(cards);
            if (analysis.isValid() && canBeat(analysis, m_lastPlay))
                playInternal(m_currentPlayer, analysis);
            else
                passInternal(m_currentPlayer);
        }

        emit stateChanged();
        processAiTurns();
    });
}

QVector<DouDiZhuController::Card> DouDiZhuController::chooseAiPlay(int player) const
{
    if (!m_lastPlay.isValid() || m_lastPlayer == player)
        return chooseLead(player);
    return chooseFollow(player);
}

QVector<DouDiZhuController::Card> DouDiZhuController::chooseLead(int player) const
{
    const auto &hand = m_hands[player];
    if (hand.isEmpty())
        return {};

    const HandAnalysis allCards = analyzeHand(hand);
    if (hand.size() <= 8 && allCards.isValid())
        return hand;

    return {hand.first()};
}

QVector<DouDiZhuController::Card> DouDiZhuController::chooseFollow(int player) const
{
    const auto &hand = m_hands[player];
    QVector<Card> candidate;

    switch (m_lastPlay.type) {
    case HandType::Single:
        for (const auto &card : hand) {
            if (card.rank > m_lastPlay.mainRank)
                return {card};
        }
        break;
    case HandType::Pair:
        for (int rank = m_lastPlay.mainRank + 1; rank <= 17; ++rank) {
            candidate = takeCardsByRank(hand, rank, 2);
            if (!candidate.isEmpty())
                return candidate;
        }
        break;
    case HandType::Triple:
        for (int rank = m_lastPlay.mainRank + 1; rank <= 17; ++rank) {
            candidate = takeCardsByRank(hand, rank, 3);
            if (!candidate.isEmpty())
                return candidate;
        }
        break;
    case HandType::TripleSingle:
        for (int rank = m_lastPlay.mainRank + 1; rank <= 15; ++rank) {
            candidate = takeCardsByRank(hand, rank, 3);
            if (candidate.isEmpty())
                continue;
            for (const auto &card : hand) {
                if (card.rank != rank) {
                    candidate.append(card);
                    return candidate;
                }
            }
        }
        break;
    case HandType::TriplePair:
        for (int rank = m_lastPlay.mainRank + 1; rank <= 15; ++rank) {
            candidate = takeCardsByRank(hand, rank, 3);
            if (candidate.isEmpty())
                continue;
            for (int pairRank = 3; pairRank <= 17; ++pairRank) {
                if (pairRank == rank)
                    continue;
                QVector<Card> pair = takeCardsByRank(hand, pairRank, 2);
                if (!pair.isEmpty()) {
                    candidate += pair;
                    return candidate;
                }
            }
        }
        break;
    case HandType::Straight:
        candidate = takeSequence(hand, m_lastPlay.length, 1, m_lastPlay.mainRank);
        if (!candidate.isEmpty())
            return candidate;
        break;
    case HandType::PairStraight:
        candidate = takeSequence(hand, m_lastPlay.length / 2, 2, m_lastPlay.mainRank);
        if (!candidate.isEmpty())
            return candidate;
        break;
    case HandType::Plane:
        candidate = takeSequence(hand, m_lastPlay.tripleCount, 3, m_lastPlay.mainRank);
        if (!candidate.isEmpty())
            return candidate;
        break;
    case HandType::PlaneSingles:
        candidate = takePlaneWithWings(hand, m_lastPlay.tripleCount, 1, m_lastPlay.mainRank);
        if (!candidate.isEmpty())
            return candidate;
        break;
    case HandType::PlanePairs:
        candidate = takePlaneWithWings(hand, m_lastPlay.tripleCount, 2, m_lastPlay.mainRank);
        if (!candidate.isEmpty())
            return candidate;
        break;
    case HandType::Bomb:
        candidate = takeSmallestBomb(hand, m_lastPlay.mainRank);
        if (!candidate.isEmpty())
            return candidate;
        break;
    case HandType::Rocket:
    case HandType::Invalid:
        break;
    }

    if (m_lastPlay.type != HandType::Bomb && m_lastPlay.type != HandType::Rocket) {
        candidate = takeSmallestBomb(hand, 0);
        if (!candidate.isEmpty())
            return candidate;
    }

    return takeRocket(hand);
}

QVector<DouDiZhuController::Card> DouDiZhuController::takeCardsByRank(const QVector<Card> &hand, int rank, int count) const
{
    QVector<Card> cards;
    for (const auto &card : hand) {
        if (card.rank == rank)
            cards.append(card);
        if (cards.size() == count)
            return cards;
    }
    return {};
}

QVector<DouDiZhuController::Card> DouDiZhuController::takeSequence(
    const QVector<Card> &hand, int rankCountNeeded, int cardsPerRank, int minMainRank) const
{
    if (rankCountNeeded <= 0)
        return {};

    for (int start = 3; start + rankCountNeeded - 1 <= 14; ++start) {
        const int high = start + rankCountNeeded - 1;
        if (high <= minMainRank)
            continue;

        QVector<Card> cards;
        bool ok = true;
        for (int rank = start; rank <= high; ++rank) {
            const QVector<Card> rankCards = takeCardsByRank(hand, rank, cardsPerRank);
            if (rankCards.isEmpty()) {
                ok = false;
                break;
            }
            cards += rankCards;
        }
        if (ok)
            return cards;
    }

    return {};
}

QVector<DouDiZhuController::Card> DouDiZhuController::takePlaneWithWings(
    const QVector<Card> &hand, int planeCount, int wingSize, int minMainRank) const
{
    QVector<Card> triples = takeSequence(hand, planeCount, 3, minMainRank);
    if (triples.isEmpty())
        return {};

    QSet<int> tripleRanks;
    for (const auto &card : triples)
        tripleRanks.insert(card.rank);

    QVector<Card> wings;
    if (wingSize == 1) {
        QSet<int> usedWingRanks;
        for (const auto &card : hand) {
            if (tripleRanks.contains(card.rank) || usedWingRanks.contains(card.rank))
                continue;
            wings.append(card);
            usedWingRanks.insert(card.rank);
            if (wings.size() == planeCount)
                break;
        }
    } else {
        for (int rank = 3; rank <= 17; ++rank) {
            if (tripleRanks.contains(rank))
                continue;
            QVector<Card> pair = takeCardsByRank(hand, rank, 2);
            if (pair.isEmpty())
                continue;
            wings += pair;
            if (wings.size() == planeCount * 2)
                break;
        }
    }

    if (wings.size() != planeCount * wingSize)
        return {};

    return triples + wings;
}

QVector<DouDiZhuController::Card> DouDiZhuController::takeSmallestBomb(const QVector<Card> &hand, int minRank) const
{
    for (int rank = minRank + 1; rank <= 15; ++rank) {
        QVector<Card> cards = takeCardsByRank(hand, rank, 4);
        if (!cards.isEmpty())
            return cards;
    }
    return {};
}

QVector<DouDiZhuController::Card> DouDiZhuController::takeRocket(const QVector<Card> &hand) const
{
    QVector<Card> smallJoker = takeCardsByRank(hand, 16, 1);
    QVector<Card> bigJoker = takeCardsByRank(hand, 17, 1);
    if (smallJoker.isEmpty() || bigJoker.isEmpty())
        return {};
    return smallJoker + bigJoker;
}

void DouDiZhuController::playInternal(int player, const HandAnalysis &analysis)
{
    for (const auto &playedCard : analysis.cards) {
        auto &hand = m_hands[player];
        for (int i = 0; i < hand.size(); ++i) {
            if (hand.at(i).id == playedCard.id) {
                hand.removeAt(i);
                break;
            }
        }
    }
    updateCardCounts();

    m_lastPlay = analysis;
    m_lastPlayer = player;
    m_passCount = 0;
    m_statusText = QStringLiteral("%1\u51fa\u4e86%2").arg(playerName(player), handTypeName(analysis.type));

    if (m_hands[player].isEmpty()) {
        finishGame(player);
        return;
    }

    m_currentPlayer = (player + 1) % 3;
}

void DouDiZhuController::passInternal(int player)
{
    ++m_passCount;
    m_statusText = QStringLiteral("%1\u4e0d\u8981").arg(playerName(player));

    if (m_passCount >= 2 && m_lastPlayer >= 0) {
        const int leader = m_lastPlayer;
        m_lastPlay = HandAnalysis();
        m_lastPlayer = -1;
        m_passCount = 0;
        m_currentPlayer = leader;
        m_statusText = QStringLiteral("\u5176\u4ed6\u73a9\u5bb6\u90fd\u4e0d\u8981\uff0c%1\u91cd\u65b0\u51fa\u724c")
            .arg(playerName(leader));
        return;
    }

    m_currentPlayer = (player + 1) % 3;
}

void DouDiZhuController::finishGame(int winner)
{
    m_winner = winner;
    m_gameOver = true;
    m_currentPlayer = winner;
    m_statusText = resultText();
}

void DouDiZhuController::updateCardCounts()
{
    for (int i = 0; i < 3; ++i)
        m_cardCounts[i] = m_hands[i].size();
}

QString DouDiZhuController::playerName(int player) const
{
    switch (player) {
    case 0:
        return QStringLiteral("\u4f60");
    case 1:
        return QStringLiteral("\u4e0b\u5bb6");
    case 2:
        return QStringLiteral("\u4e0a\u5bb6");
    default:
        return QStringLiteral("\u73a9\u5bb6");
    }
}

QString DouDiZhuController::handTypeName(HandType type) const
{
    switch (type) {
    case HandType::Single:
        return QStringLiteral("\u5355\u5f20");
    case HandType::Pair:
        return QStringLiteral("\u5bf9\u5b50");
    case HandType::Triple:
        return QStringLiteral("\u4e09\u5f20");
    case HandType::TripleSingle:
        return QStringLiteral("\u4e09\u5e26\u4e00");
    case HandType::TriplePair:
        return QStringLiteral("\u4e09\u5e26\u4e00\u5bf9");
    case HandType::Straight:
        return QStringLiteral("\u987a\u5b50");
    case HandType::PairStraight:
        return QStringLiteral("\u8fde\u5bf9");
    case HandType::Plane:
        return QStringLiteral("\u98de\u673a");
    case HandType::PlaneSingles:
        return QStringLiteral("\u98de\u673a\u5e26\u5355");
    case HandType::PlanePairs:
        return QStringLiteral("\u98de\u673a\u5e26\u5bf9");
    case HandType::Bomb:
        return QStringLiteral("\u70b8\u5f39");
    case HandType::Rocket:
        return QStringLiteral("\u738b\u70b8");
    case HandType::Invalid:
        break;
    }
    return QStringLiteral("\u672a\u77e5");
}

QString DouDiZhuController::cardRankText(int rank) const
{
    switch (rank) {
    case 11:
        return QStringLiteral("J");
    case 12:
        return QStringLiteral("Q");
    case 13:
        return QStringLiteral("K");
    case 14:
        return QStringLiteral("A");
    case 15:
        return QStringLiteral("2");
    case 16:
        return QStringLiteral("\u5c0f\u738b");
    case 17:
        return QStringLiteral("\u5927\u738b");
    default:
        return QString::number(rank);
    }
}

QString DouDiZhuController::cardSuitText(int suit) const
{
    switch (suit) {
    case 0:
        return QStringLiteral("\u2660");
    case 1:
        return QStringLiteral("\u2665");
    case 2:
        return QStringLiteral("\u2663");
    case 3:
        return QStringLiteral("\u2666");
    default:
        return QString();
    }
}

QString DouDiZhuController::cardsText(const QVector<Card> &cards) const
{
    QStringList pieces;
    QVector<Card> sorted = cards;
    sortHand(&sorted);
    for (const auto &card : sorted) {
        pieces.append(card.rank >= 16
                          ? cardRankText(card.rank)
                          : cardSuitText(card.suit) + cardRankText(card.rank));
    }
    return pieces.join(QStringLiteral(" "));
}

bool DouDiZhuController::isConsecutive(const QVector<int> &ranks, int minLength) const
{
    if (ranks.size() < minLength)
        return false;
    for (int i = 0; i < ranks.size(); ++i) {
        if (ranks.at(i) >= 15)
            return false;
        if (i > 0 && ranks.at(i) != ranks.at(i - 1) + 1)
            return false;
    }
    return true;
}
