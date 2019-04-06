
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

## Установка Вотериуса
- подключите счётчики воды к разъемам Wi-fi счётчика
<img src="https://github.com/dontsovcmc/waterius/blob/master/files/input.png" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/input.png" width="280"/> 
- включите питание
- нажмите кнопку >3сек на корпусе - включится Веб сервер для настройки
- найдите телефоном Wi-Fi точку доступа Waterius_0.5
- откройте [http://192.168.4.1](http://192.168.4.1)

### Настройки 
- имя сети домашнего Wi-Fi
- пароль домашнего Wi-Fi
#### Настройки счётчиков
- текущее показание счетчика горячей воды в кубометрах (разделитель дробного числа - точка) до 2 знака. Пример: 100.12
- текущее показание счетчика холодной воды
- кол-во литров на 1 импульс (по умолчанию 10)
#### Настройки waterius.ru или своего сервера
- сервер, куда будет отправлет JSON. Если не заполнен, отправки не будет. https://cloud.waterius.ru
- поле JSON "email". для waterius.ru введите электронную почту указанную для доступа в личный кабинет
- поле JSON "token". для waterius.ru ключ сгенерируется автоматически.
#### Настройки Blynk
- сервер blynk. blynk-cloud.com
- ключ Blynk. Копируете из проекта в приложении Blynk. Если пустой, отправки нет.
- адрес эл. почты. Blynk будет каждый день отправлять письмо.
- заголовок письма от Blynk
- текст письма от Blynk


<img src="https://github.com/dontsovcmc/waterius/blob/master/files/wifi_setup.jpg" data-canonical-src="https://github.com/dontsovcmc/waterius/blob/master/files/wifi_setup.jpg"/> 

- при желании получать эл. письма с показаниями введите свой эл. адрес (письма будут приходить ежедневно)
- можете указать URL своего HTTP сервера для получения данных ([параметры сервера](https://github.com/dontsovcmc/waterius/blob/master/Export.md))
- нажмите ОК
- убедитесь, что светодиод погас через 3-10 секунд, что говорит об успешном подключении к Wi-Fi. Если светодиод не погас, то заново откройте 192.168.4.1 и введите корректные настройки.
- откройте воду на полную, чтобы вылить больше 10л воды
- нажмите на кнопку коротким нажатием - Вотериус пришлет новые показания.
- далее Вотериус будет слать показания примерно раз в сутки

### Параметры Blynk: 

Приложение Blynk должно быть запущено (в углу квадрат "стоп").

В Blynk данные приходят на виртуальные пины Virtual_pin.

| пин | данные | размерность | примечание |   
| ---- | ---- | ---- | ---- |
| V0 | показания счетчика 0 | м3 |  |
| V1 | показания счетчика 1 | м3 |  |
| V2 | напряжение питания | В | после стабилизатора. около 3В из-за погрешности attiny85 |
| V3 | суточное потребление 0 | л |  |
| V4 | суточное потребление 1 | л |  |
| V5 | кол-во перезагрузок |  |  |


#### Подключение нескольких Вотериусов

Для того, чтобы несколько Вотериусов отсылало в один проект данные, надо добавить в меню новое устройство: Devices - New device. Выбрать Wifi, ESP8266.
Далее добавить виджеты и выбрать источник: устройство и пин


#### Электронное письмо

Для отправки показаний по эл. почте необходимо ввести адрес в настройках Вотериуса, а также добавить виджет e-mail в Blynk. 
Возможно при нажатии на виджет необходимо ввести свою электронную почту.

