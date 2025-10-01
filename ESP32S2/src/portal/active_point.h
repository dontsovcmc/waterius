/**
 * @file active_point.h
 * @brief Веб сервер для настроки ватериуса
 * @version 0.1
 * @date 2023-09-28
 *
 * @copyright Copyright (c) 2023
 *
 */
#ifndef ACTIVE_POINT_h_
#define ACTIVE_POINT_h_

class Settings;
class SlaveData;
class CalculatedData;

enum class active_point_state_t {
	Idle,
	Start,
	Run,
	Stop,
	Finish,
	Error
};

active_point_state_t active_point();

String processor_main(const String &var, const uint8_t input = 0xFF);

#endif
