#include <gtest/gtest.h>
#include "core/alarm.h"

/*
Пересчёт порогов тревог и разбор ответа attiny (issue #202).

Пороги пользователь задаёт в литрах в час (в ваттах для электричества), а
attiny сравнивает тики сторожевого таймера по 250 мс. Пересчёт здесь, потому
что вес импульса и тип счётчика знает только ЕСП.
*/

TEST(Alarm, WaterThreshold)
{
    // 600 л/ч при 10 л/имп — импульс раз в минуту, это 240 тиков
    EXPECT_EQ(flow_to_interval_ticks(600, 10, false), 240);

    // Тот же расход у счётчика на 100 л/имп — импульс раз в 10 минут
    EXPECT_EQ(flow_to_interval_ticks(600, 100, false), 2400);
}

TEST(Alarm, ElectricityThreshold)
{
    // 3000 Вт при 1000 имп/кВт*ч — 3000 импульсов в час, это 4,8 тика
    EXPECT_EQ(flow_to_interval_ticks(3000, 1000, true), 4);
}

TEST(Alarm, DisabledThreshold)
{
    // Ноль — тревога выключена, и это умолчание у прошитых устройств
    EXPECT_EQ(flow_to_interval_ticks(0, 10, false), 0);
}

TEST(Alarm, UnknownFactorIsNotDivided)
{
    // Вес импульса ноль — делить не на что, тревога выключена
    EXPECT_EQ(flow_to_interval_ticks(600, 0, false), 0);
    EXPECT_EQ(flow_to_interval_ticks(3000, 0, true), 0);
}

TEST(Alarm, TinyThresholdSaturates)
{
    /*
    Порог 1 л/ч при 100 л/имп — импульс раз в 100 часов, в uint16 не влезает.
    Насыщаем, а не обнуляем: ноль означал бы "выключено", то есть обратное
    тому, что просил пользователь.
    */
    EXPECT_EQ(flow_to_interval_ticks(1, 100, false), UINT16_MAX);
}

TEST(Alarm, HugeThresholdStaysArmed)
{
    /*
    Порог выше, чем счётчик способен выдать: округление вниз дало бы ноль,
    то есть молча выключенную тревогу.
    */
    EXPECT_EQ(flow_to_interval_ticks(60000, 1, false), 1);
    EXPECT_GT(flow_to_interval_ticks(60000, 60000, true), 0);
}

TEST(Alarm, BitsPerInput)
{
    // Вход 0 — биты 1-3, вход 1 — биты 4-6, бит 0 занят флагом питания
    const uint8_t flags = ATTINY_FLAG_ESP_POWERED_LONG |
                          (ALARM_FLOW << ATTINY_ALARM_SHIFT0) |
                          (ALARM_WET << ATTINY_ALARM_SHIFT1);

    EXPECT_EQ(alarm_bits(flags, INPUT0_RED, ATTINY_VER_ALARM), ALARM_FLOW);
    EXPECT_EQ(alarm_bits(flags, INPUT1_BLUE, ATTINY_VER_ALARM), ALARM_WET);
}

TEST(Alarm, AllBitsOfOneInput)
{
    // Три тревоги на одном входе держатся одновременно
    const uint8_t flags = (ALARM_FLOW | ALARM_LEAK | ALARM_WET) << ATTINY_ALARM_SHIFT1;

    EXPECT_EQ(alarm_bits(flags, INPUT1_BLUE, ATTINY_VER_ALARM),
              ALARM_FLOW | ALARM_LEAK | ALARM_WET);
    EXPECT_EQ(alarm_bits(flags, INPUT0_RED, ATTINY_VER_ALARM), 0);
}

TEST(Alarm, OldAttinyHasNoAlarms)
{
    /*
    У attiny 40 в этом байте только флаг питания, остальные биты ничего не
    значат. Прочитать их как тревоги — показать пользователю аварию на
    исправном устройстве.
    */
    const uint8_t flags = 0xFF;

    EXPECT_EQ(alarm_bits(flags, INPUT0_RED, ATTINY_VER_POWER_FLAGS), 0);
    EXPECT_EQ(alarm_bits(flags, INPUT1_BLUE, ATTINY_VER_POWER_FLAGS), 0);
    EXPECT_NE(alarm_bits(flags, INPUT0_RED, ATTINY_VER_ALARM), 0);
}

TEST(Alarm, ConfigurableNeedsKnownFactor)
{
    // До первой настройки вес импульса — спецзначение, пересчитывать не из чего
    EXPECT_FALSE(alarm_configurable(CounterType::NAMUR, AUTO_IMPULSE_FACTOR));
    EXPECT_FALSE(alarm_configurable(CounterType::NAMUR, AS_COLD_CHANNEL));
    EXPECT_FALSE(alarm_configurable(CounterType::NAMUR, 0));

    EXPECT_TRUE(alarm_configurable(CounterType::NAMUR, 10));
}

