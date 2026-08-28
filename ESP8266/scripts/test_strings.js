/*
Тесты веб-портала: строки и единицы измерения (data/static/strings.js) плюс
те правила из data/static/common.js, которые не про разметку.

Запуск:
    node ESP8266/scripts/test_strings.js

Файл лежит в scripts/, а не в data/ и не в test/: всё из data/ уезжает в
образ LittleFS, а каждый каталог test/ платформа считает сюитом на C++.

Проверяется таблица RESOURCES: там записано, в чём измеряется каждый
ресурс и как из импульсов получается расход. Числа обязаны совпадать с
расчётом прошивки в src/core/readings.cpp, а до появления второго языка —
ещё и быть единственным местом, где размерности вообще написаны (#358).
*/

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const STRINGS = path.join(__dirname, '..', 'data', 'static', 'strings.js');
const COMMON = path.join(__dirname, '..', 'data', 'static', 'common.js');

// Файла нет — это провал, а не повод пропустить проверку
const source = fs.readFileSync(STRINGS, 'utf8');
const sandbox = vm.createContext({});
vm.runInContext(source, sandbox, { filename: STRINGS });

/*
common.js грузится в ту же песочницу: на верхнем уровне он объявляет только
константы и функции, к DOM обращаются лишь их тела, поэтому заглушки браузера
не нужны. Проверяем из него правила, которые не про разметку.
*/
vm.runInContext(fs.readFileSync(COMMON, 'utf8'), sandbox, { filename: COMMON });

/*
Объявленные через const имена не попадают в объект контекста — они живут в
лексическом окружении области видимости, общем для всех скриптов контекста.
Поэтому до них дотягиваемся вычислением выражения в том же контексте.
*/
const ctx = new Proxy({}, {
    get: (_, name) => vm.runInContext(String(name), sandbox)
});

let executed = 0;
let failed = 0;

function test(name, body) {
    executed++;
    try {
        body();
    } catch (e) {
        failed++;
        console.error('ПРОВАЛ: ' + name + '\n  ' + e.message);
    }
}

const ALL_NAMES = [
    ctx.CounterName_WATER_COLD,
    ctx.CounterName_WATER_HOT,
    ctx.CounterName_ELECTRO,
    ctx.CounterName_GAS,
    ctx.CounterName_HEAT_GCAL,
    ctx.CounterName_PORTABLE_WATER,
    ctx.CounterName_OTHER,
    ctx.CounterName_HEAT_KWH,
];

test('у каждого типа ресурса есть название, пункт списка и инструкция', () => {
    for (const name of ALL_NAMES) {
        const r = ctx.RESOURCES[name];
        assert.ok(r, 'нет записи для ресурса ' + name);
        assert.ok(r.title, 'нет заголовка у ресурса ' + name);
        assert.ok(r.option, 'нет пункта списка у ресурса ' + name);
        assert.ok(r.instruction, 'нет инструкции у ресурса ' + name);
    }
});

test('единица показаний своя у каждого ресурса', () => {
    assert.strictEqual(ctx.unit_of(ctx.CounterName_WATER_COLD), 'м³');
    assert.strictEqual(ctx.unit_of(ctx.CounterName_WATER_HOT), 'м³');
    assert.strictEqual(ctx.unit_of(ctx.CounterName_PORTABLE_WATER), 'м³');
    assert.strictEqual(ctx.unit_of(ctx.CounterName_GAS), 'м³');
    assert.strictEqual(ctx.unit_of(ctx.CounterName_ELECTRO), 'кВт·ч');
    assert.strictEqual(ctx.unit_of(ctx.CounterName_HEAT_GCAL), 'Гкал');
    assert.strictEqual(ctx.unit_of(ctx.CounterName_HEAT_KWH), 'кВт·ч');
});

test('у типа «Другой» размерности нет, и подпись поля остаётся без запятой', () => {
    // Суть #358: что считает этот вход, знает только владелец счётчика
    assert.strictEqual(ctx.unit_of(ctx.CounterName_OTHER), '');
    assert.strictEqual(ctx.unit_suffix(ctx.CounterName_OTHER), '');
    assert.strictEqual(ctx.unit_suffix(ctx.CounterName_WATER_COLD), ', м³');
});

