#include <gtest/gtest.h>
#include <string.h>
#include "core/input.h"

/*
Тесты фиксируют текущие правила проверки пользовательского ввода.
Часть из них описывает поведение, которое стоит починить — такие помечены
ссылкой на issue. Пока тест зелёный и описывает то, что есть.
*/

// --- десятичный разделитель ---

TEST(ParseDecimal, CommaAndDotAreSameNumber)
{
    // #332: пользователь вводит показания с запятой, как принято в РФ
    float from_dot = 0.0;
    float from_comma = 0.0;

    parse_decimal("12.345", from_dot);
    parse_decimal("12,345", from_comma);

    EXPECT_FLOAT_EQ(from_dot, 12.345);
    EXPECT_FLOAT_EQ(from_comma, 12.345);
}

TEST(ParseDecimal, IntegerWithoutSeparator)
{
    // #353: пользователь забыл разделитель. Сейчас 12345 понимается
    // как 12345 кубометров, а не как 123.45.
    float v = 0.0;
    parse_decimal("12345", v);
    EXPECT_FLOAT_EQ(v, 12345.0);
}

TEST(ParseDecimal, ZeroIsValid)
{
    // 0 у счётчика — законное значение, ошибкой не считается
    float v = 42.0;
    EXPECT_EQ(parse_decimal("0", v), PARAM_OK);
    EXPECT_FLOAT_EQ(v, 0.0);
}

TEST(ParseDecimal, GarbageBecomesZeroWithoutError)
{
    // Разбор не умеет отвергать мусор: "abc" превращается в 0.0 и
    // затирает введённое ранее значение
    float v = 42.0;
    EXPECT_EQ(parse_decimal("abc", v), PARAM_OK);
    EXPECT_FLOAT_EQ(v, 0.0);
}

TEST(ParseDecimal, EmptyBecomesZero)
{
    float v = 42.0;
    parse_decimal("", v);
    EXPECT_FLOAT_EQ(v, 0.0);
}

TEST(ParseDecimal, TrailingGarbageIsIgnored)
{
    float v = 0.0;
    parse_decimal("12.3 m3", v);
    EXPECT_FLOAT_EQ(v, 12.3);
}

TEST(ParseDecimal, SpaceInsideNumberCutsItShort)
{
    // "12 345" воспринимается как 12, а не как 12345
    float v = 0.0;
    parse_decimal("12 345", v);
    EXPECT_FLOAT_EQ(v, 12.0);
}

TEST(ParseDecimal, LeadingSpacesAreSkipped)
{
    float v = 0.0;
    parse_decimal("  7,5", v);
    EXPECT_FLOAT_EQ(v, 7.5);
}

TEST(ParseDecimal, NegativeIsAccepted)
{
    // Отрицательных показаний не бывает, но разбор их пропускает
    float v = 0.0;
    parse_decimal("-5", v);
    EXPECT_FLOAT_EQ(v, -5.0);
}

TEST(ParseDecimal, SeveralCommasAreAllReplaced)
{
    // Разделители групп разрядов не поддерживаются: "1,234,5" -> 1.234
    float v = 0.0;
    parse_decimal("1,234,5", v);
    EXPECT_FLOAT_EQ(v, 1.234);
}

// --- маска из звёздочек: поле не редактировали ---

TEST(Masked, AsterisksAreDetected)
{
    EXPECT_TRUE(is_all_asterisks("****"));
    EXPECT_TRUE(is_all_asterisks("*"));
    EXPECT_TRUE(is_all_asterisks("* *"));
    EXPECT_TRUE(is_all_asterisks("\t**"));
}

TEST(Masked, EmptyIsNotMasked)
{
    // Иначе пустым значением нельзя было бы очистить пароль
    EXPECT_FALSE(is_all_asterisks(""));
}

