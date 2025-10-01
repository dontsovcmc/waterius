const queryParams = {};
let pages = {};

function _init(_pages) {
    // Должна выполняться при загрузке каждой страницы
    
    pages = _pages;
    parseQueryParams();


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
                if(parseInt(queryParams[item.name]) || (queryParams[item.name] && queryParams[item.name].toLowerCase() == 'true'))//{
                    item.checked = true;
                    //item.parentNode.classList.add('active');
                //}else{
                    //item.checked = false;
                    //item.parentNode.classList.remove('active');
                //}
                checkboxToggle(item);
                return;
            }
            if (item.localName == 'span') {
                item.textContent = queryParams[item.id];
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
        //document.getElementById('ssid').value = queryParams.ssid;
        document.querySelector('.link-row span').innerText = queryParams.ssid;
        document.querySelector('main').classList.add('ssid');
        
        if(queryParams.level && !isNaN(queryParams.level))
            document.querySelector('.link-row .icon').classList.add('l' + queryParams.level);

        return;
    }
    
    if(document.getElementById('hall_model'))
    {
        document.getElementById('hall_model').addEventListener('change', function() {
            var v = document.getElementById('hall_model').value;
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
        //if(data && data.length) {
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
        //}
        document.getElementById('wifi-name').classList.remove('hd');
    });
}
function refreshPage() {
    location.reload();
}
function getWifiRow(data, index) {
    let cl = ''
    if(index > 9) cl = ' hd';
    return `<a class="link-row${cl}" href="/wifi_password.html?ssid=${encodeURIComponent(data.ssid)}&level=${data.level}&wifi_channel=${data.wifi_channel}${queryParams.wizard ? `&wizard=true` : ''}">
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
    //const data = {};// json
    /*for (const pair of new FormData(form)) {
        data.append(pair[0], pair[1]);
        //data[pair[0]] = pair[1];// json
    }*/
    form.querySelectorAll('input,textarea,select').forEach(inp => {
        if(inp.type == 'checkbox') return inp.checked ? data.append(inp.name, 1) : data.append(inp.name, 0);
        if(inp.type == 'radio' ) return inp.checked ? data.append(inp.name, 1) : ''; //data.append(inp.name, 0);
        data.append(inp.name, inp.value.trim());
    });
    
    ajax(action, {
        method: 'POST',
        headers: {
            "Content-Type": "application/x-www-form-urlencoded"
            //"Content-Type": "application/json"// json
        },
        body: data
        //body: JSON.stringify(data)// json
    }, data =>{
        
        // ответ сервера: ошибки полей формы
        /*
        {
            "errors": {
                "form": "Текст ошибки вверху страницы", // не обязательно
                // далее по полям формы
                "ssid": "Введите имя сети",
                "password": "Введите пароль",
                ...
            },
            // тут можно доделать функционал например выводить подсказки, они в фигме есть
            "tips": {
                "password": "Длина пароля 6-20 символов",
                ...
            }
        }
        */
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
        
        //---------
        // ответ сервера: ошибка с редиректом
        /*
        {
            "redirect": "/some_page.html",
            "error": "Текст ошибки вверху страницы", // не обязательно
            // далее параметры заполнения формы на новой странице
            "ssid": "имя сети",
            "password": "123123",
            ...
        }
        */
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

        //---------
        // success
        // ответ сервера: пустой json {}
        // или с параметрами
        /*
        {
            "error": "Текст ошибки вверху страницы",
            "ssid": "имя сети",
            "password": "123123",
            ...
        }
        */
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
            try{
                callback(JSON.parse(text));
            }catch(e){
                callback(text);
            }
        })
        /*.then(res => {
            if (!res.ok) return Promise.reject(res);
            return res.json();
        })
        .then(res => {
            if(pl) preloader(false);
            callback(res);
        })*/
        .catch(err => {
            //console.log(err);
            _try++;
            if(_try == 3) {
                // вывод ошибки
                if(pl) preloader(false);

                // TODO
                // выводить ошибку в тело сраницы
                alert(err.statusText);
                console.log(err.statusText);
                return;
            }
            // повторный запрос в случае ошибки ответа через 1 сек
            setTimeout(() => {
                ajax(action, data, callback, pl, _try);
            },1000);
        });
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
                return window.location = (queryParams.wizard ? next + '?wizard=true' + '&' : next + '?') + 'factor' + '=' + data.factor + '&delta' + '=' + data.impulses * data.factor;
            formError(data.error);
            getStatus(i, next);
        }, false);
    }, 2000);
}
function getImpulses(i) {
    setTimeout(() => {
        ajax('/api/status/' + i, {}, data => {
            document.getElementById('delta').textContent = data.impulses * parseInt(document.getElementById('factor').value);
            formError(data.error);
            getImpulses(i);
        }, false);
    }, 2000);
}
function getImpulsesHall(i) {
    setTimeout(() => {
        ajax('/api/status/' + i, {}, data => {
            value = data.impulses / parseInt(document.getElementById('factor').value);
            value = parseFloat(value.toPrecision(3));
            document.getElementById('delta').textContent = value;
            formError(data.error);
            getImpulsesHall(i);
        }, false);
    }, 2000);
}
function finish(btn){
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
            var error = tr(mess.error);
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