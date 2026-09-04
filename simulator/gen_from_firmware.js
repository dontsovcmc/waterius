'use strict';
/*
 * Читает исходники прошивки и отдаёт таблицы, по которым живёт симулятор.
 *
 * Всё, что можно прочитать из прошивки, читается, а не переписывается руками:
 * имена параметров, длины полей Settings, значения по умолчанию, перечисления.
 * Так самый частый класс расхождений «в прошивке поменяли, в симуляторе забыли»
 * не возникает вовсе.
 *
 * Запуск: node gen_from_firmware.js > dist/sim/generated.js
 */

const fs = require('fs');
const path = require('path');

const REPO = path.resolve(__dirname, '..');
const ESP = path.join(REPO, 'ESP8266');

function read(file) {
    return fs.existsSync(file) ? fs.readFileSync(file, 'utf8') : '';
}

/* Заголовок с Settings переехал из setup.h в core/types.h.
   Читаем оба и всё ядро: там же живут пределы, коды ошибок и перечисления. */
function firmwareHeaders() {
    return [
        'src/setup.h',
        'src/core/types.h',
        'src/core/input.h',
        'src/core/alarm.h',
        'src/core/idle.h',
        'src/core/wifi.h',
        'src/core/diagnostics.h',
    ].map((file) => path.join(ESP, file)).filter((f) => fs.existsSync(f));
}

function parseDefines(src, out) {
    const re = /^[ \t]*#define[ \t]+([A-Za-z_][A-Za-z_0-9]*)[ \t]+([^\r\n]+)$/gm;
    let m;
    while ((m = re.exec(src))) {
        out[m[1]] = stripComment(m[2]);
    }
    return out;
}

function stripComment(value) {
    let quoted = false;
    for (let i = 0; i < value.length; i++) {
        const c = value[i];
        if (c === '"' && value[i - 1] !== '\\') quoted = !quoted;
        else if (!quoted && c === '/' && value[i + 1] === '/') return value.slice(0, i).trim();
    }
    return value.trim();
}

function parseEnums(src, out) {
    const re = /enum\s+([A-Za-z_][A-Za-z_0-9]*)\s*(?::\s*[A-Za-z_][A-Za-z_0-9]*\s*)?\{([^}]*)\}/g;
    let m;
    while ((m = re.exec(src))) {
        const values = {};
        let next = 0;
        const body = m[2].split('\n').map(stripComment).join('\n');
        body.split(',').forEach((entry) => {
            const line = entry.trim();
            if (!line) return;
            const pair = line.match(/^([A-Za-z_][A-Za-z_0-9]*)\s*(?:=\s*(.+))?$/);
            if (!pair) return;
            let value = next;
            if (pair[2] !== undefined) value = Number(pair[2].trim());
            if (!Number.isFinite(value)) return;
            values[pair[1]] = value;
            next = value + 1;
        });
        out[m[1]] = values;
    }
    return out;
}

/* Разрешает выражение вида (uint8_t) true, CounterName::WATER_HOT, HOST_LEN, "text". */
function resolve(expr, defines, enums, depth) {
    if (expr === undefined || expr === null) return null;
    let value = stripComment(String(expr)).trim();
    if (!value || (depth || 0) > 8) return null;

    value = value.replace(/^\((?:const\s+)?(?:u?int(?:8|16|32|64)_t|uint8|char|float|bool|size_t)\)\s*/, '').trim();

    if (/^"(.*)"$/.test(value)) return value.slice(1, -1);
    if (/^\{\s*0\s*\}$/.test(value)) return null; // {0} - нули или пустая строка
    if (value === 'true') return 1;
    if (value === 'false') return 0;
    if (/^-?\d+(\.\d+)?$/.test(value)) return Number(value);
    if (/^0x[0-9a-fA-F]+$/.test(value)) return parseInt(value, 16);
    if (/^-?\d+U?L?L?$/i.test(value)) return parseInt(value, 10);

    const scoped = value.match(/^([A-Za-z_][A-Za-z_0-9]*)::([A-Za-z_][A-Za-z_0-9]*)$/);
    if (scoped && enums[scoped[1]] && scoped[2] in enums[scoped[1]]) return enums[scoped[1]][scoped[2]];

    if (/^[A-Za-z_][A-Za-z_0-9]*$/.test(value)) {
        if (value in defines) return resolve(defines[value], defines, enums, (depth || 0) + 1);
        for (const name of Object.keys(enums)) {
            if (value in enums[name]) return enums[name][value];
        }
    }
    return null;
}

const TYPES = 'uint8_t|uint16_t|uint32_t|uint64_t|int8_t|int16_t|int32_t|char|float|bool|time_t';

function parseSettings(src, defines, enums) {
    const block = src.match(/struct\s+Settings\s*\{([\s\S]*?)\n\};/);
    if (!block) return [];

    const fields = [];
    const re = new RegExp(
        '^\\s*(?:const\\s+)?(' + TYPES + ')\\s+([A-Za-z_][A-Za-z_0-9]*)\\s*(?:\\[([^\\]]+)\\])?\\s*(?:=\\s*([^;]+?))?\\s*;',
        'gm'
    );
    let m;
    while ((m = re.exec(block[1]))) {
        const [, type, name, sizeExpr, defExpr] = m;
        const len = sizeExpr === undefined ? 0 : resolve(sizeExpr, defines, enums);
        const isText = type === 'char' && sizeExpr !== undefined;
        const def = resolve(defExpr, defines, enums);

        fields.push({
            name,
            type,
            kind: isText ? 'text' : (type === 'float' ? 'float' : (sizeExpr !== undefined ? 'bytes' : 'int')),
            len: len || 0,
            def: def === null ? (isText ? '' : 0) : def,
        });
    }
    return fields;
}

