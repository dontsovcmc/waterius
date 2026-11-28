# Tasks

## 1. Программатор

- [ ] 1.1 `Utils/hil/programmer.py`: `Programmer` (открытие с RTS=0/DTR=0, выход из моста сменой DTR, `cmd`, `info`, `board`, `log_begin`, `log_read`, `wave`, `wave_stat`, `hold`, `release`, `lines_off`), `ProgrammerGone`, `find_port()`, `check()` (версия ≥ 0.7.0, `# board: есть`). Проверка: `selftest/test_programmer.py` на поддельном порту — разбор ответов, отказы `# wave: …`, флаг потерь, старая версия, платы нет, пропавший порт
- [ ] 1.2 `dut.py` на роли линий: пачки `{'line': …}`, `_low` пачкой, `wet` через `hold`/`release`, `init()` с ожиданием конца пачки, потолки 32 участка / 30 с, без `rgb_*`. Проверка: `selftest/test_dut.py` переписан под поддельный программатор и проходит
- [ ] 1.3 `logwatch.py`: `poll` через `log_read` до неполного ответа, счётчик `lost_reads`, `loss_mark`/`assert_no_loss` по нему, `clear` выбирает кольцо до дна; ветки METF удалены. Проверка: `selftest/test_logwatch.py` — потеря в окне роняет ожидание, кольцо больше 4 КБ выбирается за один `poll`

## 2. Роутер и время

- [ ] 2.1 `router.py`: `Transport.run`, `SshTransport` (paramiko, одно соединение, переподключение при отказе канала, сверка `host_key` с записью при пустом поле), `restart` с переподключением, `connect()`. `requirements.txt`: `paramiko`, без `metf_python_client` и `requests`. Проверка: `selftest/test_router.py`, `test_router_guards.py` проходят; на плате — `python -m hil.router … raw "show config"` по SSH
- [ ] 2.2 `clock.py`: `RouterClock` на `ntp …` консоли; `Stand.clock` из роутера или `None`; `ensure_clock`/`arm_clock`/`requirements()` с адресом точки из `show config`; без роутера блок N пропускается с причиной. Проверка: самотест `RouterClock` на поддельной консоли; `pytest Utils/hil --stand -k "N1 or N2"` на стенде

## 3. Стенд целиком

- [ ] 3.1 `config.py`, `stand.ini.example`: `[programmer] port`, `[router] host_key`, без `[metf]`; `stand.py`, `conftest.py`, `rescue.py`, `discover.py`, `portal.py`, `reset.py` — без METF; `metf.py` удалён. Проверка: `grep -ri metf Utils/hil --include=*.py` пуст, весь `selftest/` проходит, `ruff check Utils/hil` чист
- [ ] 3.2 D7 через `series` одной пачкой, без потока. Проверка: `pytest Utils/hil --stand -k D7` на стенде, расписка — 10 замыканий
- [ ] 3.3 Документы: `Utils/hil/README.md` (оснастка, `stand.ini`), `rules.md` и прочие `0*.md` — где METF названа нынешней оснасткой, `01_wt32-eth01.md` (прошивка `2.5.0-waterius.1`, SSH, `ntp`), `CLAUDE.md` репозитория. Проверка: в документах METF осталась только как история

## 4. Прогон на железе

- [ ] 4.1 Полный прогон `pytest Utils/hil --stand` на одной прищепке; красные тесты разобраны по порядку из `rules.md`, итог и ссылка на лог — в описании PR
