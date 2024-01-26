
function fill_input_color(index) {
    var q = document.getElementById('input_color');
    if (index == 0) {
        q.innerHTML = 'Красный вход';
    } else if (index == 1) {
        q.innerHTML = 'Синий вход';
    }
}

const CounterName_WATER_COLD = 0;
const CounterName_WATER_HOT = 1;
const CounterName_ELECTRO = 2;
const CounterName_GAS = 3;
const CounterName_HEAT = 4;
const CounterName_PORTABLE_WATER = 5;
const CounterName_OTHER = 6;

function fill_instruction(counter_name) {
    var q = document.getElementById('counter_instruction');
    switch(counter_name) {
        case CounterName_WATER_COLD:
            q.innerHTML = "Спускайте воду в унитазе пока устройство не перенесёт вас на следующую страницу";
        case CounterName_WATER_HOT:
            q.innerHTML = "Откройте кран горячей воды пока устройство не перенесёт вас на следующую страницу";
        case CounterName_ELECTRO:
            q.innerHTML = "Включите электроприбор. После моргания светодиода должна открыться следующая страница. Если не открывается, значит некорректное подключение или счётчик не поддерживается.";
        case CounterName_GAS:
            q.innerHTML = "Приход импульса от газового счётчика долго ожидать, нажмите Пропустить и продолжите настройку.";
        case CounterName_HEAT:
            q.innerHTML = "Приход импульса от счётчика тепла долго ожидать, нажмите Пропустить и продолжите настройку.";
        case CounterName_PORTABLE_WATER:
            q.innerHTML = "Откройте кран питьевой воды пока устройство не перенесёт вас на следующую страницу";
        case CounterName_OTHER:
        default:
            q.innerHTML = "При приходе импульса от счётчика устройство перенесёт вас на следующую страницу";
    }
}