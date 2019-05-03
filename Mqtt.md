# Отправка данных по протоколу MQTT

## Настройка Ватериуса

В веб интерфейсе Ватериуса заполните: 

MQTT сервер: ip адрес брокера или домен
Пример: 192.168.1.10, broker.hivemq.com
MQTT порт: 1883

MQTT логин: логин для авторизированного доступа. не обязательно.
MQTT пароль: пароль для авторизированного доступа. не обязательно.

MQTT топик: топик по которому будут отсылаться данные.
По умолчанию "waterius/< chip id >/". 
Таким образом можно отличать Ватериусы друг от друга. Chip id не является случайным, лучше использовать длинный хэш.

## MQTT топики
`waterius/< chip id >/< имя параметра >`
Именa параметров аналогичны полям JSON

## Пример с локальным брокером
Настройка Ватериуса:
```
mqtt хост: 192.168.1.5
mqtt порт: 1883
mqtt топик: waterius/6901727/
mqtt логин: test
mqtt пароль: test 
```

Создаем файл с разрешенными логинами и паролями
`mosquitto_passwd -b <полный путь>/pass.txt user password`

в конфигурации mosquitto.conf:
```
allow_anonymous false
password_file <полный путь>/pass.txt
```

в конфигурации mosquitto.conf:
`bind_address 192.168.1.5 // говорим, чтобы работал на локальном ip`

Запускаем брокер с файлом конфигурации:
`mosquitto -c mosquitto.conf`


Подписываемся на топики:
`mosquitto_sub -h 192.168.1.5 -t "waterius/#" -u "test" -P "test" -v`

После запуска Ватериуса видим данные:
```
waterius/6901727/ch0 0.00
waterius/6901727/ch1 0.00
waterius/6901727/delta0 0
waterius/6901727/delta1 0
waterius/6901727/voltage 2.95
waterius/6901727/resets 0
waterius/6901727/good 1
waterius/6901727/boot 8
waterius/6901727/imp0 0
waterius/6901727/imp1 0
waterius/6901727/version 5
waterius/6901727/version_esp 0.7
```