TEST(Masked, TextWithAsteriskIsNotMasked)
{
    EXPECT_FALSE(is_all_asterisks("pass*"));
    EXPECT_FALSE(is_all_asterisks("**a**"));
}

TEST(Masked, MaskedValueLeavesFieldUnchanged)
{
    char dest[16] = "old_password";

    EXPECT_EQ(parse_text(dest, sizeof(dest), "****", true), PARAM_MASKED);
    EXPECT_STREQ(dest, "old_password");
}

// --- текстовые поля ---

TEST(ParseText, TrimsWhitespaceOnBothSides)
{
    char dest[16] = {0};

    EXPECT_EQ(parse_text(dest, sizeof(dest), "  waterius \t ", false), PARAM_OK);
    EXPECT_STREQ(dest, "waterius");
}

TEST(ParseText, TooLongGivesLengthError)
{
    char dest[8] = "keep";

    EXPECT_EQ(parse_text(dest, sizeof(dest), "0123456789", true), PARAM_ERR_LENGTH);
    EXPECT_STREQ(dest, "keep");   // при ошибке поле не трогаем
}

TEST(ParseText, LengthCheckedBeforeTrim)
{
    // Строка из пробелов длиннее буфера отвергается по длине, хотя после
    // обрезки поместилась бы. Характеризация.
    char dest[8] = "keep";

    EXPECT_EQ(parse_text(dest, sizeof(dest), "  ab      ", false), PARAM_ERR_LENGTH);
    EXPECT_STREQ(dest, "keep");
}

TEST(ParseText, ExactlyBufferSizeIsTooLong)
{
    // Нужен байт под завершающий ноль
    char dest[5] = {0};

    EXPECT_EQ(parse_text(dest, sizeof(dest), "1234", false), PARAM_OK);
    EXPECT_EQ(parse_text(dest, sizeof(dest), "12345", false), PARAM_ERR_LENGTH);
}

TEST(ParseText, EmptyRequiredGivesEmptyError)
{
    char dest[8] = "keep";

    EXPECT_EQ(parse_text(dest, sizeof(dest), "", true), PARAM_ERR_EMPTY);
    EXPECT_STREQ(dest, "keep");
}

TEST(ParseText, EmptyOptionalClearsField)
{
    char dest[8] = "keep";

    EXPECT_EQ(parse_text(dest, sizeof(dest), "", false), PARAM_OK);
    EXPECT_STREQ(dest, "");
}

// --- целые числа ---

TEST(ParseUint16, ZeroIsAnError)
{
    uint16_t v = 42;

    EXPECT_EQ(parse_uint16("0", v), PARAM_ERR_VALUE);
    EXPECT_EQ(v, 42);
}

TEST(ParseUint16, GarbageIsAnError)
{
    uint16_t v = 42;

    EXPECT_EQ(parse_uint16("abc", v), PARAM_ERR_VALUE);
    EXPECT_EQ(parse_uint16("", v), PARAM_ERR_VALUE);
    EXPECT_EQ(parse_uint16("1883abc", v), PARAM_ERR_VALUE);  // хвост после числа
    EXPECT_EQ(parse_uint16("1883.5", v), PARAM_ERR_VALUE);   // дробное - не целое
    EXPECT_EQ(v, 42);
}

TEST(ParseUint16, SpacesAroundNumberAreAllowed)
{
    // Значение приходит из формы как есть, пробелы по краям не ошибка
    uint16_t v = 0;

    EXPECT_EQ(parse_uint16("  1883  ", v), PARAM_OK);
    EXPECT_EQ(v, 1883);
}

TEST(ParseUint16, NormalValue)
{
    uint16_t v = 0;

    EXPECT_EQ(parse_uint16("1883", v), PARAM_OK);
    EXPECT_EQ(v, 1883);
}