test('вес импульса подписан в единицах ресурса', () => {
    // Вода: литры на импульс, как на шкале счётчика
    assert.strictEqual(ctx.pulse_weight_text(ctx.CounterName_WATER_COLD, 1), '1 л');
    assert.strictEqual(ctx.pulse_weight_text(ctx.CounterName_WATER_COLD, 10), '10 л');
    assert.strictEqual(ctx.pulse_weight_text(ctx.CounterName_WATER_COLD, 100), '100 л');
    // Газ и тепло: на шкале написана доля единицы показаний
    assert.strictEqual(ctx.pulse_weight_text(ctx.CounterName_GAS, 10), '0,01 м³');
    assert.strictEqual(ctx.pulse_weight_text(ctx.CounterName_HEAT_GCAL, 1), '0,001 Гкал');
    assert.strictEqual(ctx.pulse_weight_text(ctx.CounterName_HEAT_KWH, 100), '0,1 кВт·ч');
    // «Другой»: доля показаний без обещания размерности
    assert.strictEqual(ctx.pulse_weight_text(ctx.CounterName_OTHER, 10), '0,01');
});

test('расход за время настройки считается как в прошивке', () => {
    // core/readings.cpp: вода — импульсы * вес, дельта в литрах
    assert.strictEqual(ctx.delta_text(ctx.CounterName_WATER_COLD, 3, 10), ' = 30 л');
    // газ и тепло: то же самое, но показывается в единицах показаний
    assert.strictEqual(ctx.delta_text(ctx.CounterName_GAS, 3, 10), ' = 0,03 м³');
    assert.strictEqual(ctx.delta_text(ctx.CounterName_HEAT_GCAL, 5, 1), ' = 0,005 Гкал');
    // электричество: вес задан импульсами на кВт·ч, поэтому деление
    assert.strictEqual(ctx.delta_text(ctx.CounterName_ELECTRO, 640, 3200), ' = 0,2 кВт·ч');
});

test('только электричество считается импульсами на единицу', () => {
    // Ветка сравнения в core/readings.cpp ровно одна: CounterName::ELECTRO
    for (const name of ALL_NAMES) {
        const per_unit = !!ctx.RESOURCES[name].per_unit;
        assert.strictEqual(per_unit, name === ctx.CounterName_ELECTRO,
                           'неверная формула расхода у ресурса ' + name);
    }
});

test('расход не показывается, пока показывать нечего', () => {
    assert.strictEqual(ctx.delta_text(ctx.CounterName_WATER_COLD, 0, 10), '');
    assert.strictEqual(ctx.delta_text(ctx.CounterName_WATER_COLD, 3, 0), '');
    // у «Другого» размерности нет, число без единицы вводило бы в заблуждение
    assert.strictEqual(ctx.delta_text(ctx.CounterName_OTHER, 3, 10), '');
});

test('вес импульса берётся из формы, а спецзначения — из прошивки', () => {
    const auto = ctx.AUTO_IMPULSE_FACTOR;
    const as_cold = ctx.AS_COLD_CHANNEL;
    // "Авто" и "как у холодной" разворачивает прошивка
    assert.strictEqual(ctx.effective_factor(ctx.CounterName_WATER_COLD, auto, 10), 10);
    assert.strictEqual(ctx.effective_factor(ctx.CounterName_WATER_COLD, as_cold, 1), 1);
    // Обычное значение показываем сразу, не дожидаясь сохранения
    assert.strictEqual(ctx.effective_factor(ctx.CounterName_WATER_COLD, '100', 10), 100);
    // У электричества спецзначений нет: 3 импульса на кВт·ч — это 3
    assert.strictEqual(ctx.effective_factor(ctx.CounterName_ELECTRO, '3200', 0), 3200);
    assert.strictEqual(ctx.effective_factor(ctx.CounterName_ELECTRO, auto, 0), auto);
    assert.strictEqual(ctx.effective_factor(ctx.CounterName_ELECTRO, '', 3200), 3200);
});

test('числа показываются с запятой и без хвостовых нулей', () => {
    assert.strictEqual(ctx.format_number(30), '30');
    assert.strictEqual(ctx.format_number(0.01), '0,01');
    assert.strictEqual(ctx.format_number(0.2), '0,2');
    assert.strictEqual(ctx.format_number(1000), '1000');   // не "1"
    assert.strictEqual(ctx.format_number(1000.5), '1000,5');
});

test('неизвестный тип ресурса не роняет страницу', () => {
    // В настройках может лежать число от прошивки, которая новее портала
    assert.strictEqual(ctx.resource(42), ctx.RESOURCES[ctx.CounterName_OTHER]);
    assert.strictEqual(ctx.unit_suffix(42), '');
});