/* static const char PARAM_X[] PROGMEM = "x"; -> {PARAM_X: "x"} */
function parseParams(src, out) {
    const re = /^[ \t]*static\s+const\s+char\s+([A-Za-z_][A-Za-z_0-9]*)\s*\[\]\s*PROGMEM\s*=\s*"([^"]*)"\s*;/gm;
    let m;
    while ((m = re.exec(src))) out[m[1]] = m[2];
    return out;
}

/*
 * Список типов входа, которые прошивка считает известными: switch в
 * is_valid_counter_type(), core/input.cpp. Читается, а не переписывается -
 * иначе новый тип входа появился бы в списке страницы, но не в симуляторе.
 */
function validCounterTypes(enums) {
    const src = read(path.join(ESP, 'src/core/input.cpp'));
    const body = src.match(/bool\s+is_valid_counter_type[^{]*\{([\s\S]*?)\n\}/);
    if (!body) return [];

    const types = [];
    const re = /case\s+CounterType::([A-Za-z_][A-Za-z_0-9]*)\s*:/g;
    let m;
    while ((m = re.exec(body[1]))) {
        const value = (enums.CounterType || {})[m[1]];
        if (value !== undefined) types.push(value);
    }
    return types;
}

function firmwareVersion() {
    const ini = read(path.join(ESP, 'platformio.ini'));
    const m = ini.match(/^firmware_version\s*=\s*"\\"([^"\\]+)\\""/m);
    return m ? m[1] : '0.0.0';
}

/* Приблизительная занятость LittleFS: файлы лежат блоками по 4 КБ.
   Точное число знает только образ, для страницы «О программе» этого хватает. */
const FS_BLOCK = 4096;
const FS_TOTAL = 245760; // раздел 1m256 у Классики

function fsInfo(dataDir) {
    let used = 0;
    const walk = (dir) => {
        for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
            const full = path.join(dir, entry.name);
            if (entry.isDirectory()) walk(full);
            else used += Math.ceil(fs.statSync(full).size / FS_BLOCK) * FS_BLOCK;
        }
    };
    if (fs.existsSync(dataDir)) walk(dataDir);
    return { totalBytes: FS_TOTAL, usedBytes: used };
}

/*
Список типов входа снимается прямо с селектора портала: подписи в пульте обязаны
совпадать со страницей, а не быть вторым набором слов про то же самое.
*/
function counterTypeOptions() {
    const html = read(path.join(ESP, 'data/input_setup.html'));
    const select = html.match(/<select[^>]*id="counter_type"[\s\S]*?<\/select>/);
    if (!select) return [];

    const options = [];
    const re = /<option[^>]*value="(\d+)"[^>]*>([^<]+)<\/option>/g;
    let m;
    while ((m = re.exec(select[0])) !== null) {
        options.push({ value: Number(m[1]), text: m[2].trim() });
    }
    return options;
}

function generate() {
    const defines = {};
    const enums = {};
    const params = {};

    let settingsSrc = '';
    for (const header of firmwareHeaders()) {
        const src = read(header);
        parseDefines(src, defines);
        parseEnums(src, enums);
        if (/struct\s+Settings\s*\{/.test(src)) settingsSrc = src;
    }

    parseParams(read(path.join(ESP, 'src/portal/resources.h')), params);
    parseParams(read(path.join(ESP, 'src/ha/resources.h')), params);

    const settings = parseSettings(settingsSrc, defines, enums);

    return {
        firmware_version: firmwareVersion(),
        build_date_time: process.env.SIM_BUILD_DATE || new Date().toISOString().slice(0, 19).replace('T', ' '),
        defines: pick(defines, [
            'AUTO_IMPULSE_FACTOR', 'AS_COLD_CHANNEL', 'DEFAULT_WAKEUP_PERIOD_MIN',
            'MQTT_DEFAULT_PORT', 'HOST_LEN', 'EMAIL_LEN', 'COMPANY_LEN', 'PLACE_LEN',
            'MQTT_LOGIN_LEN', 'MQTT_PASSWORD_LEN', 'MQTT_TOPIC_LEN', 'SERIAL_LEN',
            'WIFI_SSID_LEN', 'WIFI_PWD_LEN', 'ATTINY_VER_ALARM', 'ALARM_STOP_MAX_HOURS',
            'COMPARE_MIN_LITERS', 'SUSPICIOUS_RATIO', 'NEIGHBOUR_MIN_IMPULSES',
            'WIFI_CHANNEL_MIN', 'WIFI_CHANNEL_MAX', 'BSSID_LEN',
        ], defines, enums),
        enums: {
            CounterType: enums.CounterType || {},
            CounterName: enums.CounterName || {},
            InputColor: enums.InputColor || {},
            ParamError: enums.ParamError || {},
            AlarmConfirm: enums.AlarmConfirm || {},
        },
        params,
        settings,
        valid_counter_types: validCounterTypes(enums),
        counter_type_options: counterTypeOptions(),
        fs: fsInfo(path.join(ESP, 'data')),
    };
}

function pick(source, names, defines, enums) {
    const out = {};
    names.forEach((name) => {
        const value = resolve(source[name], defines, enums);
        if (value !== null) out[name] = value;
    });
    return out;
}

if (require.main === module) {
    const data = generate();
    process.stdout.write(
        '/* Создан gen_from_firmware.js из исходников прошивки. Руками не править. */\n' +
        'var SIM_GENERATED = ' + JSON.stringify(data, null, 2) + ';\n' +
        "if (typeof self !== 'undefined') self.SIM_GENERATED = SIM_GENERATED;\n" +
        "if (typeof module !== 'undefined') module.exports = SIM_GENERATED;\n"
    );
}

module.exports = generate;
module.exports.generate = generate;
