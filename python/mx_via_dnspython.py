import sys
from collections.abc import Iterable
from typing import NamedTuple, cast

from dns.rdtypes.ANY.MX import MX
import dns.resolver


class MXRecord(NamedTuple):
    preference: int
    exchange: str


def get_mx_records(name: str) -> list[MXRecord]:
    answers = dns.resolver.resolve(name, "MX")
    mx_answers = cast(Iterable[MX], answers)
    return [MXRecord(mx.preference, str(mx.exchange).rstrip(".")) for mx in mx_answers]


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python mx_via_dnspython.py <domain>")
        sys.exit(1)
    domain = sys.argv[1]
    print(get_mx_records(domain))
