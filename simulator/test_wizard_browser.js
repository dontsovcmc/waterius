/*
 * Мастер настройки в настоящем браузере - вариант B проверки мастера.
 *
 * Тесты рядом (`test_simulator.js`) проверяют логику: обработчики, коды
 * ошибок, состояние после каждого шага. Разметку они не видят вовсе - в них
 * нет DOM. Здесь наоборот: страницы открываются в Chromium ровно так, как их
 * видит пользователь, а шаги мастера делаются кликами по настоящим кнопкам.
 *
 * Что ловит только этот тест: страница, которая не переходит дальше из-за
 * ошибки в скрипте, ошибка поля, которую прошивка вернула, а разметка не
 * показала, и любой `%ПЛЕЙСХОЛДЕР%`, доехавший до экрана.
 *
 * Браузер берётся установленный: playwright-core сам ничего не качает.
 *   npm install --prefix simulator
 *   npx playwright install chromium     # если браузера ещё нет
 */

'use strict';

const fs = require('fs');
const http = require('http');
const os = require('os');
const path = require('path');

const DIST = path.join(__dirname, 'dist');
const TYPES = {
    '.html': 'text/html; charset=utf-8',
    '.js': 'text/javascript; charset=utf-8',
    '.css': 'text/css; charset=utf-8',
    '.json': 'application/json; charset=utf-8',
    '.png': 'image/png',
    '.svg': 'image/svg+xml',
    '.ico': 'image/x-icon',
    '.txt': 'text/plain; charset=utf-8',
};

let failures = 0;

function check(name, problems) {
    if (problems.length) {
        failures += 1;
        console.log('FAIL ' + name);
        problems.forEach((p) => console.log('     ' + p));
    } else {
        console.log('  ok ' + name);
    }
}

/* Браузер: сначала спрашиваем playwright, потом ищем в его кэше. */
function findBrowser(chromium) {
    try {
        const declared = chromium.executablePath();
        if (declared && fs.existsSync(declared)) return declared;
    } catch (err) { /* playwright-core без реестра - ищем сами */ }

    const roots = [
        path.join(os.homedir(), 'Library', 'Caches', 'ms-playwright'),
        path.join(os.homedir(), '.cache', 'ms-playwright'),
    ];
    const names = [
        path.join('chrome-headless-shell-mac-arm64', 'chrome-headless-shell'),
        path.join('chrome-headless-shell-mac-x64', 'chrome-headless-shell'),
        path.join('chrome-headless-shell-linux', 'chrome-headless-shell'),
        path.join('chrome-mac-arm64', 'Google Chrome for Testing.app', 'Contents', 'MacOS', 'Google Chrome for Testing'),
        path.join('chrome-linux', 'chrome'),
    ];

    const found = [];
    roots.filter(fs.existsSync).forEach((root) => {
        fs.readdirSync(root)
            .filter((dir) => dir.startsWith('chromium'))
            .forEach((dir) => {
                names.forEach((name) => {
                    const full = path.join(root, dir, name);
                    if (fs.existsSync(full)) found.push({ dir, full });
                });
            });
    });
    if (!found.length) return null;

    // Самая свежая сборка: имя каталога кончается номером
    found.sort((a, b) => Number(b.dir.split('-').pop()) - Number(a.dir.split('-').pop()));
    return found[0].full;
}

function serve() {
    const server = http.createServer((req, res) => {
        const url = req.url.split('?')[0];
        const file = path.join(DIST, url === '/' ? 'index.html' : url);
        if (!file.startsWith(DIST) || !fs.existsSync(file) || fs.statSync(file).isDirectory()) {
            res.writeHead(404).end('not found');
            return;
        }
        res.writeHead(200, {
            'Content-Type': TYPES[path.extname(file)] || 'application/octet-stream',
            // Service worker перехватывает всё сам, кэш браузера тут только мешает
            'Cache-Control': 'no-store',
        });
        res.end(fs.readFileSync(file));
    });
    return new Promise((resolve) => {
        server.listen(0, '127.0.0.1', () => resolve({
            server,
            origin: 'http://127.0.0.1:' + server.address().port,
        }));
    });
}

