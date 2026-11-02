#!/usr/bin/env python3
"""Offline certificate regressions using BearSSL, not a TLS mock.

Debian/Ubuntu: apt install g++ libbearssl-dev openssl
python3 ESP8266/scripts/test_tls.py
Or pass --bearssl-dir /path/to/bearssl-0.6 after building its static library.
"""

import argparse
import calendar
import os
from pathlib import Path
import re
import shlex
import ssl
import subprocess
import tempfile
import time


def certificates(text):
    return re.findall(r"-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----", text, re.S)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bearssl-dir", type=Path)
    parser.add_argument("--openssl", default="openssl")
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[1]
    header = (project / "src/tls_certificates.h").read_text()
    anchors = certificates(header)
    count = int(re.search(r"TLS_CERTIFICATE_COUNT = (\d+);", header).group(1))
    if len(anchors) != count or count < 4:
        raise RuntimeError("The trust set must include the cloud leaf and three public roots")
    chain = certificates((project / "test/tls/letsencrypt-chain.pem").read_text())
    if len(chain) != 3:
        raise RuntimeError("The frozen Let's Encrypt fixture must contain three certificates")
    russian_chain = certificates((project / "test/tls/russian-trusted-chain.pem").read_text())
    if len(russian_chain) != 2:
        raise RuntimeError("The frozen Russian Trusted fixture must contain leaf and intermediate")

    with tempfile.TemporaryDirectory(prefix="waterius-tls-") as directory:
        work = Path(directory)

        def write_der(name, pem):
            path = work / name
            path.write_bytes(ssl.PEM_cert_to_DER_cert(pem))
            return path

        trust = [write_der(f"anchor{i}.der", pem) for i, pem in enumerate(anchors)]
        le_chain = [write_der(f"chain{i}.der", pem) for i, pem in enumerate(chain)]
        ru_chain = [write_der(f"russian{i}.der", pem) for i, pem in enumerate(russian_chain)]
        exe = work / ("verify_chain.exe" if os.name == "nt" else "verify_chain")
        command = shlex.split(os.environ.get("CXX", "g++"))
        command += ["-std=c++17", str(project / "test/tls/verify_chain.cpp"), "-o", str(exe)]
        if args.bearssl_dir:
            command += ["-I", str(args.bearssl_dir / "inc"),
                        str(args.bearssl_dir / "build/libbearssl.a")]
        else:
            command += ["-lbearssl"]
        subprocess.run(command, check=True)

        forged = work / "forged.der"
        subprocess.run([
            args.openssl, "req", "-x509", "-newkey", "rsa:2048", "-nodes",
            "-keyout", str(work / "forged.key"), "-outform", "DER", "-out", str(forged),
            "-days", "2", "-subj", "/CN=cloud.waterius.ru",
            "-addext", "basicConstraints=critical,CA:FALSE",
            "-addext", "subjectAltName=DNS:cloud.waterius.ru",
        ], check=True, capture_output=True)
        tampered = work / "tampered.der"
        der = bytearray(le_chain[0].read_bytes())
        der[-1] ^= 1  # corrupt the signature while keeping the DER well-formed
        tampered.write_bytes(der)

        # Frozen fixture date; tests remain reproducible after the LE leaf expires.
        date = calendar.timegm((2026, 11, 2, 12, 0, 0))
        cases = [
            ("cloud private leaf", date, "cloud.waterius.ru", [trust[0]], 0),
            ("cloud wrong hostname", date, "other.example", [trust[0]], 56),
            ("cloud not yet valid", calendar.timegm((2018, 1, 1, 0, 0, 0)),
             "cloud.waterius.ru", [trust[0]], 54),
            ("cloud expired", calendar.timegm((2042, 1, 1, 0, 0, 0)),
             "cloud.waterius.ru", [trust[0]], 54),
            ("forged cloud key", int(time.time()), "cloud.waterius.ru", [forged], 62),
            ("Let's Encrypt chain", date, "us.pstd.ru", le_chain, 0),
            ("Let's Encrypt wrong hostname", date, "cloud.waterius.ru", le_chain, 56),
            ("Let's Encrypt expired", calendar.timegm((2027, 2, 1, 0, 0, 0)),
             "us.pstd.ru", le_chain, 54),
            ("Let's Encrypt corrupt signature", date, "us.pstd.ru",
             [tampered, *le_chain[1:]], 52),
            ("Russian Trusted chain", date, "www.sberbank.ru", ru_chain, 0),
            ("Russian Trusted wrong hostname", date, "cloud.waterius.ru", ru_chain, 56),
            ("Russian Trusted not yet valid", calendar.timegm((2026, 7, 1, 0, 0, 0)),
             "www.sberbank.ru", ru_chain, 54),
            ("Russian Trusted expired", calendar.timegm((2027, 8, 1, 0, 0, 0)),
             "www.sberbank.ru", ru_chain, 54),
        ]

        def check(name, timestamp, hostname, certs, expected, active_trust=trust):
            result = subprocess.run(
                [str(exe), str(timestamp), hostname, str(len(active_trust)),
                 *map(str, active_trust), *map(str, certs)],
                check=True, capture_output=True, text=True,
            )
            actual = int(result.stdout.strip())
            if actual != expected:
                raise AssertionError(f"{name}: BearSSL returned {actual}, expected {expected}")
            print(f"PASS: {name}", flush=True)

        for case in cases:
            check(*case)
        check("Russian Trusted root required", date, "www.sberbank.ru", ru_chain, 62, trust[:3])
        print(f"Passed {len(cases) + 1} BearSSL certificate checks")


if __name__ == "__main__":
    main()
