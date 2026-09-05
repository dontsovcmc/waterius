/*
 * Порт чистого ядра прошивки: ESP8266/src/core/.
 *
 * Разложено так же, как там, чтобы правку в прошивке было куда переносить:
 * input.cpp - правила проверки полей, url.cpp - адрес брокера,
 * diagnostics.cpp - плашки настройки счётчиков, readings.cpp - вес импульса,
 * alarm.cpp - можно ли настроить тревоги, wifi.cpp - быстрый коннект.
 */
(function (root) {
    'use strict';

    function G() { return root.SIM_GENERATED; }
    function E() { return root.SIM_GENERATED.enums.ParamError; }

    var encoder = typeof TextEncoder !== 'undefined' ? new TextEncoder() : null;

    // Длина строки в байтах: поля Settings меряются байтами, кириллица - два байта
    function byteLength(text) {
        if (encoder) return encoder.encode(text).length;
        return Buffer.byteLength(text, 'utf8');
    }

    /* ---- input.cpp ---- */

    function isAllAsterisks(value) {
        return value.length > 0 && /^[* \t]+$/.test(value);
    }

    function copyTrimmed(value, size) {
        var text = value.replace(/^\s+|\s+$/g, '');
        while (text.length && byteLength(text) >= size) text = text.slice(0, -1);
        return text;
    }

    /* Возвращает {err, value}: значение осмысленно только при PARAM_OK. */
    function parseText(value, size, required) {
        if (byteLength(value) >= size) return { err: E().PARAM_ERR_LENGTH };
        if (required && value.length === 0) return { err: E().PARAM_ERR_EMPTY };
        if (isAllAsterisks(value)) return { err: E().PARAM_MASKED };
        return { err: E().PARAM_OK, value: copyTrimmed(value, size) };
    }

    function parseDecimal(value) {
        var parsed = parseFloat(String(value).replace(',', '.'));
        return { err: E().PARAM_OK, value: isNaN(parsed) ? 0 : parsed }; // atof: мусор - ноль
    }

    /*
     * Порт parse_long(): цифры были, после числа ничего нет, влезает в границы.
     * Слабый разбор пропускал "abc" как ноль, а "-1" во флажке давал 255.
     */
    function parseLong(value, min, max) {
        if (value === null || value === undefined) return null;
        var text = String(value);
        var match = text.match(/^[ \t\n\r]*([+-]?\d+)[ \t\n\r]*$/);
        if (!match) return null;
        var parsed = Number(match[1]);
        if (!isFinite(parsed) || parsed < min || parsed > max) return null;
        return parsed;
    }

    function parseUint16(value, zeroOk) {
        var parsed = parseLong(value, zeroOk ? 0 : 1, 65535);
        if (parsed === null) return { err: E().PARAM_ERR_VALUE };
        return { err: E().PARAM_OK, value: parsed };
    }

    function parseUint8(value, zeroOk) {
        var parsed = parseLong(value, zeroOk ? 0 : 1, 255);
        if (parsed === null) return { err: E().PARAM_ERR_VALUE };
        return { err: E().PARAM_OK, value: parsed };
    }

    function parseBool(value) {
        var parsed = parseLong(value, 0, 1);
        if (parsed === null) return { err: E().PARAM_ERR_VALUE };
        return { err: E().PARAM_OK, value: parsed };
    }

    // Список снят с switch в is_valid_counter_type(): руками его тут нет
    function isValidCounterType(counterType) {
        return G().valid_counter_types.indexOf(counterType) >= 0;
    }

    function isWaterCounter(counterName) {
        var CN = G().enums.CounterName;
        return counterName === CN.WATER_COLD || counterName === CN.WATER_HOT ||
               counterName === CN.PORTABLE_WATER;
    }

    function hasDecimalSeparator(value) {
        return String(value).indexOf('.') >= 0 || String(value).indexOf(',') >= 0;
    }

    /* У водяного счётчика на шкале всегда есть литры: целое число - забытая запятая. */
    function checkReading(value, counterName) {
        if (isWaterCounter(counterName) && !hasDecimalSeparator(value)) {
            return E().PARAM_ERR_NO_COMMA;
        }
        return E().PARAM_OK;
    }

    function countsImpulses(counterType) {
        var CT = G().enums.CounterType;
        return counterType !== CT.NONE && counterType !== CT.LEAKAGE && counterType !== CT.LEAKAGE_NC;
    }

    /* ---- url.cpp: адрес брокера ---- */

    var PLAIN_SCHEMES = ['mqtt', 'tcp', 'http'];
    var ENCRYPTED_SCHEMES = ['mqtts', 'ssl', 'tls', 'wss', 'https'];

    function parseBrokerHost(value, size) {
        var text = String(value).replace(/^\s+/, '');
        var host = text;

        var separator = text.indexOf('://');
        if (separator >= 0) {
            var scheme = text.slice(0, separator).toLowerCase();
            if (ENCRYPTED_SCHEMES.indexOf(scheme) >= 0) return { err: E().PARAM_ERR_TLS };
            if (PLAIN_SCHEMES.indexOf(scheme) < 0) return { err: E().PARAM_ERR_VALUE };
            host = text.slice(separator + 3);
        }

        var path = host.indexOf('/');
        if (path >= 0) {
            if (byteLength(host.slice(0, path)) >= G().defines.HOST_LEN) return { err: E().PARAM_ERR_LENGTH };
            host = host.slice(0, path); // путь для MQTT поверх TCP ничего не значит
        }

        // Порт менять молча нельзя: подключились бы не туда
        if (host.indexOf(':') >= 0) return { err: E().PARAM_ERR_PORT_IN_HOST };

        return parseText(host, size, true);
    }

    /* ---- readings.cpp ---- */

    var IMPULS_LIMIT_1 = 3; // меньше трёх импульсов - 10 л/имп, больше - 1 л/имп

    function getAutoFactor(runtimeImpulses, impulses, factor, factorCold) {
        var D = G().defines;
        if (factor === D.AUTO_IMPULSE_FACTOR) return (runtimeImpulses - impulses <= IMPULS_LIMIT_1) ? 10 : 1;
        if (factor === D.AS_COLD_CHANNEL) return factorCold;
        return factor;
    }

    /* ---- types.h: вес импульса задан, а не остался спецзначением ---- */

    function factorConfigured(factor) {
        var D = G().defines;
        return factor > 0 && factor !== D.AUTO_IMPULSE_FACTOR && factor !== D.AS_COLD_CHANNEL;
    }

    /* ---- alarm.cpp: что страница тревог может показать на входе ---- */

    function alarmInputState(counterType, factor, attinyVersion) {
        var S = G().enums.AlarmInputState;
        if (!countsImpulses(counterType)) return S.ALARM_INPUT_NO_INPUT;
        if (attinyVersion < G().defines.ATTINY_VER_ALARM) return S.ALARM_INPUT_NO_ATTINY;
        if (!factorConfigured(factor)) return S.ALARM_INPUT_NO_FACTOR;
        return S.ALARM_INPUT_READY;
    }

    /* ---- diagnostics.cpp: плашки настройки счётчиков ---- */

    function consumedImpulses(impulses, start) {
        return impulses > start ? impulses - start : 0;
    }

    function consumedLiters(impulses, start, factor) {
        return consumedImpulses(impulses, start) * factor;
    }

    function factorTooBig(factor, factorOther, liters, litersOther) {
        var D = G().defines;
        if (factor !== factorOther * 10) return false;
        if (liters < D.COMPARE_MIN_LITERS || litersOther < D.COMPARE_MIN_LITERS) return false;
        return Math.floor(liters / litersOther) >= D.SUSPICIOUS_RATIO;
    }

    function silentInput(counterType, factor, impulses, start, neighbourImpulses) {
        var CT = G().enums.CounterType;
        var D = G().defines;
        if (counterType === CT.NONE || !factorConfigured(factor)) return false;
        if (consumedImpulses(impulses, start) > 0) return false;
        return neighbourImpulses >= D.NEIGHBOUR_MIN_IMPULSES;
    }

    /* data - снимок сеанса, с типами входов, обновлёнными из attiny */
    function checkSetup(sett, data) {
        var problems = { factor_too_big0: false, factor_too_big1: false,
                         silent_input0: false, silent_input1: false };

        var impulses0 = consumedImpulses(data.impulses0, sett.impulses0_start);
        var impulses1 = consumedImpulses(data.impulses1, sett.impulses1_start);

        problems.silent_input0 = silentInput(data.counter_type0, sett.factor0,
                                             data.impulses0, sett.impulses0_start, impulses1);
        problems.silent_input1 = silentInput(data.counter_type1, sett.factor1,
                                             data.impulses1, sett.impulses1_start, impulses0);

        // Расходы сравнимы только между водой
        if (!isWaterCounter(sett.counter0_name) || !isWaterCounter(sett.counter1_name)) return problems;
        if (!factorConfigured(sett.factor0) || !factorConfigured(sett.factor1)) return problems;

        var liters0 = consumedLiters(data.impulses0, sett.impulses0_start, sett.factor0);
        var liters1 = consumedLiters(data.impulses1, sett.impulses1_start, sett.factor1);

        problems.factor_too_big0 = factorTooBig(sett.factor0, sett.factor1, liters0, liters1);
        problems.factor_too_big1 = factorTooBig(sett.factor1, sett.factor0, liters1, liters0);

        return problems;
    }

    /* ---- wifi.cpp: быстрый коннект ---- */

    function parseWifiChannel(value) {
        var D = G().defines;
        var channel = parseInt(String(value), 10);
        if (isNaN(channel) || channel < D.WIFI_CHANNEL_MIN || channel > D.WIFI_CHANNEL_MAX) return 0;
        return channel;
    }

    /* Возвращает массив из шести байт или null: половина адреса хуже, чем ничего. */
    function parseBssid(value) {
        var digits = String(value === undefined || value === null ? '' : value).replace(/[:-]/g, '');
        if (!/^[0-9a-fA-F]{12}$/.test(digits)) return null;
        var bytes = [];
        for (var i = 0; i < 12; i += 2) bytes.push(parseInt(digits.substr(i, 2), 16));
        return bytes;
    }

    function hasBssid(bssid) {
        return !!bssid && bssid.some(function (byte) { return byte !== 0; });
    }

    var SimCore = {
        byteLength: byteLength,
        isAllAsterisks: isAllAsterisks,
        parseText: parseText,
        parseDecimal: parseDecimal,
        parseUint16: parseUint16,
        parseUint8: parseUint8,
        parseBool: parseBool,
        isValidCounterType: isValidCounterType,
        isWaterCounter: isWaterCounter,
        checkReading: checkReading,
        countsImpulses: countsImpulses,
        parseBrokerHost: parseBrokerHost,
        getAutoFactor: getAutoFactor,
        factorConfigured: factorConfigured,
        alarmInputState: alarmInputState,
        checkSetup: checkSetup,
        parseWifiChannel: parseWifiChannel,
        parseBssid: parseBssid,
        hasBssid: hasBssid,
    };

    root.SimCore = SimCore;
    if (typeof module !== 'undefined') module.exports = SimCore;
})(typeof self !== 'undefined' ? self : globalThis);
