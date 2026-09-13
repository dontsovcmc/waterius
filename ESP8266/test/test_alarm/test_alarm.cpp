#include <gtest/gtest.h>
#include "core/alarm.h"

/*
Пересчёт порогов тревог и разбор ответа attiny (issue #202).

Человек задаёт литры, литры в час и часы. attiny считает тиками по 250 мс и
квантами тишины: делить там дорого, а веса импульса она не знает. Поэтому
весь пересчёт здесь, и проверять его надо здесь же.

Ключевое тождество, из которого всё растёт:

    квант = вес импульса / Q

Расход ровно в Q даёт один импульс за квант и потому паузы такой длины не
оставляет никогда. Значит квант - это не отдельная настройка, а тот же порог
расхода в других единицах.
*/

TEST(Volume, LitresToPulses)
{
    EXPECT_EQ(volume_to_pulses(300, 10), 30u);
    EXPECT_EQ(volume_to_pulses(300, 1), 300u);
}

TEST(Volume, DisabledIsZero)
{
    EXPECT_EQ(volume_to_pulses(0, 10), 0u);
    EXPECT_EQ(volume_to_pulses(300, 0), 0u);
}

TEST(Volume, BelowOnePulseStaysArmed)
{
    // Порог меньше одного импульса: округление вниз дало бы "выключено",
    // а пользователь просил обратное
    EXPECT_EQ(volume_to_pulses(5, 10), 1u);
}

TEST(Quantum, RateToTicks)
{
    // 10 л/имп и 10 л/ч - импульс раз в час, то есть 14400 тиков
    EXPECT_EQ(rate_to_quantum_ticks(10, 10), 14400u);
    // вдвое больший порог - вдвое короче квант
    EXPECT_EQ(rate_to_quantum_ticks(20, 10), 7200u);
    // 1 л/имп при том же пороге - квант вдесятеро короче
    EXPECT_EQ(rate_to_quantum_ticks(10, 1), 1440u);
}

TEST(Quantum, DisabledIsZero)
{
    EXPECT_EQ(rate_to_quantum_ticks(0, 10), 0u);
    EXPECT_EQ(rate_to_quantum_ticks(10, 0), 0u);
}

TEST(Quantum, TooSmallRateSaturates)
{
    // Ниже alarm_min_rate квант не влезает в uint16. Портал такое не
    // пропускает, но чужая настройка не должна ломать детектор
    EXPECT_EQ(rate_to_quantum_ticks(1, 10), UINT16_MAX);
}

TEST(Quantum, HugeRateStaysArmed)
{
    // Порог выше, чем счётчик способен выдать импульсов: ноль означал бы
    // "выключено", а просили обратное
    EXPECT_EQ(rate_to_quantum_ticks(65535, 1), 1u);
}

TEST(Quanta, HoursToQuanta)
{
    // квант час - в сутках 24 кванта
    EXPECT_EQ(hours_to_quanta(24, 10, 10), 24u);
    // квант полчаса - тех же суток хватает на 48
    EXPECT_EQ(hours_to_quanta(24, 20, 10), 48u);
}

TEST(Quanta, DisabledIsZero)
{
    EXPECT_EQ(hours_to_quanta(0, 10, 10), 0u);
    EXPECT_EQ(hours_to_quanta(24, 0, 10), 0u);
    EXPECT_EQ(hours_to_quanta(24, 10, 0), 0u);
}

/*
Пределы для проверки ввода в портале. Неверная пара "порог расхода - часы"
ломает тревогу молча, поэтому портал обязан её не принять.
*/
TEST(Limits, MinRateFitsQuantumInUint16)
{
    const uint16_t q = alarm_min_rate(10);
    EXPECT_GT(q, 0u);
    EXPECT_LT(rate_to_quantum_ticks(q, 10), UINT16_MAX);
    EXPECT_EQ(rate_to_quantum_ticks(q - 1, 10), UINT16_MAX);
}

TEST(Limits, MinHoursGivesAtLeastTwoQuanta)
{
    // При кванте в час двух квантов - это два часа
    EXPECT_EQ(alarm_min_hours(10, 10), 2u);
    // При кванте в 20 минут хватает и часа
    EXPECT_EQ(alarm_min_hours(30, 10), 1u);
    EXPECT_GE(hours_to_quanta(alarm_min_hours(10, 10), 10, 10), 2u);
}

/*
Пороги канала целиком.
*/
TEST(Thresholds, ReadyChannel)
{
    const AlarmThresholds t = alarm_thresholds(false, CounterType::NAMUR,
                                               300, 10, 24, 10);
    EXPECT_EQ(t.vol_pulses, 30u);
    EXPECT_EQ(t.quantum_ticks, 14400u);
    EXPECT_EQ(t.leak_quanta, 24u);
}

