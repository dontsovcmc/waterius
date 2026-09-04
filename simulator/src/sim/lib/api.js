/*
 * Обработчики /api/*. Порт ESP8266/src/portal/active_point_api.cpp.
 *
 * Слой-адаптер: распаковывает параметры запроса, зовёт ядро (core.js) и
 * раскладывает коды ошибок по полям JSON-ответа - ровно как в прошивке.
 * Каждая функция меняет state и возвращает {json}, {redirect} или {text}.
 */
(function (root) {
    'use strict';

    function G() { return root.SIM_GENERATED; }
    function P() { return root.SIM_GENERATED.params; }
    function E() { return root.SIM_GENERATED.enums.ParamError; }
    function C() { return root.SimCore; }
    function A() { return root.SIM_GENERATED.enums.AlarmConfirm; }

    var ERR_ATTINY = '16';
    var ERR_NO_LINK = '7';

    /* Порт report_param_error(): код ядра уезжает в JSON строкой. */
    function reportParamError(name, errors, err) {
        if (err === E().PARAM_OK || err === E().PARAM_MASKED) return;
        errors[name] = String(err);
    }

    /* ---- адаптеры save_param ---- */

    function saveText(p, state, field, size, errors, required) {
        var result = C().parseText(p.value, size, required !== false);
        if (result.err === E().PARAM_OK) state.sett[field] = result.value;
        else reportParamError(p.name, errors, result.err);
    }

    function saveBrokerHost(p, state, field, size, errors) {
        var result = C().parseBrokerHost(p.value, size);
        if (result.err === E().PARAM_OK) state.sett[field] = result.value;
        else reportParamError(p.name, errors, result.err);
    }

    function saveUint16(p, state, field, errors, zeroOk) {
        var result = C().parseUint16(p.value, !!zeroOk);
        if (result.err === E().PARAM_OK) state.sett[field] = result.value;
        else reportParamError(p.name, errors, result.err);
    }

    /*
     * Порог остановки потребления: часы, не больше ALARM_STOP_MAX_HOURS.
     * Выше счётчик простоя просто не досчитает - он ведётся в минутах.
     */
    function saveStopParam(p, state, field, errors) {
        var result = C().parseUint16(p.value, true); // ноль - выключено
        var err = result.err;
        if (err === E().PARAM_OK && result.value > G().defines.ALARM_STOP_MAX_HOURS) {
            err = E().PARAM_ERR_VALUE;
        }
        if (err === E().PARAM_OK) state.sett[field] = result.value;
        else reportParamError(p.name, errors, err);
    }

    function saveUint8(p, state, field, errors, zeroOk) {
        var result = C().parseUint8(p.value, !!zeroOk);
        if (result.err === E().PARAM_OK) state.sett[field] = result.value;
        else reportParamError(p.name, errors, result.err);
    }

    function saveBool(p, state, field, errors) {
        var result = C().parseBool(p.value);
        if (result.err === E().PARAM_OK) state.sett[field] = result.value;
        else reportParamError(p.name, errors, result.err);
    }

    /*
     * Бит маски "кому доклад о тревоге обязан доехать" (#202). Порт
     * save_confirm_bit(): при отвергнутом значении бит остаётся как был.
     */
    function saveConfirmBit(p, state, bit, errors) {
        var result = C().parseBool(p.value);
        if (result.err !== E().PARAM_OK) {
            reportParamError(p.name, errors, result.err);
            return;
        }
        if (result.value) state.sett.alarm_confirm |= bit;
        else state.sett.alarm_confirm &= ~bit;
    }

    /*
     * Показания. Записываются только при успехе: иначе после забытой запятой
     * уехали бы и стартовые импульсы, и показания не починились бы вводом
     * правильного числа. Поэтому, в отличие от остальных, возвращает результат.
     */
    function saveReading(p, state, field, errors, counterName) {
        var err = C().checkReading(p.value, counterName);
        if (err !== E().PARAM_OK) {
            reportParamError(p.name, errors, err);
            return false;
        }
        state.sett[field] = C().parseDecimal(p.value).value;
        return true;
    }

    /* Порт IPAddress::fromString() */
    function parseIp(text) {
        var parts = String(text).trim().split('.');
        if (parts.length !== 4) return null;
        var bytes = [];
        for (var i = 0; i < 4; i++) {
            if (!/^\d{1,3}$/.test(parts[i])) return null;
            if (Number(parts[i]) > 255) return null;
            bytes.push(Number(parts[i]));
        }
        return ((bytes[3] << 24) | (bytes[2] << 16) | (bytes[1] << 8) | bytes[0]) >>> 0;
    }

    function saveIp(p, state, field, errors) {
        var value = parseIp(p.value);
        if (value === null) errors[p.name] = String(E().PARAM_ERR_VALUE);
        else state.sett[field] = value;
    }

    /* ---- вспомогательное ---- */

    function findParam(params, name) {
        for (var i = 0; i < params.length; i++) {
            if (params[i].name === name) return params[i];
        }
        return null;
    }

    function findWizard(params) {
        var p = findParam(params, P().PARAM_WIZARD);
        return !!p && p.value === P().PARAM_TRUE;
    }

    function getParamUint8(params, name) {
        var p = findParam(params, name);
        return p ? (parseInt(p.value, 10) || 0) : 0xFF;
    }

    function coldFactor(state) {
        return C().getAutoFactor(state.attiny.impulses1, state.session.impulses1,
                                 state.sett.factor1, state.sett.factor1);
    }

    /* Сброс кэша быстрого коннекта: канал и BSSID от прошлого роутера. */
    function forgetFastConnect(state) {
        state.sett.wifi_channel = 0;
        state.wifi.bssid = [0, 0, 0, 0, 0, 0];
    }

    /*
     * Канал и BSSID выбранной сети из скрытых полей формы: зная пару, ЕСП
     * подключается без полного скана эфира. Зовётся после applySettings, а не
     * из его цепочки: сохранение SSID сбрасывает кэш коннекта.
     */
    function saveFastConnect(params, state) {
        var channelParam = findParam(params, P().PARAM_WIFI_CHANNEL);
        var bssidParam = findParam(params, P().PARAM_BSSID);
        if (!channelParam || !bssidParam) return false;

        var channel = C().parseWifiChannel(channelParam.value);
        var bssid = C().parseBssid(bssidParam.value);
        if (!channel || !bssid || !C().hasBssid(bssid)) return false;

        state.sett.wifi_channel = channel;
        state.wifi.bssid = bssid;
        return true;
    }

    /* ---- applyInputParameter ---- */

    function applyInputParameter(p, errors, input, state) {
        var name = p.name;
        var D = G().defines;
        var sett = state.sett;

        if (name === P().PARAM_CHANNEL_START || name === P().s_ch) {
            if (input === 0 && saveReading(p, state, 'channel0_start', errors, sett.counter0_name)) {
                sett.impulses0_start = state.attiny.impulses0;
                sett.impulses0_previous = sett.impulses0_start;
            } else if (input === 1 && saveReading(p, state, 'channel1_start', errors, sett.counter1_name)) {
                sett.impulses1_start = state.attiny.impulses1;
                sett.impulses1_previous = sett.impulses1_start;
            }
        } else if (name === P().PARAM_SERIAL) {
            if (input === 0) saveText(p, state, 'serial0', D.SERIAL_LEN, errors, false);
            else if (input === 1) saveText(p, state, 'serial1', D.SERIAL_LEN, errors, false);
        } else if (name === P().PARAM_COUNTER_NAME || name === P().s_cname) {
            if (input === 0) saveUint8(p, state, 'counter0_name', errors, true);
            else if (input === 1) saveUint8(p, state, 'counter1_name', errors, true);
        } else if (name === P().PARAM_COUNTER_TYPE || name === P().s_ctype) {
            if (input === 0 || input === 1) {
                if (!state.attiny.link) {
                    errors[p.name] = ERR_ATTINY; // masterI2C.setCountersType не ответил
                } else {
                    state.attiny['counter_type' + input] = parseInt(p.value, 10) || 0;
                }
            }
        } else if (name === P().PARAM_ALARM_FLOW || name === P().s_af) {
            // Порог расхода: л/ч для объёма, Вт для электричества. 0 - выключено
            if (input === 0) saveUint16(p, state, 'alarm_flow0', errors, true);
            else if (input === 1) saveUint16(p, state, 'alarm_flow1', errors, true);
        } else if (name === P().PARAM_ALARM_LEAK || name === P().s_al) {
            if (input === 0) saveUint16(p, state, 'alarm_leak0', errors, true);
            else if (input === 1) saveUint16(p, state, 'alarm_leak1', errors, true);
        } else if (name === P().PARAM_ALARM_STOP || name === P().s_as) {
            if (input === 0) saveStopParam(p, state, 'alarm_stop0', errors);
            else if (input === 1) saveStopParam(p, state, 'alarm_stop1', errors);
        } else if (name === P().PARAM_FACTOR || name === P().s_f) {
            var value = parseInt(p.value, 10) || 0;
            if (value === D.AUTO_IMPULSE_FACTOR || value === D.AS_COLD_CHANNEL) {
                var cold = coldFactor(state);
                if (input === 0) {
                    sett.factor0 = C().getAutoFactor(state.attiny.impulses0, state.session.impulses0, sett.factor0, cold);
                } else if (input === 1) {
                    sett.factor1 = cold;
                }
            } else if (input === 0) {
                saveUint16(p, state, 'factor0', errors);
            } else if (input === 1) {
                saveUint16(p, state, 'factor1', errors);
            }
        }
    }

    function applyInputSettings(params, errors, input, state) {
        params.forEach(function (p) { applyInputParameter(p, errors, input, state); });
    }

    /* ---- applySettings ---- */

    function applyCheckBoxParameter(p, errors, state) {
        var name = p.name;
        if (name === P().PARAM_WATERIUS_ON) saveBool(p, state, 'waterius_on', errors);
        else if (name === P().PARAM_HTTP_ON) saveBool(p, state, 'http_on', errors);
        else if (name === P().PARAM_MQTT_ON) saveBool(p, state, 'mqtt_on', errors);
        else if (name === P().PARAM_DHCP_OFF) saveBool(p, state, 'dhcp_off', errors);
        else if (name === P().PARAM_MQTT_AUTO_DISCOVERY) saveBool(p, state, 'mqtt_auto_discovery', errors);
        else if (name === P().PARAM_MQTT_RETAIN) saveBool(p, state, 'mqtt_retain', errors);
        else if (name === P().PARAM_SEND_ON_CONSUMPTION || name === P().s_sc) saveBool(p, state, 'send_on_consumption', errors);
        else if (name === P().PARAM_VACATION || name === P().s_vac) saveBool(p, state, 'vacation', errors);
        else if (name === P().PARAM_CONFIRM_WATERIUS || name === P().s_ackw) saveConfirmBit(p, state, A().CONFIRM_WATERIUS, errors);
        else if (name === P().PARAM_CONFIRM_HTTP || name === P().s_ackh) saveConfirmBit(p, state, A().CONFIRM_HTTP, errors);
        else if (name === P().PARAM_CONFIRM_MQTT || name === P().s_ackm) saveConfirmBit(p, state, A().CONFIRM_MQTT, errors);
    }

    function applyNonCheckBoxParameter(p, errors, state) {
        var name = p.name;
        var D = G().defines;
        var sett = state.sett;

        if (sett.waterius_on) {
            if (name === P().PARAM_WATERIUS_HOST) saveText(p, state, 'waterius_host', D.HOST_LEN, errors);
            else if (name === P().PARAM_WATERIUS_EMAIL) saveText(p, state, 'waterius_email', D.EMAIL_LEN, errors);
        }
        if (sett.http_on) {
            if (name === P().PARAM_HTTP_URL) saveText(p, state, 'http_url', D.HOST_LEN, errors);
        }
        if (sett.mqtt_on) {
            if (name === P().PARAM_MQTT_HOST) saveBrokerHost(p, state, 'mqtt_host', D.HOST_LEN, errors);
            else if (name === P().PARAM_MQTT_PORT) saveUint16(p, state, 'mqtt_port', errors);
            else if (name === P().PARAM_MQTT_LOGIN) saveText(p, state, 'mqtt_login', D.MQTT_LOGIN_LEN, errors, false);
            else if (name === P().PARAM_MQTT_PASSWORD) saveText(p, state, 'mqtt_password', D.MQTT_PASSWORD_LEN, errors, false);
            else if (name === P().PARAM_MQTT_TOPIC) saveText(p, state, 'mqtt_topic', D.MQTT_TOPIC_LEN, errors, false);

            if (sett.mqtt_auto_discovery) {
                if (name === P().PARAM_MQTT_DISCOVERY_TOPIC) saveText(p, state, 'mqtt_discovery_topic', D.MQTT_TOPIC_LEN, errors, false);
            }
        }
        if (sett.dhcp_off) {
            if (name === P().PARAM_IP) saveIp(p, state, 'ip', errors);
            else if (name === P().PARAM_GATEWAY) saveIp(p, state, 'gateway', errors);
            else if (name === P().PARAM_MASK) saveIp(p, state, 'mask', errors);
        }

        if (name === P().s_period_min) {
            saveUint16(p, state, 'wakeup_per_min', errors);
            sett.period_min_tuned = sett.wakeup_per_min; // reset_period_min_tuned()
        } else if (name === P().s_voltage_cal) {
            saveUint8(p, state, 'voltage_cal', errors);
        } else if (name === P().PARAM_NTP_SERVER) {
            saveText(p, state, 'ntp_server', D.HOST_LEN, errors);
        } else if (name === P().PARAM_SSID) {
            saveText(p, state, 'wifi_ssid', D.WIFI_SSID_LEN, errors);
            forgetFastConnect(state); // сеть сменилась - канал и BSSID больше не годятся
        } else if (name === P().PARAM_PASSWORD) {
            saveText(p, state, 'wifi_password', D.WIFI_PWD_LEN, errors, false);
            forgetFastConnect(state);
        } else if (name === P().PARAM_WIFI_PHY_MODE) {
            saveUint8(p, state, 'wifi_phy_mode', errors, true);
        } else if (name === P().PARAM_COMPANY) {
            saveText(p, state, 'company', D.COMPANY_LEN, errors, false);
        } else if (name === P().PARAM_PLACE) {
            saveText(p, state, 'place', D.PLACE_LEN, errors, false);
        }
    }

    function applySettings(params, errors, state) {
        // Вначале галочки: дальше проверяются только включённые разделы
        params.forEach(function (p) { applyCheckBoxParameter(p, errors, state); });
        params.forEach(function (p) { applyNonCheckBoxParameter(p, errors, state); });
    }

    /* Имена параметров, которые обработчики сохранения умеют принимать. Проверяется тестом. */
    function savedParams() {
        var p = P();
        return [
            p.PARAM_WATERIUS_ON, p.PARAM_HTTP_ON, p.PARAM_MQTT_ON, p.PARAM_DHCP_OFF,
            p.PARAM_MQTT_AUTO_DISCOVERY, p.PARAM_MQTT_RETAIN, p.PARAM_SEND_ON_CONSUMPTION,
            p.PARAM_VACATION, p.s_sc, p.s_vac,
            p.PARAM_CONFIRM_WATERIUS, p.PARAM_CONFIRM_HTTP, p.PARAM_CONFIRM_MQTT,
            p.s_ackw, p.s_ackh, p.s_ackm,
            p.PARAM_WATERIUS_HOST, p.PARAM_WATERIUS_EMAIL, p.PARAM_HTTP_URL,
            p.PARAM_MQTT_HOST, p.PARAM_MQTT_PORT, p.PARAM_MQTT_LOGIN, p.PARAM_MQTT_PASSWORD,
            p.PARAM_MQTT_TOPIC, p.PARAM_MQTT_DISCOVERY_TOPIC,
            p.PARAM_IP, p.PARAM_GATEWAY, p.PARAM_MASK, p.s_period_min, p.s_voltage_cal,
            p.PARAM_NTP_SERVER, p.PARAM_SSID, p.PARAM_PASSWORD, p.PARAM_WIFI_PHY_MODE,
            p.PARAM_COMPANY, p.PARAM_PLACE,
            p.PARAM_CHANNEL_START, p.s_ch, p.PARAM_SERIAL, p.PARAM_COUNTER_NAME, p.s_cname,
            p.PARAM_COUNTER_TYPE, p.s_ctype, p.PARAM_FACTOR, p.s_f,
            p.PARAM_ALARM_FLOW, p.PARAM_ALARM_LEAK, p.PARAM_ALARM_STOP, p.s_af, p.s_al, p.s_as,
            p.PARAM_ALARM_FLOW0, p.PARAM_ALARM_FLOW1, p.PARAM_ALARM_LEAK0, p.PARAM_ALARM_LEAK1,
            p.PARAM_ALARM_STOP0, p.PARAM_ALARM_STOP1,
            // Пара быстрого коннекта разбирается вне общей цепочки, в save_fast_connect
            p.PARAM_WIFI_CHANNEL, p.PARAM_BSSID,
        ].filter(Boolean);
    }

    /* ---- ручки ---- */

    function getConnectStatus(state, now) {
        var WL = root.SimState.WL;
        if (state.wifi.connecting) {
            if (now - state.wifi.connect_started < root.SimState.CONNECT_MS) return { json: {} };
            state.wifi.status = root.SimState.OUTCOMES[state.wifi.outcome];
            state.wifi.connecting = false;
        }
        return { json: {
            redirect: state.wifi.status === WL.CONNECTED ? '/input/1/setup.html' : '/wifi_settings.html',
        } };
    }

    function getNetworks(state) {
        return { json: state.wifi.networks.map(function (net) {
            return { ssid: net.ssid, level: net.level, wifi_channel: net.wifi_channel, bssid: net.bssid };
        }) };
    }

    function saveConnect(params, state) {
        var errors = {};
        var channel = state.sett.wifi_channel;
        applySettings(params, errors, state);
        var wizard = findWizard(params);

        if (Object.keys(errors).length) return { json: { errors: errors } };

        saveFastConnect(params, state);

        var changed = channel !== state.sett.wifi_channel;
        var url = '/api/start_connect';
        if (changed && wizard) url += '?wizard=true&error=0';
        else if (changed) url += '?error=0';
        else if (wizard) url += '?wizard=true';
        return { json: { redirect: url } };
    }

    function startConnect(params, state, now) {
        state.wifi.connecting = true;
        state.wifi.connect_started = now;
        state.wifi.status = root.SimState.WL.DISCONNECTED;
        return { redirect: findWizard(params) ? '/wifi_connect.html?wizard=true' : '/wifi_connect.html' };
    }

    function inputProblem(error, input, page) {
        return { error: error, input: input, link_text: '5', link: '/input/' + input + '/' + page + '.html' };
    }

    function mainStatus(state) {
        var WL = root.SimState.WL;
        var D = G().defines;
        var list = [];

        if (state.portal.esp_restarted) list.push({ error: '22' }); // #354

        // Тип входа мог смениться в этом сеансе (#360)
        state.session.counter_type0 = state.attiny.counter_type0;
        state.session.counter_type1 = state.attiny.counter_type1;

        var problems = C().checkSetup(state.sett, state.session);
        if (problems.factor_too_big0) list.push(inputProblem('23', 0, 'settings'));
        if (problems.factor_too_big1) list.push(inputProblem('23', 1, 'settings'));
        if (problems.silent_input0) list.push(inputProblem('24', 0, 'setup'));
        if (problems.silent_input1) list.push(inputProblem('24', 1, 'setup'));

        var status = state.wifi.status;
        if (status === WL.CONNECT_FAILED || status === WL.CONNECTION_LOST || status === WL.WRONG_PASSWORD) {
            list.push({ error: '1', link_text: '5', link: '/wifi_settings.html?status_code=' + status });
        } else if (state.sett.factor1 === D.AUTO_IMPULSE_FACTOR) {
            if (status === WL.CONNECTED) list.push({ error: '2', link_text: '5', link: '/input/1/setup.html' });
            else list.push({ error: '3', link_text: '6', link: '/captive_portal_start.html' });
        }
        return { json: list };
    }

    function inputStatus(state, index) {
        if (!state.attiny.link) return { json: { error: ERR_NO_LINK } };

        var cold = coldFactor(state);
        if (index === 0) {
            return { json: {
                state: state.attiny.impulses0 > state.session.impulses0 ? 1 : 0,
                factor: C().getAutoFactor(state.attiny.impulses0, state.session.impulses0, state.sett.factor0, cold),
                impulses: state.attiny.impulses0 - state.session.impulses0,
            } };
        }
        return { json: {
            state: state.attiny.impulses1 > state.session.impulses1 ? 1 : 0,
            factor: cold,
            impulses: state.attiny.impulses1 - state.session.impulses1,
        } };
    }

    function save(params, state) {
        var errors = {};
        applySettings(params, errors, state);
        applyInputSettings(params, errors, getParamUint8(params, P().PARAM_INPUT), state);
        return { json: { errors: errors } };
    }

    /*
     * Пороги тревог (#202): страница одна на оба входа, поэтому параметры
     * именные. Чего нет в запросе, того не трогаем.
     */
    function saveAlarms(params, state) {
        var errors = {};
        var optional = function (name, field, saver) {
            var p = findParam(params, name);
            if (p) saver(p, state, field, errors, true);
        };

        optional(P().PARAM_ALARM_FLOW0, 'alarm_flow0', saveUint16);
        optional(P().PARAM_ALARM_LEAK0, 'alarm_leak0', saveUint16);
        optional(P().PARAM_ALARM_FLOW1, 'alarm_flow1', saveUint16);
        optional(P().PARAM_ALARM_LEAK1, 'alarm_leak1', saveUint16);
        optional(P().PARAM_ALARM_STOP0, 'alarm_stop0', saveStopParam);
        optional(P().PARAM_ALARM_STOP1, 'alarm_stop1', saveStopParam);

        var vacation = findParam(params, P().PARAM_VACATION);
        if (vacation) saveBool(vacation, state, 'vacation', errors);

        // Спрятанной галочки в запросе нет - бит остаётся как был
        var ack = [
            [P().PARAM_CONFIRM_WATERIUS, A().CONFIRM_WATERIUS],
            [P().PARAM_CONFIRM_HTTP, A().CONFIRM_HTTP],
            [P().PARAM_CONFIRM_MQTT, A().CONFIRM_MQTT]
        ];
        ack.forEach(function (pair) {
            var p = findParam(params, pair[0]);
            if (p) saveConfirmBit(p, state, pair[1], errors);
        });

        var json = { errors: errors };
        if (!Object.keys(errors).length) json.redirect = '/index.html';
        return { json: json };
    }

    function saveInputType(params, state) {
        var errors = {};
        var CN = G().enums.CounterName;
        var CT = G().enums.CounterType;
        var D = G().defines;
        var input = getParamUint8(params, P().PARAM_INPUT);

        applyInputSettings(params, errors, input, state);

        var redirect = null;
        var type = state.attiny['counter_type' + input];
        if (input === 0 || input === 1) {
            var name = input === 0 ? state.sett.counter0_name : state.sett.counter1_name;
            var factor = input === 0 ? state.sett.factor0 : state.sett.factor1;
            var firstSetup = input === 0 ? D.AS_COLD_CHANNEL : D.AUTO_IMPULSE_FACTOR;

            // Датчик протечки - не счётчик: ни веса импульса, ни показаний (#202)
            if (type === CT.LEAKAGE || type === CT.LEAKAGE_NC) redirect = '/index.html';
            else if (name === CN.ELECTRO) redirect = '/input/' + input + '/input_electro_detect.html';
            else if (type === CT.NONE) redirect = '/index.html';
            else if (factor === firstSetup) redirect = '/input/' + input + '/detect.html';
            else redirect = '/input/' + input + '/settings.html';
        }

        if (redirect && findWizard(params)) redirect += '?wizard=true';

        var json = { errors: errors };
        if (redirect) json.redirect = redirect;
        return { json: json };
    }

    function turnoff(state) {
        state.portal.exited = true;
        return { text: '' };
    }

    function factoryReset(state) {
        var fresh = root.SimState.defaultState();
        state.sett = fresh.sett;
        state.attiny.counter_type0 = fresh.attiny.counter_type0;
        state.attiny.counter_type1 = fresh.attiny.counter_type1;
        return { json: { redirect: '/' } };
    }

    /* now - Date.now(), чтобы обработчики оставались чистыми функциями. */
    function handle(pathname, method, params, state, now) {
        switch (pathname) {
            case '/api/networks': return getNetworks(state);
            case '/api/save_connect': return saveConnect(params, state);
            case '/api/start_connect': return startConnect(params, state, now);
            case '/api/connect_status': return getConnectStatus(state, now);
            case '/api/save': return save(params, state);
            case '/api/save_alarms': return saveAlarms(params, state);
            case '/api/save_input_type': return saveInputType(params, state);
            case '/api/main_status': return mainStatus(state);
            case '/api/status/0': return inputStatus(state, 0);
            case '/api/status/1': return inputStatus(state, 1);
            case '/api/turnoff': return turnoff(state);
            case '/api/reset': return factoryReset(state);
            default: return null;
        }
    }

    function routes() {
        return ['/api/networks', '/api/save_connect', '/api/start_connect', '/api/connect_status',
                '/api/save', '/api/save_alarms', '/api/save_input_type', '/api/main_status',
                '/api/status/0', '/api/status/1', '/api/turnoff', '/api/reset'];
    }

    var SimApi = {
        handle: handle,
        routes: routes,
        savedParams: savedParams,
        applySettings: applySettings,
        applyInputSettings: applyInputSettings,
        parseIp: parseIp,
    };

    root.SimApi = SimApi;
    if (typeof module !== 'undefined') module.exports = SimApi;
})(typeof self !== 'undefined' ? self : globalThis);
