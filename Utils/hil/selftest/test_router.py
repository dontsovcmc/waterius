"""
Консоль роутера по SSH - без железа: клиент paramiko подменён.

Цена вопроса известна по прогону 20.09: консоль WT32-ETH01 закрыла соединение на
середине многочасового набора, и дальше каждая команда падала - ни один из оставшейся
сотни тестов до прошивки не дошёл. Здесь проверяется, что обрыв переживается, обрыв на
середине ответа называется своим именем, а ключ хоста сверяется.
"""

from __future__ import annotations

from typing import Any

import paramiko
import pytest

from ..router import RouterError, SshTransport

KEY = 'SHA256:8doTdx+7W6nTt2WSld7oC9ZSgQTeJWRawgRl6/Ye5fY'


class Key:
    def __init__(self, fingerprint: str) -> None:
        self.fingerprint = fingerprint


class Stream:
    def __init__(self, data: bytes, broken: bool = False) -> None:
        self._data = data
        self._broken = broken

    def read(self) -> bytes:
        if self._broken:
            raise paramiko.SSHException('channel closed')
        return self._data


class Router:
    """Роутер за подменённым клиентом: ключ, пароль и поломки задаёт тест."""

    def __init__(self, key: str = KEY, password: str = 'secret') -> None:
        self.key = key
        self.password = password
        self.connections = 0
        self.commands: list[str] = []
        self.refuse_channel = 0         # столько раз не открыть канал
        self.break_answer = 0           # столько раз оборвать посреди ответа

    def client(self) -> Any:
        router = self

        class Transport:
            def set_keepalive(self, interval: int) -> None:
                pass

        class Client:
            def set_missing_host_key_policy(self, policy: Any) -> None:
                self.policy = policy

            def connect(self, host: str, **kwargs: Any) -> None:
                self.policy.missing_host_key(self, host, Key(router.key))
                if kwargs['password'] != router.password:
                    raise paramiko.AuthenticationException('denied')
                router.connections += 1

            def get_transport(self) -> Transport:
                return Transport()

            def exec_command(self, line: str, timeout: float) -> tuple[None, Stream, Stream]:
                if router.refuse_channel:
                    router.refuse_channel -= 1
                    raise paramiko.SSHException('unable to open channel')
                router.commands.append(line)
                if router.break_answer:
                    router.break_answer -= 1
                    return None, Stream(b'', broken=True), Stream(b'')
                return None, Stream(f'ответ на {line}\n'.encode()), Stream(b'')

            def close(self) -> None:
                pass

        return Client()


def ssh(router: Router, host_key: str = KEY, pinned: list[str] | None = None) -> SshTransport:
    return SshTransport('192.168.1.240', 'secret', host_key,
                        pin_key=None if pinned is None else pinned.append,
                        client=router.client)


def test_команда_после_обрыва_уходит_на_поднятой_заново_консоли() -> None:
    router = Router()
    transport = ssh(router)
    router.refuse_channel = 1

    assert transport.run('show config', 4.0) == 'ответ на show config\n'
    assert router.connections == 2, 'консоль не поднималась заново'
    assert router.commands == ['show config'], 'команда ушла дважды'


def test_обрыв_на_середине_ответа_назван_своим_именем() -> None:
    router = Router()
    transport = ssh(router)
    router.break_answer = 1

    with pytest.raises(RouterError, match='оборвалась на середине'):
        transport.run('show acl', 4.0)
    assert transport.run('show acl', 4.0) == 'ответ на show acl\n'


def test_первый_вход_записывает_ключ() -> None:
    pinned: list[str] = []

    transport = ssh(Router(), host_key='', pinned=pinned)

    assert pinned == [KEY]
    assert transport.host_key == KEY


def test_чужой_ключ_останавливает_подъём() -> None:
    with pytest.raises(RouterError, match='ключ хоста SHA256:чужой.*записан SHA256:8doT'):
        ssh(Router(key='SHA256:чужой'))


def test_неверный_пароль_не_ждёт_минуту() -> None:
    with pytest.raises(RouterError, match='пароль консоли не подошёл'):
        SshTransport('192.168.1.240', 'не тот', KEY, client=Router().client)
