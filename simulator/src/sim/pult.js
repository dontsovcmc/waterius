/*
 * Пульт: имитация железа Ватериуса.
 *
 * Состоянием владеет service worker, пульт только шлёт команды и показывает,
 * что получилось. Страницы портала пульт не трогает - они должны остаться
 * ровно такими, как в прошивке.
 */
'use strict';

/* Главная страница открывается кнопкой, остальные - списком под ней */
var PORTAL_LINKS = [
    { path: '/captive_portal_start.html', text: 'Мастер настройки' },
    { path: '/wifi_settings.html', text: 'Wi-Fi' },
    { path: '/input/1/setup.html', text: 'Вход 1 (синий)' },
    { path: '/input/0/setup.html', text: 'Вход 0 (красный)' },
    { path: '/setup_send.html', text: 'Отправка показаний' },
    { path: '/alarms.html', text: 'Тревоги' },
    { path: '/about.html', text: 'О программе' },
    { path: '/logs.html', text: 'Логи' },
    { path: '/reset.html', text: 'Сброс к заводским' },
];

/* Типы входа - те же, что в селекторе портала: снимаются с input_setup.html */
var COUNTER_TYPES = (self.SIM_GENERATED && self.SIM_GENERATED.counter_type_options) || [];

var WL_NAMES = {
    0: 'код 0 (idle)',
    1: 'сеть не найдена',
    2: 'сканирование завершено',
    3: 'подключён',
    4: 'ошибка подключения',
    5: 'связь потеряна',
    6: 'неверный пароль',
    7: 'отключён',
    255: 'нет модуля',
};

var state = null;
var queue = Promise.resolve(); // команды идут по очереди: быстрые клики не теряются

function el(id) { return document.getElementById(id); }

/* Поле, в котором стоит курсор, не трогаем: иначе значение прыгает под руками. */
function fill(id, value) {
    var node = el(id);
    if (document.activeElement !== node) node.value = value;
}

function setStatus(text, isError) {
    var node = el('status');
    node.textContent = text;
    node.classList.toggle('err', !!isError);
}

function autoRefresh() {
    return el('auto-refresh').checked;
}

function send(command) {
    command.refresh = autoRefresh();
    queue = queue.then(function () {
        return fetch('/sim-api/cmd', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(command),
        })
            .then(function (res) { return res.json(); })
            .then(function (fresh) { state = fresh; draw(); })
            .catch(function (err) { setStatus('Команда не прошла: ' + err.message, true); });
    });
    return queue;
}

function load() {
    return fetch('/sim-api/state')
        .then(function (res) { return res.json(); })
        .then(function (fresh) { state = fresh; draw(); })
        .catch(function (err) { setStatus('Не читается состояние: ' + err.message, true); });
}

/* ---- отрисовка ---- */

function drawLinks() {
    var box = el('links');
    if (box.childElementCount) return;
    PORTAL_LINKS.forEach(function (link) {
        var item = document.createElement('li');
        var a = document.createElement('a');
        a.href = link.path;
        a.target = 'waterius-portal';
        a.textContent = link.text;
        item.appendChild(a);
        box.appendChild(item);
    });
}

function drawInputs() {
    var box = el('inputs');
    if (!box.childElementCount) {
        [1, 0].forEach(function (input) {
            var block = document.createElement('div');
            block.className = 'input-block';
            block.innerHTML =
                '<h3><span class="circle' + (input === 0 ? ' hot' : '') + '"></span>Вход ' + input +
                (input === 0 ? ' (красный)' : ' (синий)') + '</h3>' +
                '<div class="row">' +
                '<label class="field">тип входа<select data-type="' + input + '"></select></label>' +
                '<button class="btn btn-sm" data-impulse="' + input + '">+1 импульс</button>' +
                '<label class="field">серия<input type="number" data-count="' + input + '" value="10" min="1" max="1000"></label>' +
                '<button class="btn btn-sm btn2" data-series="' + input + '">Подать серию</button>' +
                '</div>' +
                '<p class="note" data-info="' + input + '"></p>';
            box.appendChild(block);
        });

        box.querySelectorAll('select[data-type]').forEach(function (select) {
            COUNTER_TYPES.forEach(function (type) {
                var option = document.createElement('option');
                option.value = type.value;
                option.textContent = type.text;
                select.appendChild(option);
            });
            select.addEventListener('change', function () {
                var patch = { attiny: {} };
                patch.attiny['counter_type' + select.dataset.type] = Number(select.value);
                send({ type: 'patch', state: patch });
            });
        });

        box.querySelectorAll('button[data-impulse]').forEach(function (button) {
            button.addEventListener('click', function () {
                send({ type: 'impulse', input: Number(button.dataset.impulse), count: 1 });
            });
        });

        box.querySelectorAll('button[data-series]').forEach(function (button) {
            button.addEventListener('click', function () {
                var input = Number(button.dataset.series);
                var count = Number(box.querySelector('input[data-count="' + input + '"]').value) || 1;
                send({ type: 'impulse', input: input, count: count });
            });
        });
    }

    [0, 1].forEach(function (input) {
        var attiny = state.attiny['impulses' + input];
        var session = state.session['impulses' + input];
        var factor = state.sett['factor' + input];
        box.querySelector('select[data-type="' + input + '"]').value = state.attiny['counter_type' + input];
        box.querySelector('p[data-info="' + input + '"]').textContent =
            'импульсов всего ' + attiny + ', за сеанс ' + (attiny - session) + ', вес ' + factor + ' (3 — определяется, 7 — как у холодной)';
    });
}