TEST(ParseUint16, OverflowIsRejected)
{
    // 70000 не влезает в uint16_t. Раньше значение молча превращалось в
    // 4464: портал отвечал "сохранено", а в настройках оказывался другой
    // порт MQTT. Теперь это ошибка, и поле остаётся прежним.
    uint16_t v = 1883;

    EXPECT_EQ(parse_uint16("70000", v), PARAM_ERR_VALUE);
    EXPECT_EQ(parse_uint16("65536", v), PARAM_ERR_VALUE);
    EXPECT_EQ(parse_uint16("99999999999999999999", v), PARAM_ERR_VALUE);
    EXPECT_EQ(v, 1883);

    // Верхняя граница остаётся допустимой
    EXPECT_EQ(parse_uint16("65535", v), PARAM_OK);
    EXPECT_EQ(v, 65535);
}

TEST(ParseUint16, NegativeIsRejected)
{
    // Отрицательное в беззнаковом поле стало бы большим положительным
    uint16_t v = 1883;

    EXPECT_EQ(parse_uint16("-1", v), PARAM_ERR_VALUE);
}

TEST(ParseUint16, ZeroAllowedOnDemand)
{
    /*
    По умолчанию ноль - ошибка: это и незаполненное поле, и мусор. Но у порогов
    тревог (#202) ноль значит "выключено" и приходит из формы штатно.
    */
    uint16_t v = 42;

    EXPECT_EQ(parse_uint16("0", v, true), PARAM_OK);
    EXPECT_EQ(v, 0);

    // Разрешение нуля не отменяет остальных проверок
    EXPECT_EQ(parse_uint16("-1", v, true), PARAM_ERR_VALUE);
    EXPECT_EQ(parse_uint16("65536", v, true), PARAM_ERR_VALUE);
    EXPECT_EQ(parse_uint16("abc", v, true), PARAM_ERR_VALUE);

    // Отвергнутое значение не затирает прежнее
    EXPECT_EQ(v, 0);
}

TEST(ParseUint8, ZeroOkFlagSwitchesBehaviour)
{
    uint8_t v = 42;

    EXPECT_EQ(parse_uint8("0", v, false), PARAM_ERR_VALUE);
    EXPECT_EQ(v, 42);

    EXPECT_EQ(parse_uint8("0", v, true), PARAM_OK);
    EXPECT_EQ(v, 0);
}

TEST(ParseUint8, OutOfRangeIsRejected)
{
    uint8_t v = 42;

    EXPECT_EQ(parse_uint8("256", v, true), PARAM_ERR_VALUE);
    EXPECT_EQ(parse_uint8("-1", v, true), PARAM_ERR_VALUE);
    EXPECT_EQ(parse_uint8("abc", v, true), PARAM_ERR_VALUE);
    EXPECT_EQ(v, 42);

    // 255 - это CounterType::NONE, выключенный вход
    EXPECT_EQ(parse_uint8("255", v, true), PARAM_OK);
    EXPECT_EQ(v, 255);
}

// --- флажки ---

TEST(ParseBool, ZeroAndOneAreAccepted)
{
    uint8_t v = 42;

    EXPECT_EQ(parse_bool("0", v), PARAM_OK);
    EXPECT_EQ(v, 0);

    EXPECT_EQ(parse_bool("1", v), PARAM_OK);
    EXPECT_EQ(v, 1);
}

TEST(ParseBool, TwoIsRejected)
{
    uint8_t v = 1;

    EXPECT_EQ(parse_bool("2", v), PARAM_ERR_VALUE);
    EXPECT_EQ(v, 1);
}

TEST(ParseBool, NegativeIsRejected)
{
    // Раньше проверялась только верхняя граница: "-1" проходил и
    // превращался в 255, то есть выключатель оказывался включённым.
    uint8_t v = 0;

    EXPECT_EQ(parse_bool("-1", v), PARAM_ERR_VALUE);
    EXPECT_EQ(v, 0);
}

