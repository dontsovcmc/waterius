const queryParams = {};
let pages = {};
function _init(_pages) {
    // Должна выполняться при загрузке каждой страницы
    
    pages = _pages;
    parseQueryParams();

    if(document.querySelector('form')) {
        // заполнение полей формы из location.search
        document.querySelectorAll('input,textarea').forEach(item => {
            if(item.type == 'checkbox'){
                if(parseInt(queryParams[item.name])) item.checked = true;
                else item.checked = false;
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
}
function parseQueryParams(){
    // parse location.search
    //const _s = window.location.search.substring(1).split(/[\=\&]/);
    const _s = decodeURI(window.location.search).substring(1).split(/[\=\&]/);
    if(_s.length) {
        for(let i = 0; i < _s.length; i = i + 2){
            if(_s[i] && _s[i + 1])
                //queryParams[decodeURIComponent(_s[i])] = decodeURIComponent(_s[i + 1]);
                queryParams[_s[i]] = _s[i + 1];
        }
    }
}
function formError(html){
    const _fe = document.querySelector('.form-error');
    if(!_fe) return;
    if(!html) return _fe.classList.add('hd');
    _fe.innerHTML = html;
    _fe.classList.remove('hd');
}
function getWifiList(_pages){
    _init(_pages);

    const showAllBtn = document.getElementById('show-all');
    ajax('/api/networks', {}, data => {
        //if(data && data.length) {
            let html = '';
            data.forEach((item, index) => {
                html += getWifiRow(item, index);
            });
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
function getWifiRow(data, index) {
    let cl = ''
    if(index > 9) cl = ' hd';
    return `<div class="link-row${cl}">
        <div class="icon l${data.level}">
            <img src="/images/icons.png">
        </div>
        ${data.ssid}
        <a href="/wifi_password.html?ssid=${encodeURIComponent(data.ssid)}&level=${data.level}${queryParams.wizard ? `&wizard=true` : ''}">
            <div class="icon arrow">
                <img src="/images/icons.png">
            </div>
        </a>
    </div>`;
}
function formSubmit(event, form, action) {
    event.preventDefault();

    const data = new URLSearchParams();
    //const data = {};// json
    /*for (const pair of new FormData(form)) {
        data.append(pair[0], pair[1]);
        //data[pair[0]] = pair[1];// json
    }*/
    form.querySelectorAll('input,textarea').forEach(inp => {
        if(inp.type == 'checkbox') return inp.checked ? data.append(inp.name, 1) : data.append(inp.name, 0);
        if(inp.type == 'radio' ) return inp.checked ? data.append(inp.name, 1) : ''; //data.append(inp.name, 0);
        data.append(inp.name, inp.value.trim());
    });
    
    ajax('/api/' + action, {
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
        if(data.errors) {
            document.querySelectorAll('.f-row p.error').forEach(item => item.classList.add('hd'));
            for(let k in data.errors) {
                const _el = document.getElementById(k + '-error');
                if(!_el) continue;
                _el.classList.remove('hd')
                _el.innerText = data.errors[k];
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
function showForm(cls){
    document.querySelector(cls).classList.toggle('hd');
    document.querySelector('.form-error').classList.add('hd');
}
function getStatus(i) {
    setTimeout(() => {
        ajax('/api/status/' + i, {}, data => {
            if(data.state == 1)
                return window.location = (queryParams.wizard ? pages.next_wizard + '&' : pages.next + '?') + 'factor' + i + '=' + data.factor;
                
            getStatus(i);
            //formError(data.error);// ???
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
    if(!next) return window.location = queryParams.wizard ? pages.back_wizard : pages.back;
    return window.location = queryParams.wizard ? pages.next_wizard : pages.next;
}
function mainSettings(){
    ajax('/api/main_settings', {}, data => {
        if(!data.length) return;
        const html = [];
        data.forEach(mess => {
            html.push(`<p class="form-error mt24">${mess.error}${mess.link ? `<br><br><a class="link" href="${mess.link}">${mess.link_text}</a>` : ''}</p>`);
        });
        document.getElementById('mainInfoText').innerHTML = html.join('');
    });
}