test('название входа подставляется в сообщение, а без него текст цел', () => {
    // Сообщения про вход одни на оба входа: подставляется только название
    const red = ctx.tr(ctx.S_FACTOR_TOO_BIG, ctx.INPUT_PLACE[0]);
    assert.ok(red.includes('на красном входе'), 'название входа не подставилось: ' + red);
    assert.ok(!red.includes('%s'), 'в тексте остался маркер подстановки: ' + red);
    assert.ok(ctx.tr(ctx.S_INPUT_SILENT, ctx.INPUT_PLACE[1]).includes('на синем входе'));

    // mainStatus() зовёт tr() с INPUT_PLACE[undefined] для всех остальных
    // сообщений: текст без маркера не должен от этого пострадать
    const plain = ctx.tr(ctx.S_WIFI_CONNECT, undefined);
    assert.strictEqual(plain, 'Ошибка подключения к Wi-Fi');
});

test('у каждого сообщения про вход есть место для названия', () => {
    for (const id of [ctx.S_FACTOR_TOO_BIG, ctx.S_INPUT_SILENT]) {
        assert.ok(ctx.tr(id).includes('%s'), 'в сообщении ' + id + ' некуда подставить вход');
    }
    assert.strictEqual(ctx.INPUT_PLACE.length, 2, 'входа два: красный и синий');
});

test('до устройства не дошли только сетевые ошибки, а не ответы прошивки', () => {
    // fetch() отвергает промис объектом ошибки — связи нет
    assert.strictEqual(ctx.is_device_unreachable(new TypeError('Failed to fetch')), true);
    assert.strictEqual(ctx.is_device_unreachable(new TypeError('NetworkError')), true);
    // Safari на телефоне пишет своё, и это основной браузер наших пользователей
    assert.strictEqual(ctx.is_device_unreachable(new TypeError('Load failed')), true);
    assert.strictEqual(ctx.is_device_unreachable(undefined), true);

    // Ответ сервера с плохим статусом (Response) — прошивка жива, окно про
    // Wi-Fi было бы враньём
    assert.strictEqual(ctx.is_device_unreachable({status: 500, statusText: 'Internal Error'}), false);
    assert.strictEqual(ctx.is_device_unreachable({status: 404, statusText: 'Not Found'}), false);
});

test('в окне потери связи есть заголовок, объяснение и подпись кнопки', () => {
    assert.strictEqual(ctx.tr(ctx.S_LOST_LINK_TITLE), 'Нет связи с Ватериусом');
    assert.strictEqual(ctx.tr(ctx.S_RETRY), 'Повторить');

    // Текст обязан назвать оба выхода: вернуться в сеть и включить режим заново
    const text = ctx.tr(ctx.S_PLEASE_RECONNECT_WIFI);
    assert.ok(text.includes('waterius-'), 'не сказано, к какой сети подключаться: ' + text);
    assert.ok(text.includes('кнопку'), 'не сказано, что делать, если сети нет: ' + text);
});

test('повторы ограничены, иначе вкладка стучится вечно', () => {
    assert.ok(ctx.AJAX_TRIES >= 2, 'один обрыв не должен сразу открывать окно');
    assert.ok(ctx.LOST_LINK_RETRY_MS >= 2000, 'фоновые повторы не должны частить');
    assert.ok(ctx.LOST_LINK_GIVE_UP_MS > ctx.LOST_LINK_RETRY_MS);
});

test('типы входа названы для всех значений из списка на странице', () => {
    // Значения из data/input_setup.html
    for (const type of [0, 2, 4, 5, 0xFF]) {
        assert.ok(ctx.COUNTER_TYPES[type], 'нет названия у типа входа ' + type);
    }
});

test('порог расхода подписан по ресурсу канала', () => {
    /*
    У электричества вес импульса задан наоборот - импульсами на киловатт-час,
    - и порог считается по другой формуле. Единица должна об этом говорить,
    иначе пользователь введёт литры там, где ждут ватты (#202).
    */
    assert.ok(ctx.alarm_flow_label(ctx.CounterName_ELECTRO).includes('Вт'));
    assert.ok(ctx.alarm_flow_label(ctx.CounterName_WATER_COLD).includes('л/ч'));
    assert.ok(ctx.alarm_flow_label(ctx.CounterName_GAS).includes('л/ч'));
});

/*
Договор разметки со скриптом. Полноценного браузера здесь нет, поэтому
страницы проверяются как текст — но именно на этих стыках уже ломалось:
страница электричества звала функцию, которая писала в несуществующий
элемент, и счётчик расхода не обновлялся вообще.
*/
const DATA_DIR = path.join(__dirname, '..', 'data');
const PAGES = fs.readdirSync(DATA_DIR).filter(f => f.endsWith('.html'));

function page(name) {
    return fs.readFileSync(path.join(DATA_DIR, name), 'utf8');
}

