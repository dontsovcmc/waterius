// Второй язык это копия этого файла

// Целой фразой: в другом языке предлог и порядок слов свои
const INPUT_PLACE = ["на красном входе", "на синем входе"];

function fill_input_color(index) {
    var q = document.getElementById('input_color');
    if (index == 0) {
        q.innerHTML = 'Красный вход';
    } else if (index == 1) {
        q.innerHTML = 'Синий вход';
    }
}

const U_M3 = "м³";
const U_L = "л";
const U_KWH = "кВт·ч";
const U_GCAL = "Гкал";
const U_IMPULSE = "имп.";
const U_NONE = "";          // не показывать

const DECIMAL_POINT = ",";
const UNIT_SEPARATOR = ", ";

const CounterName_WATER_COLD = 0;
const CounterName_WATER_HOT = 1;
const CounterName_ELECTRO = 2;
const CounterName_GAS = 3;
const CounterName_HEAT_GCAL = 4;
const CounterName_PORTABLE_WATER = 5;
const CounterName_OTHER = 6;
const CounterName_HEAT_KWH = 7;

// 1, 3 и 4 сняты, числа заняты
const CounterType_NAMUR = 0;
const CounterType_ELECTRONIC = 2;
const CounterType_LEAKAGE = 5;
const CounterType_SIGNAL_NC = 6;
const CounterType_NONE = 0xFF;

// core/types.h
const AUTO_IMPULSE_FACTOR = 3;
const AS_COLD_CHANNEL = 7;

// Поля в docs/setup-portal.md, числа как в core/readings.cpp
const HEAT_INSTRUCTION = "Приход импульса от&nbspсчётчика тепла долго ожидать, нажмите Пропустить и&nbspпродолжите настройку.";

const RESOURCES = {};

RESOURCES[CounterName_WATER_COLD] = {
    title: "Холодная&nbspвода",
    option: "Холодную воду",
    instruction: "Спускайте воду в&nbspунитазе пока устройство не&nbspперенесёт вас на&nbspследующую страницу",
    unit: U_M3, pulse: U_L, pulse_div: 1, delta: U_L, delta_div: 1
};
RESOURCES[CounterName_WATER_HOT] = {
    title: "Горячая&nbspвода",
    option: "Горячую воду",
    instruction: "Откройте кран горячей воды пока устройство не&nbspперенесёт вас на&nbspследующую страницу",
    unit: U_M3, pulse: U_L, pulse_div: 1, delta: U_L, delta_div: 1
};
RESOURCES[CounterName_PORTABLE_WATER] = {
    title: "Питьевая&nbspвода",
    option: "Питьевую воду",
    instruction: "Откройте кран питьевой воды пока устройство не&nbspперенесёт вас на&nbspследующую страницу",
    unit: U_M3, pulse: U_L, pulse_div: 1, delta: U_L, delta_div: 1
};
RESOURCES[CounterName_ELECTRO] = {
    title: "Электричество",
    option: "Электричество",
    instruction: "Включите электроприбор. После моргания светодиода должна открыться следующая страница. Если не&nbspоткрывается, значит некорректное подключение или&nbspсчётчик не&nbspподдерживается.",
    unit: U_KWH, pulse: U_KWH, pulse_div: 1, delta: U_KWH, delta_div: 1,
    per_unit: true
};
// В прошивку вес уходит тысячными
RESOURCES[CounterName_GAS] = {
    title: "Газ",
    option: "Газ",
    instruction: "Приход импульса от&nbspгазового счётчика долго ожидать, нажмите Пропустить и&nbspпродолжите настройку.",
    unit: U_M3, pulse: U_M3, pulse_div: 1000, delta: U_M3, delta_div: 1000
};
RESOURCES[CounterName_HEAT_GCAL] = {
    title: "Тепло, Гкал",
    option: "Тепло (Гкал)",
    instruction: HEAT_INSTRUCTION,
    unit: U_GCAL, pulse: U_GCAL, pulse_div: 1000, delta: U_GCAL, delta_div: 1000
};
RESOURCES[CounterName_HEAT_KWH] = {
    title: "Тепло, кВт·ч",
    option: "Тепло (кВт·ч)",
    instruction: HEAT_INSTRUCTION,
    unit: U_KWH, pulse: U_KWH, pulse_div: 1000, delta: U_KWH, delta_div: 1000
};
RESOURCES[CounterName_OTHER] = {
    title: "Другой",
    option: "Другой",
    instruction: "При приходе импульса от&nbspсчётчика устройство перенесёт вас на&nbspследующую страницу",
    unit: U_NONE, pulse: U_NONE, pulse_div: 1000, delta: U_NONE, delta_div: 1000
};

const COUNTER_TYPES = {};
COUNTER_TYPES[CounterType_NAMUR] = "Механический (Геркон, Намур)";
COUNTER_TYPES[CounterType_ELECTRONIC] = "Электронный";
COUNTER_TYPES[CounterType_LEAKAGE] = "Протечка (нормально-разомкнутый)";
COUNTER_TYPES[CounterType_SIGNAL_NC] = "Сигнализация (нормально-замкнутый)";
COUNTER_TYPES[CounterType_NONE] = "Выключен";

