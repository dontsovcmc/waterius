import socket


def free_port() -> int:
    """Порт, который сейчас никто не слушает: 1883 может быть занят прогоном."""
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return int(sock.getsockname()[1])