TEST(ParseBool, GarbageIsRejected)
{
    // Мусор больше не выключает флажок молча: значение остаётся прежним,
    // а отправитель получает код ошибки. Home Assistant шлёт "1"/"0",
    // так что "true" сюда прийти не должно - но если пришло, это ошибка.
    uint8_t v = 1;

    EXPECT_EQ(parse_bool("abc", v), PARAM_ERR_VALUE);
    EXPECT_EQ(parse_bool("", v), PARAM_ERR_VALUE);
    EXPECT_EQ(parse_bool("true", v), PARAM_ERR_VALUE);
    EXPECT_EQ(v, 1);
}

// --- обрезка пробелов ---

TEST(CopyTrimmed, TruncatesToBufferSize)
{
    char dest[4] = {0};

    copy_trimmed(dest, "abcdefg", sizeof(dest));

    EXPECT_STREQ(dest, "abc");
}

TEST(CopyTrimmed, AllWhitespaceGivesEmptyString)
{
    char dest[8] = "keep";

    copy_trimmed(dest, "   \t  ", sizeof(dest));

    EXPECT_STREQ(dest, "");
}

TEST(CopyTrimmed, ZeroSizeDoesNotWrite)
{
    char dest[4] = "abc";

    copy_trimmed(dest, "xyz", 0);

    EXPECT_STREQ(dest, "abc");
}

// --- #353: забытая запятая в показаниях счётчика воды ---

TEST(CheckReading, WaterTypesAreRecognised)
{
    EXPECT_TRUE(is_water_counter(CounterName::WATER_COLD));
    EXPECT_TRUE(is_water_counter(CounterName::WATER_HOT));
    EXPECT_TRUE(is_water_counter(CounterName::PORTABLE_WATER));

    EXPECT_FALSE(is_water_counter(CounterName::ELECTRO));
    EXPECT_FALSE(is_water_counter(CounterName::GAS));
    EXPECT_FALSE(is_water_counter(CounterName::HEAT_GCAL));
    EXPECT_FALSE(is_water_counter(CounterName::HEAT_KWT));
    EXPECT_FALSE(is_water_counter(CounterName::OTHER));
}

TEST(CheckReading, WaterWithFractionIsAccepted)
{
    EXPECT_EQ(check_reading("0.0", CounterName::WATER_COLD), PARAM_OK);
    EXPECT_EQ(check_reading("123.45", CounterName::WATER_COLD), PARAM_OK);
    EXPECT_EQ(check_reading("123,456", CounterName::WATER_HOT), PARAM_OK);
}

TEST(CheckReading, BigWaterScaleIsAccepted)
{
    // Ради этих пользователей правило и переделано: предела шкалы больше нет,
    // счётчик на 250 тысяч кубов вводится как есть — с дробной частью
    EXPECT_EQ(check_reading("250000.123", CounterName::WATER_COLD), PARAM_OK);
    EXPECT_EQ(check_reading("99999.999", CounterName::WATER_HOT), PARAM_OK);
}

TEST(CheckReading, WaterWithoutSeparatorLooksLikeMissingComma)
{
    // Пользователь переписал шкалу подряд: 12345 вместо 123.45
    EXPECT_EQ(check_reading("12345", CounterName::WATER_COLD), PARAM_ERR_NO_COMMA);
    EXPECT_EQ(check_reading("5678", CounterName::WATER_HOT), PARAM_ERR_NO_COMMA);
    EXPECT_EQ(check_reading("123", CounterName::PORTABLE_WATER), PARAM_ERR_NO_COMMA);
}

TEST(CheckReading, EmptyWaterReadingIsRejectedToo)
{
    // Пустая строка разделителя не содержит, а показания без дробной части
    // сохранять нельзя — поле обязательное
    EXPECT_EQ(check_reading("", CounterName::WATER_COLD), PARAM_ERR_NO_COMMA);
}

