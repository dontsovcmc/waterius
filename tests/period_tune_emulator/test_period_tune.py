import pytest


def limit_coefficient(x, d=0.3):
    fractional = x - int(x)
    if fractional < 1.0 - d:
        return fractional + 1
    else:
        return fractional


@pytest.mark.parametrize(
    "x, expected",
    [
        (0.10, 1.10),
        (0.70, 0.70),
        (1.35, 1.35),
        (3.05, 1.05),
        (4.99, 0.99),
        (5.65, 1.65),
        (6.70, 0.70),
        (7.20, 1.20),
        (2.95, 0.95),
        (1.00, 1.00),
    ]
)
def test_limit_coefficient(x, expected):
    assert limit_coefficient(x, 0.3) == pytest.approx(expected)
