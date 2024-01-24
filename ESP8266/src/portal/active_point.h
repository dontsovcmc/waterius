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

void start_active_point(Settings &sett, CalculatedData &cdata);

String processor_main(const String &var, const uint8_t input = 0xFF);

#endif
