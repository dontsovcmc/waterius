#include "wakeup.h"

#include <math.h>

WakeupTune tune_wakeup(const time_t now, const time_t base_time, const time_t last_send,
                       const uint16_t wakeup_per_min, const uint16_t period_min_tuned)
{
    WakeupTune tune;

    // 1. Оценка коэффициента 'k' на основе результатов последнего сна
    // time_t обычно хранит секунды, поэтому difftime вернет разницу в секундах.
    double actual_slept_min = difftime(now, last_send) / 60.0;
    tune.slept_min = actual_slept_min;

    // Поправку можно оценить только по нормально завершившемуся сну.
    // Неположительный сон означает, что часы переставили назад — например
    // NTP не ответил, а перед запросом время уже сбросили. Частоту attiny
    // такой сон не характеризует, поэтому поправка остаётся нейтральной.
    double k_estimated = 1.0;
    if (period_min_tuned > 0 && actual_slept_min > 0.0)
    {
        k_estimated = actual_slept_min / period_min_tuned;

        // Если было отключение интернета, то k_estimated будет больше 2.0.
        // floor, а не приведение к uint16_t: приведение double, не попадающего
        // в диапазон целевого типа, — неопределённое поведение, а здесь оно
        // достижимо и для отрицательных значений, и для очень больших.
        k_estimated -= floor(k_estimated);
        if (k_estimated < 0.7) {  // корректируем не больше чем на 30% пробуждение
            k_estimated += 1;
        }
    }
    // Дальше k_estimated всегда в диапазоне [0.7, 1.7)

    // 2. Определение следующей целевой временной отметки
    double time_since_base_min = difftime(now, base_time) / 60.0;

    // Целочисленное деление для определения количества прошедших периодов
    long long target_num = static_cast<long long>(floor(time_since_base_min / wakeup_per_min)) + 1;

    time_t next_expected = base_time + target_num * wakeup_per_min * 60;
    double minutes_to_next = difftime(next_expected, now) / 60.0;

    // 3. Защита от "ловушки": если до цели меньше минуты или до пробуждения меньше 30% периода, целимся в следующую точку
    if (minutes_to_next < 1.0 || minutes_to_next < (wakeup_per_min * 0.3)) {
        target_num++;
        next_expected = base_time + target_num * wakeup_per_min * 60;
        minutes_to_next = difftime(next_expected, now) / 60.0;
    }

    // 4. Расчет и округление нового периода сна.
    // Проверка "k не близок к нулю" не нужна: по построению k >= 0.7
    double ideal_period_tuned_float = minutes_to_next / k_estimated;

    // Результат обязан влезать в uint16_t: приведение double за пределами
    // диапазона типа — неопределённое поведение. Достижимо при длинном
    // периоде: свыше ~45875 минут (32 суток) деление на k = 0.7 выходит
    // за 65535, и вместо 46 суток устройство просыпалось бы через сутки.
    // Запись через !(x >= 1.0) заодно отсекает NaN.
    if (!(ideal_period_tuned_float >= 1.0) || ideal_period_tuned_float > 65535.0)
    {
        ideal_period_tuned_float = wakeup_per_min;
    }

    // Округляем до ближайшего целого и приводим к целевому типу uint16_t
    tune.period_min_tuned = static_cast<uint16_t>(round(ideal_period_tuned_float));

    return tune;
}

uint16_t period_after_user_change(const uint16_t wakeup_per_min)
{
    return wakeup_per_min * 0.9;
}
