
#include "setup.h"
#include "wleds.h"
#include "Logging.h"

// Сколько держать светодиод погашенным перед вспышками кода, мс
#define LED_OFF_BEFORE_CODE_MS 300
#define LED_FLASH_MS 200
#define LED_PAUSE_MS 400


void setup_leds()
{
#if WATERIUS_MODEL == WATERIUS_MODEL_2
    pinMode(CH0_LED_PIN, OUTPUT);
    pinMode(CH1_LED_PIN, OUTPUT);
    pinMode(BUTTON_STATE_PIN, INPUT);

    digitalWrite(CH0_LED_PIN, LOW);
    digitalWrite(CH1_LED_PIN, LOW);
#endif
}

void release_leds()
{
#if WATERIUS_MODEL == WATERIUS_MODEL_2
    pinMode(BUTTON_STATE_PIN, INPUT);
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

void blynk_error(enum ErrorBlynks code)
{
    LOG_INFO(F("Blynk: code=") << (int)code);

    // Светодиод сидит на линии TX, и пока он моргает, UART не работает: хвост
    // лога дописываем заранее, а после вспышек поднимаем лог заново
    LOG_END();
    pinMode(UART_TX_PIN, OUTPUT);

    /*
    Пауза перед первой вспышкой. Светодиод классики зажигается высоким
    уровнем, а линия TX в покое как раз высокая: светодиод там горит
    постоянно, пока идёт лог. Без паузы первая вспышка сливалась бы с этим
    свечением, и код было бы не сосчитать.
    */
    digitalWrite(UART_TX_PIN, LOW);
    delay(LED_OFF_BEFORE_CODE_MS);

    uint8_t times = code;
    while (times--)
    {
        digitalWrite(UART_TX_PIN, HIGH);
        delay(LED_FLASH_MS);
        digitalWrite(UART_TX_PIN, LOW);
        if (times)
        {
            delay(LED_PAUSE_MS);
        }
    }

    pinMode(UART_TX_PIN, INPUT);
    LOG_BEGIN(LOG_BAUD);
}
