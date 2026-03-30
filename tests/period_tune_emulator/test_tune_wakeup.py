
import pytest
import time
import logging
import random
from period_tune_emulator import pt, randomize_k, sleep, tune_wakeup


logging.basicConfig(level=logging.INFO, format='%(message)s', force=True)
base_time = int(time.mktime(time.strptime("2025-01-01 12:00:00", "%Y-%m-%d %H:%M:%S")))
N = 100
k_estimated = 0


@pytest.mark.parametrize("period_min, k, connection", [
    (60, 1.01, 0.3),
    (1440, 1.05, 0.1),
    (1440, 1.05, 0.5),
    (15, 1.01, 0.1),
])
def test_tune_wakeup(period_min: int,
                     k: float,
                     connection: float):
    period_min_tuned = period_min
    current_time = base_time
    last_wake_time = 0

    logging.info(f"Старт: base_time={pt(base_time)}, period_min={period_min}, k={k}, connection={connection}")
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
