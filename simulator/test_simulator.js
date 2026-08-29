'use strict';
/*
 * Проверки симулятора. Ловят расхождение с прошивкой раньше, чем его увидит человек.
 *
 * Запуск: node simulator/test_simulator.js
 */

const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const DATA = path.join(ROOT, 'ESP8266/data');

globalThis.SIM_GENERATED = require('./gen_from_firmware.js')();
const SimCore = require("./src/sim/lib/core.js");
const SimState = require('./src/sim/lib/state.js');
const SimProcessor = require('./src/sim/lib/processor.js');
const SimApi = require('./src/sim/lib/api.js');
const SimRouter = require('./src/sim/lib/router.js');

let failed = 0;

function check(name, problems) {
    if (problems.length === 0) {
        console.log('ok   ' + name);
    } else {
        failed++;
        console.log('FAIL ' + name);
        problems.forEach((problem) => console.log('       ' + problem));
    }
}

function htmlFiles() {
    return fs.readdirSync(DATA).filter((file) => file.endsWith('.html'));
}

function read(file) {
    return fs.readFileSync(path.join(DATA, file), 'utf8');
}

/* 1. Каждый %плейсхолдер% страниц симулятору известен. */
function testPlaceholders() {
    const known = new Set(SimProcessor.known());
    const problems = [];

    htmlFiles().forEach((file) => {
        const found = read(file).match(/%[A-Za-z_][A-Za-z_0-9]*%/g) || [];
        new Set(found).forEach((token) => {
            const name = token.slice(1, -1);
            if (!known.has(name)) problems.push(file + ': %' + name + '% симулятор не подставляет');
        });
    });

    check('плейсхолдеры страниц известны симулятору', problems);
}

