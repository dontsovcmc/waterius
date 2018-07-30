
# Установка счётчика
Для работы Вотериуса нужно приложение Blynk.cc на телефоне, а также настроить само устройство.

## Настройка приложения Blynk
- [Установите приложение Blynk на телефон](https://www.blynk.cc/getting-started).
- Авторизируйтесь в нём
- Нажмите на значок QR кода вверху, чтобы скопировать проект waterius (Андроид)
<img src="https://github.com/dontsovcmc/waterius/blob/master/files/step01.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/step01.png" width="200"/> 
- Наводим камеру телефона на QR код:
<img src="https://github.com/dontsovcmc/waterius/blob/master/files/qr.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/qr.png" width="200"/> 
- Будет создан клон проекта. Теперь надо узнать уникальный ключ для отправки данных в проект.
- Нажимаем "старт", затем "стоп":
<img src="https://github.com/dontsovcmc/waterius/blob/master/files/step02.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/step02.png" width="280"/> 
- Нажимаем на "гайку" для настройки:
<img src="https://github.com/dontsovcmc/waterius/blob/master/files/step03.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/step03.png" width="280"/> 
- Нажимаем "Скопировать код в буфер телефона" (удобно, если с телефона будем заходить на waterius) или "отправить код по эл. почте" (удобно при настройке с ноутбука)
<img src="https://github.com/dontsovcmc/waterius/blob/master/files/step04.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/step04.png" width="280"/> 

## Настройка Вотериуса
- подключите счётчики воды к разъемам Wi-fi счётчика
<img src="https://github.com/dontsovcmc/waterius/blob/master/files/input.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/input.png" width="280"/> 
- включите питание
- нажмите кнопку на корпусе - включится Веб сервер для настройки
- найдите телефоном Wi-Fi точку доступа Waterius_0.4.2
- откройте [http://192.168.4.1](http://192.168.4.1)

### Настройки: 
- имя и пароль от домашнего Wi-Fi
- токен из приложения Blynk
- текущие показания счетчиков воды в кубометрах (разделитель дробного числа - точка) до 2 знака. Пример: 100.12
- кол-во литров на 1 импульс (по умолчанию 10). 

<img src="https://github.com/dontsovcmc/waterius/blob/master/files/wifi_setup.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/wifi_setup.png" width="280"/> 

- при желании получать эл. письма с показаниями введите свой эл. адрес (письма будут приходить ежедневно)
- нажмите ОК
- убедитесь, что светодиод погас через 3-10 секунд, что говорит об успешном подключении к Wi-Fi. Если светодиод не погас, то заново откройте 192.168.4.1 и введите корректные настройки.
- через 3 минуты счетчик выйдет на связь и передаст показания в приложение Blynk
- откройте воду на полную, чтобы вылить за 3 мин больше 10л воды и убедиться, что Вотериус пришлет новые показания.
- далее Вотериус будет слать показания примерно раз в сутки