const S_COUNTER_DISABLED = "Отключён";

// Чужое число не должно ронять страницу
function resource(counter_name) {
    var r = RESOURCES[Number(counter_name)];
    return r ? r : RESOURCES[CounterName_OTHER];
}

function fill_title(q, counter_name)
{
    q.innerHTML = resource(counter_name).title;
}

function fill_counter_title(counter_name)
{
    var qlist = document.querySelectorAll('[id^="counter_title"]');
    qlist.forEach(function(q) {
        fill_title(q, counter_name);
    });
}

function fill_counter0_title(counter_name, counter_type)
{
    fill_counter_n_title('counter0_title', counter_name, counter_type);
}

function fill_counter1_title(counter_name, counter_type)
{
    fill_counter_n_title('counter1_title', counter_name, counter_type);
}

function fill_counter_n_title(id, counter_name, counter_type)
{
    var q = document.getElementById(id);
    if (counter_type == CounterType_NONE) {
        q.innerHTML = S_COUNTER_DISABLED;
    } else {
        fill_title(q, counter_name);
    }
}

function fill_instruction(counter_name) {
    document.getElementById('counter_instruction').innerHTML = resource(counter_name).instruction;
}

function fill_resource_options() {
    var names = document.getElementById('counter_name');
    if (names) {
        names.querySelectorAll('option').forEach(function(o) {
            o.textContent = resource(o.value).option;
        });
    }
    var types = document.getElementById('counter_type');
    if (types) {
        types.querySelectorAll('option').forEach(function(o) {
            if (o.disabled) {
                o.remove();   // этот тип attiny не знает
                return;
            }
            var name = COUNTER_TYPES[Number(o.value)];
            if (name) o.textContent = name;
        });
    }
}

function format_number(value) {
    var s = Number(value).toFixed(3);
    if (s.indexOf(".") >= 0) {
        s = s.replace(/0+$/, "").replace(/\.$/, "");
    }
    return s.replace(".", DECIMAL_POINT);
}

function unit_of(counter_name) {
    return resource(counter_name).unit;
}

// Без размерности пусто: иначе висячая запятая
function unit_suffix(counter_name) {
    var u = unit_of(counter_name);
    return u ? UNIT_SEPARATOR + u : U_NONE;
}

function pulse_weight_text(counter_name, value) {
    var r = resource(counter_name);
    var num = format_number(value / r.pulse_div);
    return r.pulse ? num + " " + r.pulse : num;
}

function delta_text(counter_name, impulses, factor) {
    var r = resource(counter_name);
    if (!r.delta || !(factor > 0) || !(impulses > 0)) {
        return U_NONE;
    }
    var value = r.per_unit ? impulses / factor : impulses * factor / r.delta_div;
    return " = " + format_number(value) + " " + r.delta;
}

// Спецзначения разворачивает прошивка
function effective_factor(counter_name, form_value, api_factor) {
    var v = Number(form_value);
    if (resource(counter_name).per_unit) {
        return v > 0 ? v : Number(api_factor);
    }
    if (v > 0 && v != AUTO_IMPULSE_FACTOR && v != AS_COLD_CHANNEL) {
        return v;
    }
    return Number(api_factor);
}

function fill_units(counter_name) {
    document.querySelectorAll('[data-unit]').forEach(function(q) {
        var name = q.dataset.name !== undefined ? q.dataset.name : counter_name;
        switch (q.dataset.unit) {
            case 'total':
                q.textContent = unit_suffix(name);
                break;
            case 'unit':
                q.textContent = unit_of(name);
                break;
            case 'impulses':
                q.textContent = U_IMPULSE;
                break;
        }
    });
    fill_factor_options(counter_name);
}

// Значения не трогаем: прошивка ждёт те же числа
function fill_factor_options(counter_name) {
    var q = document.getElementById('factor');
    if (!q || q.tagName != 'SELECT') {
        return;
    }
    q.querySelectorAll('option').forEach(function(o) {
        var v = Number(o.value);
        if (v == AUTO_IMPULSE_FACTOR || v == AS_COLD_CHANNEL) {
            return;
        }
        o.textContent = pulse_weight_text(counter_name, v);
    });
}

const S_ANOTHER_CHANNEL = 0;
const S_WIFI_CONNECT = 1;
const S_SETUP_COUNTERS = 2;
const S_NEED_SETUP = 3;
const S_CONNECTING = 4;
const S_SETUP = 5;
const S_LETSGO = 6;
const S_NO_LINK = 7;
const S_WL_CONNECTION_LOST = 8;
const S_WL_WRONG_PASSWORD = 9;
const S_WL_IDLE_STATUS = 10;
const S_WL_DISCONNECTED = 11;
const S_WL_NO_SHIELD = 12;
const S_WL_SCAN_COMPLETED = 13;
const S_ERROR_LENGTH_ERROR = 14;
const S_ERROR_VALUE = 15;
const S_ERROR_ATTINY_ERROR = 16;
const S_ERROR_EMPTY = 17;
const S_PLEASE_RECONNECT_WIFI = 18;
const S_ERROR_NO_COMMA = 19;
const S_ERROR_TLS = 20;
const S_ERROR_PORT_IN_HOST = 21;
const S_ESP_RESTARTED = 22;
const S_FACTOR_TOO_BIG = 23;
const S_INPUT_SILENT = 24;
const S_LOST_LINK_TITLE = 25;
const S_RETRY = 26;
const S_ALARM_FLOW = 27;
const S_ALARM_LEAK = 28;
const S_ALARM_WET = 29;
const S_ALARM_STOP = 30;
const S_ALARM_CLEAR = 31;
const S_ALARM_SIGNAL = 32;