TEST(Thresholds, NoImpulsesNoThresholds)
{
    for (const uint8_t type : {(uint8_t)CounterType::NONE,
                               (uint8_t)CounterType::LEAKAGE,
                               (uint8_t)CounterType::LEAKAGE_NC})
    {
        const AlarmThresholds t = alarm_thresholds(false, type, 300, 10, 24, 10);
        EXPECT_EQ(t.vol_pulses, 0u);
        EXPECT_EQ(t.quantum_ticks, 0u);
        EXPECT_EQ(t.leak_quanta, 0u);
    }
}

TEST(Thresholds, UnknownFactorIsNotGuessed)
{
    // До первой настройки входа в весе лежит спецзначение: взять его за
    // литры на импульс значило бы отдать attiny порог, о котором не просили
    for (const uint16_t factor : {(uint16_t)AUTO_IMPULSE_FACTOR,
                                  (uint16_t)AS_COLD_CHANNEL})
    {
        const AlarmThresholds t = alarm_thresholds(false, CounterType::NAMUR,
                                                   300, 10, 24, factor);
        EXPECT_EQ(t.vol_pulses, 0u);
        EXPECT_EQ(t.quantum_ticks, 0u);
    }
}

TEST(Thresholds, LeakNeedsBothNumbers)
{
    // Порознь порог расхода и часы смысла не имеют
    EXPECT_EQ(alarm_thresholds(false, CounterType::NAMUR, 0, 10, 0, 10).leak_quanta, 0u);
    EXPECT_EQ(alarm_thresholds(false, CounterType::NAMUR, 0, 0, 24, 10).leak_quanta, 0u);
    EXPECT_EQ(alarm_thresholds(false, CounterType::NAMUR, 0, 0, 24, 10).quantum_ticks, 0u);
}

/*
Режим "я уехал" (#88): тревогой становится любой расход.
*/
TEST(Vacation, AnyPulseIsAlarm)
{
    const AlarmThresholds t = alarm_thresholds(true, CounterType::NAMUR,
                                               300, 10, 24, 10);
    EXPECT_EQ(t.vol_pulses, 1u);
}

TEST(Vacation, WorksWithoutKnownImpulseWeight)
{
    // Пересчитывать единицы незачем, когда тревогой объявлен любой импульс.
    // Иначе включённый режим молча не сработал бы там, где нужен не меньше
    for (const uint16_t factor : {(uint16_t)AUTO_IMPULSE_FACTOR,
                                  (uint16_t)AS_COLD_CHANNEL})
    {
        EXPECT_EQ(alarm_thresholds(true, CounterType::NAMUR, 0, 0, 0, factor).vol_pulses, 1u);
    }
}

TEST(Vacation, OverridesDisabledAlarm)
{
    EXPECT_EQ(alarm_thresholds(true, CounterType::NAMUR, 0, 0, 0, 10).vol_pulses, 1u);
}

TEST(Vacation, SkipsInputsWithoutImpulses)
{
    EXPECT_EQ(alarm_thresholds(true, CounterType::LEAKAGE, 0, 0, 0, 10).vol_pulses, 0u);
    EXPECT_EQ(alarm_thresholds(true, CounterType::NONE, 0, 0, 0, 10).vol_pulses, 0u);
}