async function impulses(page, input, count) {
    await page.evaluate(
        ([channel, times]) => fetch('/sim-api/cmd', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ type: 'impulse', input: channel, count: times }),
        }).then((res) => res.json()),
        [input, count],
    );
}

async function run() {
    let chromium;
    try {
        ({ chromium } = require('playwright-core'));
    } catch (err) {
        console.log('playwright-core не установлен: npm install --prefix simulator');
        process.exit(2);
    }

    const executablePath = findBrowser(chromium);
    if (!executablePath) {
        console.log('нет браузера: npx playwright install chromium');
        process.exit(2);
    }

    if (!fs.existsSync(DIST)) {
        console.log('нет сборки симулятора: simulator/build.sh');
        process.exit(2);
    }

    const { server, origin } = await serve();
    const browser = await chromium.launch({ executablePath });
    const context = await browser.newContext({ viewport: { width: 390, height: 844 } });
    const page = await context.newPage();

    const crashes = [];
    page.on('pageerror', (err) => crashes.push(err.message));

    try {
        // Пульт регистрирует service worker: без него портала нет вовсе
        await page.goto(origin + '/', { waitUntil: 'load' });
        await page.waitForFunction(() => navigator.serviceWorker.controller !== null, null,
            { timeout: 15000 });

        await wizard(page, origin);
        await alarms(page, origin);
        await placeholders(page, origin);
        await lostLink(page, origin);
        check('на страницах мастера нет ошибок в скриптах', crashes);
        await pagesOutsideWizard(page, origin);
        await wrongPassword(page, origin);
    } finally {
        await browser.close();
        server.close();
    }
}

async function wizard(page, origin) {
    const problems = [];
    const at = () => new URL(page.url()).pathname;

    await page.goto(origin + '/captive_portal_start.html');
    await page.click('text=Начать');
    await page.waitForURL(/wifi_list\.html/, { timeout: 10000 });

    // Список сетей рисует скрипт по ответу /api/networks
    const network = page.locator('.wifi-list a.link-row').first();
    await network.waitFor({ timeout: 10000 });
    const ssid = (await network.innerText()).trim();
    if (!ssid) problems.push('в списке сетей нет ни одной строки с именем');
    await network.click();

    await page.waitForURL(/wifi_password\.html/, { timeout: 10000 });
    const filled = await page.inputValue('#ssid');
    if (filled !== ssid) {
        problems.push('на странице пароля другая сеть: "' + filled + '" вместо "' + ssid + '"');
    }
    await page.fill('#password', 'secret123');
    await page.click('button[type=submit]');

    // Подключение идёт с задержкой, страница сама уходит на настройку входа
    await page.waitForURL(/input\/1\/setup\.html/, { timeout: 30000 });
    console.log('     подключились, мастер на ' + at());

    await page.click('button[type=submit], .btn[type=submit]');
    await page.waitForURL(/input\/1\/(detect|settings)\.html/, { timeout: 10000 });

    if (at().endsWith('detect.html')) {
        // Страница определения счётчика ждёт импульсов и уходит дальше сама
        await impulses(page, 1, 4);
        await page.waitForURL(/input\/1\/settings\.html/, { timeout: 20000 });
    }

    // Показания без разделителя: ошибку возвращает прошивка, показать её
    // обязана страница
    await page.fill('#channel_start', '123456');
    await page.click('button[type=submit]');
    const error = page.locator('.error:visible, .err:visible, [class*=error]:visible').first();
    let shown = '';
    try {
        await error.waitFor({ timeout: 8000 });
        shown = (await error.innerText()).trim();
    } catch (err) {
        problems.push('показания без литров приняты молча: ошибка не показана');
    }
    if (shown === '') problems.push('блок ошибки виден, но пуст: пользователь не узнает причину');
    else console.log('     ошибка на экране: ' + shown.replace(/\s+/g, ' ').slice(0, 80));

    await page.fill('#channel_start', '12.345');
    await page.click('button[type=submit]');
    await page.waitForURL((url) => !url.pathname.endsWith('/input/1/settings.html'),
        { timeout: 15000 });

    check('мастер проходится кликами в браузере', problems);
}

