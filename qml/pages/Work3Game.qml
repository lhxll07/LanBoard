import QtQuick

Item {
    id: root

    width: 960
    height: 540
    signal exitRequested()

    property int sideWidth: 200
    property int gameAreaWidth: 560
    property int gameLeft: sideWidth
    property int gameRight: gameLeft + gameAreaWidth
    property int gameTop: 48
    property int gridSize: 40
    property int blockSize: gridSize
    property real playerDrawScale: 1.5
    property real playerDrawSize: blockSize * playerDrawScale
    property int expPickupPadding: 60
    property real pickupRangeMultiplier: 1.0
    property real pickupRangeIncreasePerLevel: 0.2
    property real maxPickupRangeMultiplier: 3.0
    readonly property int currentPickupPadding: Math.round(expPickupPadding * pickupRangeMultiplier)
    property int initialEnemyHealth: 3
    property int enemySpawnBoostLevel: 20
    property int enemyHealthBoostLevel: 10
    property int enemyMaxHealthBoostLevel: 20
    property int normalMaxEnemyHealth: 20
    property int boostedMaxEnemyHealth: 50
    readonly property bool enemySpawnBoostActive: level >= enemySpawnBoostLevel
    readonly property bool enemyHealthBoostActive: level >= enemyHealthBoostLevel
    readonly property bool enemyMaxHealthBoostActive: level >= enemyMaxHealthBoostLevel
    readonly property int maxEnemyHealth: enemyMaxHealthBoostActive ? boostedMaxEnemyHealth : normalMaxEnemyHealth

    property string screenState: "menu"
    property bool running: false
    property bool playerShootingEnabled: true
    property bool playerMoving: false
    property bool choosingUpgrade: false
    property bool choosingBulletToAdd: false
    property bool choosingBulletToUpgrade: false
    property bool choosingBulletsToMerge: false

    property int playerX: gameAreaWidth / 2 - blockSize / 2
    property int playerY: height - 120
    property int score: 0
    property int bestScore: 0
    property int maxLives: 50
    property int lives: 50
    property int level: 1
    property int experience: 0
    property int expToNext: 22
    property int currentEnemyHealth: initialEnemyHealth
    property int fireIntervalMs: 350
    property int fireLoopIntervalMs: 25
    property int playerFireTick: 0
    property int playerFireElapsedMs: 0
    property int enemySpawnIntervalMs: 500
    property int enemyHealthIncreaseIntervalMs: 5000
    readonly property int currentEnemySpawnIntervalMs: enemySpawnBoostActive ? Math.max(1, Math.floor(enemySpawnIntervalMs * 0.5)) : enemySpawnIntervalMs
    readonly property int currentEnemyHealthIncreaseIntervalMs: enemyHealthBoostActive ? Math.max(1, Math.floor(enemyHealthIncreaseIntervalMs * 0.5)) : enemyHealthIncreaseIntervalMs
    property int enemyUpdateIntervalMs: 30
    property int enemyContactDamageCooldownMs: 1000
    property real enemyMoveSpeedScale: 0.6
    property real healthPotionDropChance: 0.10
    property int healthPotionHealAmount: 3
    property int frostBurstRadius: 90
    property int frostBurstFreezeMs: 1000
    property int openingEnemyReductionMs: 35000
    property int enemySpawnElapsedMs: 0
    property int nextEnemyId: 1
    readonly property bool openingEnemySpawnSlowActive: enemySpawnElapsedMs < openingEnemyReductionMs
    readonly property int activeEnemySpawnIntervalMs: openingEnemySpawnSlowActive ? currentEnemySpawnIntervalMs * 2 : currentEnemySpawnIntervalMs

    property var moveDir: ({ left: false, right: false, up: false, down: false })
    readonly property bool mobileInputEnabled: Qt.platform.os === "android"
    readonly property real joystickBaseSize: 104
    readonly property real joystickHandleTravel: 30
    property bool joystickTracking: false
    property bool joystickVisible: false
    property real joystickAnchorX: 0
    property real joystickAnchorY: 0
    property real joystickDx: 0
    property real joystickDy: 0
    property var enemies: []
    property var bullets: []
    property var enemyBullets: []
    property var expOrbs: []
    property var healthPotions: []
    property var deathFragments: []
    property var bulletLevels: [1]
    property var rectangleBulletLevels: []
    property var mergedBullets: []
    property var selectedMergeBulletIndexes: []
    property var bulletLastFireTimes: ({})
    property var upgradeChoices: []
    property var upgradeBulletChoices: []
    property var usedUpgradeRefreshIndexes: []
    property int upgradeSelectionIndex: 0
    property int upgradeSelectionRow: 0
    property int selectionCardWidth: 170
    property int selectionCardHeight: 236
    property int maxBulletQuality: 2

    readonly property string imageRoot: "../assets/work3"
    readonly property var enemySprites: [
        imageRoot + "/enemy-red.png",
        imageRoot + "/enemy-orange.png",
        imageRoot + "/enemy-purple.png",
        imageRoot + "/enemy-green.png",
        imageRoot + "/enemy-pink.png"
    ]
    readonly property var enemyColors: [
        "#e84d4f",
        "#f28c3c",
        "#8d5bd8",
        "#49a861",
        "#e85f9f"
    ]

    function fileUrl(path) {
        return "file:///" + String(path).replace(/\\/g, "/")
    }

    function bulletImage(shape) {
        return shape === "rect" ? imageRoot + "/b2.png" : imageRoot + "/b1.png"
    }

    function bulletColor(level) {
        if (level === 2)
            return "#4caf5f"
        if (level === 3)
            return "#2d82e6"
        if (level >= 4)
            return "#aa5adc"
        return "#f5d241"
    }

    function qualityColor(quality) {
        quality = normalizedBulletQuality(quality)
        if (quality === 2)
            return "#ec5fa8"
        return "#aa5adc"
    }

    function normalizedBulletQuality(quality) {
        var value = Number(quality)
        if (!isFinite(value))
            value = 1
        return Math.max(1, Math.min(maxBulletQuality, Math.floor(value)))
    }

    function bulletQualityLabel(quality) {
        quality = normalizedBulletQuality(quality)
        if (quality === 1)
            return "A"
        return "S"
    }

    function circleBulletDamage(level) {
        return level
    }

    function rectangleBulletDamage(level) {
        return 0.5
    }

    function snowflakeSlowTicks(level) {
        var value = Number(level)
        if (!isFinite(value))
            value = 1
        value = Math.max(1, Math.min(3, Math.floor(value)))
        return 50 + (value - 1) * 25
    }

    function snowflakeSlowSeconds(level) {
        return snowflakeSlowTicks(level) * 30 / 1000
    }

    function snowflakeSlowLabel(level) {
        return "减速 " + snowflakeSlowSeconds(level).toFixed(1) + "s"
    }

    function snowflakeSlowUpgradeLabel(level) {
        return snowflakeSlowLabel(level) + " > " + snowflakeSlowSeconds(Math.min(3, level + 1)).toFixed(1) + "s"
    }

    function frostBurstFreezeTicks() {
        return Math.ceil(frostBurstFreezeMs / enemyUpdateIntervalMs)
    }

    function formatNumber(value) {
        var number = Number(value)
        if (!isFinite(number))
            return "0"
        if (Math.abs(number - Math.round(number)) < 0.01)
            return String(Math.round(number))
        return number.toFixed(1)
    }

    function mergedBulletDamage(bullet) {
        return bullet.baseDamage + Math.max(0, bullet.level - 1) * normalizedBulletQuality(bullet.quality)
    }

    function nextMergedBulletDamage(bullet) {
        var nextLevel = Math.min(3, bullet.level + 1)
        return mergedBulletDamage({
            level: nextLevel,
            quality: normalizedBulletQuality(bullet.quality),
            baseDamage: bullet.baseDamage
        })
    }

    function synthesisName(type) {
        if (type === "frostBurst")
            return "Frost Burst"
        if (type === "triad")
            return "三芒弹"
        if (type === "frostSplit")
            return "裂霜弹"
        return "合成弹"
    }

    function synthesisEffectLabel(type) {
        if (type === "frostBurst")
            return "Area Freeze"
        if (type === "triad")
            return "前方三向"
        if (type === "frostSplit")
            return "命中分裂"
        return "品质强化"
    }

    function synthesisTypeForMerge(selectedOptions) {
        if (!selectedOptions || selectedOptions.length !== 2)
            return "merged"

        var circleFamilyCount = 0
        var frostFamilyCount = 0
        var plainRectCount = 0
        for (var i = 0; i < selectedOptions.length; ++i) {
            var option = selectedOptions[i]
            if (!option)
                continue
            if (option.synthesisType === "frostSplit") {
                frostFamilyCount += 1
            } else if (option.shape === "rect") {
                plainRectCount += 1
            } else {
                circleFamilyCount += 1
            }
        }

        if (plainRectCount === 2 && circleFamilyCount === 0 && frostFamilyCount === 0)
            return "frostBurst"
        if (circleFamilyCount > 0 && (plainRectCount > 0 || frostFamilyCount > 0))
            return "frostSplit"
        if (frostFamilyCount > 0)
            return "frostSplit"
        if (circleFamilyCount === 2)
            return "triad"
        return "merged"
    }

    function synthesisShape(type) {
        return (type === "frostSplit" || type === "frostBurst") ? "rect" : "circle"
    }

    function bulletFireTickInterval(quality, shape) {
        var value = normalizedBulletQuality(quality)
        if (shape === "rect")
            return value === 1 ? 4 : 2
        return value === 1 ? 2 : 1
    }

    function bulletFireLevelMultiplier(level) {
        var value = Number(level)
        if (!isFinite(value))
            value = 1
        value = Math.max(1, Math.min(3, Math.floor(value)))
        return Math.pow(0.9, value - 1)
    }

    function bulletFireIntervalMs(level, quality, shape, synthesisType) {
        if (synthesisType === "frostBurst")
            return 3000
        var interval = fireIntervalMs * bulletFireTickInterval(quality, shape) * bulletFireLevelMultiplier(level)
        return Math.max(fireLoopIntervalMs, Math.round(interval))
    }

    function canFireBulletNow(key, level, quality, shape, elapsedMs, synthesisType) {
        var interval = bulletFireIntervalMs(level, quality, shape, synthesisType)
        var lastFire = bulletLastFireTimes[key]
        if (lastFire === undefined || elapsedMs - Number(lastFire) >= interval) {
            var fireTimes = bulletLastFireTimes
            fireTimes[key] = elapsedMs
            bulletLastFireTimes = fireTimes
            return true
        }
        return false
    }

    function bulletFireIntervalLabel(level, quality, shape, synthesisType) {
        var seconds = bulletFireIntervalMs(level, quality, shape, synthesisType) / 1000
        return "间隔 " + seconds.toFixed(1) + "s"
    }

    function bulletFireUpgradeLabel(level, quality, shape, synthesisType) {
        var nextLevel = Math.min(3, level + 1)
        return bulletFireIntervalLabel(level, quality, shape, synthesisType) + " > " +
               (bulletFireIntervalMs(nextLevel, quality, shape, synthesisType) / 1000).toFixed(2) + "s"
    }

    function totalOwnedBullets() {
        return bulletLevels.length + rectangleBulletLevels.length + mergedBullets.length
    }

    function clampValue(value, minValue, maxValue) {
        return Math.max(minValue, Math.min(maxValue, value))
    }

    function clearJoystickInput() {
        joystickTracking = false
        joystickVisible = false
        joystickDx = 0
        joystickDy = 0
    }

    function resetMoveDir() {
        moveDir.left = false
        moveDir.right = false
        moveDir.up = false
        moveDir.down = false
        clearJoystickInput()
        playerMoving = false
    }

    function refreshPlayerMoving() {
        playerMoving = moveDir.left || moveDir.right || moveDir.up || moveDir.down
                || Math.abs(joystickDx) > 0.01 || Math.abs(joystickDy) > 0.01
    }

    function beginJoystick(x, y) {
        if (!mobileInputEnabled || !running || screenState !== "playing" || choosingUpgrade)
            return

        var radius = joystickBaseSize / 2
        joystickTracking = true
        joystickVisible = true
        joystickAnchorX = clampValue(x, gameLeft + radius, gameRight - radius)
        joystickAnchorY = clampValue(y, gameTop + radius, height - radius)
        joystickDx = 0
        joystickDy = 0
        refreshPlayerMoving()
    }

    function updateJoystick(x, y) {
        if (!joystickTracking)
            return

        var dx = (x - joystickAnchorX) / joystickHandleTravel
        var dy = (y - joystickAnchorY) / joystickHandleTravel
        var length = Math.sqrt(dx * dx + dy * dy)
        if (length > 1) {
            dx /= length
            dy /= length
        }

        joystickDx = dx
        joystickDy = dy
        refreshPlayerMoving()
    }

    function endJoystick() {
        clearJoystickInput()
        refreshPlayerMoving()
    }

    function expRequirementForLevel(levelValue) {
        var required = 90 + Math.max(0, levelValue - 1) * 60
        return Math.floor(required * 0.25)
    }

    function increasePickupRange() {
        var nextMultiplier = pickupRangeMultiplier + pickupRangeIncreasePerLevel
        pickupRangeMultiplier = Math.min(maxPickupRangeMultiplier,
                                         Math.round(nextMultiplier * 10) / 10)
    }

    function resetGame() {
        playerX = gameAreaWidth / 2 - blockSize / 2
        playerY = height - 120
        score = 0
        lives = maxLives
        level = 1
        experience = 0
        expToNext = expRequirementForLevel(level)
        pickupRangeMultiplier = 1.0
        currentEnemyHealth = initialEnemyHealth
        bulletLevels = [1]
        rectangleBulletLevels = []
        mergedBullets = []
        playerFireTick = 0
        playerFireElapsedMs = 0
        bulletLastFireTimes = ({})
        enemySpawnElapsedMs = 0
        nextEnemyId = 1
        fireIntervalMs = 350
        choosingUpgrade = false
        choosingBulletToAdd = false
        choosingBulletToUpgrade = false
        choosingBulletsToMerge = false
        selectedMergeBulletIndexes = []
        upgradeBulletChoices = []
        usedUpgradeRefreshIndexes = []
        upgradeSelectionIndex = 0
        upgradeSelectionRow = 0
        enemies = []
        bullets = []
        enemyBullets = []
        expOrbs = []
        healthPotions = []
        deathFragments = []
        upgradeChoices = []
        resetMoveDir()
    }

    function startGame() {
        resetGame()
        screenState = "playing"
        running = true
        keyHandler.forceActiveFocus()
    }

    function stopGame() {
        running = false
        bestScore = Math.max(bestScore, score)
        screenState = "menu"
        choosingUpgrade = false
        choosingBulletToAdd = false
        choosingBulletToUpgrade = false
        choosingBulletsToMerge = false
        selectedMergeBulletIndexes = []
        upgradeChoices = []
        upgradeBulletChoices = []
        usedUpgradeRefreshIndexes = []
        upgradeSelectionIndex = 0
        upgradeSelectionRow = 0
        resetMoveDir()
        keyHandler.forceActiveFocus()
    }

    function pauseGame() {
        if (!running || choosingUpgrade || screenState !== "playing")
            return

        screenState = "paused"
        resetMoveDir()
    }

    function resumeGame() {
        if (!running || screenState !== "paused")
            return

        screenState = "playing"
        keyHandler.forceActiveFocus()
    }

    function endGame() {
        running = false
        bestScore = Math.max(bestScore, score)
        screenState = "gameOver"
        choosingUpgrade = false
        choosingBulletToAdd = false
        choosingBulletToUpgrade = false
        choosingBulletsToMerge = false
        selectedMergeBulletIndexes = []
        upgradeChoices = []
        upgradeBulletChoices = []
        usedUpgradeRefreshIndexes = []
        upgradeSelectionIndex = 0
        upgradeSelectionRow = 0
        resetMoveDir()
    }

    function spawnEnemy() {
        if (!running || choosingUpgrade)
            return

        enemySpawnElapsedMs += activeEnemySpawnIntervalMs

        var laneCount = Math.max(1, Math.floor(gameAreaWidth / blockSize))
        var laneIndex = Math.floor(Math.random() * laneCount)
        var type = Math.floor(Math.random() * 5)
        enemies = enemies.concat([{
            id: nextEnemyId++,
            x: laneIndex * blockSize,
            y: gameTop,
            w: blockSize,
            h: blockSize,
            speed: 2,
            contactDamageCooldownMs: 0,
            health: currentEnemyHealth,
            type: type
        }])
    }

    function increaseEnemyHealth() {
        if (!running || choosingUpgrade)
            return
        if (currentEnemyHealth < maxEnemyHealth)
            currentEnemyHealth += 1
    }

    function rectsIntersect(a, b) {
        return a.x < b.x + b.w &&
               a.x + a.w > b.x &&
               a.y < b.y + b.h &&
               a.y + a.h > b.y
    }

    function nearestEnemyIndex(px, py) {
        var bestIndex = -1
        var bestDistance = Number.MAX_VALUE
        for (var i = 0; i < enemies.length; ++i) {
            var ex = enemies[i].x + enemies[i].w / 2
            var ey = enemies[i].y + enemies[i].h / 2
            var dx = ex - px
            var dy = ey - py
            var distance = dx * dx + dy * dy
            if (distance < bestDistance) {
                bestDistance = distance
                bestIndex = i
            }
        }
        return bestIndex
    }

    function velocityToTarget(cx, cy, speed) {
        var direction = { x: 0, y: -1 }
        var targetIndex = nearestEnemyIndex(playerX + blockSize / 2, playerY + blockSize / 2)
        if (targetIndex >= 0) {
            var target = enemies[targetIndex]
            var dx = target.x + target.w / 2 - cx
            var dy = target.y + target.h / 2 - cy
            var distance = Math.sqrt(dx * dx + dy * dy)
            if (distance > 0) {
                direction.x = dx / distance
                direction.y = dy / distance
            }
        }
        return { x: direction.x * speed, y: direction.y * speed }
    }

    function rotateVelocity(velocity, radians) {
        var cosine = Math.cos(radians)
        var sine = Math.sin(radians)
        return {
            x: velocity.x * cosine - velocity.y * sine,
            y: velocity.x * sine + velocity.y * cosine
        }
    }

    function frostSplitBullets(bullet, enemy) {
        var speed = Math.sqrt(bullet.vx * bullet.vx + bullet.vy * bullet.vy)
        if (speed <= 0)
            speed = 7
        var splitDamage = bullet.damage * 0.5
        var split = []
        for (var i = 0; i < 8; ++i) {
            var angle = -Math.PI / 2 + Math.PI * 2 * i / 8
            split.push({
                cx: enemy.x + enemy.w / 2,
                cy: enemy.y + enemy.h / 2,
                w: 12,
                h: 12,
                vx: Math.cos(angle) * speed,
                vy: Math.sin(angle) * speed,
                damage: splitDamage,
                level: bullet.level,
                quality: bullet.quality,
                shape: "rect",
                merged: true,
                synthesisType: "frostSplit",
                splitChild: true,
                hitEnemyIds: ({})
            })
        }
        return split
    }

    function fireBullet() {
        if (!running || choosingUpgrade || enemies.length === 0)
            return

        var baseSize = 12
        var speed = 8
        var normalCount = bulletLevels.length
        var mergedCount = mergedBullets.length
        var shotCount = normalCount + mergedCount
        var spacing = baseSize + 5
        var startX = playerX + blockSize / 2 - spacing * (shotCount - 1) / 2
        var nextBullets = bullets.slice()
        playerFireElapsedMs += fireLoopIntervalMs
        var fireTimeMs = playerFireElapsedMs

        for (var i = 0; i < shotCount; ++i) {
            var mergedInfo = i < normalCount ? null : mergedBullets[i - normalCount]
            var fireKey = mergedInfo ? "merged:" + (i - normalCount) : "circle:" + i
            var synthesisType = mergedInfo ? (mergedInfo.synthesisType || "merged") : ""
            var levelValue = mergedInfo ? mergedInfo.level : bulletLevels[i]
            var qualityValue = mergedInfo ? normalizedBulletQuality(mergedInfo.quality) : 1
            var damageValue = mergedInfo ? mergedBulletDamage(mergedInfo) : circleBulletDamage(levelValue)
            var bulletShape = mergedInfo ? (mergedInfo.shape || synthesisShape(synthesisType)) : "circle"
            if (!canFireBulletNow(fireKey, levelValue, qualityValue, bulletShape, fireTimeMs, synthesisType))
                continue

            var cx = startX + spacing * i
            var cy = playerY - baseSize / 2
            var velocity = velocityToTarget(cx, cy, speed)

            if (synthesisType === "triad") {
                var spreadAngles = [-0.28, 0, 0.28]
                for (var s = 0; s < spreadAngles.length; ++s) {
                    var triadVelocity = rotateVelocity(velocity, spreadAngles[s])
                    nextBullets.push({
                        cx: cx,
                        cy: cy,
                        w: baseSize,
                        h: baseSize,
                        vx: triadVelocity.x,
                        vy: triadVelocity.y,
                        damage: damageValue,
                        level: levelValue,
                        quality: qualityValue,
                        shape: "circle",
                        merged: true,
                        synthesisType: synthesisType
                    })
                }
                continue
            }

            var bulletWidth = synthesisType === "frostBurst" ? 30 : synthesisType === "frostSplit" ? 18 : baseSize
            var bulletHeight = synthesisType === "frostBurst" ? 30 : synthesisType === "frostSplit" ? 12 : baseSize
            nextBullets.push({
                cx: cx,
                cy: cy,
                w: bulletWidth,
                h: bulletHeight,
                vx: velocity.x,
                vy: velocity.y,
                damage: damageValue,
                level: levelValue,
                quality: qualityValue,
                shape: bulletShape,
                merged: mergedInfo !== null,
                synthesisType: synthesisType
            })
        }

        if (rectangleBulletLevels.length > 0) {
            var rectW = 18
            var rectH = 12
            var rectSpacing = rectW + 5
            var rectStartX = playerX + blockSize / 2 - rectSpacing * (rectangleBulletLevels.length - 1) / 2
            for (var r = 0; r < rectangleBulletLevels.length; ++r) {
                var rectLevel = rectangleBulletLevels[r]
                if (!canFireBulletNow("rect:" + r, rectLevel, 1, "rect", fireTimeMs))
                    continue

                var rcx = rectStartX + rectSpacing * r
                var rcy = playerY - rectH / 2 - baseSize
                var rectVelocity = velocityToTarget(rcx, rcy, speed)
                nextBullets.push({
                    cx: rcx,
                    cy: rcy,
                    w: rectW,
                    h: rectH,
                    vx: rectVelocity.x,
                    vy: rectVelocity.y,
                    damage: rectangleBulletDamage(rectLevel),
                    level: rectLevel,
                    quality: 1,
                    shape: "rect",
                    merged: false
                })
            }
        }

        playerFireTick += 1
        bullets = nextBullets
    }

    function dropExpOrb(cx, cy) {
        var count = 1 + Math.floor(Math.random() * 3)
        var nextOrbs = expOrbs.slice()
        for (var i = 0; i < count; ++i) {
            var angle = Math.PI * 2 * i / count + Math.random() * 0.65
            var distance = count === 1 ? 0 : 7 + Math.random() * 8
            nextOrbs.push({
                cx: cx + Math.cos(angle) * distance,
                cy: cy + Math.sin(angle) * distance,
                size: 12,
                value: 10,
                attracting: false,
                pullTicks: 0
            })
        }
        expOrbs = nextOrbs
    }

    function dropHealthPotion(cx, cy) {
        if (Math.random() >= healthPotionDropChance)
            return

        var angle = Math.random() * Math.PI * 2
        var distance = 8 + Math.random() * 10
        healthPotions = healthPotions.concat([{
            cx: cx + Math.cos(angle) * distance,
            cy: cy + Math.sin(angle) * distance,
            size: 16,
            healAmount: healthPotionHealAmount,
            attracting: false,
            pullTicks: 0
        }])
    }

    function applyFrostBurstExplosion(cx, cy, enemyList) {
        var nextEnemies = enemyList.slice()
        var freezeTicks = frostBurstFreezeTicks()
        var radiusSquared = frostBurstRadius * frostBurstRadius
        for (var i = 0; i < nextEnemies.length; ++i) {
            var enemy = Object.assign({}, nextEnemies[i])
            var ex = enemy.x + enemy.w / 2
            var ey = enemy.y + enemy.h / 2
            var dx = ex - cx
            var dy = ey - cy
            if (dx * dx + dy * dy <= radiusSquared) {
                enemy.freezeTicks = Math.max(Math.max(0, Number(enemy.freezeTicks) || 0), freezeTicks)
                nextEnemies[i] = enemy
            }
        }

        deathFragments = deathFragments.concat([{
            kind: "ring",
            cx: cx,
            cy: cy,
            vx: 0,
            vy: 0,
            size: frostBurstRadius * 1.35,
            rotation: 0,
            vr: 0,
            color: "#8bdcff",
            life: 22,
            maxLife: 22
        }])
        return nextEnemies
    }

    function spawnEnemyDeathEffect(enemy) {
        var cx = enemy.x + enemy.w / 2
        var cy = enemy.y + enemy.h / 2
        var color = enemyColors[enemy.type] || "#ffffff"
        var fragments = [{
            kind: "ring",
            cx: cx,
            cy: cy,
            vx: 0,
            vy: 0,
            size: enemy.w * 0.55,
            rotation: 0,
            vr: 0,
            color: color,
            life: 16,
            maxLife: 16
        }]

        for (var i = 0; i < 12; ++i) {
            var angle = Math.PI * 2 * i / 12 + (Math.random() - 0.5) * 0.35
            var speed = 2.2 + Math.random() * 2.4
            var life = 20 + Math.floor(Math.random() * 12)
            fragments.push({
                kind: "shard",
                cx: cx + (Math.random() - 0.5) * enemy.w * 0.35,
                cy: cy + (Math.random() - 0.5) * enemy.h * 0.35,
                vx: Math.cos(angle) * speed,
                vy: Math.sin(angle) * speed - 0.8,
                size: 5 + Math.random() * 6,
                rotation: Math.random() * 360,
                vr: -14 + Math.random() * 28,
                color: color,
                life: life,
                maxLife: life
            })
        }

        deathFragments = deathFragments.concat(fragments)
    }

    function updateDeathFragments() {
        if (deathFragments.length === 0)
            return

        var nextFragments = []
        for (var i = 0; i < deathFragments.length; ++i) {
            var fragment = Object.assign({}, deathFragments[i])
            fragment.life -= 1
            if (fragment.life <= 0)
                continue

            if (fragment.kind !== "ring") {
                fragment.cx += fragment.vx
                fragment.cy += fragment.vy
                fragment.vy += 0.12
                fragment.rotation += fragment.vr
            }
            nextFragments.push(fragment)
        }
        deathFragments = nextFragments
    }

    function updateEnemies() {
        if (!running || choosingUpgrade)
            return

        var playerRect = { x: playerX, y: playerY, w: blockSize, h: blockSize }
        var nextEnemies = []

        for (var i = 0; i < enemies.length; ++i) {
            var enemy = Object.assign({}, enemies[i])
            var slowTicks = Math.max(0, Number(enemy.slowTicks) || 0)
            var freezeTicks = Math.max(0, Number(enemy.freezeTicks) || 0)
            var contactCooldownMs = Math.max(0, Number(enemy.contactDamageCooldownMs) || 0)
            enemy.y += enemy.speed * enemyMoveSpeedScale * (freezeTicks > 0 ? 0 : slowTicks > 0 ? 0.45 : 1)
            if (slowTicks > 0)
                enemy.slowTicks = slowTicks - 1
            if (freezeTicks > 0)
                enemy.freezeTicks = freezeTicks - 1
            if (contactCooldownMs > 0)
                contactCooldownMs = Math.max(0, contactCooldownMs - enemyUpdateIntervalMs)
            enemy.contactDamageCooldownMs = contactCooldownMs

            if (rectsIntersect(playerRect, enemy)) {
                if (contactCooldownMs <= 0) {
                    lives -= 1
                    enemy.contactDamageCooldownMs = enemyContactDamageCooldownMs
                    if (lives <= 0) {
                        endGame()
                        return
                    }
                }
            }

            if (enemy.y <= height)
                nextEnemies.push(enemy)
        }

        enemies = nextEnemies
    }

    function updateBullets() {
        if (!running || choosingUpgrade)
            return

        var nextBullets = []
        var nextEnemies = enemies.slice()

        for (var i = 0; i < bullets.length; ++i) {
            var bullet = Object.assign({}, bullets[i])
            bullet.cx += bullet.vx
            bullet.cy += bullet.vy

            var bulletRect = {
                x: bullet.cx - bullet.w / 2,
                y: bullet.cy - bullet.h / 2,
                w: bullet.w,
                h: bullet.h
            }

            if (bulletRect.x + bulletRect.w < 0 ||
                bulletRect.x > gameAreaWidth ||
                bulletRect.y + bulletRect.h < gameTop ||
                bulletRect.y > height) {
                continue
            }

            var hit = false
            var piercingSplit = bullet.synthesisType === "frostSplit" && bullet.splitChild
            if (piercingSplit && !bullet.hitEnemyIds)
                bullet.hitEnemyIds = ({})
            for (var j = nextEnemies.length - 1; j >= 0; --j) {
                if (rectsIntersect(bulletRect, nextEnemies[j])) {
                    var enemy = Object.assign({}, nextEnemies[j])
                    var enemyKey = enemy.id !== undefined ? String(enemy.id) : "index:" + j
                    if (piercingSplit && bullet.hitEnemyIds[enemyKey])
                        continue

                    enemy.health -= bullet.damage
                    if (bullet.shape === "rect")
                        enemy.slowTicks = Math.max(Math.max(0, Number(enemy.slowTicks) || 0),
                                                   snowflakeSlowTicks(bullet.level))
                    if (piercingSplit) {
                        var hitIds = bullet.hitEnemyIds
                        hitIds[enemyKey] = true
                        bullet.hitEnemyIds = hitIds
                    } else {
                        hit = true
                    }
                    if (bullet.synthesisType === "frostSplit" && !bullet.splitChild) {
                        var splitBullets = frostSplitBullets(bullet, enemy)
                        for (var s = 0; s < splitBullets.length; ++s)
                            nextBullets.push(splitBullets[s])
                    }
                    if (bullet.synthesisType === "frostBurst") {
                        nextEnemies[j] = enemy
                        nextEnemies = applyFrostBurstExplosion(bullet.cx, bullet.cy, nextEnemies)
                        enemy = Object.assign({}, nextEnemies[j])
                    }
                    if (enemy.health <= 0) {
                        spawnEnemyDeathEffect(enemy)
                        dropExpOrb(enemy.x + enemy.w / 2, enemy.y + enemy.h / 2)
                        dropHealthPotion(enemy.x + enemy.w / 2, enemy.y + enemy.h / 2)
                        nextEnemies.splice(j, 1)
                        score += 10
                    } else {
                        nextEnemies[j] = enemy
                    }
                    if (!piercingSplit)
                        break
                }
            }

            if (piercingSplit || !hit)
                nextBullets.push(bullet)
        }

        bullets = nextBullets
        enemies = nextEnemies
    }

    function fireEnemyBullets() {
        if (!running || choosingUpgrade)
            return

        var nextEnemyBullets = enemyBullets.slice()
        var playerCenterX = playerX + blockSize / 2
        var playerCenterY = playerY + blockSize / 2
        for (var i = 0; i < enemies.length; ++i) {
            var enemy = enemies[i]
            if (enemy.type !== 0)
                continue

            var cx = enemy.x + enemy.w / 2
            var cy = enemy.y + enemy.h / 2
            var dx = playerCenterX - cx
            var dy = playerCenterY - cy
            var distance = Math.sqrt(dx * dx + dy * dy)
            var vx = 0
            var vy = 4
            if (distance > 0) {
                vx = dx / distance * 4
                vy = dy / distance * 4
            }
            nextEnemyBullets.push({ cx: cx, cy: cy, size: 8, vx: vx, vy: vy })
        }
        enemyBullets = nextEnemyBullets
    }

    function updateEnemyBullets() {
        if (!running || choosingUpgrade)
            return

        var playerRect = { x: playerX, y: playerY, w: blockSize, h: blockSize }
        var nextEnemyBullets = []

        for (var i = 0; i < enemyBullets.length; ++i) {
            var bullet = Object.assign({}, enemyBullets[i])
            bullet.cx += bullet.vx
            bullet.cy += bullet.vy
            var bulletRect = {
                x: bullet.cx - bullet.size / 2,
                y: bullet.cy - bullet.size / 2,
                w: bullet.size,
                h: bullet.size
            }

            if (bulletRect.x + bulletRect.w < 0 ||
                bulletRect.x > gameAreaWidth ||
                bulletRect.y + bulletRect.h < gameTop ||
                bulletRect.y > height) {
                continue
            }

            if (rectsIntersect(playerRect, bulletRect)) {
                lives -= 1
                if (lives <= 0) {
                    endGame()
                    return
                }
                continue
            }

            nextEnemyBullets.push(bullet)
        }

        enemyBullets = nextEnemyBullets
    }

    function collectExpOrbs() {
        if (!running || choosingUpgrade)
            return

        var pickupRect = {
            x: playerX - currentPickupPadding,
            y: playerY - currentPickupPadding,
            w: blockSize + currentPickupPadding * 2,
            h: blockSize + currentPickupPadding * 2
        }
        var playerCenterX = playerX + blockSize / 2
        var playerCenterY = playerY + blockSize / 2
        var nextOrbs = []
        var collected = false

        for (var i = 0; i < expOrbs.length; ++i) {
            var orb = Object.assign({}, expOrbs[i])
            var orbRect = {
                x: orb.cx - orb.size / 2,
                y: orb.cy - orb.size / 2,
                w: orb.size,
                h: orb.size
            }

            if (!orb.attracting && rectsIntersect(pickupRect, orbRect))
                orb.attracting = true

            if (orb.attracting) {
                var dx = playerCenterX - orb.cx
                var dy = playerCenterY - orb.cy
                var distance = Math.sqrt(dx * dx + dy * dy)
                if (distance <= 8) {
                    collected = true
                    experience += orb.value
                    continue
                }

                orb.pullTicks = (orb.pullTicks || 0) + 1
                var step = Math.min(distance, 5 + Math.min(10, orb.pullTicks * 0.45))
                if (distance > 0) {
                    orb.cx += dx / distance * step
                    orb.cy += dy / distance * step
                }
                nextOrbs.push(orb)
            } else {
                nextOrbs.push(orb)
            }
        }

        expOrbs = nextOrbs
        if (collected)
            checkLevelUp()
    }

    function collectHealthPotions() {
        if (!running || choosingUpgrade)
            return

        var pickupRect = {
            x: playerX - currentPickupPadding,
            y: playerY - currentPickupPadding,
            w: blockSize + currentPickupPadding * 2,
            h: blockSize + currentPickupPadding * 2
        }
        var playerCenterX = playerX + blockSize / 2
        var playerCenterY = playerY + blockSize / 2
        var nextPotions = []

        for (var i = 0; i < healthPotions.length; ++i) {
            var potion = Object.assign({}, healthPotions[i])
            var potionRect = {
                x: potion.cx - potion.size / 2,
                y: potion.cy - potion.size / 2,
                w: potion.size,
                h: potion.size
            }

            if (!potion.attracting && rectsIntersect(pickupRect, potionRect))
                potion.attracting = true

            if (potion.attracting) {
                var dx = playerCenterX - potion.cx
                var dy = playerCenterY - potion.cy
                var distance = Math.sqrt(dx * dx + dy * dy)
                if (distance <= 8) {
                    lives = Math.min(maxLives, lives + potion.healAmount)
                    continue
                }

                potion.pullTicks = (potion.pullTicks || 0) + 1
                var step = Math.min(distance, 5 + Math.min(10, potion.pullTicks * 0.45))
                if (distance > 0) {
                    potion.cx += dx / distance * step
                    potion.cy += dy / distance * step
                }
                nextPotions.push(potion)
            } else {
                nextPotions.push(potion)
            }
        }

        healthPotions = nextPotions
    }

    function checkLevelUp() {
        if (choosingUpgrade || experience < expToNext)
            return

        experience -= expToNext
        level += 1
        increasePickupRange()
        expToNext = expRequirementForLevel(level)
        showUpgradeChoices()
    }

    function canUpgradeBullet() {
        for (var i = 0; i < bulletLevels.length; ++i) {
            if (bulletLevels[i] < 3)
                return true
        }
        for (var j = 0; j < rectangleBulletLevels.length; ++j) {
            if (rectangleBulletLevels[j] < 3)
                return true
        }
        for (var m = 0; m < mergedBullets.length; ++m) {
            if (mergedBullets[m].level < 3)
                return true
        }
        return false
    }

    function mergeReadyQualityCounts() {
        var counts = ({})
        for (var i = 0; i < bulletLevels.length; ++i) {
            if (bulletLevels[i] === 3)
                counts[1] = (counts[1] || 0) + 1
        }
        for (var j = 0; j < rectangleBulletLevels.length; ++j) {
            if (rectangleBulletLevels[j] === 3)
                counts[1] = (counts[1] || 0) + 1
        }
        for (var m = 0; m < mergedBullets.length; ++m) {
            var bullet = mergedBullets[m]
            var quality = normalizedBulletQuality(bullet.quality)
            if (bullet.level === 3 && quality < maxBulletQuality)
                counts[quality] = (counts[quality] || 0) + 1
        }
        return counts
    }

    function canMergeBullet() {
        var counts = mergeReadyQualityCounts()
        for (var quality in counts) {
            if (Number(quality) < maxBulletQuality && counts[quality] >= 2)
                return true
        }
        return false
    }

    function showUpgradeChoices() {
        choosingUpgrade = true
        choosingBulletToAdd = false
        choosingBulletToUpgrade = false
        choosingBulletsToMerge = false
        selectedMergeBulletIndexes = []
        upgradeBulletChoices = []
        usedUpgradeRefreshIndexes = []
        upgradeSelectionIndex = 0
        upgradeSelectionRow = 0
        resetMoveDir()

        var choices = []
        if (totalOwnedBullets() < 5)
            choices.push({ title: "获得更多子弹", description: "选择获得圆形或冰花子弹。", type: "add" })
        if (canUpgradeBullet())
            choices.push({ title: "升级子弹", description: "将一颗未满 3 级的子弹提升 1 级，合成子弹也可升级。", type: "upgrade" })
        if (canMergeBullet())
            choices.push({ title: "合成升级", description: "消耗两颗同品质 3 级子弹，生成品质 +1、等级 1 的合成子弹。", type: "merge" })

        upgradeChoices = choices
        keyHandler.forceActiveFocus()
    }

    function resumeGameAfterUpgrade() {
        choosingUpgrade = false
        choosingBulletToAdd = false
        choosingBulletToUpgrade = false
        choosingBulletsToMerge = false
        selectedMergeBulletIndexes = []
        upgradeChoices = []
        upgradeBulletChoices = []
        usedUpgradeRefreshIndexes = []
        upgradeSelectionIndex = 0
        upgradeSelectionRow = 0
        bulletLastFireTimes = ({})
        keyHandler.forceActiveFocus()
    }

    function applyUpgrade(index) {
        var choice = upgradeChoices[index]
        if (!choice)
            return

        choosingBulletToAdd = choice.type === "add"
        choosingBulletToUpgrade = choice.type === "upgrade"
        choosingBulletsToMerge = choice.type === "merge"
        selectedMergeBulletIndexes = []
        usedUpgradeRefreshIndexes = []
        upgradeBulletChoices = choice.type === "upgrade" ? drawUpgradeBulletOptions() : []
        upgradeSelectionIndex = 0
        upgradeSelectionRow = 0
    }

    function addShapeOptions() {
        if (totalOwnedBullets() >= 5)
            return []
        return [
            { title: "圆形", shape: "circle", damage: 1 },
            { title: "冰花", shape: "rect", damage: rectangleBulletDamage(1), slow: snowflakeSlowLabel(1) }
        ]
    }

    function applyBulletAddChoice(index) {
        var option = addShapeOptions()[index]
        if (!option)
            return
        if (option.shape === "rect") {
            rectangleBulletLevels = rectangleBulletLevels.concat([1])
        } else {
            bulletLevels = bulletLevels.concat([1])
        }
        resumeGameAfterUpgrade()
    }

    function allUpgradeBulletOptions() {
        var options = []
        for (var i = 0; i < bulletLevels.length; ++i) {
            options.push({
                source: "circle",
                index: i,
                level: bulletLevels[i],
                quality: 1,
                shape: "circle",
                damage: circleBulletDamage(bulletLevels[i]),
                nextDamage: circleBulletDamage(Math.min(3, bulletLevels[i] + 1)),
                merged: false
            })
        }
        for (var j = 0; j < rectangleBulletLevels.length; ++j) {
            options.push({
                source: "rect",
                index: j,
                level: rectangleBulletLevels[j],
                quality: 1,
                shape: "rect",
                damage: rectangleBulletDamage(rectangleBulletLevels[j]),
                nextDamage: rectangleBulletDamage(Math.min(3, rectangleBulletLevels[j] + 1)),
                merged: false
            })
        }
        for (var m = 0; m < mergedBullets.length; ++m) {
            var merged = mergedBullets[m]
            var quality = normalizedBulletQuality(merged.quality)
            var synthesisType = merged.synthesisType || "merged"
            options.push({
                source: "merged",
                index: m,
                level: merged.level,
                quality: quality,
                shape: merged.shape || synthesisShape(synthesisType),
                synthesisType: synthesisType,
                name: merged.name || synthesisName(synthesisType),
                damage: mergedBulletDamage(merged),
                nextDamage: nextMergedBulletDamage(merged),
                merged: true
            })
        }
        return options
    }

    function upgradeableBulletOptions() {
        return allUpgradeBulletOptions().filter(function(option) {
            return option.level < 3
        })
    }

    function upgradeableBulletCount() {
        return upgradeableBulletOptions().length
    }

    function canRefreshUpgradeBulletChoices() {
        return upgradeableBulletCount() > 2
    }

    function drawUpgradeBulletOptions() {
        var upgradeable = upgradeableBulletOptions()
        if (upgradeable.length <= 2)
            return upgradeable

        var pool = upgradeable.slice()
        var picked = []
        for (var i = 0; i < 2; ++i) {
            var pickIndex = Math.floor(Math.random() * pool.length)
            picked.push(pool[pickIndex])
            pool.splice(pickIndex, 1)
        }
        return picked
    }

    function upgradeBulletOptions() {
        return upgradeBulletChoices
    }

    function upgradeBulletChoiceKey(option) {
        if (!option)
            return ""
        return option.source + ":" + option.index
    }

    function upgradeRefreshUsed(index) {
        return usedUpgradeRefreshIndexes.indexOf(index) >= 0
    }

    function markUpgradeRefreshUsed(index) {
        if (upgradeRefreshUsed(index))
            return
        usedUpgradeRefreshIndexes = usedUpgradeRefreshIndexes.concat([index])
    }

    function choosingMainUpgradeOption() {
        return choosingUpgrade &&
               !choosingBulletToAdd &&
               !choosingBulletToUpgrade &&
               !choosingBulletsToMerge
    }

    function upgradeOptionCount() {
        if (!choosingUpgrade)
            return 0
        if (choosingMainUpgradeOption())
            return upgradeChoices.length
        if (choosingBulletToAdd)
            return addShapeOptions().length
        if (choosingBulletToUpgrade)
            return upgradeBulletOptions().length
        if (choosingBulletsToMerge)
            return mergeBulletOptions().length
        return 0
    }

    function canFocusUpgradeRefresh(index) {
        return choosingBulletToUpgrade &&
               canRefreshUpgradeBulletChoices() &&
               index >= 0 &&
               index < upgradeBulletOptions().length &&
               !upgradeRefreshUsed(index)
    }

    function clampUpgradeSelection() {
        var count = upgradeOptionCount()
        if (count <= 0) {
            upgradeSelectionIndex = 0
            upgradeSelectionRow = 0
            return
        }

        upgradeSelectionIndex = Math.max(0, Math.min(count - 1, upgradeSelectionIndex))
        if (upgradeSelectionRow !== 1 || !canFocusUpgradeRefresh(upgradeSelectionIndex))
            upgradeSelectionRow = 0
    }

    function moveUpgradeSelection(delta) {
        var count = upgradeOptionCount()
        if (count <= 0)
            return

        upgradeSelectionIndex = (upgradeSelectionIndex + delta + count) % count
        if (upgradeSelectionRow === 1 && !canFocusUpgradeRefresh(upgradeSelectionIndex))
            upgradeSelectionRow = 0
    }

    function setUpgradeSelectionRow(row) {
        upgradeSelectionRow = row === 1 && canFocusUpgradeRefresh(upgradeSelectionIndex) ? 1 : 0
    }

    function activateUpgradeSelection() {
        clampUpgradeSelection()
        if (upgradeOptionCount() <= 0)
            return

        if (upgradeSelectionRow === 1) {
            refreshUpgradeBulletChoice(upgradeSelectionIndex)
            upgradeSelectionRow = 0
            return
        }

        if (choosingMainUpgradeOption()) {
            applyUpgrade(upgradeSelectionIndex)
        } else if (choosingBulletToAdd) {
            applyBulletAddChoice(upgradeSelectionIndex)
        } else if (choosingBulletToUpgrade) {
            applyBulletUpgrade(upgradeSelectionIndex)
        } else if (choosingBulletsToMerge) {
            applyBulletMergeChoice(upgradeSelectionIndex)
        }
    }

    function handleUpgradeSelectionKey(key) {
        if (!choosingUpgrade)
            return false

        clampUpgradeSelection()
        if (key === Qt.Key_Left || key === Qt.Key_A) {
            moveUpgradeSelection(-1)
            return true
        }
        if (key === Qt.Key_Right || key === Qt.Key_D) {
            moveUpgradeSelection(1)
            return true
        }
        if (key === Qt.Key_Up || key === Qt.Key_W) {
            if (upgradeSelectionRow === 1)
                setUpgradeSelectionRow(0)
            else
                moveUpgradeSelection(-1)
            return true
        }
        if (key === Qt.Key_Down || key === Qt.Key_S) {
            if (upgradeSelectionRow === 0 && canFocusUpgradeRefresh(upgradeSelectionIndex))
                setUpgradeSelectionRow(1)
            else {
                setUpgradeSelectionRow(0)
                moveUpgradeSelection(1)
            }
            return true
        }
        if (key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Space) {
            activateUpgradeSelection()
            return true
        }

        return false
    }

    function refreshUpgradeBulletChoice(index) {
        if (!choosingBulletToUpgrade || !canRefreshUpgradeBulletChoices() || upgradeRefreshUsed(index))
            return

        var choices = upgradeBulletChoices.slice()
        if (index < 0 || index >= choices.length)
            return

        var shownKeys = ({})
        for (var i = 0; i < choices.length; ++i)
            shownKeys[upgradeBulletChoiceKey(choices[i])] = true

        var candidates = upgradeableBulletOptions().filter(function(option) {
            return !shownKeys[upgradeBulletChoiceKey(option)]
        })
        if (candidates.length === 0)
            return

        var pickIndex = Math.floor(Math.random() * candidates.length)
        choices[index] = candidates[pickIndex]
        upgradeBulletChoices = choices
        markUpgradeRefreshUsed(index)
    }

    function applyBulletUpgrade(index) {
        var option = upgradeBulletOptions()[index]
        if (!option || option.level >= 3)
            return

        if (option.source === "rect") {
            var rects = rectangleBulletLevels.slice()
            rects[option.index] += 1
            rectangleBulletLevels = rects
        } else if (option.source === "merged") {
            var merged = mergedBullets.slice()
            var upgraded = Object.assign({}, merged[option.index])
            upgraded.level += 1
            merged[option.index] = upgraded
            mergedBullets = merged
        } else {
            var circles = bulletLevels.slice()
            circles[option.index] += 1
            bulletLevels = circles
        }
        resumeGameAfterUpgrade()
    }

    function mergeBulletOptions() {
        var options = []
        var qualityCounts = mergeReadyQualityCounts()
        for (var i = 0; i < bulletLevels.length; ++i) {
            options.push({
                source: "circle",
                sourceIndex: i,
                listIndex: options.length,
                selectable: bulletLevels[i] === 3 && qualityCounts[1] >= 2,
                level: bulletLevels[i],
                quality: 1,
                damage: circleBulletDamage(bulletLevels[i]),
                shape: "circle",
                merged: false
            })
        }
        for (var m = 0; m < mergedBullets.length; ++m) {
            var merged = mergedBullets[m]
            var quality = normalizedBulletQuality(merged.quality)
            var synthesisType = merged.synthesisType || "merged"
            options.push({
                source: "merged",
                sourceIndex: m,
                listIndex: options.length,
                selectable: quality < maxBulletQuality && merged.level === 3 && qualityCounts[quality] >= 2,
                level: merged.level,
                quality: quality,
                damage: mergedBulletDamage(merged),
                shape: merged.shape || synthesisShape(synthesisType),
                synthesisType: synthesisType,
                name: merged.name || synthesisName(synthesisType),
                merged: true
            })
        }
        for (var r = 0; r < rectangleBulletLevels.length; ++r) {
            options.push({
                source: "rect",
                sourceIndex: r,
                listIndex: options.length,
                selectable: rectangleBulletLevels[r] === 3 && qualityCounts[1] >= 2,
                level: rectangleBulletLevels[r],
                quality: 1,
                damage: rectangleBulletDamage(rectangleBulletLevels[r]),
                shape: "rect",
                merged: false
            })
        }
        return options
    }

    function applyBulletMergeChoice(index) {
        var options = mergeBulletOptions()
        var option = options[index]
        if (!option || !option.selectable)
            return

        var selected = selectedMergeBulletIndexes.slice()
        var existing = selected.indexOf(index)
        if (existing >= 0) {
            selected.splice(existing, 1)
            selectedMergeBulletIndexes = selected
            return
        }

        if (selected.length >= 2)
            selected = []
        if (selected.length === 1) {
            var firstOption = options[selected[0]]
            if (!firstOption || firstOption.quality !== option.quality)
                selected = []
        }
        selected.push(index)
        selectedMergeBulletIndexes = selected

        if (selected.length === 2)
            mergeSelectedBullets()
    }

    function mergeSelectedBullets() {
        var options = mergeBulletOptions()
        var selected = selectedMergeBulletIndexes.slice().sort(function(a, b) { return b - a })
        if (selected.length !== 2) {
            selectedMergeBulletIndexes = []
            return
        }

        var selectedOptions = []
        var mergedDamage = 0
        var mergeQuality = -1

        for (var i = 0; i < selected.length; ++i) {
            var option = options[selected[i]]
            if (!option || !option.selectable) {
                selectedMergeBulletIndexes = []
                return
            }
            if (mergeQuality < 0)
                mergeQuality = option.quality
            if (option.quality !== mergeQuality) {
                selectedMergeBulletIndexes = []
                return
            }
            mergedDamage += option.damage
            selectedOptions.push(option)
        }
        if (mergeQuality >= maxBulletQuality) {
            selectedMergeBulletIndexes = []
            return
        }

        var synthesisType = synthesisTypeForMerge(selectedOptions)

        var circles = bulletLevels.slice()
        var rects = rectangleBulletLevels.slice()
        var merged = mergedBullets.slice()
        for (var j = 0; j < selected.length; ++j) {
            var selectedOption = options[selected[j]]
            if (selectedOption.source === "circle")
                circles.splice(selectedOption.sourceIndex, 1)
            else if (selectedOption.source === "rect")
                rects.splice(selectedOption.sourceIndex, 1)
            else if (selectedOption.source === "merged")
                merged.splice(selectedOption.sourceIndex, 1)
        }

        bulletLevels = circles
        rectangleBulletLevels = rects
        mergedBullets = merged.concat([{
            level: 1,
            quality: Math.min(maxBulletQuality, mergeQuality + 1),
            baseDamage: mergedDamage,
            shape: synthesisShape(synthesisType),
            synthesisType: synthesisType,
            name: synthesisName(synthesisType)
        }])
        resumeGameAfterUpgrade()
    }

    function movePlayer() {
        if (!running || choosingUpgrade)
            return

        var step = 5
        if (Math.abs(joystickDx) > 0.01 || Math.abs(joystickDy) > 0.01) {
            playerX += joystickDx * step
            playerY += joystickDy * step
        } else {
            if (moveDir.left)
                playerX -= step
            if (moveDir.right)
                playerX += step
            if (moveDir.up)
                playerY -= step
            if (moveDir.down)
                playerY += step
        }

        playerX = Math.max(0, Math.min(gameAreaWidth - blockSize, playerX))
        playerY = Math.max(gameTop, Math.min(height - blockSize, playerY))
    }

    Item {
        id: keyHandler
        anchors.fill: parent
        focus: true

        Keys.onPressed: function(event) {
            if (event.isAutoRepeat)
                return
            if (choosingUpgrade) {
                event.accepted = root.handleUpgradeSelectionKey(event.key)
                return
            }
            if (!running)
                return
            if (event.key === Qt.Key_Left || event.key === Qt.Key_A) {
                moveDir.left = true
                event.accepted = true
            } else if (event.key === Qt.Key_Right || event.key === Qt.Key_D) {
                moveDir.right = true
                event.accepted = true
            } else if (event.key === Qt.Key_Up || event.key === Qt.Key_W) {
                moveDir.up = true
                event.accepted = true
            } else if (event.key === Qt.Key_Down || event.key === Qt.Key_S) {
                moveDir.down = true
                event.accepted = true
            }
            root.refreshPlayerMoving()
        }

        Keys.onReleased: function(event) {
            if (event.isAutoRepeat)
                return
            if (event.key === Qt.Key_Left || event.key === Qt.Key_A) {
                moveDir.left = false
                event.accepted = true
            } else if (event.key === Qt.Key_Right || event.key === Qt.Key_D) {
                moveDir.right = false
                event.accepted = true
            } else if (event.key === Qt.Key_Up || event.key === Qt.Key_W) {
                moveDir.up = false
                event.accepted = true
            } else if (event.key === Qt.Key_Down || event.key === Qt.Key_S) {
                moveDir.down = false
                event.accepted = true
            }
            root.refreshPlayerMoving()
        }

        Component.onCompleted: forceActiveFocus()
    }

    Timer {
        interval: 20
        repeat: true
        running: root.running && root.screenState === "playing" && !root.choosingUpgrade
        onTriggered: root.movePlayer()
    }

    Timer {
        interval: root.enemyUpdateIntervalMs
        repeat: true
        running: root.running && root.screenState === "playing" && !root.choosingUpgrade
        onTriggered: root.updateEnemies()
    }

    Timer {
        interval: root.activeEnemySpawnIntervalMs
        repeat: true
        running: root.running && root.screenState === "playing" && !root.choosingUpgrade
        onTriggered: root.spawnEnemy()
    }

    Timer {
        interval: 16
        repeat: true
        running: root.running && root.screenState === "playing" && !root.choosingUpgrade && root.bullets.length > 0
        onTriggered: root.updateBullets()
    }

    Timer {
        interval: 16
        repeat: true
        running: root.deathFragments.length > 0
        onTriggered: root.updateDeathFragments()
    }

    Timer {
        interval: 16
        repeat: true
        running: root.running && root.screenState === "playing" && !root.choosingUpgrade && root.expOrbs.length > 0
        onTriggered: root.collectExpOrbs()
    }

    Timer {
        interval: 16
        repeat: true
        running: root.running && root.screenState === "playing" && !root.choosingUpgrade && root.healthPotions.length > 0
        onTriggered: root.collectHealthPotions()
    }

    Timer {
        interval: root.fireLoopIntervalMs
        repeat: true
        running: root.playerShootingEnabled && root.running && root.screenState === "playing" && !root.choosingUpgrade
        onTriggered: root.fireBullet()
    }

    Timer {
        interval: 1200
        repeat: true
        running: root.running && root.screenState === "playing" && !root.choosingUpgrade
        onTriggered: root.fireEnemyBullets()
    }

    Timer {
        interval: 16
        repeat: true
        running: root.running && root.screenState === "playing" && !root.choosingUpgrade && root.enemyBullets.length > 0
        onTriggered: root.updateEnemyBullets()
    }

    Timer {
        interval: root.currentEnemyHealthIncreaseIntervalMs
        repeat: true
        running: root.running && root.screenState === "playing" && !root.choosingUpgrade && root.currentEnemyHealth < root.maxEnemyHealth
        onTriggered: root.increaseEnemyHealth()
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#171317" }
            GradientStop { position: 0.48; color: "#24262a" }
            GradientStop { position: 1.0; color: "#100d11" }
        }
    }

    Rectangle {
        x: 0
        y: 0
        width: root.gameLeft
        height: parent.height
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#20171a" }
            GradientStop { position: 0.42; color: "#344343" }
            GradientStop { position: 1.0; color: "#151214" }
        }

        Repeater {
            model: 5
            delegate: Rectangle {
                x: -18
                y: 40 + index * 112
                width: 88
                height: 58
                radius: 20
                color: "#7b858355"
                border.width: 2
                border.color: "#25292b"
            }
        }
    }

    Rectangle {
        x: root.gameLeft
        y: 0
        width: root.gameAreaWidth
        height: parent.height
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#898d88" }
            GradientStop { position: 0.5; color: "#a4a29b" }
            GradientStop { position: 1.0; color: "#767a76" }
        }

        Canvas {
            anchors.fill: parent
            opacity: 0.72
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.fillStyle = "rgba(255,255,255,0.05)"
                for (var py = root.gameTop; py < height; py += root.gridSize * 2) {
                    for (var px = 0; px < width; px += root.gridSize * 2)
                        ctx.fillRect(px + 2, py + 2, root.gridSize - 4, root.gridSize - 4)
                }
                ctx.strokeStyle = "rgba(42,51,50,0.62)"
                ctx.lineWidth = 1
                for (var y = root.gameTop; y < height; y += root.gridSize) {
                    ctx.beginPath()
                    ctx.moveTo(0, y + 0.5)
                    ctx.lineTo(width, y + 0.5)
                    ctx.stroke()
                }
                for (var x = 0; x < width; x += root.gridSize) {
                    ctx.beginPath()
                    ctx.moveTo(x + 0.5, root.gameTop)
                    ctx.lineTo(x + 0.5, height)
                    ctx.stroke()
                }
                ctx.strokeStyle = "rgba(255,255,255,0.18)"
                ctx.lineWidth = 2
                ctx.strokeRect(12, root.gameTop + 10, width - 24, height - root.gameTop - 22)
            }
        }
    }

    Rectangle {
        x: root.gameLeft - 4
        width: 8
        height: parent.height
        color: "#2b3836"
        border.width: 1
        border.color: "#82c6ce66"
    }

    Rectangle {
        x: root.gameRight
        y: 0
        width: root.width - root.gameRight
        height: parent.height
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#20171a" }
            GradientStop { position: 0.42; color: "#344343" }
            GradientStop { position: 1.0; color: "#151214" }
        }

        Repeater {
            model: 5
            delegate: Rectangle {
                x: parent.width - 70
                y: 40 + index * 112
                width: 88
                height: 58
                radius: 20
                color: "#7b858355"
                border.width: 2
                border.color: "#25292b"
            }
        }
    }

    Rectangle {
        x: root.gameRight - 4
        width: 8
        height: parent.height
        color: "#2b3836"
        border.width: 1
        border.color: "#82c6ce66"
    }

    Rectangle {
        x: root.gameLeft
        y: root.gameTop - 2
        width: root.gameAreaWidth
        height: 2
        color: "#495250"
    }

    HudButton {
        x: 26
        y: 18
        width: 56
        height: 42
        visible: root.screenState === "playing"
        text: "暂停"
        onClicked: root.pauseGame()
        z: 20
    }

    LevelBadge {
        x: 46
        y: 132
        width: 70
        height: 70
        levelValue: root.level
        progress: root.expToNext > 0 ? root.experience / root.expToNext : 0
        z: 20
    }

    BulletDock {
        x: 28
        y: 220
        width: 88
        height: 290
        z: 20
    }

    Rectangle {
        x: 88
        y: 18
        width: 74
        height: 42
        radius: 10
        color: "#22262d"
        border.width: 2
        border.color: "#717982"
        z: 20
    }

    HealthBar {
        id: healthHud
        x: 156
        y: 22
        width: 230
        height: 28
        value: root.lives
        maximum: root.maxLives
        z: 20
    }

    Image {
        x: 98
        y: 10
        width: 54
        height: 54
        source: root.imageRoot + "/myabi-hd.png"
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        antialiasing: true
        sourceSize.width: 128
        sourceSize.height: 128
        z: 90
    }

    Item {
        x: root.width - 360
        y: 22
        width: 320
        height: 42
        visible: false
        z: 20

        Rectangle {
            anchors.fill: parent
            radius: 18
            color: "#15181e"
            border.width: 3
            border.color: "#5f666e"
        }

        Rectangle {
            x: 16
            y: 14
            width: parent.width - 92
            height: 10
            radius: 5
            color: "#20242a"

            Rectangle {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width * 0.92
                height: parent.height
                radius: parent.radius
                color: "#6cff38"
            }
        }

        Rectangle {
            x: parent.width - 72
            y: 5
            width: 58
            height: 32
            radius: 8
            color: "#243743"
            border.width: 2
            border.color: "#78d5f1"

            Text {
                anchors.centerIn: parent
                text: "BOSS"
                color: "#b8ecff"
                font.pixelSize: 12
                font.bold: true
            }
        }
    }

    SkillButton {
        x: root.width - 146
        y: root.height - 86
        keyText: "X"
        iconSource: root.imageRoot + "/b1.png"
        accentColor: "#ffd733"
        visible: false
        z: 20
    }

    SkillButton {
        x: root.width - 78
        y: root.height - 86
        keyText: "Y"
        iconSource: root.imageRoot + "/b2.png"
        accentColor: "#b25bff"
        visible: false
        z: 20
    }

    Repeater {
        model: root.enemies
        delegate: Item {
            property var enemy: modelData
            x: root.gameLeft + enemy.x
            y: enemy.y
            width: enemy.w
            height: enemy.h
            z: 31

            Rectangle {
                x: -6
                y: -6
                width: parent.width + 12
                height: parent.height + 12
                radius: width / 2
                color: root.enemyColors[enemy.type]
                opacity: 0.32
            }

            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: root.enemyColors[enemy.type]
                border.width: 3
                border.color: "#ffffff66"
            }

            Rectangle {
                x: parent.width * 0.24
                y: parent.height * 0.32
                width: parent.width * 0.18
                height: parent.height * 0.2
                radius: width / 2
                color: "#171022"
            }

            Rectangle {
                x: parent.width * 0.58
                y: parent.height * 0.32
                width: parent.width * 0.18
                height: parent.height * 0.2
                radius: width / 2
                color: "#171022"
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                y: parent.height * 0.58
                text: root.formatNumber(enemy.health)
                color: "#ffffff"
                font.bold: true
                font.pixelSize: 12
            }

            Rectangle {
                visible: (Number(enemy.slowTicks) || 0) > 0
                anchors.centerIn: parent
                width: parent.width + 16
                height: parent.height + 16
                radius: width / 2
                color: "transparent"
                border.width: 3
                border.color: "#8bdcff"
                opacity: 0.78
            }

            Rectangle {
                visible: (Number(enemy.freezeTicks) || 0) > 0
                anchors.centerIn: parent
                width: parent.width + 24
                height: parent.height + 24
                radius: width / 2
                color: "#338bdcff"
                border.width: 4
                border.color: "#dff8ff"
                opacity: 0.88
            }
        }
    }

    Repeater {
        model: root.deathFragments
        delegate: Item {
            property var fragment: modelData
            property real progress: 1 - Math.max(0, fragment.life) / Math.max(1, fragment.maxLife)
            property real alpha: Math.max(0, fragment.life) / Math.max(1, fragment.maxLife)
            property bool ring: fragment.kind === "ring"

            x: root.gameLeft + fragment.cx - width / 2
            y: fragment.cy - height / 2
            width: ring ? fragment.size + progress * 52 : fragment.size
            height: ring ? width : fragment.size * 0.72
            rotation: ring ? 0 : fragment.rotation
            opacity: ring ? alpha * 0.62 : alpha
            z: 34

            Rectangle {
                anchors.fill: parent
                radius: parent.ring ? width / 2 : 2
                color: parent.ring ? "transparent" : fragment.color
                border.width: parent.ring ? 3 : 1
                border.color: parent.ring ? fragment.color : "#ffffff99"
            }
        }
    }

    Repeater {
        model: root.expOrbs
        delegate: Item {
            property var orb: modelData
            property bool attracting: !!orb.attracting
            property real drawSize: orb.size * (attracting ? 1.28 : 1)

            x: root.gameLeft + orb.cx - drawSize / 2
            y: orb.cy - drawSize / 2
            width: drawSize
            height: drawSize
            z: 36

            Rectangle {
                visible: parent.attracting
                anchors.centerIn: parent
                width: parent.width + 14
                height: width
                radius: width / 2
                color: "#50be69"
                opacity: 0.24
            }

            Rectangle {
                anchors.centerIn: parent
                width: parent.width
                height: parent.height
                radius: width / 2
                color: "#50be69"
                border.width: 1
                border.color: "#d6ffe0"
            }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                y: parent.height * 0.18
                width: parent.width * 0.34
                height: parent.height * 0.22
                radius: width / 2
                color: "#f0fff4"
                opacity: parent.attracting ? 0.9 : 0.65
            }
        }
    }

    Repeater {
        model: root.healthPotions
        delegate: Item {
            property var potion: modelData
            property bool attracting: !!potion.attracting
            property real drawSize: potion.size * (attracting ? 1.24 : 1)

            x: root.gameLeft + potion.cx - drawSize / 2
            y: potion.cy - drawSize / 2
            width: drawSize
            height: drawSize
            z: 37

            Rectangle {
                visible: parent.attracting
                anchors.centerIn: parent
                width: parent.width + 14
                height: width
                radius: width / 2
                color: "#ff4f6f"
                opacity: 0.22
            }

            Rectangle {
                anchors.centerIn: parent
                width: parent.width * 0.82
                height: parent.height
                radius: 4
                color: "#f5f7fb"
                border.width: 2
                border.color: "#ff4f6f"
            }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                y: parent.height * 0.08
                width: parent.width * 0.42
                height: parent.height * 0.16
                radius: 2
                color: "#ff4f6f"
            }

            Rectangle {
                anchors.centerIn: parent
                width: parent.width * 0.18
                height: parent.height * 0.58
                radius: 2
                color: "#ff4f6f"
            }

            Rectangle {
                anchors.centerIn: parent
                width: parent.width * 0.52
                height: parent.height * 0.18
                radius: 2
                color: "#ff4f6f"
            }
        }
    }

    Repeater {
        model: root.bullets
        delegate: Item {
            property var bullet: modelData
            property real drawSize: Math.max(18, Math.max(bullet.w, bullet.h) + 6)
            readonly property color effectColor: bullet.shape === "rect"
                                                ? "#8bdcff"
                                                : bullet.merged
                                                ? root.qualityColor(bullet.quality || 2)
                                                : root.bulletColor(bullet.level || 1)
            readonly property real travelAngle: Math.atan2(bullet.vy, bullet.vx) * 180 / Math.PI
            x: root.gameLeft + bullet.cx - drawSize / 2
            y: bullet.cy - drawSize / 2
            width: drawSize
            height: drawSize
            z: 35

            Rectangle {
                anchors.centerIn: parent
                width: parent.drawSize * 1.35
                height: Math.max(6, parent.drawSize * 0.28)
                radius: height / 2
                color: parent.effectColor
                opacity: 0.22
                rotation: parent.travelAngle
            }

            Rectangle {
                anchors.centerIn: parent
                width: parent.drawSize * 1.22
                height: parent.drawSize * 1.22
                radius: width / 2
                color: parent.effectColor
                opacity: 0.18
            }

            Image {
                anchors.fill: parent
                source: bullet.merged && bullet.synthesisType !== "frostSplit" && bullet.synthesisType !== "frostBurst" ? "" : root.bulletImage(bullet.shape)
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
                antialiasing: true
                sourceSize.width: 64
                sourceSize.height: 64
            }

            Rectangle {
                anchors.centerIn: parent
                width: parent.width * 0.65
                height: parent.height * 0.65
                radius: width / 2
                visible: bullet.merged && bullet.synthesisType !== "frostSplit" && bullet.synthesisType !== "frostBurst"
                color: root.qualityColor(bullet.quality || 2)
                border.width: 2
                border.color: "#f4e0ff"
            }

            Rectangle {
                anchors.centerIn: parent
                width: parent.drawSize * 0.34
                height: parent.drawSize * 0.34
                radius: width / 2
                color: "#ffffff"
                opacity: 0.34
            }
        }
    }

    Repeater {
        model: root.enemyBullets
        delegate: Item {
            property var bullet: modelData
            readonly property real travelAngle: Math.atan2(bullet.vy, bullet.vx) * 180 / Math.PI
            x: root.gameLeft + bullet.cx - bullet.size / 2
            y: bullet.cy - bullet.size / 2
            width: bullet.size
            height: bullet.size

            Rectangle {
                anchors.centerIn: parent
                width: parent.width + 20
                height: 5
                radius: height / 2
                color: "#ff8a76"
                opacity: 0.26
                rotation: parent.travelAngle
            }

            Rectangle {
                anchors.centerIn: parent
                width: parent.width + 12
                height: parent.height + 12
                radius: width / 2
                color: "#ff3828"
                opacity: 0.28
            }

            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: "#ff5038"
                border.width: 2
                border.color: "#ffb7aa"
            }
        }
    }

    Image {
        id: playerSprite
        property real walkTilt: 0
        property real walkBob: 0

        x: root.gameLeft + root.playerX + root.blockSize / 2 - root.playerDrawSize / 2
        y: root.playerY + root.blockSize / 2 - root.playerDrawSize / 2 + walkBob
        width: root.playerDrawSize
        height: root.playerDrawSize
        rotation: walkTilt
        transformOrigin: Item.Center
        source: root.imageRoot + "/myabi-hd.png"
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        antialiasing: true
        sourceSize.width: Math.ceil(root.playerDrawSize * 4)
        sourceSize.height: Math.ceil(root.playerDrawSize * 4)
        z: 31

        SequentialAnimation on walkTilt {
            running: root.playerMoving && root.running && root.screenState === "playing" && !root.choosingUpgrade
            loops: Animation.Infinite
            NumberAnimation { to: -5; duration: 90; easing.type: Easing.InOutQuad }
            NumberAnimation { to: 5; duration: 180; easing.type: Easing.InOutQuad }
            NumberAnimation { to: 0; duration: 90; easing.type: Easing.InOutQuad }
            onStopped: playerSprite.walkTilt = 0
        }

        SequentialAnimation on walkBob {
            running: root.playerMoving && root.running && root.screenState === "playing" && !root.choosingUpgrade
            loops: Animation.Infinite
            NumberAnimation { to: -3; duration: 90; easing.type: Easing.InOutQuad }
            NumberAnimation { to: 2; duration: 90; easing.type: Easing.InOutQuad }
            NumberAnimation { to: 0; duration: 90; easing.type: Easing.InOutQuad }
            onStopped: playerSprite.walkBob = 0
        }
    }

    Canvas {
        anchors.fill: parent
        visible: root.screenState === "playing"
        opacity: 0.12
        z: 30
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.fillStyle = "#000000"
            for (var y = 0; y < height; y += 4)
                ctx.fillRect(0, y, width, 1)
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: root.screenState === "playing"
        z: 29
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#66000000" }
            GradientStop { position: 0.22; color: "#00000000" }
            GradientStop { position: 0.78; color: "#00000000" }
            GradientStop { position: 1.0; color: "#66000000" }
        }
    }

    MultiPointTouchArea {
        id: joystickTouchArea

        x: root.gameLeft
        y: root.gameTop
        width: root.gameAreaWidth
        height: root.height - root.gameTop
        enabled: root.mobileInputEnabled
                 && root.running
                 && root.screenState === "playing"
                 && !root.choosingUpgrade
        mouseEnabled: false
        z: 42

        touchPoints: [
            TouchPoint { id: joystickTouchPoint }
        ]

        onPressed: {
            if (joystickTouchPoint.pressed)
                root.beginJoystick(joystickTouchArea.x + joystickTouchPoint.x,
                                   joystickTouchArea.y + joystickTouchPoint.y)
        }

        onUpdated: {
            if (joystickTouchPoint.pressed)
                root.updateJoystick(joystickTouchArea.x + joystickTouchPoint.x,
                                    joystickTouchArea.y + joystickTouchPoint.y)
        }

        onReleased: root.endJoystick()
        onCanceled: root.endJoystick()
    }

    Item {
        id: joystickBase

        x: root.joystickAnchorX - width / 2
        y: root.joystickAnchorY - height / 2
        width: root.joystickBaseSize
        height: width
        visible: root.mobileInputEnabled && root.joystickVisible
        z: 43

        Rectangle {
            x: 4
            y: 7
            width: parent.width
            height: parent.height
            radius: width / 2
            color: "#50000000"
        }

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "#c011151c"
            border.width: 3
            border.color: "#8bdcff"
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 12
            radius: width / 2
            color: "#1c5f73b9"
            border.width: 2
            border.color: "#556cffd6"
        }

        Rectangle {
            x: parent.width / 2 - width / 2 + root.joystickDx * root.joystickHandleTravel
            y: parent.height / 2 - height / 2 + root.joystickDy * root.joystickHandleTravel
            width: 42
            height: 42
            radius: width / 2
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#e8edf1" }
                GradientStop { position: 0.52; color: "#8bdcff" }
                GradientStop { position: 1.0; color: "#2d82e6" }
            }
            border.width: 2
            border.color: "#ffffff"
        }
    }

    Text {
        visible: false
        x: root.gameLeft
        y: 360
        width: root.gameAreaWidth
        height: 40
        text: "Click Start to play"
        color: "#464b55"
        font.pixelSize: 18
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    Rectangle {
        anchors.fill: parent
        visible: root.choosingUpgrade
        color: "#c0101117"
        z: 50

        Canvas {
            anchors.fill: parent
            opacity: 0.22
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = "#ffffff"
                ctx.lineWidth = 1
                for (var x = -height; x < width; x += 10) {
                    ctx.beginPath()
                    ctx.moveTo(x, height)
                    ctx.lineTo(x + height, 0)
                    ctx.stroke()
                }
            }
        }

        Rectangle {
            id: upgradePanel

            property bool choosingMainOption: !root.choosingBulletToAdd &&
                                              !root.choosingBulletToUpgrade &&
                                              !root.choosingBulletsToMerge
            property int cardCount: choosingMainOption ? root.upgradeChoices.length
                                    : root.choosingBulletToAdd ? root.addShapeOptions().length
                                    : root.choosingBulletToUpgrade ? root.upgradeBulletOptions().length
                                    : root.choosingBulletsToMerge ? root.mergeBulletOptions().length
                                    : 0
            property int cardSpacing: choosingMainOption ? 16
                                      : root.choosingBulletToUpgrade ? 22
                                      : root.choosingBulletToAdd ? 18
                                      : 16
            property int rowY: choosingMainOption ? 0
                               : root.choosingBulletsToMerge ? 46
                               : root.choosingBulletToUpgrade ? 42
                               : 34
            property int upgradeRefreshGap: 12
            property int upgradeRefreshSize: 54
            property bool showUpgradeRefresh: root.choosingBulletToUpgrade &&
                                              root.canRefreshUpgradeBulletChoices()
            property int contentHeight: rowY + root.selectionCardHeight +
                                        (showUpgradeRefresh
                                         ? upgradeRefreshGap + upgradeRefreshSize + 10
                                         : 10)

            anchors.centerIn: parent
            width: Math.min(parent.width - 56,
                            Math.max(root.selectionCardWidth * 2 + 18,
                                     cardCount * root.selectionCardWidth +
                                     Math.max(0, cardCount - 1) * cardSpacing))
            height: Math.min(parent.height - 96, contentHeight)
            radius: 24
            color: "transparent"
            border.width: 0
            border.color: "#cbd3df"

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                y: -52
                width: Math.min(parent.width - 40, 430)
                height: 40
                radius: 20
                color: "#08090c"
                border.width: 3
                border.color: "#a9f21c"

                Text {
                    anchors.centerIn: parent
                    width: parent.width - 28
                    text: upgradePanel.choosingMainOption ? "你升级了，请选择升级奖励"
                          : root.choosingBulletToAdd ? "选择要获得的子弹"
                          : root.choosingBulletToUpgrade ? "选择要升级的子弹"
                          : root.choosingBulletsToMerge ? "选择要合成的两颗子弹"
                          : "等级 " + root.level
                    color: "#ffffff"
                    font.pixelSize: 20
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Text {
                visible: root.choosingBulletsToMerge
                x: 24
                y: 2
                width: parent.width - 48
                height: 24
                color: "#dfe7ef"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: "已选择 " + root.selectedMergeBulletIndexes.length + "/2"
            }

            Row {
                visible: root.choosingBulletToAdd
                anchors.horizontalCenter: parent.horizontalCenter
                y: 34
                spacing: 18

                Repeater {
                    model: root.choosingBulletToAdd ? root.addShapeOptions() : []
                    delegate: BulletChoiceCard {
                        width: root.selectionCardWidth
                        height: root.selectionCardHeight
                        title: modelData.title
                        subtitle: "伤害 " + root.formatNumber(modelData.damage)
                                  + (modelData.slow ? " / " + modelData.slow : "")
                        shape: modelData.shape
                        levelValue: 1
                        enabledChoice: true
                        keyboardFocused: root.upgradeSelectionIndex === index &&
                                         root.upgradeSelectionRow === 0
                        onClicked: root.applyBulletAddChoice(index)
                    }
                }
            }

            Row {
                visible: root.choosingBulletToUpgrade
                anchors.horizontalCenter: parent.horizontalCenter
                y: 42
                spacing: 22

                Repeater {
                    model: root.choosingBulletToUpgrade ? root.upgradeBulletOptions() : []
                    delegate: Column {
                        id: upgradeChoiceColumn

                        property int choiceIndex: index

                        width: root.selectionCardWidth
                        spacing: upgradePanel.showUpgradeRefresh ? upgradePanel.upgradeRefreshGap : 0

                        BulletUpgradeCard {
                            shape: modelData.shape
                            synthesisType: modelData.synthesisType || ""
                            title: modelData.name || ""
                            levelValue: modelData.level
                            qualityValue: modelData.quality
                            damageValue: modelData.damage
                            nextDamageValue: modelData.nextDamage
                            merged: modelData.merged
                            enabledChoice: modelData.level < 3
                            keyboardFocused: root.upgradeSelectionIndex === upgradeChoiceColumn.choiceIndex &&
                                             root.upgradeSelectionRow === 0
                            onClicked: root.applyBulletUpgrade(upgradeChoiceColumn.choiceIndex)
                        }

                        UpgradeRefreshButton {
                            visible: upgradePanel.showUpgradeRefresh
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: upgradePanel.upgradeRefreshSize
                            height: upgradePanel.showUpgradeRefresh ? upgradePanel.upgradeRefreshSize : 0
                            enabledChoice: upgradePanel.showUpgradeRefresh &&
                                           !root.upgradeRefreshUsed(upgradeChoiceColumn.choiceIndex)
                            keyboardFocused: root.upgradeSelectionIndex === upgradeChoiceColumn.choiceIndex &&
                                             root.upgradeSelectionRow === 1
                            onClicked: root.refreshUpgradeBulletChoice(upgradeChoiceColumn.choiceIndex)
                        }
                    }
                }
            }

            Row {
                visible: root.choosingBulletsToMerge
                anchors.horizontalCenter: parent.horizontalCenter
                y: 46
                spacing: 16

                Repeater {
                    model: root.choosingBulletsToMerge ? root.mergeBulletOptions() : []
                    delegate: BulletChoiceCard {
                        width: root.selectionCardWidth
                        height: root.selectionCardHeight
                        title: root.bulletQualityLabel(modelData.quality) + " Lv." + modelData.level
                        subtitle: "伤害 " + root.formatNumber(modelData.damage)
                        shape: modelData.shape
                        synthesisType: modelData.synthesisType || ""
                        levelValue: modelData.level
                        qualityValue: modelData.quality
                        merged: modelData.merged
                        selectedChoice: root.selectedMergeBulletIndexes.indexOf(index) >= 0
                        keyboardFocused: root.upgradeSelectionIndex === index &&
                                         root.upgradeSelectionRow === 0
                        enabledChoice: modelData.selectable
                        onClicked: root.applyBulletMergeChoice(index)
                    }
                }
            }

            Row {
                visible: upgradePanel.choosingMainOption
                anchors.horizontalCenter: parent.horizontalCenter
                y: 0
                spacing: 16

                Repeater {
                    model: root.upgradeChoices
                    delegate: UpgradeCard {
                        width: root.selectionCardWidth
                        height: root.selectionCardHeight
                        title: modelData.title
                        description: modelData.description
                        keyboardFocused: root.upgradeSelectionIndex === index &&
                                         root.upgradeSelectionRow === 0
                        onClicked: root.applyUpgrade(index)
                    }
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: root.screenState === "paused"
        color: "#c0101117"
        z: 70

        Canvas {
            anchors.fill: parent
            opacity: 0.22
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = "#ffffff"
                ctx.lineWidth = 1
                for (var x = -height; x < width; x += 10) {
                    ctx.beginPath()
                    ctx.moveTo(x, height)
                    ctx.lineTo(x + height, 0)
                    ctx.stroke()
                }
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: 320
            height: 270
            radius: 18
            color: "#11151c"
            border.width: 4
            border.color: "#a9f21c"

            Rectangle {
                anchors.fill: parent
                anchors.margins: 6
                radius: 14
                color: "#5f73b9"
                border.width: 2
                border.color: "#22272d"
            }

            Rectangle {
                x: 12
                y: 12
                width: parent.width - 24
                height: 94
                radius: 12
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#8a9fe1" }
                    GradientStop { position: 1.0; color: "#6076bd" }
                }
            }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                y: 28
                width: 58
                height: 58
                radius: 29
                color: "#7d8fc9"
                border.width: 4
                border.color: "#2f3947"

                Text {
                    anchors.centerIn: parent
                    text: "II"
                    color: "#415079"
                    font.pixelSize: 28
                    font.bold: true
                }
            }

            Text {
                x: 30
                y: 122
                width: parent.width - 60
                height: 36
                text: "游戏暂停"
                color: "#ffffff"
                font.pixelSize: 20
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter

                Rectangle {
                    anchors.fill: parent
                    z: -1
                    radius: 16
                    color: "#2d3138"
                    border.width: 1
                    border.color: "#171a20"
                }
            }

            HudButton {
                anchors.horizontalCenter: parent.horizontalCenter
                y: 176
                width: 220
                height: 36
                text: "继续"
                accentColor: "#a9f21c"
                onClicked: root.resumeGame()
            }

            HudButton {
                anchors.horizontalCenter: parent.horizontalCenter
                y: 222
                width: 220
                height: 36
                text: "退出"
                accentColor: "#7f8a95"
                onClicked: root.stopGame()
            }
        }
    }

    Item {
        anchors.fill: parent
        visible: root.screenState === "menu" || root.screenState === "gameOver"
        z: 80

        Rectangle {
            anchors.fill: parent
            color: "#c0101117"

            Canvas {
                anchors.fill: parent
                opacity: 0.22
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = "#ffffff"
                    ctx.lineWidth = 1
                    for (var x = -height; x < width; x += 10) {
                        ctx.beginPath()
                        ctx.moveTo(x, height)
                        ctx.lineTo(x + height, 0)
                        ctx.stroke()
                    }
                }
            }
        }

        Rectangle {
            id: menuPanel
            anchors.centerIn: parent
            width: root.screenState === "gameOver" ? 390 : 370
            height: root.screenState === "gameOver" ? 460 : 400
            radius: 20
            color: "#11151c"
            border.width: 4
            border.color: "#a9f21c"

            Rectangle {
                anchors.fill: parent
                anchors.margins: 6
                radius: 15
                color: "#5f73b9"
                border.width: 2
                border.color: "#22272d"
            }

            Rectangle {
                x: 12
                y: 12
                width: parent.width - 24
                height: 118
                radius: 13
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#8a9fe1" }
                    GradientStop { position: 1.0; color: "#6076bd" }
                }
            }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                y: 28
                width: 92
                height: 92
                radius: 46
                color: "#151920"
                border.width: 5
                border.color: "#2f3947"

                Image {
                    anchors.centerIn: parent
                    width: 54
                    height: 54
                    source: root.imageRoot + "/b2.png"
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                    sourceSize.width: 128
                    sourceSize.height: 128
                }
            }

            Text {
                x: 40
                y: 146
                width: parent.width - 80
                height: 40
                text: root.screenState === "gameOver" ? "挑战结束" : "弹幕试炼"
                color: "#ffffff"
                font.pixelSize: 22
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter

                Rectangle {
                    anchors.fill: parent
                    z: -1
                    radius: 18
                    color: "#2d3138"
                    border.width: 1
                    border.color: "#171a20"
                }
            }

            MenuPreviewSlot {
                anchors.horizontalCenter: parent.horizontalCenter
                y: 202
                imageSource: root.imageRoot + "/myabi-hd.png"
                accentColor: "#49d4bd"
            }

            HudButton {
                anchors.horizontalCenter: parent.horizontalCenter
                y: root.screenState === "gameOver" ? 296 : 304
                width: 230
                height: 42
                text: root.screenState === "gameOver" ? "重新开始" : "开始游戏"
                accentColor: "#a9f21c"
                onClicked: root.startGame()
            }

            HudButton {
                anchors.horizontalCenter: parent.horizontalCenter
                y: root.screenState === "gameOver" ? 350 : 356
                width: 230
                height: 38
                text: root.screenState === "gameOver" ? "返回菜单" : "退出"
                accentColor: "#7f8a95"
                onClicked: {
                    if (root.screenState === "gameOver") {
                        root.screenState = "menu"
                    } else {
                        root.exitRequested()
                    }
                }
            }

            Row {
                visible: root.screenState === "gameOver"
                anchors.horizontalCenter: parent.horizontalCenter
                y: 398
                spacing: 12

                MenuStat {
                    label: "LEVEL"
                    value: root.screenState === "gameOver" ? root.level : 1
                }
            }
        }
    }

    component MenuPreviewSlot: Rectangle {
        property string imageSource: ""
        property color accentColor: "#8df3e7"

        width: 86
        height: 86
        radius: 16
        color: "#11151c"
        border.width: 4
        border.color: accentColor

        Rectangle {
            anchors.fill: parent
            anchors.margins: 6
            radius: 12
            color: "#5f73b9"
            border.width: 2
            border.color: "#22272d"
        }

        Rectangle {
            anchors.centerIn: parent
            width: 62
            height: 62
            radius: 31
            color: "#151920"
            border.width: 4
            border.color: "#2f3947"
        }

        Image {
            anchors.centerIn: parent
            width: 48
            height: 48
            source: imageSource
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
            sourceSize.width: 160
            sourceSize.height: 160
        }
    }

    component MenuStat: Rectangle {
        property string label: ""
        property int value: 0

        width: 128
        height: 58
        radius: 12
        color: "#11151c"
        border.width: 3
        border.color: "#7f8a95"

        Rectangle {
            anchors.fill: parent
            anchors.margins: 5
            radius: 8
            color: "#5f73b9"
            border.width: 1
            border.color: "#22272d"
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 9
            text: parent.label
            color: "#ffffff"
            font.pixelSize: 10
            font.bold: true
        }

        Rectangle {
            x: 12
            y: 26
            width: parent.width - 24
            height: 24
            radius: 9
            color: "#d8dce0"
            border.width: 1
            border.color: "#7d848c"
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 27
            text: parent.value
            color: "#2c3138"
            font.pixelSize: 20
            font.bold: true
        }
    }

    component HudButton: Rectangle {
        id: button
        property string text: ""
        property color accentColor: "#7f8a95"
        signal clicked()

        radius: Math.min(18, height / 2)
        color: mouseArea.pressed ? "#020304" : mouseArea.containsMouse ? "#151923" : "#08090c"
        border.width: 2
        border.color: mouseArea.containsMouse ? accentColor : "#252a31"

        Text {
            anchors.centerIn: parent
            text: button.text
            color: "#e8edf1"
            font.pixelSize: Math.max(14, Math.min(18, parent.height * 0.42))
            font.bold: true
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: button.clicked()
        }
    }

    component StatusChip: Rectangle {
        property string text: ""
        property color accentColor: "#2d82e6"

        radius: 8
        color: "#11151c"
        border.width: 2
        border.color: accentColor

        Rectangle {
            anchors.fill: parent
            anchors.margins: 4
            radius: 6
            color: "#5f73b9"
            border.width: 1
            border.color: "#22272d"
        }

        Rectangle {
            x: 8
            y: 7
            width: 8
            height: parent.height - 14
            radius: 4
            color: parent.accentColor
        }

        Text {
            x: 22
            width: parent.width - 32
            height: parent.height
            text: parent.text
            color: "#ffffff"
            font.pixelSize: 16
            font.bold: true
            verticalAlignment: Text.AlignVCenter
        }
    }

    component HealthBar: Rectangle {
        id: healthBar

        property int value: 0
        property int maximum: 1
        readonly property real ratio: maximum > 0 ? Math.max(0, Math.min(1, value / maximum)) : 0

        radius: height / 2
        border.width: 3
        border.color: "#5d656b"
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#3e454a" }
            GradientStop { position: 1.0; color: "#101317" }
        }

        Rectangle {
            x: 14
            y: 9
            width: parent.width - 28
            height: parent.height - 18
            radius: height / 2
            color: "#0b0d10"
            border.width: 1
            border.color: "#000000"

            Rectangle {
                width: parent.width * healthBar.ratio
                height: parent.height
                radius: parent.radius
                gradient: Gradient {
                    GradientStop { position: 0.0; color: healthBar.ratio > 0.2 ? "#d8ff28" : "#ff5b5b" }
                    GradientStop { position: 1.0; color: healthBar.ratio > 0.2 ? "#24ff56" : "#d62237" }
                }
            }
        }
    }

    component LevelBadge: Item {
        property int levelValue: 1
        property real progress: 0

        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: 2
            anchors.topMargin: 3
            radius: width / 2
            color: "#55000000"
        }

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#eef1f4" }
                GradientStop { position: 0.52; color: "#707479" }
                GradientStop { position: 1.0; color: "#2f3135" }
            }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 6
            radius: width / 2
            color: "#111217"
            border.width: 4
            border.color: "#6a6f74"
        }

        Canvas {
            id: levelRingCanvas
            property real badgeProgress: parent.progress

            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.lineWidth = 4
                ctx.lineCap = "round"
                ctx.strokeStyle = "#ffb224"
                ctx.beginPath()
                ctx.arc(width / 2,
                        height / 2,
                        width / 2 - 8,
                        Math.PI * 0.75,
                        Math.PI * 0.75 + Math.PI * 1.5 * Math.max(0, Math.min(1, badgeProgress)),
                        false)
                ctx.stroke()
            }
            onBadgeProgressChanged: requestPaint()
        }

        Text {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -4
            text: parent.levelValue
            color: "#ffbe2d"
            font.pixelSize: 22
            font.bold: true
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            y: parent.height * 0.61
            text: "LEVEL"
            color: "#96989e"
            font.pixelSize: 6
            font.bold: true
        }
    }

    component BulletDock: Rectangle {
        id: dock

        radius: 28
        border.width: 3
        border.color: "#697178"
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#484f54" }
            GradientStop { position: 0.48; color: "#1c2025" }
            GradientStop { position: 1.0; color: "#090b0e" }
        }

        Repeater {
            model: 5
            delegate: BulletSlot {
                x: (dock.width - width) / 2
                y: 18 + index * 54
                slotIndex: index
            }
        }
    }

    component BulletSlot: Item {
        id: slot

        property int slotIndex: 0
        property var displayInfos: root.bulletLevels.map(function(v) {
            return { shape: "circle", level: v, quality: 1, damage: root.circleBulletDamage(v), merged: false }
        }).concat(root.mergedBullets.map(function(v) {
            var synthesisType = v.synthesisType || "merged"
            return {
                shape: v.shape || root.synthesisShape(synthesisType),
                level: v.level,
                quality: root.normalizedBulletQuality(v.quality),
                damage: root.mergedBulletDamage(v),
                merged: true,
                synthesisType: synthesisType,
                name: v.name || root.synthesisName(synthesisType)
            }
        })).concat(root.rectangleBulletLevels.map(function(v) {
            return { shape: "rect", level: v, quality: 1, damage: root.rectangleBulletDamage(v), merged: false }
        }))
        property var info: slotIndex < displayInfos.length ? displayInfos[slotIndex] : null

        width: 54
        height: 50

        Rectangle {
            x: 3
            y: 4
            width: 44
            height: 44
            radius: 22
            color: "#5f000000"
        }

        Rectangle {
            x: 0
            y: 0
            width: 44
            height: 44
            radius: 22
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#f6f8f9" }
                GradientStop { position: 0.45; color: "#8d9297" }
                GradientStop { position: 1.0; color: "#2f3134" }
            }
        }

        Rectangle {
            x: 6
            y: 6
            width: 32
            height: 32
            radius: 16
            color: "#0a0b0d"
        }

        Image {
            visible: parent.info !== null && (!parent.info.merged || parent.info.synthesisType === "frostSplit" || parent.info.synthesisType === "frostBurst")
            x: 10
            y: 10
            width: 24
            height: 24
            source: parent.info ? root.bulletImage(parent.info.shape) : ""
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
            antialiasing: true
            sourceSize.width: 64
            sourceSize.height: 64
        }

        Rectangle {
            visible: parent.info !== null && parent.info.merged && parent.info.synthesisType !== "frostSplit" && parent.info.synthesisType !== "frostBurst"
            x: 12
            y: 12
            width: 20
            height: 20
            radius: 10
            color: root.qualityColor(parent.info ? parent.info.quality : 2)
            border.width: 1
            border.color: "#f4e0ff"
        }

        Text {
            visible: parent.info === null
            x: 7
            y: 5
            width: 30
            height: 30
            text: "+"
            color: "#5f656d"
            font.pixelSize: 28
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        Rectangle {
            x: 5
            y: 35
            width: 34
            height: 13
            radius: 3
            color: parent.info ? "#57585a" : "#4a4b4d"
            border.width: 1
            border.color: "#1c1d20"

            Text {
                anchors.centerIn: parent
                text: slot.info ? root.bulletQualityLabel(slot.info.quality) + " L" + slot.info.level : "--"
                color: slot.info ? "#f5f6f8" : "#a5a8ac"
                font.pixelSize: 8
                font.bold: true
            }
        }
    }

    component SkillButton: Item {
        property string keyText: ""
        property string iconSource: ""
        property color accentColor: "#ffd733"

        width: 60
        height: 76

        Rectangle {
            x: 4
            y: 0
            width: 54
            height: 54
            radius: 27
            color: "#0b0d10"
            border.width: 4
            border.color: "#5d656b"
        }

        Rectangle {
            x: 10
            y: 6
            width: 42
            height: 42
            radius: 21
            color: "#15191e"
            border.width: 3
            border.color: accentColor

            Image {
                anchors.centerIn: parent
                width: 28
                height: 28
                source: iconSource
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
                sourceSize.width: 96
                sourceSize.height: 96
            }
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 52
            width: 22
            height: 22
            radius: 11
            color: "#050609"
            border.width: 2
            border.color: "#24313a"

            Text {
                anchors.centerIn: parent
                text: keyText
                color: accentColor
                font.pixelSize: 14
                font.bold: true
            }
        }
    }

    component BulletChoiceCard: Rectangle {
        id: card
        property string title: ""
        property string subtitle: ""
        property string shape: "circle"
        property string synthesisType: ""
        property int levelValue: 1
        property int qualityValue: 1
        property bool merged: false
        property bool enabledChoice: true
        property bool selectedChoice: false
        property bool keyboardFocused: false
        signal clicked()

        width: 170
        height: 236
        radius: 16
        color: "#11151c"
        border.width: keyboardFocused ? 5 : 4
        border.color: selectedChoice ? "#a9f21c" : keyboardFocused ? "#ffffff" : enabledChoice ? "#7f8a95" : "#5b626a"
        opacity: enabledChoice || selectedChoice || keyboardFocused ? 1.0 : 0.56
        scale: keyboardFocused ? 1.04 : 1.0

        Rectangle {
            anchors.fill: parent
            anchors.margins: 6
            radius: 12
            color: selectedChoice || keyboardFocused ? "#7d8fc9" : "#5f73b9"
            border.width: 2
            border.color: "#22272d"
        }

        Rectangle {
            x: 12
            y: 12
            width: parent.width - 24
            height: 78
            radius: 11
            gradient: Gradient {
                GradientStop { position: 0.0; color: selectedChoice || keyboardFocused ? "#b5c6ff" : "#8a9fe1" }
                GradientStop { position: 1.0; color: selectedChoice || keyboardFocused ? "#7790df" : "#6076bd" }
            }
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 26
            width: 54
            height: 54
            radius: 27
            color: "#151920"
            border.width: 4
            border.color: "#2f3947"

            Image {
                visible: !card.merged || card.synthesisType === "frostSplit" || card.synthesisType === "frostBurst"
                anchors.centerIn: parent
                width: 34
                height: 34
                source: root.bulletImage(card.shape)
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
                antialiasing: true
                sourceSize.width: 96
                sourceSize.height: 96
            }

            Rectangle {
                visible: card.merged && card.synthesisType !== "frostSplit" && card.synthesisType !== "frostBurst"
                anchors.centerIn: parent
                width: 28
                height: 28
                radius: 14
                color: root.qualityColor(card.qualityValue)
                border.width: 2
                border.color: "#f4e0ff"
            }
        }

        Rectangle {
            visible: true
            x: parent.width - 48
            y: 18
            width: 30
            height: 24
            radius: 10
            color: root.qualityColor(card.qualityValue)
            border.width: 2
            border.color: "#20252b"

            Text {
                anchors.centerIn: parent
                text: root.bulletQualityLabel(card.qualityValue)
                color: "#11151c"
                font.pixelSize: 15
                font.bold: true
            }
        }

        Text {
            x: 14
            y: 100
            width: parent.width - 28
            height: 30
            text: card.title
            color: "#ffffff"
            font.pixelSize: 15
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            Rectangle {
                anchors.fill: parent
                z: -1
                radius: 14
                color: "#2d3138"
                border.width: 1
                border.color: "#171a20"
            }
        }

        Rectangle {
            x: 12
            y: 142
            width: parent.width - 24
            height: 44
            radius: 10
            color: "#d8dce0"
            border.width: 2
            border.color: "#7d848c"

            Text {
                anchors.centerIn: parent
                width: parent.width - 20
                text: card.subtitle
                color: "#2c3138"
                font.pixelSize: 13
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.WordWrap
            }
        }

        Rectangle {
            visible: false
            anchors.horizontalCenter: parent.horizontalCenter
            y: parent.height - 38
            width: 118
            height: 28
            radius: 14
            color: "#08090c"
            border.width: 2
            border.color: "#252a31"

            Text {
                anchors.centerIn: parent
                text: card.enabledChoice ? "选择" : "不可选"
                color: "#ffffff"
                font.pixelSize: 14
                font.bold: true
            }
        }

        MouseArea {
            anchors.fill: parent
            enabled: card.enabledChoice
            onClicked: card.clicked()
        }
    }

    component BulletUpgradeCard: Rectangle {
        id: card

        property string shape: "circle"
        property string title: ""
        property string synthesisType: ""
        property int levelValue: 1
        property int qualityValue: 1
        property real damageValue: 1
        property real nextDamageValue: 0
        property bool merged: false
        property bool enabledChoice: true
        property bool keyboardFocused: false
        signal clicked()

        readonly property int nextLevel: Math.min(3, levelValue + 1)
        readonly property real nextDamage: nextDamageValue > 0 ? nextDamageValue
                                           : shape === "rect"
                                           ? root.rectangleBulletDamage(nextLevel)
                                           : root.circleBulletDamage(nextLevel)
        readonly property string displayTitle: card.merged
                                               ? (card.title.length > 0 ? card.title : root.synthesisName(card.synthesisType))
                                               : shape === "rect" ? "冰花" : "圆形"

        width: root.selectionCardWidth
        height: root.selectionCardHeight
        radius: 16
        color: "#11151c"
        border.width: keyboardFocused ? 5 : 4
        border.color: keyboardFocused ? "#ffffff" : enabledChoice ? "#a9f21c" : "#6c747d"
        opacity: enabledChoice || keyboardFocused ? 1.0 : 0.56
        scale: keyboardFocused ? 1.04 : 1.0

        Rectangle {
            anchors.fill: parent
            anchors.margins: 6
            radius: 12
            color: keyboardFocused ? "#7d8fc9" : "#5f73b9"
            border.width: 2
            border.color: "#23282f"
        }

        Rectangle {
            x: 14
            y: 14
            width: parent.width - 28
            height: 78
            radius: 11
            gradient: Gradient {
                GradientStop { position: 0.0; color: keyboardFocused ? "#b5c6ff" : "#8a9fe1" }
                GradientStop { position: 1.0; color: keyboardFocused ? "#7790df" : "#6076bd" }
            }
        }

        Rectangle {
            x: 16
            y: 16
            width: 64
            height: 22
            radius: 9
            color: "#4964b7"
            border.width: 2
            border.color: "#273354"

            Text {
                anchors.centerIn: parent
                text: "Lv." + card.levelValue + " +1"
                color: "#ffffff"
                font.pixelSize: 12
                font.bold: true
            }
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 38
            width: 48
            height: 48
            radius: 24
            color: "#7d8fc9"
            border.width: 4
            border.color: "#2f3947"

            Image {
                anchors.centerIn: parent
                width: 30
                height: 30
                source: root.bulletImage(card.shape)
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
                sourceSize.width: 128
                sourceSize.height: 128
            }
        }

        Rectangle {
            x: parent.width - 48
            y: 18
            width: 30
            height: 24
            radius: 10
            color: root.qualityColor(card.qualityValue)
            border.width: 2
            border.color: "#20252b"

            Text {
                anchors.centerIn: parent
                text: root.bulletQualityLabel(card.qualityValue)
                color: "#11151c"
                font.pixelSize: 15
                font.bold: true
            }
        }

        Text {
            x: 14
            y: 100
            width: parent.width - 28
            height: 30
            text: root.bulletQualityLabel(card.qualityValue) + " 品质 " + card.displayTitle
            color: "#ffffff"
            font.pixelSize: 15
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            Rectangle {
                anchors.fill: parent
                z: -1
                radius: 14
                color: "#2d3138"
                border.width: 1
                border.color: "#171a20"
            }
        }

        Rectangle {
            x: 12
            y: 142
            width: parent.width - 24
            height: 44
            radius: 10
            color: "#d8dce0"
            border.width: 2
            border.color: "#7d848c"

            Text {
                x: 10
                y: 5
                width: parent.width - 20
                height: 16
                text: "伤害 " + root.formatNumber(card.damageValue) + " > " + root.formatNumber(card.nextDamage)
                color: "#2c3138"
                font.pixelSize: 12
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                x: 10
                y: 23
                width: parent.width - 20
                height: 16
                text: root.bulletFireUpgradeLabel(card.levelValue, card.qualityValue, shape, card.synthesisType)
                color: "#2c3138"
                font.pixelSize: 12
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Rectangle {
            visible: false
            anchors.horizontalCenter: parent.horizontalCenter
            y: parent.height - 38
            width: 118
            height: 28
            radius: 14
            color: "#232824"
            border.width: 2
            border.color: "#6e756f"

            Text {
                anchors.centerIn: parent
                text: enabledChoice ? "选择" : "已满级"
                color: "#ffffff"
                font.pixelSize: 14
                font.bold: true
            }
        }

        MouseArea {
            anchors.fill: parent
            enabled: card.enabledChoice
            onClicked: card.clicked()
        }
    }

    component UpgradeRefreshButton: Rectangle {
        id: button

        property bool enabledChoice: true
        property bool keyboardFocused: false
        signal clicked()

        width: 54
        height: 54
        radius: width / 2
        color: "#11151c"
        border.width: keyboardFocused ? 4 : 3
        border.color: keyboardFocused ? "#ffffff" : enabledChoice ? "#252a31" : "#6c747d"
        opacity: enabledChoice || keyboardFocused ? 1.0 : 0.56
        scale: keyboardFocused ? 1.08 : 1.0

        Rectangle {
            anchors.fill: parent
            anchors.margins: 5
            radius: width / 2
            color: keyboardFocused ? "#7d8fc9" : "#5f73b9"
            border.width: 2
            border.color: "#22272d"
        }

        Rectangle {
            anchors.centerIn: parent
            width: 36
            height: 36
            radius: width / 2
            color: "#7d8fc9"
            border.width: 3
            border.color: "#2f3947"

            Canvas {
                id: refreshIcon

                anchors.centerIn: parent
                width: 24
                height: 24

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = "#ffffff"
                    ctx.fillStyle = "#ffffff"
                    ctx.lineWidth = 3
                    ctx.lineCap = "round"
                    ctx.beginPath()
                    ctx.arc(width / 2, height / 2, 7.5, Math.PI * 0.15, Math.PI * 1.72, false)
                    ctx.stroke()

                    ctx.save()
                    ctx.translate(width / 2, height / 2)
                    ctx.rotate(Math.PI * 1.72)
                    ctx.beginPath()
                    ctx.moveTo(7.5, 0)
                    ctx.lineTo(3.5, -4)
                    ctx.lineTo(3, 2.8)
                    ctx.closePath()
                    ctx.fill()
                    ctx.restore()
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            enabled: button.enabledChoice
            onClicked: button.clicked()
        }
    }

    component UpgradeCard: Rectangle {
        id: card
        property string title: ""
        property string description: ""
        property bool keyboardFocused: false
        signal clicked()

        width: root.selectionCardWidth
        height: root.selectionCardHeight
        radius: 16
        color: "#11151c"
        border.width: keyboardFocused ? 5 : 4
        border.color: keyboardFocused ? "#ffffff" : "#a9f21c"
        scale: keyboardFocused ? 1.04 : 1.0

        Rectangle {
            anchors.fill: parent
            anchors.margins: 6
            radius: 12
            color: keyboardFocused ? "#7d8fc9" : "#5f73b9"
            border.width: 2
            border.color: "#22272d"
        }

        Rectangle {
            x: 12
            y: 12
            width: parent.width - 24
            height: 78
            radius: 11
            gradient: Gradient {
                GradientStop { position: 0.0; color: keyboardFocused ? "#b5c6ff" : "#8a9fe1" }
                GradientStop { position: 1.0; color: keyboardFocused ? "#7790df" : "#6076bd" }
            }
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 26
            width: 54
            height: 54
            radius: 27
            color: "#7d8fc9"
            border.width: 4
            border.color: "#2f3947"

            Text {
                anchors.centerIn: parent
                text: "G"
                color: "#415079"
                font.pixelSize: 32
                font.bold: true
            }
        }

        Text {
            x: 14
            y: 100
            width: parent.width - 28
            height: 30
            text: card.title
            color: "#ffffff"
            font.pixelSize: 15
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            Rectangle {
                anchors.fill: parent
                z: -1
                radius: 14
                color: "#2d3138"
                border.width: 1
                border.color: "#171a20"
            }
        }

        Rectangle {
            x: 12
            y: 142
            width: parent.width - 24
            height: 44
            radius: 10
            color: "#d8dce0"
            border.width: 2
            border.color: "#7d848c"

            Text {
                x: 10
                y: 5
                width: parent.width - 20
                height: parent.height - 10
                text: card.description
                color: "#2c3138"
                font.pixelSize: 12
                font.bold: true
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Rectangle {
            visible: false
            anchors.horizontalCenter: parent.horizontalCenter
            y: parent.height - 38
            width: 118
            height: 28
            radius: 14
            color: "#08090c"
            border.width: 2
            border.color: "#252a31"

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                text: "A"
                color: "#18e35f"
                font.pixelSize: 14
                font.bold: true
            }

            Text {
                anchors.centerIn: parent
                text: "选择"
                color: "#ffffff"
                font.pixelSize: 14
                font.bold: true
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: card.clicked()
        }
    }

    Component.onCompleted: resetGame()
}
