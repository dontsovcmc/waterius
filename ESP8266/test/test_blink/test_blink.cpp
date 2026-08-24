#include <gtest/gtest.h>
#include "core/blink.h"
#include "core/types.h"

/*
Светодиод — единственный канал, по которому Ватериус2 объясняет пользователю,
чем закончился сеанс: интернета может не быть, и сервер про сеанс не узнает.
Поэтому важно, какой именно код выбирается, когда упало сразу несколько вещей.
*/

TEST(BlinkCode, SuccessfulSessionBlinksGreen)
{
    SessionStatus status;
    status.cloud = SEND_OK;

    EXPECT_EQ(blink_code(status), ERROR_OK);
}

TEST(BlinkCode, SilentWakeupIsNotAnError)
{
    // Режим "только при расходе": Wi-Fi не поднимали, никто не отправлял.
    // Значения по умолчанию не должны выглядеть как отказ (#361)
    SessionStatus status;

    EXPECT_EQ(blink_code(status), ERROR_OK);
}

TEST(BlinkCode, ConfigWinsOverEverything)
{
    // Конфигурация не загрузилась — этим же кодом моргает несостоявшийся
    // обмен с attiny. Всё, что дальше, недостоверно
    SessionStatus status;
    status.config_loaded = false;
    status.wifi_connected = false;
    status.cloud = SEND_NO_CONNECTION;
    status.low_voltage = true;

    EXPECT_EQ(blink_code(status), ERROR_CONFIG);
}

TEST(BlinkCode, RouterWinsOverCloud)
{
    // Без роутера облако не могло ответить в принципе: показываем причину,
    // а не следствие
    SessionStatus status;
    status.wifi_connected = false;
    status.cloud = SEND_NO_CONNECTION;
    status.mqtt = SEND_NO_CONNECTION;

    EXPECT_EQ(blink_code(status), ERROR_CONNECT_ROUTER);
}

TEST(BlinkCode, CloudAnswerIsDistinctFromNoConnection)
{
    SessionStatus no_connection;
    no_connection.cloud = SEND_NO_CONNECTION;

    SessionStatus bad_answer;
    bad_answer.cloud = SEND_BAD_ANSWER;

    EXPECT_EQ(blink_code(no_connection), ERROR_CONNECT_CLOUD);
    EXPECT_EQ(blink_code(bad_answer), ERROR_CLOUD_ANSWER);
}

TEST(BlinkCode, MqttFailureIsReportedWhenCloudIsFine)
{
    SessionStatus status;
    status.cloud = SEND_OK;
    status.mqtt = SEND_NO_CONNECTION;

    EXPECT_EQ(blink_code(status), ERROR_CONNECT_MQTT);
}

TEST(BlinkCode, LowVoltageShowsUpOnlyWhenSessionWentThrough)
{
    // Просевшее питание — не причина отказа, а повод заняться источником.
    // Если сеанс всё же упал, полезнее увидеть, на чём именно
    SessionStatus ok_session;
    ok_session.cloud = SEND_OK;
    ok_session.low_voltage = true;

    SessionStatus failed_session;
    failed_session.cloud = SEND_NO_CONNECTION;
    failed_session.low_voltage = true;

#if WATERIUS_MODEL == WATERIUS_MODEL_2
    EXPECT_EQ(blink_code(ok_session), ERROR_LOW_VOLTAGE);
#else
    EXPECT_EQ(blink_code(ok_session), ERROR_OK);
#endif
    EXPECT_EQ(blink_code(failed_session), ERROR_CONNECT_CLOUD);
}

TEST(BlinkCode, ClassicDoesNotBlinkLowVoltage)
{
    /*
    На классике ESP меряет собственное питание после регулятора
    (voltage.cpp:update зовёт ESP.getVcc), а регулятор держит 3,0 В. До
    порога 2,9 В остаётся 100 мВ, то есть о состоянии источника значение
    молчит, пока регулятор не сдастся. Успевает сработать только правило
    просадки, а просадка при включении радио бывает и на здоровом
    устройстве: вышла бы одна вспышка почти каждый сеанс.
    */
    SessionStatus status;
    status.cloud = SEND_OK;
    status.low_voltage = true;

#if WATERIUS_MODEL == WATERIUS_MODEL_1
    EXPECT_EQ(blink_code(status), ERROR_OK);
#else
    EXPECT_EQ(blink_code(status), ERROR_LOW_VOLTAGE);
#endif
}

TEST(BlinkCode, SkippedSenderIsNotAFailure)
{
    // waterius.ru выключен, данные ушли по MQTT — моргать ошибкой не за что
    SessionStatus status;
    status.cloud = SEND_SKIPPED;
    status.mqtt = SEND_OK;

    EXPECT_EQ(blink_code(status), ERROR_OK);
}

TEST(MergeStatus, WorstResultWins)
{
    // Два облачных получателя, одна вспышка: показываем худший исход
    EXPECT_EQ(merge_status(SEND_OK, SEND_NO_CONNECTION), SEND_NO_CONNECTION);
    EXPECT_EQ(merge_status(SEND_NO_CONNECTION, SEND_OK), SEND_NO_CONNECTION);
    EXPECT_EQ(merge_status(SEND_BAD_ANSWER, SEND_NO_CONNECTION), SEND_NO_CONNECTION);
    EXPECT_EQ(merge_status(SEND_OK, SEND_BAD_ANSWER), SEND_BAD_ANSWER);
}

TEST(MergeStatus, SkippedIsNeutral)
{
    // Ненастроенный получатель не портит итог и не улучшает его
    EXPECT_EQ(merge_status(SEND_SKIPPED, SEND_OK), SEND_OK);
    EXPECT_EQ(merge_status(SEND_OK, SEND_SKIPPED), SEND_OK);
    EXPECT_EQ(merge_status(SEND_SKIPPED, SEND_NO_CONNECTION), SEND_NO_CONNECTION);
    EXPECT_EQ(merge_status(SEND_SKIPPED, SEND_SKIPPED), SEND_SKIPPED);
}

TEST(BlinkCode, CodesMatchDocumentedBlinkCounts)
{
    // Число вспышек напечатано в FAQ.md — менять значения нельзя
    EXPECT_EQ(ERROR_OK, 0);
    EXPECT_EQ(ERROR_LOW_VOLTAGE, 1);
    EXPECT_EQ(ERROR_CONNECT_ROUTER, 2);
    EXPECT_EQ(ERROR_CONNECT_CLOUD, 3);
    EXPECT_EQ(ERROR_CONNECT_MQTT, 4);
    EXPECT_EQ(ERROR_CONFIG, 5);
    EXPECT_EQ(ERROR_CLOUD_ANSWER, 6);
}
