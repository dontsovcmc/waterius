#ifndef _WATERIUS_CORE_CONNECT_LOG_h
#define _WATERIUS_CORE_CONNECT_LOG_h

/*
Журнал ошибок связи: как прошли попытки до текущей посылки.

Между сеансами питание ЕСП снято, поэтому журнал лежит в Settings, а не в ОЗУ.
Не очищается: новая ошибка с прежним кодом видна по росту wifi_connect_errors.

Часть чистого ядра src/core: без Arduino.h и логирования.
*/

#include "types.h"

#include <stddef.h>
#include <stdint.h>

// Имя не разрешилось: коды клиента и брокера такого случая не различают
#define CONNECT_CODE_DNS -100

// "mqtt:-32768" три раза, две запятые и ноль
#define CONNECT_LOG_TEXT_LEN 36

void connect_log_push(Settings &sett, ConnectStage stage, int16_t code);

// От старой записи к новой: wifi:201,wtr:-1,wtr:500. Пока ошибок не было - пустая строка
void connect_log_text(const Settings &sett, char *out, size_t size);

#endif