TEST(Alarm, ConfigurableForElectricity)
{
    // Электричество разрешено: формула другая, но вес импульса известен
    EXPECT_TRUE(alarm_configurable(CounterType::ELECTRONIC, 1000));
}

TEST(Alarm, DisabledInputHasNoAlarms)
{
    EXPECT_FALSE(alarm_configurable(CounterType::NONE, 10));
}

TEST(Alarm, LeakSensorHasNoFlowThreshold)
{
    // Датчик протечки не считает импульсы: порог расхода ему не из чего считать
    EXPECT_FALSE(alarm_configurable(CounterType::LEAKAGE, 10));
    EXPECT_FALSE(alarm_configurable(CounterType::LEAKAGE_NC, 10));
}

// --- режим "я уехал" (#88) ---

TEST(Vacation, OverridesThreshold)
{
    // Уехал - тревогой становится любой расход, а не только выше порога
    EXPECT_EQ(alarm_interval_ticks(true, CounterType::NAMUR, 600, 10, false), UINT16_MAX);
    EXPECT_EQ(alarm_interval_ticks(true, CounterType::ELECTRONIC, 3000, 1000, true), UINT16_MAX);
}

TEST(Vacation, OffKeepsUserThreshold)
{
    EXPECT_EQ(alarm_interval_ticks(false, CounterType::NAMUR, 600, 10, false),
              flow_to_interval_ticks(600, 10, false));
}

TEST(Vacation, OverridesEvenDisabledAlarm)
{
    // Порог не задан, но уехал - расход всё равно должен стать тревогой:
    // иначе режим молча не работал бы на самых частых настройках
    EXPECT_EQ(alarm_interval_ticks(true, CounterType::NAMUR, 0, 10, false), UINT16_MAX);
}

TEST(Vacation, WorksWithoutKnownImpulseWeight)
{
    /*
    Вес импульса нужен, чтобы пересчитать литры в час в тики. Режиму отпуска
    пересчитывать нечего: тревогой объявлен любой импульс. Спецзначение веса
    (вход ещё не настроен до конца) не должно молча выключать режим.
    */
    EXPECT_EQ(alarm_interval_ticks(true, CounterType::NAMUR, 0, AUTO_IMPULSE_FACTOR, false),
              UINT16_MAX);
    EXPECT_EQ(alarm_interval_ticks(true, CounterType::NAMUR, 0, AS_COLD_CHANNEL, false),
              UINT16_MAX);

    // А обычный порог без веса импульса по-прежнему не посчитать
    EXPECT_EQ(alarm_interval_ticks(false, CounterType::NAMUR, 600, AUTO_IMPULSE_FACTOR, false), 0);
}

TEST(Vacation, SkipsInputsWithoutImpulses)
{
    // У датчика протечки и выключенного входа импульсов нет
    EXPECT_EQ(alarm_interval_ticks(true, CounterType::LEAKAGE, 600, 10, false), 0);
    EXPECT_EQ(alarm_interval_ticks(true, CounterType::LEAKAGE_NC, 600, 10, false), 0);
    EXPECT_EQ(alarm_interval_ticks(true, CounterType::NONE, 600, 10, false), 0);
}

TEST(Vacation, SaturatedThresholdNeverClears)
{
    /*
    attiny снимает тревогу расхода, когда (ticks >> 1) > min_interval. При
    максимальном пороге это невозможно ни при каком ticks, поэтому "расход
    был" держится до выключения режима - и оповещение приходит один раз.
    */
    const uint16_t interval = alarm_interval_ticks(true, CounterType::NAMUR, 0, 10, false);

    for (uint32_t ticks = 0; ticks <= UINT16_MAX; ticks += 257)
    {
        ASSERT_LE(ticks >> 1, interval) << "ticks=" << ticks;
    }
    EXPECT_LE((uint32_t)UINT16_MAX >> 1, interval);
}

/*
Квитанция о доставке тревоги (#202).

Пока квитанции нет, attiny будит ЕСП снова: до ALARM_MAX_TRIES попыток и не
больше ALARM_MAX_SESSIONS внеплановых сеансов на период пробуждения. Значит
"доставлено" оплачивается батареей, и решает тут не наблюдение, а настройка
пользователя.
*/

// Сеанс, в котором всё, что было настроено, отработало без ошибок
static SessionStatus session(const SendStatus waterius, const SendStatus http,
                             const SendStatus mqtt)
{
    SessionStatus status;
    status.waterius = waterius;
    status.http = http;
    status.mqtt = mqtt;
    status.delivered_any = (waterius == SEND_OK || http == SEND_OK || mqtt == SEND_OK);
    return status;
}