/* 2. Каждый адрес, на который ссылаются страницы и common.js, обслуживается. */
/* Закомментированная разметка и код - не ссылки: в about.html так спрятана английская версия. */
function dropComments(text) {
    return text.replace(/<!--[\s\S]*?-->/g, '').replace(/\/\*[\s\S]*?\*\//g, '');
}

function testRoutes() {
    const apiRoutes = new Set(SimApi.routes());
    const problems = [];
    const sources = htmlFiles().map((file) => [file, dropComments(read(file))]);
    sources.push(['static/common.js', dropComments(fs.readFileSync(path.join(DATA, 'static/common.js'), 'utf8'))]);

    sources.forEach(([file, text]) => {
        const found = text.match(/['"]\/[A-Za-z0-9_/.\-]*['"]/g) || [];
        new Set(found).forEach((token) => {
            const url = token.slice(1, -1);
            if (url.startsWith('//')) return;                       // внешняя ссылка
            if (url.endsWith('/')) return;                          // адрес собирается в коде: '/api/status/' + i
            if (!/\.html$|^\/api\//.test(url)) return;              // адреса файлов проверяет тест 4
            if (apiRoutes.has(url) || SimRouter.route(url)) return;
            problems.push(file + ': ' + url + ' симулятор не обслуживает');
        });
    });

    check('адреса страниц и API обслуживаются', problems);
}

/*
 * 3. Каждое поле формы принимается обработчиком сохранения.
 *
 * Тест ловит и настоящие ошибки прошивки: поле на странице есть, а сохранить его
 * некому. Известные случаи перечислены ниже - при появлении новых тест покраснеет.
 */
const NOT_SAVED = {
    input: 'служебное поле: выбирает вход, само не сохраняется',
    wizard: 'служебное поле мастера настройки',
    mac_address: 'только показ, менять нечего',
    counter_model: 'вспомогательный список, заполняет поле factor',
};

function testFormFields() {
    const saved = new Set(SimApi.savedParams());
    const problems = [];

    htmlFiles().forEach((file) => {
        const controls = read(file).match(/<(?:input|select|textarea)\b[^>]*>/g) || [];
        controls.forEach((tag) => {
            const name = (tag.match(/\bname="([A-Za-z_0-9]+)"/) || [])[1];
            if (!name || saved.has(name) || NOT_SAVED[name]) return;
            problems.push(file + ': поле ' + name + ' никто не сохраняет');
        });
    });

    check('поля форм принимаются обработчиком сохранения', problems);
}

/* 4. Каждая страница маршрута существует и отрисовывается без мусора. */
function testRender() {
    const state = SimState.newSession(SimState.defaultState());
    const problems = [];

    SimRouter.pages().forEach((page) => {
        const file = path.join(DATA, page.file);
        if (!fs.existsSync(file)) {
            // start.html объявлен в прошивке, но в образ не входит - страница отдаст 404
            if (page.file === 'start.html') return;
            problems.push(page.path + ': нет файла ' + page.file);
            return;
        }
        const html = SimProcessor.render(fs.readFileSync(file, 'utf8'), state, page.input);
        if (/%[A-Za-z_][A-Za-z_0-9]*%/.test(html)) problems.push(page.path + ': остался плейсхолдер');
        if (html.includes('undefined') || html.includes('NaN')) problems.push(page.path + ': в разметку попало undefined/NaN');
    });

    check('страницы отрисовываются без мусора', problems);
}

/* 5. Настройки читаются из прошивки, а не переписаны руками. */
function testGenerated() {
    const problems = [];
    const gen = globalThis.SIM_GENERATED;

    if (!gen.settings.length) problems.push('не разобрана структура Settings');
    if (!Object.keys(gen.params).length) problems.push('не разобраны имена параметров');
    if (!gen.enums.CounterType || gen.enums.CounterType.NONE !== 255) problems.push('не разобран CounterType');
    if (gen.defines.AUTO_IMPULSE_FACTOR === undefined) problems.push('не разобран AUTO_IMPULSE_FACTOR');

    const sett = SimState.defaultSettings();
    if (sett.mqtt_port !== gen.defines.MQTT_DEFAULT_PORT) problems.push('умолчание mqtt_port не из прошивки');
    if (SimState.fieldLength('waterius_host') !== gen.defines.HOST_LEN) problems.push('длина waterius_host не из прошивки');

    check('таблицы сняты с исходников прошивки', problems);
}

/*
 * 6. Мастер настройки проходится от Wi-Fi до сохранения показаний.
 *
 * Сценарий гоняет те же обработчики, что и service worker, поэтому ломается,
 * если перенос логики из прошивки разъехался по существу, а не по именам полей.
 */
function testWizard() {
    const problems = [];
    const state = SimState.newSession(SimState.defaultState());
    const params = (object) => Object.keys(object).map((name) => ({ name, value: String(object[name]) }));
    const call = (url, data, now) => SimApi.handle(url, 'POST', params(data || {}), state, now || 0);

    // Канал и BSSID приезжают скрытыми полями из списка сетей: коннект без скана
    const connect = call('/api/save_connect', {
        ssid: 'MyHome', password: 'secret123', wizard: 'true',
        wifi_channel: '1', bssid: '5c:cf:7f:aa:bb:01',
    });
    if (connect.json.redirect !== '/api/start_connect?wizard=true') problems.push('save_connect: ' + JSON.stringify(connect.json));
    if (state.sett.wifi_ssid !== 'MyHome') problems.push('ssid не сохранён');
    if (state.sett.wifi_channel !== 1) problems.push('канал быстрого коннекта не сохранён');
    if (!SimCore.hasBssid(state.wifi.bssid)) problems.push('bssid быстрого коннекта не сохранён');

    // Мусор в паре - полный скан, а не половина адреса
    const broken = call('/api/save_connect', {
        ssid: 'MyHome', password: 'secret123',
        wifi_channel: '1', bssid: 'не-адрес',
    });
    if (state.sett.wifi_channel !== 0) problems.push('битый bssid оставил канал');
    if (broken.json.redirect !== '/api/start_connect?error=0') problems.push('смена канала не отмечена: ' + JSON.stringify(broken.json));

    const start = call('/api/start_connect', { wizard: 'true' }, 1000);
    if (start.redirect !== '/wifi_connect.html?wizard=true') problems.push('start_connect: ' + JSON.stringify(start));

    const pending = call('/api/connect_status', {}, 2000);
    if (pending.json.redirect) problems.push('connect_status ответил раньше, чем подключился');

    const done = call('/api/connect_status', {}, 9000);
    if (done.json.redirect !== '/input/1/setup.html') problems.push('connect_status: ' + JSON.stringify(done.json));

    const type = call('/api/save_input_type', { input: '1', counter_name: '0', counter_type: '0', wizard: 'true' });
    if (type.json.redirect !== '/input/1/detect.html?wizard=true') problems.push('save_input_type: ' + JSON.stringify(type.json));

    state.attiny.impulses1 += 2; // подали два импульса краном
    const status = SimApi.handle('/api/status/1', 'GET', [], state, 0);
    if (status.json.state !== 1) problems.push('вход не увидел импульсы');
    if (status.json.factor !== 10) problems.push('вес импульса определился как ' + status.json.factor + ', ждали 10');

    const save = call('/api/save', { input: '1', channel_start: '12,345', factor: '3', serial: '' });
    if (Object.keys(save.json.errors).length) problems.push('сохранение показаний: ' + JSON.stringify(save.json.errors));
    if (state.sett.factor1 !== 10) problems.push('вес импульса не записан: ' + state.sett.factor1);
    if (Math.abs(state.sett.channel1_start - 12.345) > 1e-6) problems.push('показания через запятую не разобраны');
    if (state.sett.impulses1_start !== state.attiny.impulses1) problems.push('снимок импульсов не сделан');

    const main = SimApi.handle('/api/main_status', 'GET', [], state, 0);
    if (main.json.length) problems.push('после настройки главная всё ещё зовёт настраивать');

    // Проверка длины: 200 байт в поле на 64
    state.sett.mqtt_on = 1;
    const long = call('/api/save', { mqtt_host: 'x'.repeat(200) });
    if (long.json.errors.mqtt_host !== '14') problems.push('длинный mqtt_host не отвергнут');

    /*
     * Датчик протечки настраивается и возвращается на страницу тем же типом.
     * Пока is_valid_counter_type() про типы 5 и 6 не знал, страница показывала
     * NAMUR, и следующее сохранение затирало настройку.
     */
    const leak = call('/api/save_input_type', { input: '0', counter_type: '5' });
    if (leak.json.redirect !== '/index.html') problems.push('датчик протечки уводит не на главную');
    if (SimProcessor.value('counter0_type', state, 0) !== '5') {
        problems.push('датчик протечки показан как тип ' + SimProcessor.value('counter0_type', state, 0));
    }

    // Без связи с attiny тип входа не сохраняется
    state.attiny.link = false;
    const noLink = call('/api/save_input_type', { input: '0', counter_type: '1' });
    if (noLink.json.errors.counter_type !== '16') problems.push('без attiny тип входа сохранился');
    if (SimApi.handle('/api/status/0', 'GET', [], state, 0).json.error !== '7') problems.push('без attiny вход не сообщил об ошибке');

    check('мастер настройки проходится целиком', problems);
}

testPlaceholders();
testRoutes();
testFormFields();
testRender();
testGenerated();
testWizard();

console.log(failed ? '\nпровалено проверок: ' + failed : '\nвсе проверки пройдены');
process.exit(failed ? 1 : 0);