TEST(CheckReading, ElectricityIsNotChecked)
{
    // Целые кВт*ч — обычное дело, счётчик за 20 лет накручивает шестизначные
    EXPECT_EQ(check_reading("123456", CounterName::ELECTRO), PARAM_OK);
    EXPECT_EQ(check_reading("999999", CounterName::ELECTRO), PARAM_OK);
}

TEST(CheckReading, GasAndHeatAreNotChecked)
{
    EXPECT_EQ(check_reading("100000", CounterName::GAS), PARAM_OK);
    EXPECT_EQ(check_reading("100000", CounterName::HEAT_GCAL), PARAM_OK);
    EXPECT_EQ(check_reading("100000", CounterName::HEAT_KWT), PARAM_OK);
    EXPECT_EQ(check_reading("100000", CounterName::OTHER), PARAM_OK);
}

TEST(HasDecimalSeparator, DotAndCommaAreEqual)
{
    EXPECT_TRUE(has_decimal_separator("1.0"));
    EXPECT_TRUE(has_decimal_separator("1,0"));
    EXPECT_TRUE(has_decimal_separator("1."));

    EXPECT_FALSE(has_decimal_separator("10"));
    EXPECT_FALSE(has_decimal_separator(""));
}

TEST(CheckReading, ErrorCodeMatchesWebInterface)
{
    // Код разбирается в data/static/strings.js, менять нельзя
    EXPECT_EQ(PARAM_ERR_NO_COMMA, 19);
}

TEST(CounterTypes, KnownTypesAreAccepted)
{
    EXPECT_TRUE(is_valid_counter_type(CounterType::NAMUR));
    EXPECT_TRUE(is_valid_counter_type(CounterType::DISCRETE));
    EXPECT_TRUE(is_valid_counter_type(CounterType::ELECTRONIC));
    EXPECT_TRUE(is_valid_counter_type(CounterType::HALL));
    EXPECT_TRUE(is_valid_counter_type(CounterType::ELECTRONIC_HIGH));
    EXPECT_TRUE(is_valid_counter_type(CounterType::LEAKAGE));
    EXPECT_TRUE(is_valid_counter_type(CounterType::LEAKAGE_NC));
    EXPECT_TRUE(is_valid_counter_type(CounterType::NONE));
}

TEST(CounterTypes, EverySelectorOptionIsKnown)
{
    // Значения из селектора «Тип входа» в data/input_setup.html. Тип, который
    // страница предлагает, а функция не признаёт, вернётся на страницу как
    // NAMUR и затрёт настройку при следующем сохранении
    const uint8_t options[] = {0, 1, 2, 3, 4, 5, 6, 255};
    for (uint8_t option : options)
    {
        EXPECT_TRUE(is_valid_counter_type(option)) << "тип входа " << (int)option;
    }
}

TEST(CounterTypes, UnknownTypeIsRejected)
{
    // Число уезжает в attiny командой 'C'. Незнакомое значение там означает
    // ненастроенный вход, который молча ничего не считает
    EXPECT_FALSE(is_valid_counter_type(7));
    EXPECT_FALSE(is_valid_counter_type(100));
    EXPECT_FALSE(is_valid_counter_type(254));
}

TEST(CounterTypes, ValuesMatchAttinyFirmware)
{
    // Те же числа в Attiny85/src/counter.h:CounterType и в селекторе
    // data/input_setup.html — менять нельзя
    EXPECT_EQ(CounterType::NAMUR, 0);
    EXPECT_EQ(CounterType::DISCRETE, 1);
    EXPECT_EQ(CounterType::ELECTRONIC, 2);
    EXPECT_EQ(CounterType::HALL, 3);
    EXPECT_EQ(CounterType::ELECTRONIC_HIGH, 4);
    EXPECT_EQ(CounterType::LEAKAGE, 5);
    EXPECT_EQ(CounterType::LEAKAGE_NC, 6);
    EXPECT_EQ(CounterType::NONE, 255);
}
