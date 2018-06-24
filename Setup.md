
# Установка счётчика

## Настройка приложения Blynk
- [Установите приложение Blynk на телефон](https://www.blynk.cc/getting-started).
- Авторизируйтесь в нём
- Нажмите на значем QR кода вверху, чтобы скопировать проект waterius

<img src="https://github.com/dontsovcmc/waterius/blob/master/files/step01.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/step01.png" width="280"/> 

Наводим камеру телефона на QR код:
<img src="https://github.com/dontsovcmc/waterius/blob/master/files/qr.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/qr.png" width="300"/> 

- Создастся проект. Теперь надо узнать уникальный ключ для отправки данных в проект.
- Нажимаем "стоп"

<img src="https://github.com/dontsovcmc/waterius/blob/master/files/step02.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/step02.png" width="280"/> 

- Нажимаем на "гайку" для настройки:

<img src="https://github.com/dontsovcmc/waterius/blob/master/files/step03.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/step03.png" width="280"/> 

- Нажимаем "Скопировать код в буфер телефона" (удобно, если с телефона будем заходить на waterius) или "отправить код по эл. почте" (удобно, если настройка с ноутбука)

<img src="https://github.com/dontsovcmc/waterius/blob/master/files/step04.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/step04.png" width="280"/> 

- подключите счётчики воды к разъемам Wi-fi счётчика
- включите питание
- откройте воду, чтобы вылилось больше 10л воды (чтобы убедиться, что счётчики подключены)
- нажмите кнопку на корпусе - включится Веб сервер для настройки
- найдите телефоном Wi-Fi точку доступа ImpulsCounter_0.4
- откройте [http://192.168.4.1](http://192.168.4.1)
- введите: имя и пароль от Wi-Fi, свободный ip адрес для счётчика (обычно 192.168.0.x, где x > 20), текущие показания счетчиков воды в кубометрах (разделитель дробного числа - точка), кол-во литров на 1 импульс (по умолчанию 10). 

<img src="https://github.com/dontsovcmc/waterius/blob/master/files/wifi_setup.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/wifi_setup.png" width="280"/> 

- при желании получать эл. письма с показаниями введите свой эл. адрес (письма будут приходить ежедневно)
- нажмите ОК
- через 2 минуты счетчик выйдет на связь и передаст показания в приложение Blynk
- далее он будет слать показания раз в сутки