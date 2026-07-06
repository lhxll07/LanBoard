#include "survivornetcodec.h"

#include <cstring>

#include <QtEndian>
#include <QtMath>

namespace LanBoard::Survivor::NetCodec {

namespace {

constexpr quint32 PacketMagic = 0x5642534Cu;  // LSBV
constexpr quint8 PacketVersion = 1;
constexpr qreal PositionScale = 1024.0;
constexpr qreal RadiusScale = 4096.0;
constexpr qreal ScalarScale = 1000.0;

class PacketWriter
{
public:
    void writeUInt8(quint8 value) { m_data.append(static_cast<char>(value)); }
    void writeBool(bool value) { writeUInt8(value ? 1 : 0); }

    void writeInt16(qint16 value)
    {
        const qint16 le = qToLittleEndian(value);
        m_data.append(reinterpret_cast<const char *>(&le), sizeof(le));
    }

    void writeUInt16(quint16 value)
    {
        const quint16 le = qToLittleEndian(value);
        m_data.append(reinterpret_cast<const char *>(&le), sizeof(le));
    }

    void writeInt32(qint32 value)
    {
        const qint32 le = qToLittleEndian(value);
        m_data.append(reinterpret_cast<const char *>(&le), sizeof(le));
    }

    void writeUInt32(quint32 value)
    {
        const quint32 le = qToLittleEndian(value);
        m_data.append(reinterpret_cast<const char *>(&le), sizeof(le));
    }

    void writeUInt64(quint64 value)
    {
        const quint64 le = qToLittleEndian(value);
        m_data.append(reinterpret_cast<const char *>(&le), sizeof(le));
    }

    void writeString(const QString &value)
    {
        QByteArray utf8 = value.toUtf8();
        if (utf8.size() > 0xffff)
            utf8.truncate(0xffff);
        writeUInt16(static_cast<quint16>(utf8.size()));
        m_data.append(utf8);
    }

    QByteArray take() { return std::move(m_data); }

private:
    QByteArray m_data;
};

class PacketReader
{
public:
    PacketReader(const char *data, qsizetype size)
        : m_cursor(data)
        , m_end(data + size)
    {
    }

    bool readUInt8(quint8 &value)
    {
        if (!ensure(sizeof(value)))
            return false;
        value = static_cast<quint8>(*m_cursor++);
        return true;
    }

    bool readBool(bool &value)
    {
        quint8 raw = 0;
        if (!readUInt8(raw))
            return false;
        value = raw != 0;
        return true;
    }

    bool readInt16(qint16 &value)
    {
        if (!ensure(sizeof(value)))
            return false;
        qint16 raw = 0;
        memcpy(&raw, m_cursor, sizeof(raw));
        m_cursor += sizeof(raw);
        value = qFromLittleEndian(raw);
        return true;
    }

    bool readUInt16(quint16 &value)
    {
        if (!ensure(sizeof(value)))
            return false;
        quint16 raw = 0;
        memcpy(&raw, m_cursor, sizeof(raw));
        m_cursor += sizeof(raw);
        value = qFromLittleEndian(raw);
        return true;
    }

    bool readInt32(qint32 &value)
    {
        if (!ensure(sizeof(value)))
            return false;
        qint32 raw = 0;
        memcpy(&raw, m_cursor, sizeof(raw));
        m_cursor += sizeof(raw);
        value = qFromLittleEndian(raw);
        return true;
    }

    bool readUInt32(quint32 &value)
    {
        if (!ensure(sizeof(value)))
            return false;
        quint32 raw = 0;
        memcpy(&raw, m_cursor, sizeof(raw));
        m_cursor += sizeof(raw);
        value = qFromLittleEndian(raw);
        return true;
    }

    bool readUInt64(quint64 &value)
    {
        if (!ensure(sizeof(value)))
            return false;
        quint64 raw = 0;
        memcpy(&raw, m_cursor, sizeof(raw));
        m_cursor += sizeof(raw);
        value = qFromLittleEndian(raw);
        return true;
    }

    bool readString(QString &value)
    {
        quint16 byteCount = 0;
        if (!readUInt16(byteCount) || !ensure(byteCount))
            return false;
        value = QString::fromUtf8(m_cursor, byteCount);
        m_cursor += byteCount;
        return true;
    }

private:
    bool ensure(qsizetype size) const { return m_cursor + size <= m_end; }

