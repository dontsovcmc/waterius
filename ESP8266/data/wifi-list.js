// fetch
// get wifi list from server
setTimeout(() => {
    setWifiList([
        {name:'HAUWEI-B311_F9E1', level: 5},
        {name:'ERROR_PASSWORD', level: 4},
        {name:'ZTE_8ds9f8sd67', level: 3},
        {name:'C78F56', level: 2},
        {name:'C78F56_5G', level: 1},
        {name:'wifi-6', level: 1},
        {name:'wifi-7', level: 1},
        {name:'wifi-8', level: 1},
        {name:'wifi-9', level: 1},
        {name:'wifi-10', level: 1},
        {name:'wifi-11', level: 1},
        {name:'wifi-12', level: 1},
        {name:'wifi-13', level: 1},
        {name:'wifi-14', level: 1},
    ]);
}, 1000);
const showAllBtn = document.getElementById('show-all');
showAllBtn.onclick = function() {
    document.querySelectorAll('.link-row.hd').forEach(item => item.classList.remove('hd'));
    showAllBtn.classList.add('hd');
}
function setWifiList(data) {
    document.querySelector('.preloader').classList.remove('show');
    if(data && data.length) {
        let html = '';
        data.forEach((item, index) => {
            html += getWifiRow(item, index);
        });
        document.querySelector('.wifi-list').innerHTML = html;
        if(data.length > 5) showAllBtn.classList.remove('hd');
    }
    document.getElementById('wifi-name').classList.remove('hd');
}
function getWifiRow(data, index) {
    let cl = ''
    if(index > 4) cl = ' hd';
    return `<a class="link-row${cl}" href="wifi-password.html#name=${encodeURIComponent(data.name)}&level=${data.level}">
        <div class="icon l${data.level}">
            <img src="wifi.png">
        </div>
        ${data.name}
        <div class="icon arrow">
            <svg width="24" height="24" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
                <path d="M8.20403 5.29688C7.99268 5.08553 7.87395 4.79889 7.87395 4.5C7.87395 4.20112 7.99268 3.91447 8.20403 3.70313C8.41537 3.49178 8.70201 3.37305 9.0009 3.37305C9.29979 3.37305 9.58643 3.49178 9.79778 3.70313L17.2978 11.2031C17.4027 11.3076 17.4859 11.4318 17.5427 11.5686C17.5994 11.7053 17.6287 11.8519 17.6287 12C17.6287 12.1481 17.5994 12.2947 17.5427 12.4314C17.4859 12.5682 17.4027 12.6924 17.2978 12.7969L9.79778 20.2969C9.58643 20.5082 9.29979 20.627 9.0009 20.627C8.70201 20.627 8.41537 20.5082 8.20403 20.2969C7.99268 20.0855 7.87395 19.7989 7.87395 19.5C7.87395 19.2011 7.99268 18.9145 8.20403 18.7031L14.9062 12.0009L8.20403 5.29688Z" fill="black"/>
            </svg>
        </div>
    </a>`;
}