function drawWifi() {
    el('wifi-outcome').value = state.wifi.outcome;
    el('wifi-status').value = WL_NAMES[state.wifi.status] || state.wifi.status;
    var box = el('wifi-networks');
    if (document.activeElement !== box) {
        box.value = state.wifi.networks.map(function (net) {
            return [net.ssid, net.level, net.wifi_channel].join(', ');
        }).join('\n');
    }
}

function draw() {
    if (!state) return;
    drawLinks();
    drawInputs();
    drawWifi();

    fill('attiny-version', state.attiny.version);
    fill('attiny-voltage', state.attiny.voltage);
    el('attiny-link').checked = state.attiny.link;
    el('esp-restarted').checked = state.portal.esp_restarted;
    el('session-note').textContent = 'Сеансов настройки: ' + state.attiny.setup_started_counter +
        (state.portal.exited ? '. Портал выключен кнопкой «Готово» — нажмите кнопку Ватериуса.' : '');
    el('dump').textContent = JSON.stringify(state, null, 2);
}

/* ---- органы управления ---- */

function bind() {
    document.querySelectorAll('button[data-cmd]').forEach(function (button) {
        button.addEventListener('click', function () { send({ type: button.dataset.cmd }); });
    });

    el('refresh').addEventListener('click', function () { fetch('/sim-api/refresh'); });

    el('attiny-version').addEventListener('change', function (event) {
        send({ type: 'patch', state: { attiny: { version: Number(event.target.value) } } });
    });
    el('attiny-voltage').addEventListener('change', function (event) {
        send({ type: 'patch', state: { attiny: { voltage: Number(event.target.value) } } });
    });
    el('attiny-link').addEventListener('change', function (event) {
        send({ type: 'patch', state: { attiny: { link: event.target.checked } } });
    });
    el('esp-restarted').addEventListener('change', function (event) {
        send({ type: 'patch', state: { portal: { esp_restarted: event.target.checked } } });
    });
    el('wifi-outcome').addEventListener('change', function (event) {
        send({ type: 'patch', state: { wifi: { outcome: event.target.value } } });
    });

    el('wifi-save').addEventListener('click', function () {
        var networks = el('wifi-networks').value.split('\n').map(function (line) {
            var parts = line.split(',');
            if (!parts[0] || !parts[0].trim()) return null;
            return {
                ssid: parts[0].trim(),
                level: Number(parts[1]) || 1,
                wifi_channel: Number(parts[2]) || 1,
            };
        }).filter(Boolean);
        send({ type: 'patch', state: { wifi: { networks: networks } } });
    });
}

/* ---- запуск ---- */

function start() {
    if (!('serviceWorker' in navigator)) {
        setStatus('Браузер не умеет service worker — симулятор не заработает. Нужен обычный режим окна.', true);
        return;
    }

    navigator.serviceWorker.register('/sw.js')
        .then(function () { return navigator.serviceWorker.ready; })
        .then(function () {
            if (navigator.serviceWorker.controller) return null;
            return new Promise(function (resolve) {
                navigator.serviceWorker.addEventListener('controllerchange', resolve, { once: true });
                setTimeout(resolve, 1500);
            });
        })
        .then(function () {
            if (!navigator.serviceWorker.controller) {
                if (!sessionStorage.getItem('sim-reloaded')) {
                    sessionStorage.setItem('sim-reloaded', '1');
                    location.reload();
                    return;
                }
                setStatus('Service worker не взял управление. Обновите страницу.', true);
                return;
            }
            sessionStorage.removeItem('sim-reloaded');
            setStatus('Симулятор работает. Страницы портала отдаются из образа прошивки.');
            bind();
            load();
        })
        .catch(function (err) {
            setStatus('Не удалось запустить: ' + err.message, true);
        });

    try {
        new BroadcastChannel('waterius-sim').onmessage = function () { load(); };
    } catch (e) {
        setInterval(load, 3000); // старый браузер: просто опрашиваем
    }
}

start();