    const char *m_cursor = nullptr;
    const char *m_end = nullptr;
};

qint16 encodeRelativeCoord(qreal value, qreal origin)
{
    return static_cast<qint16>(qBound(-32768,
                                      qRound((value - origin) * PositionScale),
                                      32767));
}

qreal decodeRelativeCoord(qint16 encoded, qreal origin)
{
    return origin + static_cast<qreal>(encoded) / PositionScale;
}

qint32 encodeAbsoluteCoord(qreal value)
{
    return qRound(value * PositionScale);
}

qreal decodeAbsoluteCoord(qint32 encoded)
{
    return static_cast<qreal>(encoded) / PositionScale;
}

quint16 encodeRadius(qreal value)
{
    return static_cast<quint16>(qBound(0, qRound(value * RadiusScale), 0xffff));
}

qreal decodeRadius(quint16 encoded)
{
    return static_cast<qreal>(encoded) / RadiusScale;
}

qint16 encodeScalar(qreal value)
{
    return static_cast<qint16>(qBound(-32768, qRound(value * ScalarScale), 32767));
}

qreal decodeScalar(qint16 encoded)
{
    return static_cast<qreal>(encoded) / ScalarScale;
}

void writeHeader(PacketWriter &writer, PacketKind kind)
{
    writer.writeUInt32(PacketMagic);
    writer.writeUInt8(PacketVersion);
    writer.writeUInt8(static_cast<quint8>(kind));
}

bool readHeader(PacketReader &reader, PacketKind expectedKind)
{
    quint32 magic = 0;
    quint8 version = 0;
    quint8 kind = 0;
    return reader.readUInt32(magic)
        && reader.readUInt8(version)
        && reader.readUInt8(kind)
        && magic == PacketMagic
        && version == PacketVersion
        && kind == static_cast<quint8>(expectedKind);
}

PacketKind packetKind(const char *data, qsizetype size)
{
    if (!data || size < 6)
        return PacketKind::Invalid;

    PacketReader reader(data, size);
    quint32 magic = 0;
    quint8 version = 0;
    quint8 kind = 0;
    if (!reader.readUInt32(magic)
        || !reader.readUInt8(version)
        || !reader.readUInt8(kind)
        || magic != PacketMagic
        || version != PacketVersion) {
        return PacketKind::Invalid;
    }
    return static_cast<PacketKind>(kind);
}

void writePlayerCore(PacketWriter &writer,
                     const PlayerState &player,
                     const QVector2D &origin)
{
    writer.writeInt16(static_cast<qint16>(player.playerId));
    writer.writeInt16(encodeRelativeCoord(player.position.x(), origin.x()));
    writer.writeInt16(encodeRelativeCoord(player.position.y(), origin.y()));
    writer.writeInt16(static_cast<qint16>(player.hp));
    writer.writeInt16(static_cast<qint16>(player.maxHp));
    writer.writeUInt8(player.alive ? 1 : 0);
    writer.writeUInt8(static_cast<quint8>(qBound(0, player.colorIndex, 255)));
}

void writePlayerProgression(PacketWriter &writer, const PlayerState &player)
{
    writer.writeInt16(static_cast<qint16>(player.playerId));
    writer.writeInt16(static_cast<qint16>(player.hp));
    writer.writeInt16(static_cast<qint16>(player.maxHp));
    writer.writeBool(player.alive);
    writer.writeUInt8(static_cast<quint8>(qBound(0, player.colorIndex, 255)));
    writer.writeInt16(static_cast<qint16>(player.level));
    writer.writeInt32(player.exp);
    writer.writeInt32(player.expToNext);
    writer.writeInt16(static_cast<qint16>(player.attackDamage));
    writer.writeUInt8(static_cast<quint8>(player.bladeWeaponLevel));
    writer.writeUInt8(static_cast<quint8>(player.projectileCount));
    writer.writeUInt8(static_cast<quint8>(player.projectilePierce));
    writer.writeUInt8(static_cast<quint8>(player.orbitBladeLevel));
    writer.writeUInt8(static_cast<quint8>(player.orbitBladeCount));
    writer.writeInt16(static_cast<qint16>(player.orbitBladeDamage));
    writer.writeUInt8(static_cast<quint8>(player.fireWandLevel));
    writer.writeInt16(static_cast<qint16>(player.fireWandDamage));
    writer.writeInt16(static_cast<qint16>(player.fireWandCooldownBaseMs));
    writer.writeInt16(encodeScalar(player.fireWandProjectileSpeedMultiplier));
    writer.writeUInt8(static_cast<quint8>(player.garlicLevel));
    writer.writeInt16(static_cast<qint16>(player.garlicDamage));
    writer.writeInt16(static_cast<qint16>(player.garlicCooldownBaseMs));
    writer.writeUInt8(static_cast<quint8>(player.crossLevel));
    writer.writeInt16(static_cast<qint16>(player.crossDamage));
    writer.writeUInt8(static_cast<quint8>(player.crossAmount));
    writer.writeUInt16(static_cast<quint16>(player.crossPierce));
    writer.writeUInt8(static_cast<quint8>(player.santaWaterLevel));
    writer.writeInt16(static_cast<qint16>(player.santaWaterDamage));
    writer.writeUInt8(static_cast<quint8>(player.santaWaterAmount));
    writer.writeUInt16(static_cast<quint16>(player.santaWaterDurationMs));
    writer.writeUInt16(static_cast<quint16>(player.santaWaterCooldownBaseMs));
    writer.writeUInt8(static_cast<quint8>(player.wingsPassiveLevel));
    writer.writeUInt8(static_cast<quint8>(player.emptyTomePassiveLevel));
    writer.writeUInt8(static_cast<quint8>(player.candelabradorPassiveLevel));
    writer.writeUInt8(static_cast<quint8>(player.attractorbPassiveLevel));
    writer.writeUInt8(static_cast<quint8>(player.hollowHeartPassiveLevel));
    writer.writeUInt8(static_cast<quint8>(player.spinachPassiveLevel));
    writer.writeBool(player.fireWandEvolved);
    writer.writeBool(player.santaWaterEvolved);
    writer.writeUInt16(static_cast<quint16>(player.orbitBladeDurationMs));
    writer.writeUInt16(static_cast<quint16>(player.orbitBladeCooldownBaseMs));
}

bool readPlayerProgression(PacketReader &reader, PlayerState &player)
{
    qint16 playerId = 0;
    qint16 hp = 0;
    qint16 maxHp = 0;
    quint8 colorIndex = 0;
    qint16 level = 0;
    qint16 attackDamage = 0;
    qint16 orbitBladeDamage = 0;
    qint16 fireWandDamage = 0;
    qint16 fireWandCooldownBaseMs = 0;
    qint16 fireWandProjectileSpeed = 0;
    qint16 garlicDamage = 0;
    qint16 garlicCooldownBaseMs = 0;
    qint16 crossDamage = 0;
    qint16 santaWaterDamage = 0;
    quint8 bladeWeaponLevel = 0;
    quint8 projectileCount = 0;
    quint8 projectilePierce = 0;
    quint8 orbitBladeLevel = 0;
    quint8 orbitBladeCount = 0;
    quint8 fireWandLevel = 0;
    quint8 garlicLevel = 0;
    quint8 crossLevel = 0;
    quint8 crossAmount = 0;
    quint16 crossPierce = 0;
    quint8 santaWaterLevel = 0;
    quint8 santaWaterAmount = 0;
    quint16 santaWaterDurationMs = 0;
    quint16 santaWaterCooldownBaseMs = 0;
    quint8 wingsPassiveLevel = 0;
    quint8 emptyTomePassiveLevel = 0;
    quint8 candelabradorPassiveLevel = 0;
    quint8 attractorbPassiveLevel = 0;
    quint8 hollowHeartPassiveLevel = 0;
    quint8 spinachPassiveLevel = 0;
    bool alive = false;
    bool fireWandEvolved = false;
    bool santaWaterEvolved = false;
    quint16 orbitBladeDurationMs = 0;
    quint16 orbitBladeCooldownBaseMs = 0;

    if (!reader.readInt16(playerId)
        || !reader.readInt16(hp)
        || !reader.readInt16(maxHp)
        || !reader.readBool(alive)
        || !reader.readUInt8(colorIndex)
        || !reader.readInt16(level)
        || !reader.readInt32(player.exp)
        || !reader.readInt32(player.expToNext)
        || !reader.readInt16(attackDamage)
        || !reader.readUInt8(bladeWeaponLevel)
        || !reader.readUInt8(projectileCount)
        || !reader.readUInt8(projectilePierce)
        || !reader.readUInt8(orbitBladeLevel)
        || !reader.readUInt8(orbitBladeCount)
        || !reader.readInt16(orbitBladeDamage)
        || !reader.readUInt8(fireWandLevel)
        || !reader.readInt16(fireWandDamage)
        || !reader.readInt16(fireWandCooldownBaseMs)
        || !reader.readInt16(fireWandProjectileSpeed)
        || !reader.readUInt8(garlicLevel)
        || !reader.readInt16(garlicDamage)
        || !reader.readInt16(garlicCooldownBaseMs)
        || !reader.readUInt8(crossLevel)
        || !reader.readInt16(crossDamage)
        || !reader.readUInt8(crossAmount)
        || !reader.readUInt16(crossPierce)
        || !reader.readUInt8(santaWaterLevel)
        || !reader.readInt16(santaWaterDamage)
        || !reader.readUInt8(santaWaterAmount)
        || !reader.readUInt16(santaWaterDurationMs)
        || !reader.readUInt16(santaWaterCooldownBaseMs)
        || !reader.readUInt8(wingsPassiveLevel)
        || !reader.readUInt8(emptyTomePassiveLevel)
        || !reader.readUInt8(candelabradorPassiveLevel)
        || !reader.readUInt8(attractorbPassiveLevel)
        || !reader.readUInt8(hollowHeartPassiveLevel)
        || !reader.readUInt8(spinachPassiveLevel)
        || !reader.readBool(fireWandEvolved)
        || !reader.readBool(santaWaterEvolved)
        || !reader.readUInt16(orbitBladeDurationMs)
        || !reader.readUInt16(orbitBladeCooldownBaseMs)) {
        return false;
    }

    player.playerId = playerId;
    player.hp = hp;
    player.maxHp = maxHp;
    player.alive = alive;
    player.colorIndex = colorIndex;
    player.level = level;
    player.attackDamage = attackDamage;
    player.bladeWeaponLevel = bladeWeaponLevel;
    player.projectileCount = projectileCount;
    player.projectilePierce = projectilePierce;
    player.orbitBladeLevel = orbitBladeLevel;
    player.orbitBladeCount = orbitBladeCount;
    player.orbitBladeDamage = orbitBladeDamage;
    player.fireWandLevel = fireWandLevel;
    player.fireWandDamage = fireWandDamage;
    player.fireWandCooldownBaseMs = fireWandCooldownBaseMs;
    player.fireWandProjectileSpeedMultiplier = decodeScalar(fireWandProjectileSpeed);
    player.garlicLevel = garlicLevel;
    player.garlicDamage = garlicDamage;
    player.garlicCooldownBaseMs = garlicCooldownBaseMs;
    player.crossLevel = crossLevel;
    player.crossDamage = crossDamage;
    player.crossAmount = crossAmount;
    player.crossPierce = crossPierce;
    player.santaWaterLevel = santaWaterLevel;
    player.santaWaterDamage = santaWaterDamage;
    player.santaWaterAmount = santaWaterAmount;
    player.santaWaterDurationMs = santaWaterDurationMs;
    player.santaWaterCooldownBaseMs = santaWaterCooldownBaseMs;
    player.wingsPassiveLevel = wingsPassiveLevel;
    player.emptyTomePassiveLevel = emptyTomePassiveLevel;
    player.candelabradorPassiveLevel = candelabradorPassiveLevel;
    player.attractorbPassiveLevel = attractorbPassiveLevel;
    player.hollowHeartPassiveLevel = hollowHeartPassiveLevel;
    player.spinachPassiveLevel = spinachPassiveLevel;
    player.fireWandEvolved = fireWandEvolved;
    player.santaWaterEvolved = santaWaterEvolved;
    player.orbitBladeDurationMs = orbitBladeDurationMs;
    player.orbitBladeCooldownBaseMs = orbitBladeCooldownBaseMs;
    return true;
}

void writeChoice(PacketWriter &writer, const UpgradeChoice &choice)
{
    writer.writeString(choice.id);
    writer.writeString(choice.title);
    writer.writeString(choice.description);
    writer.writeString(choice.category);
    writer.writeUInt8(static_cast<quint8>(qBound(0, choice.currentLevel, 255)));
    writer.writeUInt8(static_cast<quint8>(qBound(0, choice.maxLevel, 255)));
}

bool readChoice(PacketReader &reader, UpgradeChoice &choice)
{
    quint8 currentLevel = 0;
    quint8 maxLevel = 0;
    return reader.readString(choice.id)
        && reader.readString(choice.title)
        && reader.readString(choice.description)
        && reader.readString(choice.category)
        && reader.readUInt8(currentLevel)
        && reader.readUInt8(maxLevel)
        && (choice.currentLevel = currentLevel, true)
        && (choice.maxLevel = maxLevel, true);
}

void writeReward(PacketWriter &writer, const ChestReward &reward)
{
    writer.writeString(reward.title);
    writer.writeString(reward.description);
    writer.writeString(reward.category);
    writer.writeBool(reward.evolved);
}

bool readReward(PacketReader &reader, ChestReward &reward)
{
    return reader.readString(reward.title)
        && reader.readString(reward.description)
        && reader.readString(reward.category)
        && reader.readBool(reward.evolved);
}

}  // namespace

PacketKind packetKind(const QByteArray &payload)
{
    return packetKind(payload.constData(), payload.size());
}

QByteArray encodeFastNetworkState(const FastNetworkState &state)
{
    PacketWriter writer;
    writeHeader(writer, PacketKind::FastState);
    writer.writeUInt64(state.seq);
    quint8 flags = 0;
    if (state.running)
        flags |= 0x01;
    if (state.gameOver)
        flags |= 0x02;
    if (state.hasLocalPlayer)
        flags |= 0x04;
    writer.writeUInt8(flags);
    writer.writeInt32(state.survivalTimeMs);
    writer.writeInt32(state.killCount);
    writer.writeInt16(static_cast<qint16>(state.interactionPlayerId));
    writer.writeUInt16(encodeRadius(state.auraRadius));
    writer.writeString(state.waveLabel);
    writer.writeInt32(encodeAbsoluteCoord(state.origin.x()));
    writer.writeInt32(encodeAbsoluteCoord(state.origin.y()));
    writer.writeUInt8(static_cast<quint8>(qMin(state.players.size(), 255)));
    writer.writeUInt16(static_cast<quint16>(qMin(state.snapshot.enemies.size(), 0xffff)));
    writer.writeUInt16(static_cast<quint16>(qMin(state.snapshot.orbitals.size(), 0xffff)));
    writer.writeUInt16(static_cast<quint16>(qMin(state.snapshot.projectiles.size(), 0xffff)));
    writer.writeUInt16(static_cast<quint16>(qMin(state.snapshot.pickups.size(), 0xffff)));
    writer.writeUInt16(static_cast<quint16>(qMin(state.snapshot.zones.size(), 0xffff)));

    if (state.hasLocalPlayer)
        writePlayerProgression(writer, state.localPlayer);

    for (const PlayerState &player : state.players)
        writePlayerCore(writer, player, state.origin);

    for (const RenderEnemy &enemy : state.snapshot.enemies) {
        writer.writeInt32(enemy.id);
        writer.writeInt16(encodeRelativeCoord(enemy.x, state.origin.x()));
        writer.writeInt16(encodeRelativeCoord(enemy.y, state.origin.y()));
        writer.writeUInt16(encodeRadius(enemy.radius));
        writer.writeUInt16(static_cast<quint16>(qBound(0, enemy.hp, 0xffff)));
        writer.writeUInt16(static_cast<quint16>(qBound(0, enemy.maxHp, 0xffff)));
        writer.writeUInt8(static_cast<quint8>(qBound(0, enemy.kind, 255)));
        quint8 flagsValue = 0;
        if (enemy.elite)
            flagsValue |= 0x01;
        if (enemy.chestCarrier)
            flagsValue |= 0x02;
        writer.writeUInt8(flagsValue);
        writer.writeUInt16(static_cast<quint16>(qBound(0, enemy.hitFlashMs, 0xffff)));
    }

    for (const RenderOrbital &orbital : state.snapshot.orbitals) {
        writer.writeInt16(encodeRelativeCoord(orbital.x, state.origin.x()));
        writer.writeInt16(encodeRelativeCoord(orbital.y, state.origin.y()));
        writer.writeUInt16(encodeRadius(orbital.radius));
    }

    for (const RenderProjectile &projectile : state.snapshot.projectiles) {
        writer.writeInt16(encodeRelativeCoord(projectile.x, state.origin.x()));
        writer.writeInt16(encodeRelativeCoord(projectile.y, state.origin.y()));
        writer.writeUInt16(encodeRadius(projectile.radius));
        writer.writeUInt8(static_cast<quint8>(qBound(0, projectile.kind, 255)));
    }

    for (const RenderPickup &pickup : state.snapshot.pickups) {
        writer.writeInt16(encodeRelativeCoord(pickup.x, state.origin.x()));
        writer.writeInt16(encodeRelativeCoord(pickup.y, state.origin.y()));
        writer.writeUInt16(encodeRadius(pickup.radius));
        writer.writeUInt16(static_cast<quint16>(qBound(0, pickup.exp, 0xffff)));
        writer.writeUInt8(static_cast<quint8>(qBound(0, pickup.kind, 255)));
    }

    for (const RenderZone &zone : state.snapshot.zones) {
        writer.writeInt16(encodeRelativeCoord(zone.x, state.origin.x()));
        writer.writeInt16(encodeRelativeCoord(zone.y, state.origin.y()));
        writer.writeUInt16(encodeRadius(zone.radius));
        writer.writeUInt16(static_cast<quint16>(qBound(0, zone.lifeMs, 0xffff)));
        writer.writeUInt16(static_cast<quint16>(qBound(0, zone.totalLifeMs, 0xffff)));
        writer.writeUInt8(static_cast<quint8>(qBound(0, zone.kind, 255)));
    }

    return writer.take();
}

bool decodeFastNetworkState(const QByteArray &payload,
                            FastNetworkState &decoded,
                            const QVector<PlayerState> &existingPlayers,
                            int localPlayerId)
{
    decoded = {};
    PacketReader reader(payload.constData(), payload.size());
    if (!readHeader(reader, PacketKind::FastState))
        return false;

    quint8 flags = 0;
    qint16 interactionPlayerId = -1;
    quint16 auraRadius = 0;
    qint32 originX = 0;
    qint32 originY = 0;
    quint8 playerCount = 0;
    quint16 enemyCount = 0;
    quint16 orbitalCount = 0;
    quint16 projectileCount = 0;
    quint16 pickupCount = 0;
    quint16 zoneCount = 0;
    if (!reader.readUInt64(decoded.seq)
        || !reader.readUInt8(flags)
        || !reader.readInt32(decoded.survivalTimeMs)
        || !reader.readInt32(decoded.killCount)
        || !reader.readInt16(interactionPlayerId)
        || !reader.readUInt16(auraRadius)
        || !reader.readString(decoded.waveLabel)
        || !reader.readInt32(originX)
        || !reader.readInt32(originY)
        || !reader.readUInt8(playerCount)
        || !reader.readUInt16(enemyCount)
        || !reader.readUInt16(orbitalCount)
        || !reader.readUInt16(projectileCount)
        || !reader.readUInt16(pickupCount)
        || !reader.readUInt16(zoneCount)) {
        return false;
    }

    decoded.running = (flags & 0x01) != 0;
    decoded.gameOver = (flags & 0x02) != 0;
    decoded.hasLocalPlayer = (flags & 0x04) != 0;
    decoded.interactionPlayerId = interactionPlayerId;
    decoded.auraRadius = decodeRadius(auraRadius);
    decoded.origin = QVector2D(decodeAbsoluteCoord(originX), decodeAbsoluteCoord(originY));

    if (decoded.hasLocalPlayer && !readPlayerProgression(reader, decoded.localPlayer))
        return false;

    QHash<int, const PlayerState *> existingPlayersById;
    existingPlayersById.reserve(existingPlayers.size());
    for (const PlayerState &player : existingPlayers)
        existingPlayersById.insert(player.playerId, &player);

    decoded.players.reserve(playerCount);
    decoded.snapshot.players.reserve(playerCount);
    for (int i = 0; i < playerCount; ++i) {
        PlayerState player;
        qint16 playerId = 0;
        if (!reader.readInt16(playerId))
            return false;
        if (const auto it = existingPlayersById.constFind(playerId);
            it != existingPlayersById.cend() && it.value()) {
            player = *it.value();
        }
        player.playerId = playerId;
        qint16 relX = 0;
        qint16 relY = 0;
        qint16 hp = 0;
        qint16 maxHp = 0;
        quint8 alive = 0;
        quint8 colorIndex = 0;
        if (!reader.readInt16(relX)
            || !reader.readInt16(relY)
            || !reader.readInt16(hp)
            || !reader.readInt16(maxHp)
            || !reader.readUInt8(alive)
            || !reader.readUInt8(colorIndex)) {
            return false;
        }
        player.position = QVector2D(decodeRelativeCoord(relX, decoded.origin.x()),
                                    decodeRelativeCoord(relY, decoded.origin.y()));
        player.hp = hp;
        player.maxHp = maxHp;
        player.alive = alive != 0;
        player.colorIndex = colorIndex;
        player.local = player.playerId == localPlayerId;
        decoded.players.append(player);
        decoded.snapshot.players.append({
            player.position.x(),
            player.position.y(),
            player.hp,
            player.maxHp,
            player.alive,
            player.local,
            player.colorIndex
        });
    }

    decoded.snapshot.enemies.reserve(enemyCount);
    for (int i = 0; i < enemyCount; ++i) {
        RenderEnemy enemy;
        qint16 relX = 0;
        qint16 relY = 0;
        quint16 radius = 0;
        quint16 hp = 0;
        quint16 maxHp = 0;
        quint8 kind = 0;
        quint8 enemyFlags = 0;
        quint16 hitFlashMs = 0;
        if (!reader.readInt32(enemy.id)
            || !reader.readInt16(relX)
            || !reader.readInt16(relY)
            || !reader.readUInt16(radius)
            || !reader.readUInt16(hp)
            || !reader.readUInt16(maxHp)
            || !reader.readUInt8(kind)
            || !reader.readUInt8(enemyFlags)
            || !reader.readUInt16(hitFlashMs)) {
            return false;
        }
        enemy.x = decodeRelativeCoord(relX, decoded.origin.x());
        enemy.y = decodeRelativeCoord(relY, decoded.origin.y());
        enemy.radius = decodeRadius(radius);
        enemy.hp = hp;
        enemy.maxHp = maxHp;
        enemy.kind = kind;
        enemy.elite = (enemyFlags & 0x01) != 0;
        enemy.chestCarrier = (enemyFlags & 0x02) != 0;
        enemy.hitFlashMs = hitFlashMs;
        decoded.snapshot.enemies.append(enemy);
    }

    decoded.snapshot.orbitals.reserve(orbitalCount);
    for (int i = 0; i < orbitalCount; ++i) {
        RenderOrbital orbital;
        qint16 relX = 0;
        qint16 relY = 0;
        quint16 radius = 0;
        if (!reader.readInt16(relX)
            || !reader.readInt16(relY)
            || !reader.readUInt16(radius)) {
            return false;
        }
        orbital.x = decodeRelativeCoord(relX, decoded.origin.x());
        orbital.y = decodeRelativeCoord(relY, decoded.origin.y());
        orbital.radius = decodeRadius(radius);
        decoded.snapshot.orbitals.append(orbital);
    }

    decoded.snapshot.projectiles.reserve(projectileCount);
    for (int i = 0; i < projectileCount; ++i) {
        RenderProjectile projectile;
        qint16 relX = 0;
        qint16 relY = 0;
        quint16 radius = 0;
        quint8 kind = 0;
        if (!reader.readInt16(relX)
            || !reader.readInt16(relY)
            || !reader.readUInt16(radius)
            || !reader.readUInt8(kind)) {
            return false;
        }
        projectile.x = decodeRelativeCoord(relX, decoded.origin.x());
        projectile.y = decodeRelativeCoord(relY, decoded.origin.y());
        projectile.radius = decodeRadius(radius);
        projectile.kind = kind;
        decoded.snapshot.projectiles.append(projectile);
    }

    decoded.snapshot.pickups.reserve(pickupCount);
    for (int i = 0; i < pickupCount; ++i) {
        RenderPickup pickup;
        qint16 relX = 0;
        qint16 relY = 0;
        quint16 radius = 0;
        quint16 exp = 0;
        quint8 kind = 0;
        if (!reader.readInt16(relX)
            || !reader.readInt16(relY)
            || !reader.readUInt16(radius)
            || !reader.readUInt16(exp)
            || !reader.readUInt8(kind)) {
            return false;
        }
        pickup.x = decodeRelativeCoord(relX, decoded.origin.x());
        pickup.y = decodeRelativeCoord(relY, decoded.origin.y());
        pickup.radius = decodeRadius(radius);
        pickup.exp = exp;
        pickup.kind = kind;
        decoded.snapshot.pickups.append(pickup);
    }

    decoded.snapshot.zones.reserve(zoneCount);
    for (int i = 0; i < zoneCount; ++i) {
        RenderZone zone;
        qint16 relX = 0;
        qint16 relY = 0;
        quint16 radius = 0;
        quint16 lifeMs = 0;
        quint16 totalLifeMs = 0;
        quint8 kind = 0;
        if (!reader.readInt16(relX)
            || !reader.readInt16(relY)
            || !reader.readUInt16(radius)
            || !reader.readUInt16(lifeMs)
            || !reader.readUInt16(totalLifeMs)
            || !reader.readUInt8(kind)) {
            return false;
        }
        zone.x = decodeRelativeCoord(relX, decoded.origin.x());
        zone.y = decodeRelativeCoord(relY, decoded.origin.y());
        zone.radius = decodeRadius(radius);
        zone.lifeMs = lifeMs;
        zone.totalLifeMs = totalLifeMs;
        zone.kind = kind;
        decoded.snapshot.zones.append(zone);
    }

    return true;
}

QByteArray encodeHudNetworkState(const HudNetworkState &state)
{
    PacketWriter writer;
    writeHeader(writer, PacketKind::HudState);
    writer.writeInt16(static_cast<qint16>(state.interactionPlayerId));
    writer.writeString(state.chestTitle);
    writer.writeUInt8(static_cast<quint8>(qMin(state.levelUpChoices.size(), 255)));
    for (const UpgradeChoice &choice : state.levelUpChoices)
        writeChoice(writer, choice);

    writer.writeUInt8(static_cast<quint8>(qMin(state.chestRewards.size(), 255)));
    for (const ChestReward &reward : state.chestRewards)
        writeReward(writer, reward);

    return writer.take();
}

bool decodeHudNetworkState(const QByteArray &payload, HudNetworkState &decoded)
{
    decoded = {};
    PacketReader reader(payload.constData(), payload.size());
    if (!readHeader(reader, PacketKind::HudState))
        return false;

    qint16 interactionPlayerId = -1;
    if (!reader.readInt16(interactionPlayerId)
        || !reader.readString(decoded.chestTitle)) {
        return false;
    }

    decoded.interactionPlayerId = interactionPlayerId;

    quint8 choiceCount = 0;
    if (!reader.readUInt8(choiceCount))
        return false;
    decoded.levelUpChoices.reserve(choiceCount);
    for (int i = 0; i < choiceCount; ++i) {
        UpgradeChoice choice;
        if (!readChoice(reader, choice))
            return false;
        decoded.levelUpChoices.append(choice);
    }

    quint8 rewardCount = 0;
    if (!reader.readUInt8(rewardCount))
        return false;
    decoded.chestRewards.reserve(rewardCount);
    for (int i = 0; i < rewardCount; ++i) {
        ChestReward reward;
        if (!readReward(reader, reward))
            return false;
        decoded.chestRewards.append(reward);
    }

    return true;
}

QByteArray encodeInputPacket(qreal horizontal, qreal vertical)
{
    PacketWriter writer;
    writeHeader(writer, PacketKind::Input);
    writer.writeInt16(encodeScalar(horizontal));
    writer.writeInt16(encodeScalar(vertical));
    return writer.take();
}

bool decodeInputPacket(const QByteArray &payload, qreal &horizontal, qreal &vertical)
{
    horizontal = 0.0;
    vertical = 0.0;
    PacketReader reader(payload.constData(), payload.size());
    if (!readHeader(reader, PacketKind::Input))
        return false;

    qint16 encodedHorizontal = 0;
    qint16 encodedVertical = 0;
    if (!reader.readInt16(encodedHorizontal)
        || !reader.readInt16(encodedVertical)) {
        return false;
    }

    horizontal = decodeScalar(encodedHorizontal);
    vertical = decodeScalar(encodedVertical);
    return true;
}

QByteArray encodeChooseLevelUpPacket(const QString &upgradeId)
{
    PacketWriter writer;
    writeHeader(writer, PacketKind::ChooseLevelUp);
    writer.writeString(upgradeId.trimmed());
    return writer.take();
}

bool decodeChooseLevelUpPacket(const QByteArray &payload, QString &upgradeId)
{
    upgradeId.clear();
    PacketReader reader(payload.constData(), payload.size());
    return readHeader(reader, PacketKind::ChooseLevelUp)
        && reader.readString(upgradeId);
}

QByteArray encodeCloseChestPacket()
{
    PacketWriter writer;
    writeHeader(writer, PacketKind::CloseChest);
    return writer.take();
}

bool decodeCloseChestPacket(const QByteArray &payload)
{
    PacketReader reader(payload.constData(), payload.size());
    return readHeader(reader, PacketKind::CloseChest);
}

}  // namespace LanBoard::Survivor::NetCodec
