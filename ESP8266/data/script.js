function g(a) {
    return document.getElementById(a) 
};

function send(u, t, f) {
    const a = new XMLHttpRequest(); 
    a.open('GET', u); 
    a.timeout = t; 
    a.onreadystatechange = function (d) { 
        if (a.readyState === 4 && a.status === 200) 
        { 
            f(a.responseText); 
        } 
    }; 
    a.send(); 
}; 

window.onload = function () {

    function states() {

        function ok(d) { 
            var b = JSON.parse(d); 
            Object.keys(b).forEach(function (e) { 
                if (g(e)) { 
                    g(e).innerHTML = b[e]; 
                }; 
            }) 
        };

        function h(i, s) {
            if (s) {
                g(i).removeAttribute("hidden");
            } else {
                g(i).setAttribute("hidden", true);
            }
        }

        function checkFactor() { 
            var fi = g('factorCold'); 
            var fu = g('factorHot'); 
            h("fc_fb_control", fi.value == 2);
            h("fh_fb_control", fu.value == 2);
            h("cloud_off", g("whost").value != "https://cloud.waterius.ru");
            h("cloud_off2", !Boolean(g("wmail").value));
            h("failsave", Boolean(g("fail").text));
        }; 
        checkFactor(); 
        send('/states', 500, ok); 
        if (parseInt(g('elapsed').textContent) < 4) { 
            clearInterval(j); 
            alert('Ватериус выключился. Начните настройку заново, зажав кнопку на 5–10 секунд.'); 
        }
    } 
    states(); 
    var j = setInterval(states, 2000);

    function ntw() { 
        function ok2(a) { 
            var h = '<h3>Выберите свою Wi-Fi сеть</h3>'; 
            g('networks').innerHTML = h.concat(a); 
            clearInterval(loadn); 
        }; 
        send('/networks', 7000, ok2); 
    } 
    ntw(); 
    var loadn = setInterval(ntw, 8000);

    function mailCh() { 
        var mail = g('wmail'); 
        if (mail.value == 'test@waterius.ru') { 
            mail.value = ''; 
        } 
    } 
    mailCh();

    send('/config', 500, function (d) { 
        var b = JSON.parse(d); 
        Object.keys(b).forEach(function (e) { 
            if (g(e)) { 
                g(e).value = b[e]; 
            }; 
        }) 
    });
};

function advSett() { 
    var a = g('chbox'); 
    var b = 'none'; 
    if (a.checked) { 
        b = 'block' 
    } 
    g('advanced').style.display = b; 
};

function c(a) { 
    g('s').value = a.innerText || a.textContent; 
    g('p').focus(); 
};