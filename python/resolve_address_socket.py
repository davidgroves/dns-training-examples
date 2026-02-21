#!/usr/bin/env python3
"""Resolve A/AAAA records with socket.getaddrinfo()."""

import socket
import sys

def main() -> int:
    if len(sys.argv) not in (2, 3):
        print("Usage: resolve_address_socket.py <name> [A|AAAA]", file=sys.stderr)
        return 1

    kind = (sys.argv[2] if len(sys.argv) == 3 else "A").upper()
    family = {"A": socket.AF_INET, "AAAA": socket.AF_INET6}.get(kind)
    if family is None:
        print("Type must be A or AAAA.", file=sys.stderr)
        return 1

    try:
        infos = socket.getaddrinfo(sys.argv[1], None, family)
    except socket.gaierror as e:
        print(e, file=sys.stderr)
        return 1

    for addr in {info[4][0] for info in infos}:
        print(addr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
