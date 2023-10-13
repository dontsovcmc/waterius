

class Settings:
    ssid: str
    password: str
    dhcp_on: bool | None = None
    gateway_ip: str = "192.168.0.1"
    static_ip: str = "192.168.0.111"
    mask: str = "255.255.255.0"
    mac_address: str = "00-1B-63-84-45-Е6"

    factor0: int = 0,
    factor1: int = 0


settings = Settings()


class State:
    state0: int = 0,
    state1: int = 0,
    delta0: float = 0.0,
    delta1: float = 0.0


state = State()
