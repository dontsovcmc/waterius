#!/usr/bin/env bash
#
# Прогон хостовых тестов бизнес-логики.
#
# Скрипт общий для двух проектов: ЕСП зовёт его напрямую, attiny — через
# обёртку Attiny85/scripts/run_tests.sh, которая задаёт PROJECT_DIR и
# DEFAULT_ENVS. Проверка "ноль выполненных тестов - это провал" одна на оба.
#
# pio test выходит с кодом 0, даже если не собрал ни одного теста — например
# из-за фильтра, который ни с чем не совпал, или из-за окружения, где тесты
# отключены. Такой прогон выглядит успешным, ничего не проверив, поэтому
# результат берётся из машиночитаемого отчёта, а не из текста вывода:
# в JUnit есть счётчик выполненных тестов, и ноль здесь означает провал.
#
# Использование:
#   scripts/run_tests.sh                        # обе модели прошивки
#   scripts/run_tests.sh native_2               # одна
#   scripts/run_tests.sh native_2 -f test_url   # аргументы после окружений уходят в pio
#
set -euo pipefail

PIO="${PIO:-$HOME/.platformio/penv/bin/pio}"
PROJECT_DIR="${PROJECT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
DEFAULT_ENVS="${DEFAULT_ENVS:-native_classic native_2}"

# Окружения — ведущие аргументы без дефиса; остальное уходит в pio как есть
ENVIRONMENTS=()
while [ $# -gt 0 ] && [[ "$1" != -* ]]; do
    ENVIRONMENTS+=("$1")
    shift
done
if [ ${#ENVIRONMENTS[@]} -eq 0 ]; then
    read -ra ENVIRONMENTS <<< "$DEFAULT_ENVS"
fi

ENV_ARGS=()
for env in "${ENVIRONMENTS[@]}"; do
    ENV_ARGS+=(-e "$env")
done

# mktemp -d, а не -t с шаблоном: GNU и BSD расходятся в трактовке шаблона,
# и в CI на Linux вариант с суффиксом .xml не создался бы вовсе
REPORT_DIR="$(mktemp -d)"
REPORT="$REPORT_DIR/tests.xml"
trap 'rm -rf "$REPORT_DIR"' EXIT

set +e
"$PIO" test -d "$PROJECT_DIR" "${ENV_ARGS[@]}" --junit-output-path "$REPORT" "$@"
PIO_STATUS=$?
set -e

if [ ! -s "$REPORT" ]; then
    echo "ОШИБКА: pio не создал отчёт $REPORT — тесты не выполнялись" >&2
    exit 1
fi

# Счётчик из корневого <testsuites tests="N">
EXECUTED="$(python3 - "$REPORT" <<'PY'
import sys
import xml.etree.ElementTree as ET
print(ET.parse(sys.argv[1]).getroot().get("tests", "0"))
PY
)"

if [ "$EXECUTED" -eq 0 ]; then
    echo "ОШИБКА: не выполнено ни одного теста (окружения: ${ENVIRONMENTS[*]})." >&2
    echo "Прогон без тестов не является успехом: проверьте test_filter, test_ignore и имена окружений." >&2
    exit 1
fi

if [ $PIO_STATUS -ne 0 ]; then
    exit $PIO_STATUS
fi

echo "Выполнено тестов: $EXECUTED (окружения: ${ENVIRONMENTS[*]})"
