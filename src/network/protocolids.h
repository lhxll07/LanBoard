#pragma once

#include <QString>

namespace LanBoard::Protocol {

inline const QString Join = QStringLiteral("join");
inline const QString ListRooms = QStringLiteral("list_rooms");
inline const QString RoomsList = QStringLiteral("rooms_list");
inline const QString CreateRoom = QStringLiteral("create_room");
inline const QString JoinRoom = QStringLiteral("join_room");
inline const QString RoomState = QStringLiteral("room_state");
inline const QString Ready = QStringLiteral("ready");
inline const QString StartGame = QStringLiteral("start_game");
inline const QString GameStart = QStringLiteral("game_start");
inline const QString SwitchRoomGame = QStringLiteral("switch_room_game");
inline const QString ChangeSeat = QStringLiteral("change_seat");
inline const QString PlacePiece = QStringLiteral("place_piece");
inline const QString FlightRoll = QStringLiteral("flight_roll");
inline const QString FlightMove = QStringLiteral("flight_move");
inline const QString Surrender = QStringLiteral("surrender");
inline const QString GameOver = QStringLiteral("game_over");
inline const QString DouDiZhuPlay = QStringLiteral("ddz_play");
inline const QString DouDiZhuPass = QStringLiteral("ddz_pass");
inline const QString DouDiZhuState = QStringLiteral("ddz_state");
inline const QString Error = QStringLiteral("error");

}  // namespace LanBoard::Protocol