/*
Тревоги на пульте: галочка на бит. Проверяется то, чего не видно без DOM -
что галочки вообще нарисовались, что биты складываются, а не заменяют друг
друга, и что портал после этого показывает блок снятия.
*/
async function alarms(page, origin) {
    const problems = [];

    await page.goto(origin + '/', { waitUntil: 'load' });
    // сам input спрятан стилем переключателя, кликается подпись
    await page.waitForSelector('#alarm-flags input[type=checkbox]', { state: 'attached', timeout: 10000 });

    const boxes = await page.locator('#alarm-flags input[type=checkbox]').count();
    if (boxes !== 6) problems.push('галочек тревог ' + boxes + ', ожидалось 6 (три бита на канал)');

    // Две тревоги на одном канале: набор, которого старый список не давал
    await page.click('label[for="alarm0-ALARM_FLOW"]');
    await page.click('label[for="alarm0-ALARM_WET"]');
    await page.click('label[for="alarm1-ALARM_LEAK"]');

    const state = await page.evaluate(() => fetch('/sim-api/state').then((r) => r.json()));
    const D = require('./gen_from_firmware.js')().defines;
    const want = (D.ALARM_FLOW << D.ATTINY_ALARM_SHIFT0) | (D.ALARM_WET << D.ATTINY_ALARM_SHIFT0) |
                 (D.ALARM_LEAK << D.ATTINY_ALARM_SHIFT1);
    if (state.attiny.alarm_flags !== want) {
        problems.push('байт тревог ' + state.attiny.alarm_flags + ', ожидался ' + want);
    }

    await page.goto(origin + '/alarms.html', { waitUntil: 'networkidle' });
    if (await page.locator('#alarm_reset_box.hd').count()) {
        problems.push('тревоги подняты, а блок снятия на странице тревог спрятан');
    }

    await page.goto(origin + '/', { waitUntil: 'load' });
    // сам input спрятан стилем переключателя, кликается подпись
    await page.waitForSelector('#alarm-flags input[type=checkbox]', { state: 'attached', timeout: 10000 });
    if (!(await page.isChecked('#alarm0-ALARM_WET'))) {
        problems.push('пульт не показал поднятую тревогу после перезагрузки страницы');
    }

    check('тревоги поднимаются галочками и доезжают до портала', problems);
}

async function placeholders(page, origin) {
    const problems = [];
    const pages = ['/index.html', '/wifi_list.html', '/input/1/settings.html',
                   '/alarms.html', '/setup_send.html', '/about.html'];

    for (const url of pages) {
        await page.goto(origin + url, { waitUntil: 'networkidle' });
        const text = await page.locator('body').innerText();
        const left = text.match(/%[A-Za-z_][A-Za-z_0-9]*%/g);
        if (left) problems.push(url + ': ' + Array.from(new Set(left)).join(', '));
    }

    check('на экране не осталось плейсхолдеров', problems);
}

async function sim(page, command) {
    await page.evaluate((body) => fetch('/sim-api/cmd', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body),
    }).then((res) => res.json()), command);
}

// Мастер до шага подключения: сеть из списка, пароль, «Сохранить»
async function toConnectStep(page, origin) {
    await page.goto(origin + '/captive_portal_start.html');
    await page.click('text=Начать');
    await page.waitForURL(/wifi_list\.html/, { timeout: 10000 });
    const network = page.locator('.wifi-list a.link-row').first();
    await network.waitFor({ timeout: 10000 });
    await network.click();
    await page.waitForURL(/wifi_password\.html/, { timeout: 10000 });
    await page.fill('#password', 'secret123');
    await page.click('button[type=submit]');
    await page.waitForURL(/wifi_connect\.html/, { timeout: 10000 });
}

