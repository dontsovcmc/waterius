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

    double k_estimated = 1.0;
    if (period_min_tuned > 0) {
        k_estimated = actual_slept_min / period_min_tuned;
    }

    // Если было отключение интернета, то k_estimated будет больше 2.0
    k_estimated = k_estimated - (uint16_t)k_estimated;
    if (k_estimated < 0.7) {  // корректируем не больше чем на 30% пробуждение
        k_estimated += 1;
    }

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

    // 4. Расчет и округление нового периода сна
    double ideal_period_tuned_float = minutes_to_next;
    if (k_estimated > 0.1) { // Проверка, что k_estimated не слишком близок к нулю
        ideal_period_tuned_float = minutes_to_next / k_estimated;
    }

    // Округляем до ближайшего целого и приводим к целевому типу uint16_t
    tune.period_min_tuned = static_cast<uint16_t>(round(ideal_period_tuned_float));

    return tune;
}

uint16_t period_after_user_change(const uint16_t wakeup_per_min)
{
    return wakeup_per_min * 0.9;
}
