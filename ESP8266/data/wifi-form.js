function copyInput(id){
    navigator.clipboard.writeText(document.getElementById(id).value);
}
function showPW(){
    const pw=document.getElementById('password');
    inp.type=='password' ? inp.type='text' : inp.type='password';
}
document.getElementById('wifi-submit').onclick = function() {
    const name = document.getElementById('name').value.trim();
    const pw = document.getElementById('password').value.trim();
    const nameErr = document.getElementById('name-error');
    const pwErr = document.getElementById('pw-error');
    const _pl = document.querySelector('.preloader');

    !name ? nameErr.classList.remove('hd') : nameErr.classList.add('hd');
    !pw ? pwErr.classList.remove('hd') : pwErr.classList.add('hd');

    if(name && pw) {
        // fetch
        // send post wifi-form
        _pl.classList.add('show');
        setTimeout(function() {
            window.location = 'meter-cold.html';
        }, 1000);
        return;
    }
}
window.onload = function() {
    const h = window.location.hash.substring(1).split(/[\=\&]/);
    if(h.length != 4) return;
    if(h[0] == 'name' && h[1]) {
        const name = decodeURI(h[1]);
        document.getElementById('name').value = name;
        document.querySelector('.link-row span').innerText = name;
    }
    if(h[2] == 'level' && !isNaN(h[3])) document.querySelector('.link-row .icon').classList.add('l' + h[3]);
}