/*
Точка Ватериуса ушла на канал роутера, и телефон выпал из её сети (K2). Страница
обязана сказать об этом окном и сама закрыть его, когда связь вернулась. Обрыв
делается на шаге подключения: на устройстве он случается именно там.
*/
async function lostLink(page, origin) {
    const problems = [];
    await sim(page, { type: 'reset' });
    await toConnectStep(page, origin);

    await sim(page, { type: 'patch', state: { portal: { phone_link: false } } });
    const box = page.locator('.modal.show');
    try {
        await box.waitFor({ timeout: 15000 });
        const text = (await box.innerText()).replace(/\s+/g, ' ');
        if (!text.includes('Нет связи с Ватериусом')) problems.push('в окне нет заголовка: ' + text);
        if (!text.includes('waterius-')) problems.push('окно не говорит, к какой сети вернуться: ' + text);
    } catch (err) {
        problems.push('связь пропала, а окна «Нет связи с Ватериусом» нет');
    }

    await sim(page, { type: 'patch', state: { portal: { phone_link: true } } });
    try {
        await page.waitForURL(/input\/1\/setup\.html/, { timeout: 30000 });
    } catch (err) {
        problems.push('связь вернулась, а мастер не пошёл дальше: ' + new URL(page.url()).pathname);
    }

    check('связь пропала на шаге подключения: окно и возврат', problems);
}

/*
Страницы вне шагов мастера: те, что телефон открывает сам, едва подключившись к
точке (captive portal), и настройка Wi-Fi из меню. Идут после удачного
подключения: причины неудачи тогда нет, и её подстановка пустая.
*/
async function pagesOutsideWizard(page, origin) {
    const problems = [];
    const crashes = [];
    const onError = (err) => crashes.push(err.message);
    page.on('pageerror', onError);

    for (const url of ['/captive_portal_start.html', '/captive_portal_connected.html',
                       '/captive_portal_error.html', '/wifi_settings.html']) {
        const before = crashes.length;
        await page.goto(origin + url, { waitUntil: 'load' });
        await page.waitForTimeout(500);
        crashes.slice(before).forEach((message) => problems.push(url + ': ' + message));
    }

    page.off('pageerror', onError);
    check('страницы вне мастера открываются без ошибок в скриптах', problems);
}

/*
Неверный пароль: причину называют и страница Wi-Fi, куда возвращает мастер, и
страница ошибки, которую телефон открывает сам.
*/
async function wrongPassword(page, origin) {
    const problems = [];
    await sim(page, { type: 'reset' });
    await sim(page, { type: 'patch', state: { wifi: { outcome: 'wrong_password' } } });
    await toConnectStep(page, origin);

    const reason = async (url) => {
        const text = (await page.locator('#wifi_connect_status').innerText()).trim();
        if (!text.includes('Некорректный пароль')) problems.push(url + ': причина «' + text + '»');
    };

    try {
        await page.waitForURL(/wifi_settings\.html/, { timeout: 30000 });
        await reason('/wifi_settings.html');
    } catch (err) {
        problems.push('с неверным паролем мастер не вернулся на страницу Wi-Fi: ' +
                      new URL(page.url()).pathname);
    }

    await page.goto(origin + '/captive_portal_error.html', { waitUntil: 'load' });
    await reason('/captive_portal_error.html');

    check('неверный пароль назван на странице Wi-Fi и на странице ошибки', problems);
}

run().then(() => {
    console.log(failures ? 'провалов: ' + failures : 'браузерные проверки прошли');
    process.exit(failures ? 1 : 0);
}).catch((err) => {
    console.log('ОШИБКА: ' + err.message);
    process.exit(1);
});