// core/types.h
const ALARM_RESET_ALL = 63;

function tr(str_id, arg) {
    var text = tr_text(Number(str_id));
    return (arg === undefined) ? text : text.replace("%s", arg);
}

function tr_text(id) {
    switch (id) {
        case S_ANOTHER_CHANNEL: return "Канал Wi-Fi роутера отличается от текущего соединения. Если телефон потеряет связь с Ватериусом, подключитесь заново.";
        case S_WIFI_CONNECT: return "Ошибка подключения к Wi-Fi";
        case S_SETUP_COUNTERS: return "Ватериус успешно подключился к Wi-Fi. Теперь настроим счётчики.";
        case S_NEED_SETUP: return "Ватериус ещё не настроен";
        case S_CONNECTING: return "выполняется подключение...";
        case S_SETUP: return "Настроить";
        case S_LETSGO: return "Приступить";
        case S_NO_LINK: return "Ошибка связи с МК";
        case S_WL_CONNECTION_LOST: return "Ошибка подключения. Что может помочь:<br>1) Проверьте, что Wi-Fi не Only N, шифрование WPA, WPA2.<br>2) Выключите DHCP на этой странице, заполнив ip роутера и Ватериуса.<br>3) Пропишите MAC адрес Ватериуса в роутере.<br>4) Пришлите нам <a class='link' download='waterius_log.txt' href='http://192.168.4.1/api/log'>журнал</a> подключения.";
        case S_WL_WRONG_PASSWORD: return "Ошибка подключения: Некорректный пароль";
        case S_WL_IDLE_STATUS : return "";  // не ошибка
        case S_WL_DISCONNECTED: return "";  // не ошибка
        case S_WL_NO_SHIELD: return "Ошибка подключения: Код 255";
        case S_WL_SCAN_COMPLETED: return "Ошибка подключения: Код 2";
        case S_ERROR_LENGTH_ERROR: return "Превышена длина поля";
        case S_ERROR_VALUE: return "Неверное значение";
        case S_ERROR_ATTINY_ERROR: return "Ошибка связи с attiny";
        case S_ERROR_EMPTY: return "Значение не может быть пустым";
        case S_PLEASE_RECONNECT_WIFI: return "Подключитесь к Wi-Fi сети waterius-xxxx заново. Если её нет, а лампочка погасла, то режим настройки закончился: нажмите кнопку на 5 сек ещё раз.";
        case S_ERROR_NO_COMMA: return "Показания воды вводятся с литрами: 123.456. Целое число обычно означает забытую запятую";
        case S_ERROR_TLS: return "Шифрованное подключение к MQTT не поддерживается. Укажите адрес без mqtts:// и ssl://";
        case S_ERROR_PORT_IN_HOST: return "Порт указывайте в отдельном поле, а не в адресе";
        case S_ESP_RESTARTED: return "Ватериус внештатно перезагрузился. Возможно, батарейки садятся.";
        case S_FACTOR_TOO_BIG: return "Счётчик %s насчитал в разы больше второго. Похоже, вес импульса завышен в 10 раз: проверьте, сколько литров на импульс написано на счётчике.";
        case S_INPUT_SILENT: return "Счётчик %s не насчитал ни одного импульса. Проверьте подключение и тип входа.";
        case S_LOST_LINK_TITLE: return "Нет связи с Ватериусом";
        case S_RETRY: return "Повторить";
        case S_ALARM_FLOW: return "Много воды сразу %s: за полчаса вылилось больше порога. Проверьте краны и шланги.";
        case S_ALARM_LEAK: return "Протечка %s: расход не опускался до нуля дольше заданного времени. Проверьте бачок унитаза и краны.";
        case S_ALARM_WET: return "Обнаружена протечка %s. Уберите влагу с датчика и снимите тревогу.";
        case S_ALARM_STOP: return "Расхода нет %s дольше заданного времени. Проверьте, не перекрыта ли вода и на месте ли провод счётчика. Снимется сама при первом расходе.";
        case S_ALARM_CLEAR: return "Снять тревогу";
        case S_ALARM_SIGNAL: return "Сработала сигнализация %s. Проверьте провод и снимите тревогу.";
        default:
            return "Незвестный id строки: " + String(id);
    }
}

function fill_tr_id(status, id) {
    var q = document.getElementById(id);
    // Пустой код - подключено, объяснять нечего: Number('') дал бы строку 0
    q.innerHTML = (status === '' || status === undefined) ? '' : tr(status);
}
