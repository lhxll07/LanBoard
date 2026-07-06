#pragma once

#include <QObject>

/**
 * @brief 抽象基类：所有游戏控制器的生命周期接口
 *
 * 统一四款游戏的公共生命周期：开始、重置、胜负判断。
 * 每个子类再添加自己特有的 Q_PROPERTY 和 Q_INVOKABLE。
 *
 * 注意事项：
 * - 不声明 Q_PROPERTY，由各子类自行声明（避免多重信号冲突）
 * - gameOverChanged 信号在基类声明，子类直接 emit 即可
 */
class GameControllerBase : public QObject
{
    Q_OBJECT

public:
    explicit GameControllerBase(QObject *parent = nullptr);
    ~GameControllerBase() override = default;

    // ---- 生命周期接口（子类必须实现） ----
    virtual bool isGameOver() const = 0;
    virtual int winner() const = 0;
    virtual void startNewGame() = 0;
    virtual void reset() = 0;

signals:
    /** 对局结束（胜负已分） */
    void gameOverChanged();
};