test('на странице тревог у каждого поля есть подпись и обработчик', () => {
    /*
    Страница одна на оба канала, поэтому подписи заполняются не общим
    fill_units, а маркерами data-alarm-label с номером входа.
    */
    const html = page('alarms.html');

    for (const field of ['alarm_flow0', 'alarm_leak0', 'alarm_stop0',
                         'alarm_flow1', 'alarm_leak1', 'alarm_stop1']) {
        assert.ok(html.includes('name="' + field + '"'), 'нет поля ' + field);
        assert.ok(html.includes('%' + field + '%'), 'поле ' + field + ' не подставляется прошивкой');
    }

    for (const input of ['0', '1']) {
        assert.ok(html.includes('data-alarm-label="' + input + '"'),
                  'у входа ' + input + ' нет подписи порога');
    }

    assert.ok(html.includes('fill_alarms('), 'страница не зовёт fill_alarms');
    assert.ok(html.includes("'/api/save_alarms'"), 'форма шлёт настройки не туда');
});

test('пороги расхода и остановки прячутся по отдельности', () => {
    /*
    Порог расхода считает attiny, и ему нужен известный вес импульса; остановку
    считает сама ЕСП по приросту импульсов, поэтому она доступна и на attiny 40.
    Один общий признак готовности спрятал бы работающую тревогу.
    */
    const html = page('alarms.html');

    for (const input of ['0', '1']) {
        assert.ok(html.includes('id="thresholds' + input + '"'),
                  'у входа ' + input + ' пороги расхода не в своём блоке');
        assert.ok(html.includes('id="stop' + input + '"'),
                  'у входа ' + input + ' остановка не в своём блоке');
        assert.ok(html.includes('%stop_ready' + input + '%'),
                  'готовность остановки на входе ' + input + ' не подставляется');
    }
});

test('режим "я уехал" есть на странице тревог', () => {
    const html = page('alarms.html');

    assert.ok(html.includes('name="vacation"'), 'нет галочки режима отпуска');
    assert.ok(html.includes('%vacation%'), 'состояние галочки не подставляется прошивкой');
});

test('в разметке нет маркеров, которых не знает fill_units', () => {
    const known = ['total', 'unit', 'impulses'];
    for (const name of PAGES) {
        const html = page(name);
        for (const m of html.matchAll(/data-unit="([^"]*)"/g)) {
            assert.ok(known.includes(m[1]), name + ': неизвестный маркер ' + m[1]);
        }
    }
});

test('страницы с размерностями зовут fill_units с типом ресурса', () => {
    let pages_with_units = 0;
    for (const name of PAGES) {
        const html = page(name);
        if (!html.includes('data-unit=')) continue;
        pages_with_units++;
        assert.ok(html.includes('fill_units(%counter_name%)'),
                  name + ': маркеры есть, а fill_units(%counter_name%) не вызывается');
    }
    assert.ok(pages_with_units >= 2, 'страницы с размерностями не найдены');
});

test('опрос импульсов вызывается с типом ресурса и только существующей функцией', () => {
    for (const name of PAGES) {
        const html = page(name);
        assert.ok(!html.includes('getImpulsesHall'),
                  name + ': getImpulsesHall удалён вместе с поддержкой датчика Холла');
        for (const m of html.matchAll(/getImpulses\(([^)]*)\)/g)) {
            assert.strictEqual(m[1].split(',').length, 2,
                               name + ': getImpulses ждёт вход и тип ресурса, получил (' + m[1] + ')');
        }
    }
});

test('подписи списка ресурсов в разметке совпадают с таблицей', () => {
    // JS перепишет их при загрузке, но запасной вариант не должен врать
    const html = page('input_setup.html');
    const start = html.indexOf('id="counter_name"');
    assert.ok(start > 0, 'на странице нет списка ресурсов');
    const select = html.slice(start, html.indexOf('</select>', start));
    let options = 0;
    for (const m of select.matchAll(/<option[^>]*value="(\d+)"[^>]*>([^<]+)<\/option>/g)) {
        options++;
        const r = ctx.RESOURCES[Number(m[1])];
        assert.ok(r, 'в списке есть ресурс ' + m[1] + ', которого нет в таблице');
        assert.strictEqual(m[2], r.option, 'расходится подпись ресурса ' + m[1]);
    }
    assert.strictEqual(options, ALL_NAMES.length, 'в списке не все типы ресурса');
});

if (executed === 0) {
    console.error('ОШИБКА: не выполнено ни одного теста');
    process.exit(1);
}
if (failed > 0) {
    console.error('Провалено тестов: ' + failed + ' из ' + executed);
    process.exit(1);
}
console.log('Выполнено тестов: ' + executed + ' (портал)');
