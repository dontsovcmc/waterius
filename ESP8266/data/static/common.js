const queryParams = {};
let pages = {};

// Пользователь сам нажал "Завершить": выключение штатное
let portal_closing = false;

// Потеря связи и повторы: docs/setup-portal.md, "Потеря связи с устройством"
const AJAX_TRIES = 3;
const AJAX_RETRY_MS = 1000;
const LOST_LINK_RETRY_MS = 3000;
const LOST_LINK_GIVE_UP_MS = 5 * 60 * 1000;

function _init(_pages) {
    // Должна выполняться при загрузке каждой страницы
    
    pages = _pages;
    parseQueryParams();

    // Вернулись к вкладке - жива ли связь. Переход по обычной ссылке делает
    // браузер, там своё окно уже не покажешь. main_status не продлевает
    // режим настройки (#305)
    document.addEventListener('visibilitychange', () => {
        if (document.visibilityState != 'visible' || portal_closing) return;
        ajax('/api/main_status', {}, () => {}, false);
    });


    if(document.querySelector('form')) {
        // заполнение полей формы из location.search

        // вначале заполним из server-side rendering
        document.querySelectorAll('select').forEach(item => {
            if (item.getAttribute('option-value')) {
                item.value = item.getAttribute('option-value')
            }
        });

        // заполним из url parameters
        document.querySelectorAll('input,textarea,select,span').forEach(item => {
            if(item.type == 'checkbox'){
                if(parseInt(queryParams[item.name]) || (queryParams[item.name] && queryParams[item.name].toLowerCase() == 'true'))
                    item.checked = true;
                checkboxToggle(item);
                return;
            }
            if (item.localName == 'span') {
                // Только если параметр действительно пришёл: иначе в span
                // с размерностью или числом импульсов попадало "undefined"
                if (queryParams[item.id] !== undefined) {
                    item.textContent = queryParams[item.id];
                }
                return;
            }
            if(queryParams[item.name]) item.value = queryParams[item.name];
        });
    }

    // показ ошибки из location.search
    formError(queryParams.error);

    // скрыть текст: шаг 3/6
    if(!queryParams.wizard){
        const wizard = document.querySelector('header .fr');
        if(wizard) wizard.classList.add('hd');
    }

    // wifi-password
    if(document.getElementById('wifi-form') && queryParams.ssid) {
        document.querySelector('.link-row span').innerText = queryParams.ssid;
        document.querySelector('main').classList.add('ssid');
        
        if(queryParams.level && !isNaN(queryParams.level))
            document.querySelector('.link-row .icon').classList.add('l' + queryParams.level);

        return;
    }
    
    if(document.getElementById('counter_model'))
    {
        document.getElementById('counter_model').addEventListener('change', function() {
            var v = document.getElementById('counter_model').value;
            if (v != '') {
                document.getElementById('factor').value = v;
            }
        }, false);
    }
}
function parseQueryParams(){
    // parse location.search
    const _s = window.location.search.substring(1).split(/[\=\&]/);
    if(_s.length) {
        for(let i = 0; i < _s.length; i = i + 2){
            if(_s[i] && _s[i + 1])
                queryParams[decodeURIComponent(_s[i])] = decodeURIComponent(_s[i + 1]);
        }
    }
}
function checkboxToggle(inp){
    inp.checked ? inp.parentNode.classList.add('active') : inp.parentNode.classList.remove('active');
    if(inp.dataset && inp.dataset.form){
        document.querySelector('.form-error').classList.add('hd');
        const _from = document.querySelector(inp.dataset.form);
        inp.checked ? _from.classList.remove('hd') : _from.classList.add('hd');
    }
}
function formError(error_code){
    const _fe = document.querySelector('.form-error');
    if(!_fe) return;
    if(!error_code) return _fe.classList.add('hd');
    _fe.innerHTML = tr(error_code); 
    _fe.classList.remove('hd');
}

