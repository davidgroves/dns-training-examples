#!/usr/bin/env python3
"""
Resolve a name N times in parallel using dnspython async resolver.
Same CLI as async-resolution.c; prints total program time at the end.

Usage: async_resolution.py <name> <type> <N> [server]
       server: optional DNS server (e.g. 8.8.8.8); uses system default if omitted.
"""

import asyncio
import sys
import time

import dns.asyncresolver

RECORD_TYPES = (
    "A",
    "AAAA",
    "CNAME",
    "MX",
    "NS",
    "PTR",
    "SOA",
    "SRV",
    "TXT",
    "ANY",
)


def record_type_from_string(s: str) -> str | None:
    """Return normalized type string if valid, else None."""
    u = s.upper()
    if u not in RECORD_TYPES:
        return None
    return u


async def resolve_once(
    resolver: dns.asyncresolver.Resolver,
    name: str,
    rdtype: str,
) -> None:
    """Perform one resolution (result discarded)."""
    await resolver.resolve(name, rdtype)


async def run_resolutions(
    name: str,
    rdtype: str,
    n: int,
    server: str | None,
) -> float:
    """Run N resolutions in parallel; return elapsed time in seconds."""
    resolver = dns.asyncresolver.Resolver(configure=(server is None))
    if server is not None:
        resolver.nameservers = [server]

    start = time.perf_counter()
    await asyncio.gather(*[resolve_once(resolver, name, rdtype) for _ in range(n)])
    return time.perf_counter() - start


def main() -> int:
    if len(sys.argv) != 4 and len(sys.argv) != 5:
        print(f"Usage: {sys.argv[0]} <name> <type> <N> [server]", file=sys.stderr)
        return 1

    name = sys.argv[1]
    type_str = sys.argv[2]
    try:
        n = int(sys.argv[3])
    except ValueError:
        n = 0
    server = sys.argv[4] if len(sys.argv) == 5 else None

    type_norm = record_type_from_string(type_str)
    if type_norm is None:
        print(f"Unknown record type: {type_str}", file=sys.stderr)
        return 1
    rdtype = type_norm
    if n <= 0:
        print("N must be a positive integer", file=sys.stderr)
        return 1

    elapsed = asyncio.run(run_resolutions(name, rdtype, n, server))
    print(f"Total time: {elapsed:.6f} s ({n} resolutions)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
