function formSubmit(event, form, action, _setup = false) {
    event.preventDefault();


    if(_setup) {
        let count = 0;
        document.querySelectorAll('#waterius_on,#http_on,#mqtt_on').forEach(item => {
            if(item.checked) count++;
        });
        const _fe = document.querySelector('.form-error');
        if(!count) return _fe.classList.remove('hd');
        _fe.classList.add('hd');
    }

    const data = new URLSearchParams();
    //const data = {};// json
    for (const pair of new FormData(form)) {
        data.append(pair[0], pair[1]);
        //data[pair[0]] = pair[1];// json
    }
    ajax('/api/' + action, {
        method: 'POST',
        headers: {
            "Content-Type": "application/x-www-form-urlencoded"
            //"Content-Type": "application/json"// json
        },
        body: data
        //body: JSON.stringify(data)// json
    }, data =>{
        // callback
        if(data.redirect) {
            let uri = data.redirect;
            delete data.redirect;
            let queryString = Object.keys(data).map((k) => {
                return encodeURIComponent(k) + '=' + encodeURIComponent(data[k])
            }).join('&');
            if(queryString) uri += '?' + queryString;
            window.location = uri;
            return;
        }
        

        // ошибки заполнения формы
        if(data.errors) {
            document.querySelectorAll('.f-row p.error').forEach(item => item.classList.add('hd'));
            for(let k in data.errors) {
                const _el = document.getElementById(k + '-error');
                if(!_el) continue;
                _el.classList.remove('hd')
                _el.innerText = data.errors[k];
            }
            formError(data.errors.form);
        }
        //*/
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
            console.log(err);
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
            // повторный запрос в случае ошибки ответа через 500 мс
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
    navigator.clipboard.writeText(document.getElementById(id).value);
}
function showPW(id){
    const pw=document.getElementById(id);
    pw.type == 'password' ? pw.type = 'text' : pw.type = 'password';
}
function showForm(cls){
    document.querySelector(cls).classList.toggle('hd');
    document.querySelector('.form-error').classList.add('hd');
}
function getWifiList(){
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
        <a href="/wifi_password.html?ssid=${encodeURIComponent(data.ssid)}&level=${data.level}">
            <div class="icon arrow">
                <img src="/images/icons.png">
            </div>
        </a>
    </div>`;
}
function getStatus(value) {
    setTimeout(() => {
        ajax('/api/status/' + value, {}, data => {
            if(data.state == 1) //console.log(document.getElementById('skip').href + '?factor=' + data.factor);
                return window.location = document.getElementById('skip').href + '?factor=' + data.factor;
                getStatus(value);
        }, false);
    }, 2000);
}
function formError(html){
    const _fe = document.querySelector('.form-error');
    if(!_fe) return;
    if(!html) return _fe.classList.add('hd');
    _fe.innerHTML = html;
    _fe.classList.remove('hd');
}
function finish(btn){
    ajax('/api/turnoff', {}, data => {
        btn.classList.add('disabled');
        finishTimer(btn, 8);
    }, false);
}
function finishTimer(btn, sec){
    sec--;
    if(!sec) return window.location = '//waterius.ru/account';
    btn.innerText = sec;
    btn.disabled = true;
    setTimeout(() => finishTimer(btn, sec), 1000);
}
function getLogs(){
    ajax('/waterius_logs.txt', {}, data => document.getElementById('logs').value = data);
}
window.onload = function() {
    // back button
    const _bb = document.getElementById('back-btn');
    if(_bb) _bb.onclick = function(){
        history.back();
        return false;
    }

    // parse location.search
    const _s = window.location.search.substring(1).split(/[\=\&]/);
    const queryParams = {};
    if(_s.length) {
        for(let i = 0; i < _s.length; i = i + 2){
            if(_s[i] && _s[i + 1])
                queryParams[decodeURI(_s[i])] = decodeURI(_s[i + 1]);
        }

        // заполнение полей формы из location.search
        document.querySelectorAll('input,textarea').forEach(item => {
            if(queryParams[item.name]) 
                item.type == 'checkbox' ? item.checked = true : item.value = queryParams[item.name];
            else if(item.type == 'checkbox')
                item.checked = false;
        });

        // показ ошибки из location.search
        formError(queryParams.error);

        // вес счетчика
        const _mf = document.getElementById('meter-factor');
        if(queryParams.factor && _mf) _mf.innerText = queryParams.factor;

        // скрыть текст: шаг 3/6
        if(queryParams._setup) document.querySelector('header .fr').classList.add('hd');
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