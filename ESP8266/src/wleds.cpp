
#include "setup.h"
#include "wleds.h"
#include "Logging.h"

// Пауза перед первой вспышкой, мс
#define START_PAUSE_MS 300

/*
На классическом Ватериусе светодиод сидит на GPIO1 — это линия TX. Пока пин
занят под моргание, UART не работает, поэтому после вспышек лог поднимаем
заново: дальше по коду ещё пишутся строки про сон.
*/
static void restore_serial(uint8_t pin)
{
    if (pin == UART_TX_PIN)
    {
        LOG_BEGIN(LOG_BAUD);
    }
}


void setup_leds()
{
#if WATERIUS_MODEL == WATERIUS_MODEL_2
    pinMode(CH0_LED_PIN, OUTPUT);
    pinMode(CH1_LED_PIN, OUTPUT);
    pinMode(GREEN_LED_PIN, OUTPUT);
    pinMode(BUTTON_STATE_PIN, INPUT);

    digitalWrite(CH0_LED_PIN, LOW);
    digitalWrite(CH1_LED_PIN, LOW);
#endif
}

void release_leds()
{
#if WATERIUS_MODEL == WATERIUS_MODEL_2
    pinMode(BUTTON_STATE_PIN, INPUT);
    pinMode(GREEN_LED_PIN, INPUT);
    pinMode(CH0_LED_PIN, INPUT);
    pinMode(CH1_LED_PIN, INPUT);
#endif
}


uint8_t wait_button_release()
{
    pinMode(BUTTON_STATE_PIN, INPUT_PULLUP);

    unsigned long on_pressed_ms = 0, st = millis();
    while (digitalRead(BUTTON_STATE_PIN) == LOW) {
        yield();
        on_pressed_ms = millis() - st;
        if (on_pressed_ms > 3000) {
            digitalWrite(CH0_LED_PIN, (on_pressed_ms / 300) % 2);
            digitalWrite(CH1_LED_PIN, (on_pressed_ms / 300) % 2);
        }
    }

    digitalWrite(CH0_LED_PIN, LOW);
    digitalWrite(CH1_LED_PIN, LOW);
    pinMode(BUTTON_STATE_PIN, INPUT);

    if (on_pressed_ms > 3000)
    {
        return SETUP_MODE;
    }
    return MANUAL_TRANSMIT_MODE;
}

void blynk_led(uint8_t pin, uint8_t times, uint16_t delay_ms /* 200 */, uint16_t pause_ms /* 400 */)
{
    pinMode(pin, OUTPUT);

    /*
    Пауза перед первой вспышкой. Светодиод зажигается высоким уровнем, а
    на классике этот же вывод - линия TX, которая в покое как раз высокая:
    светодиод там горит постоянно, пока идёт лог. Без паузы первая вспышка
    сливалась бы с этим свечением, и код было бы не сосчитать.
    */
    digitalWrite(pin, LOW);
    delay(START_PAUSE_MS);

    while (times--)
    {
        yield();
        digitalWrite(pin, HIGH);
        delay(delay_ms);
        digitalWrite(pin, LOW);
        if (times)
        {
            yield();
            delay(pause_ms);
        }
    }
    pinMode(pin, INPUT);

    restore_serial(pin);
}

void blynk_error(enum ErrorBlynks code)
{
    if (code == ErrorBlynks::ERROR_OK)
    {
        blynk_led(GREEN_LED_PIN, 1);
    }
    else
    {
        blynk_led(ERROR_RED_LED_PIN, (uint8_t)code);
    }
}
