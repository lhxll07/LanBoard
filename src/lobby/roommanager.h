#pragma once

#include <QObject>

class RoomManager : public QObject
{
    Q_OBJECT

public:
    explicit RoomManager(QObject *parent = nullptr);
};