function getWifiList(_pages){
    _init(_pages);

    const showAllBtn = document.getElementById('show-all');
    ajax('/api/networks', {}, data => {
            let html = '';
            var sorted = data.sort(function(a, b) {
                return b.level - a.level;
            });

            sorted.forEach((item, index) => {
                html += getWifiRow(item, index);
            });

            if(data.length === 0) {
                html = `<button type="button" class="btn-serv mt20" onclick="refreshPage()">Обновить список сетей</button>`
            }
            document.querySelector('.wifi-list').innerHTML = html;
            if(data.length > 10) {
                showAllBtn.classList.remove('hd');
                showAllBtn.onclick = function() {
                    document.querySelectorAll('.link-row.hd').forEach(item => item.classList.remove('hd'));
                    showAllBtn.classList.add('hd');
                }
            } 
        document.getElementById('wifi-name').classList.remove('hd');
    });
}
function refreshPage() {
    location.reload();
}
function getWifiRow(data, index) {
    let cl = ''
    if(index > 9) cl = ' hd';
    return `<a class="link-row${cl}" href="/wifi_password.html?ssid=${encodeURIComponent(data.ssid)}&level=${data.level}&wifi_channel=${data.wifi_channel}&bssid=${data.bssid}${queryParams.wizard ? `&wizard=true` : ''}">
        <div class="icon l${data.level}">
            <img src="/images/icons.png">
        </div>
        ${data.ssid}
        <div class="icon arrow">
            <img src="/images/icons.png">
        </div>
    </a>`;
}
function formSubmit(event, form, action) {
    event.preventDefault();

    const data = new URLSearchParams();
    form.querySelectorAll('input,textarea,select').forEach(inp => {
        if(inp.type == 'checkbox') return inp.checked ? data.append(inp.name, 1) : data.append(inp.name, 0);
        if(inp.type == 'radio' ) return inp.checked ? data.append(inp.name, 1) : ''; //data.append(inp.name, 0);
        data.append(inp.name, inp.value.trim());
    });
    
    ajax(action, {
        method: 'POST',
        headers: {
            "Content-Type": "application/x-www-form-urlencoded"
        },
        body: data
    }, data =>{
        
        if(data.errors && Object.keys(data.errors).length > 0) {
            document.querySelectorAll('.f-row p.error').forEach(item => item.classList.add('hd'));
            for(let k in data.errors) {
                const _el = document.getElementById(k + '-error');
                if(!_el) continue;
                _el.classList.remove('hd')
                _el.innerText = tr(data.errors[k]);
            }
            formError(data.errors.form);
            return;
        }
        
        if(data.redirect) {
            let uri = data.redirect;
            delete data.redirect;
        
            let queryString = [];
            Object.keys(data).map((k) => {
                queryString.push(encodeURIComponent(k) + '=' + encodeURIComponent(data[k]));
            });
            if(queryParams.wizard) queryString.push('wizard=true');
            
            if(queryString.length) uri += '?' + queryString.join('&');
            return window.location = uri;
        }

        // Формат ответа формы - docs/setup-portal.md, "REST API портала"
        window.location = queryParams.wizard ? pages.next_wizard : pages.next;
    });
}
function ajax(action, data, callback, pl = true, _try = 0) {
    if(pl) preloader(true);
    fetch(action, data)
        .then(res => {
            if (!res.ok) return Promise.reject(res);
            return res.text();
        })
        .then(text => {
            if(pl) preloader(false);
            lostLinkDone();   // запрос дошёл - связь есть
            try{
                callback(JSON.parse(text));
            }catch(e){
                callback(text);
            }
        })
        .catch(err => {
            _try++;

            if(_try < AJAX_TRIES) {
                // Короткий обрыв: повторяем молча
                setTimeout(() => {
                    ajax(action, data, callback, pl, _try);
                }, AJAX_RETRY_MS);
                return;
            }

            if(pl) preloader(false);
            console.log(err);

            if (is_device_unreachable(err)) {
                // Счётчик попыток сбрасываем: у повтора будут свои AJAX_TRIES
                lostLink(() => ajax(action, data, callback, pl, 0));
                return;
            }

            // Ответила прошивка: это её ошибка, а не потеря связи
            if (err && err.statusText) {
                alert(err.statusText);
            }
        });
}
/*
Запрос не дошёл до устройства? fetch отвергает промис, только если ответа не
было вовсе; ответ с плохим статусом приходит объектом Response. По тексту
сообщения решать нельзя: у Safari на телефоне это просто "Load failed".
*/
function is_device_unreachable(err) {
    if (!err) return true;
    return err.status === undefined && err.statusText === undefined;
}

// Окно рисуется отсюда, как preloader(): разметку страниц трогать не надо
function lostLink(retry) {
    if (portal_closing) return;

    let box = document.querySelector('.modal');
    if (!box) {
        box = document.createElement('div');
        box.classList.add('modal');
        box.innerHTML = '<div class="modal-box">'
            + '<h3>' + tr(S_LOST_LINK_TITLE) + '</h3>'
            + '<p class="text">' + tr(S_PLEASE_RECONNECT_WIFI) + '</p>'
            + '<button class="btn" type="button">' + tr(S_RETRY) + '</button>'
            + '</div>';
        box.since = Date.now();
        document.body.appendChild(box);
    }
    box.classList.add('show');
    box.querySelector('button').onclick = retry;

    // Телефон возвращается в сеть Ватериуса сам, поэтому повторяем в фоне
    if (Date.now() - box.since < LOST_LINK_GIVE_UP_MS) {
        clearTimeout(box.timer);
        box.timer = setTimeout(retry, LOST_LINK_RETRY_MS);
    }
}