TEST(AlarmDelivered, MaskZeroKeepsOldBehaviour)
{
    // Умолчание: прошитые устройства читают ноль и не должны заметить правки
    EXPECT_TRUE(alarm_delivered(CONFIRM_ANY, session(SEND_OK, SEND_SKIPPED, SEND_NO_CONNECTION)));
    EXPECT_TRUE(alarm_delivered(CONFIRM_ANY, session(SEND_BAD_ANSWER, SEND_SKIPPED, SEND_OK)));
    EXPECT_FALSE(alarm_delivered(CONFIRM_ANY, session(SEND_BAD_ANSWER, SEND_SKIPPED, SEND_NO_CONNECTION)));
}

TEST(AlarmDelivered, RequiredRecipientMustAccept)
{
    /*
    Ради этого всё и затевалось: авария, доехавшая до облака, но не до
    домашнего брокера, доложена наполовину. Квитанции нет - attiny попробует
    ещё раз, не дожидаясь планового сеанса.
    */
    const SessionStatus status = session(SEND_OK, SEND_SKIPPED, SEND_NO_CONNECTION);

    EXPECT_TRUE(status.delivered_any);
    EXPECT_FALSE(alarm_delivered(CONFIRM_MQTT, status));
    EXPECT_TRUE(alarm_delivered(CONFIRM_WATERIUS, status));
}

TEST(AlarmDelivered, DisabledRequiredIsDropped)
{
    /*
    Галочку на брокере поставили, а сам брокер потом выключили - в том числе
    удалённо, из Home Assistant. Требовать доставки некуда, и настаивать
    нельзя: это пять лишних сеансов на каждом периоде пробуждения, пока
    пользователь не заметит.
    */
    const SessionStatus status = session(SEND_OK, SEND_SKIPPED, SEND_SKIPPED);

    EXPECT_TRUE(alarm_delivered(CONFIRM_MQTT, status));
    EXPECT_FALSE(alarm_delivered(CONFIRM_MQTT, session(SEND_BAD_ANSWER, SEND_SKIPPED, SEND_SKIPPED)));
}

TEST(AlarmDelivered, AllRequiredMustAccept)
{
    // Отмечены оба, оба настроены: один упал - квитанции нет
    EXPECT_FALSE(alarm_delivered(CONFIRM_WATERIUS | CONFIRM_MQTT,
                                 session(SEND_OK, SEND_SKIPPED, SEND_BAD_ANSWER)));
    EXPECT_FALSE(alarm_delivered(CONFIRM_WATERIUS | CONFIRM_MQTT,
                                 session(SEND_NO_CONNECTION, SEND_SKIPPED, SEND_OK)));
    EXPECT_TRUE(alarm_delivered(CONFIRM_WATERIUS | CONFIRM_MQTT,
                                session(SEND_OK, SEND_BAD_ANSWER, SEND_OK)));
}

TEST(AlarmDelivered, PartlyDisabledChecksTheRest)
{
    // Отмечены свой сервер и MQTT, свой сервер выключен - судим по MQTT
    const uint8_t mask = CONFIRM_HTTP | CONFIRM_MQTT;

    EXPECT_TRUE(alarm_delivered(mask, session(SEND_SKIPPED, SEND_SKIPPED, SEND_OK)));
    EXPECT_FALSE(alarm_delivered(mask, session(SEND_OK, SEND_SKIPPED, SEND_NO_CONNECTION)));
}

TEST(AlarmDelivered, NothingDeliveredIsNotConfirmed)
{
    // Молчаливое пробуждение или сеанс без связи: подтверждать нечего
    const SessionStatus silent;

    EXPECT_FALSE(alarm_delivered(CONFIRM_ANY, silent));
    EXPECT_FALSE(alarm_delivered(CONFIRM_WATERIUS, silent));
    EXPECT_FALSE(alarm_delivered(CONFIRM_WATERIUS | CONFIRM_HTTP | CONFIRM_MQTT, silent));
}

TEST(AlarmDelivered, BrokenSecondCloudDoesNotBlockAny)
{
    /*
    Почему delivered_any нельзя вывести из статусов: waterius.ru принял, свой
    сервер ответил не то. Слитый облачный статус скажет "плохо", хотя данные
    уехали - при маске "любой" это доставка.
    */
    const SessionStatus status = session(SEND_OK, SEND_BAD_ANSWER, SEND_SKIPPED);

    EXPECT_EQ(cloud_status(status), SEND_BAD_ANSWER);
    EXPECT_TRUE(alarm_delivered(CONFIRM_ANY, status));
    EXPECT_FALSE(alarm_delivered(CONFIRM_HTTP, status));
}