TEST(Vacation, OffKeepsUserThresholds)
{
    const AlarmThresholds t = alarm_thresholds(false, CounterType::NAMUR,
                                               300, 10, 24, 10);
    EXPECT_EQ(t.vol_pulses, 30u);
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

/*
Снятие тревог в снимке флагов (#202).

Маска приезжает в раскладке кадра 'A' - каналы вплотную, - а флаги лежат со
сдвигом под флаг питания. Перепутанный сдвиг снял бы тревогу не того канала
и не ту, а заметно это стало бы только на живом устройстве.
*/
TEST(AlarmReset, ClearsOnlyMaskedBits)
{
    const uint8_t flags = ((ALARM_FLOW | ALARM_LEAK) << ATTINY_ALARM_SHIFT0) |
                          ((ALARM_FLOW | ALARM_WET) << ATTINY_ALARM_SHIFT1);

    // Снимаем только протечку канала 0
    const uint8_t left = alarm_flags_after_reset(flags, ALARM_LEAK);

    EXPECT_EQ(alarm_bits(left, INPUT0_RED, ATTINY_VER_ALARM), ALARM_FLOW);
    EXPECT_EQ(alarm_bits(left, INPUT1_BLUE, ATTINY_VER_ALARM), ALARM_FLOW | ALARM_WET);
}

TEST(AlarmReset, SecondChannelLivesInItsOwnBits)
{
    const uint8_t flags = (ALARM_WET << ATTINY_ALARM_SHIFT0) |
                          (ALARM_WET << ATTINY_ALARM_SHIFT1);

    const uint8_t left = alarm_flags_after_reset(flags, ALARM_RESET_CH(ALARM_WET, INPUT1_BLUE));

    EXPECT_EQ(alarm_bits(left, INPUT0_RED, ATTINY_VER_ALARM), ALARM_WET);
    EXPECT_EQ(alarm_bits(left, INPUT1_BLUE, ATTINY_VER_ALARM), 0);
}

TEST(AlarmReset, AllClearsBothChannels)
{
    const uint8_t flags = ATTINY_FLAG_ESP_POWERED_LONG |
                          (ATTINY_ALARM_MASK << ATTINY_ALARM_SHIFT0) |
                          (ATTINY_ALARM_MASK << ATTINY_ALARM_SHIFT1);

    const uint8_t left = alarm_flags_after_reset(flags, ALARM_RESET_ALL);

    EXPECT_EQ(alarm_bits(left, INPUT0_RED, ATTINY_VER_ALARM), 0);
    EXPECT_EQ(alarm_bits(left, INPUT1_BLUE, ATTINY_VER_ALARM), 0);

    // Чужие биты байта маска не трогает
    EXPECT_EQ(left & ATTINY_FLAG_ESP_POWERED_LONG, ATTINY_FLAG_ESP_POWERED_LONG);
}

TEST(AlarmReset, EmptyMaskChangesNothing)
{
    const uint8_t flags = (ALARM_LEAK << ATTINY_ALARM_SHIFT0);

    EXPECT_EQ(alarm_flags_after_reset(flags, 0), flags);
}

TEST(Alarm, FactorConfiguredRejectsSpecialValues)
{
    /*
    До первой настройки входа в весе импульса лежит спецзначение из
    комбобокса. Это обычные маленькие числа: взять их за литры на импульс
    значило бы посчитать порог, о котором никто не просил.
    */
    EXPECT_FALSE(factor_configured(AUTO_IMPULSE_FACTOR));
    EXPECT_FALSE(factor_configured(AS_COLD_CHANNEL));
    EXPECT_FALSE(factor_configured(0));

    EXPECT_TRUE(factor_configured(10));
    // Электричество тоже: формула другая, но вес известен
    EXPECT_TRUE(factor_configured(1000));
}
/*
Что страница тревог показывает на входе. Причины взаимоисключающие, и врать
нельзя ни в одну сторону: "счётчик не настроен" на старой attiny отправило бы
человека настраивать то, что всё равно не заработает.
*/
TEST(AlarmInput, NoInputBeatsEverything)
{
    // Датчик протечки и выключенный вход импульсов не дают: ни новая attiny,
    // ни заданный вес тут ничего не меняют
    EXPECT_EQ(alarm_input_state(CounterType::LEAKAGE, 10, ATTINY_VER_ALARM), ALARM_INPUT_NO_INPUT);
    EXPECT_EQ(alarm_input_state(CounterType::LEAKAGE_NC, 10, ATTINY_VER_ALARM), ALARM_INPUT_NO_INPUT);
    EXPECT_EQ(alarm_input_state(CounterType::NONE, 10, ATTINY_VER_ALARM), ALARM_INPUT_NO_INPUT);
}

TEST(AlarmInput, OldAttinyHidesThresholds)
{
    EXPECT_EQ(alarm_input_state(CounterType::NAMUR, 10, ATTINY_VER_ALARM - 1),
              ALARM_INPUT_NO_ATTINY);
}

TEST(AlarmInput, UnknownFactorIsFixable)
{
    EXPECT_EQ(alarm_input_state(CounterType::NAMUR, AS_COLD_CHANNEL, ATTINY_VER_ALARM),
              ALARM_INPUT_NO_FACTOR);
    EXPECT_EQ(alarm_input_state(CounterType::NAMUR, AUTO_IMPULSE_FACTOR, ATTINY_VER_ALARM),
              ALARM_INPUT_NO_FACTOR);
}

TEST(AlarmInput, ConfiguredInputIsReady)
{
    EXPECT_EQ(alarm_input_state(CounterType::NAMUR, 10, ATTINY_VER_ALARM), ALARM_INPUT_READY);
    EXPECT_EQ(alarm_input_state(CounterType::ELECTRONIC, 1000, ATTINY_VER_ALARM),
              ALARM_INPUT_READY);
}

TEST(AlarmInput, OldAttinyWinsOverUnknownFactor)
{
    // Звать настраивать счётчик там, где порогов не будет никогда, - вранье
    EXPECT_EQ(alarm_input_state(CounterType::NAMUR, AUTO_IMPULSE_FACTOR, ATTINY_VER_ALARM - 1),
              ALARM_INPUT_NO_ATTINY);
}
/*
Квитанция о доставке тревоги (#202).

Пока квитанции нет, attiny будит ЕСП снова: до ALARM_MAX_TRIES попыток.
Значит "доставлено" оплачивается батареей, и решает тут не наблюдение, а
настройка пользователя.
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
