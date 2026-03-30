import logging
import time
import random

# --- Настройки эмуляции ---
period_min = 1440
base_time = int(time.mktime(time.strptime("2025-01-01 12:00:00", "%Y-%m-%d %H:%M:%S")))
k = 1.01  # Коэффициент неточности часов в attiny, около 1
N = 100
k_estimated = 0


def randomize_k(k_value: float) -> float:
    """
    Возвращает новое значение k, случайно изменённое в диапазоне ±1% от текущего.
    """
    delta = random.uniform(-0.01, 0.01)  # от -1% до +1%
    return k_value * (1 + delta)


def sleep(start_time: int, period_to_sleep_min: float, k_factor: float) -> int:
    """
    Симулирует "сон" устройства. Возвращает фактическое время пробуждения в виде числа секунд с 1970 года.
    """
    actual_sleep_duration_min = period_to_sleep_min * k_factor
    actual_sleep_duration_sec = actual_sleep_duration_min * 60
    return int(round(start_time + actual_sleep_duration_sec))


def pt(t: time) -> str:
    return f"{time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(t))}"


def tune_wakeup(now: int, base_time: int, last_send: int,
                wakeup_per_min: int, period_min_tuned: int) -> int:

    if last_send <= 0:
        return wakeup_per_min

    # 1. Оценка коэффициента 'k' на основе результатов последнего сна
    actual_slept_min = (now - last_send) / 60.0

    global k_estimated
    k_estimated = 1.0
    if period_min_tuned > 0:
        k_estimated = actual_slept_min / period_min_tuned

    k_estimated = k_estimated - int(k_estimated)
    if k_estimated < 0.7:  # 1 - 30%
        k_estimated += 1

    # 2. Определение следующей целевой временной отметки
    time_since_base_min = (now - base_time) / 60.0

    target_num = int(time_since_base_min // wakeup_per_min) + 1

    next_expected = base_time + target_num * wakeup_per_min * 60
    minutes_to_next = (next_expected - now) / 60.0

    # 3. Защита от "ловушки": если до цели меньше минуты, целимся в следующую точку
    if minutes_to_next < wakeup_per_min * 0.3 or minutes_to_next < 1:
        target_num += 1
        next_expected = base_time + target_num * wakeup_per_min * 60
        minutes_to_next = (next_expected - now) / 60.0

    # 4. Расчет и округление нового периода сна
    ideal_period_tuned_float = minutes_to_next
    if k_estimated > 1e-9:
        ideal_period_tuned_float = minutes_to_next / k_estimated

    period = int(round(ideal_period_tuned_float))

    return period


def main():
    """
    Главная функция для задания настроек и запуска эмуляции.
    Теперь она явно управляет состоянием устройства между итерациями.
    """
    global k
    logging.basicConfig(level=logging.INFO, format='%(message)s', force=True)

    # Локальные переменные для хранения состояния устройства
    period_min_tuned = period_min
    current_time = base_time
    last_wake_time = 0

    logging.info("Старт эмуляции\n")
    logging.info(f"Начальные данные: base_time={pt(base_time)}, period_min={period_min}, k={k}, period_min_tuned={period_min_tuned}")
    logging.info("-" * 120)

    for i in range(1, N + 1):
        logging.info(f"Итерация {i}:")

        # 1. Симуляция сна
        k = randomize_k(k)
        current_time = sleep(current_time, period_min_tuned, k)

        connection_successful = random.random() > 0.3

        # 2. Коррекция таймера
        # Передаем все необходимые данные как аргументы в tune.
        if connection_successful:
            period_min_tuned = tune_wakeup(now=current_time,
                                           base_time=base_time,
                                           last_send=last_wake_time,
                                           wakeup_per_min=period_min,
                                           period_min_tuned=period_min_tuned)
            # Пример логирования времени (можно убрать если не нужно)
            logging.info(f'now: {pt(current_time)}, base_time: {pt(base_time)}, last_send: {pt(last_wake_time)}, '
                         f'period_min_tuned: {period_min_tuned}, k_estimated: {k_estimated}')

            # 3. Обновление состояния для следующей итерации
            last_wake_time = current_time
        else:
            logging.info(f" не вышел на связь")

        logging.info("-" * 120)


if __name__ == "__main__":
    main()
