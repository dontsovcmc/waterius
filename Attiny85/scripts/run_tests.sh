#!/usr/bin/env bash
#
# Прогон хостовых тестов чистых модулей attiny.
#
# Сама логика прогона общая с ЕСП (проверка, что тесты вообще выполнялись,
# берётся из отчёта JUnit), поэтому здесь только адрес проекта и окружение
# по умолчанию.
#
# Использование:
#   scripts/run_tests.sh                        # все сюиты
#   scripts/run_tests.sh native -f test_electronic
#
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

PROJECT_DIR="$PROJECT_DIR" DEFAULT_ENVS="native" \
    exec "$PROJECT_DIR/../ESP8266/scripts/run_tests.sh" "$@"