// Связь вернулась: любой дошедший запрос закрывает окно
function lostLinkDone() {
    const box = document.querySelector('.modal');
    if (!box) return;
    clearTimeout(box.timer);
    box.remove();
}

function preloader(show) {
    let _pl = document.querySelector('.preloader');
    if(!_pl) {
        _pl = document.createElement('div');
        _pl.innerHTML = '<div class="lds-ellipsis"><div></div><div></div><div></div><div></div></div>';
        _pl.classList.add('preloader');
        document.body.appendChild(_pl);
    }
    if(show) _pl.classList.add('show');
    else _pl.classList.remove('show');
}
function copyInput(id){
    const el = document.getElementById(id);
    try{
        navigator.clipboard.writeText(el.value);
    }catch(e){
        el.select();
        document.execCommand('copy');
    }
}
function showPW(id){
    const pw=document.getElementById(id);
    pw.type == 'password' ? pw.type = 'text' : pw.type = 'password';
}

function getWiFiStatus() {
    setTimeout(() => {
        ajax('/api/connect_status', {}, data => {
            if(data.redirect) {
                if (data.params) {
                    return window.location = queryParams.wizard ? data.redirect + '?wizard=true&' + data.params : data.redirect + "?" + data.params;
                } else {
                    return window.location = queryParams.wizard ? data.redirect + '?wizard=true' : data.redirect;
                }
            }
            getWiFiStatus();
        }, false);
    }, 2000);
}
function getStatus(i, next) {
    setTimeout(() => {
        ajax('/api/status/' + i, {}, data => {
            if(data.state == 1)
                return window.location = (queryParams.wizard ? next + '?wizard=true': next);
            formError(data.error);
            getStatus(i, next);
        }, false);
    }, 2000);
}
/*
Сколько импульсов пришло с начала настройки и сколько это в единицах
ресурса. Размерность и пересчёт живут в strings.js — здесь только опрос.
*/
function getImpulses(i, counter_name) {
    setTimeout(() => {
        ajax('/api/status/' + i, {}, data => {
            const f = document.getElementById('factor');
            const factor = effective_factor(counter_name, f ? f.value : 0, data.factor);
            setText('impulses', data.impulses);
            setText('delta', delta_text(counter_name, data.impulses, factor));
            formError(data.error);
            getImpulses(i, counter_name);
        }, false);
    }, 2000);
}
function setText(id, text) {
    // Набор полей у страниц мастера разный, отсутствие элемента - не ошибка
    const q = document.getElementById(id);
    if (q) q.textContent = text;
}
function finish(btn){
    portal_closing = true;   // выключение штатное, про потерю связи молчим
    ajax('/api/turnoff', {}, () => {
        btn.classList.add('disabled');
        btn.disabled = true;
        finishTimer(btn, 8);
    }, false);
}
function finishTimer(btn, sec){
    sec--;
    if(!sec) return window.location = '//waterius.ru/account';
    btn.innerText = sec;
    setTimeout(() => finishTimer(btn, sec), 1000);
}
function getLogs(){
    ajax('/waterius_logs.txt', {}, data => document.getElementById('logs').value = data);
}
function _goto(next = false){
    if(!next) 
        return window.location = queryParams.wizard ? pages.back_wizard : pages.back;
    return window.location = queryParams.wizard ? pages.next_wizard : pages.next;
}
function mainStatus(){
    ajax('/api/main_status', {}, data => {
        if(!data.length) return;
        const html = [];
        data.forEach(mess => {
            var error = tr(mess.error, INPUT_PLACE[mess.input]);
            var link_text = tr(mess.link_text);
            html.push(`<p class="form-error mt24">${error}${mess.link ? `<br><br><a class="link" href="${mess.link}">${link_text}</a>` : ''}</p>`);
        });
        document.getElementById('mainInfoText').innerHTML = html.join('');
    });
}

function get_next_wizard_by_input(input) {
    if (input == 0) {
        return '/setup_send.html?wizard=true';
    } else {
        return '/input/0/setup.html?wizard=true';
    }
}