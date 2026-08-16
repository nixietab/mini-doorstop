#!/usr/bin/env python3
import re

ORIG = "EOSSDK-Win64-Shipping_orig"


def main():
    with open("dump.txt", "r", encoding="utf-8", errors="ignore") as f:
        content = f.read()

    match = re.search(r"\[Ordinal/Name Pointer\] Table.*?\n(.*?)(\n\n|\Z)",
                      content, re.DOTALL)
    if not match:
        print("Could not find Name Pointer Table in dump.txt")
        return

    lines = match.group(1).split("\n")
    exports = []
    for line in lines:
        parts = line.split()
        if len(parts) >= 4:
            exports.append(parts[-1])

    # De-duplicate while preserving order.
    seen = set()
    exports = [e for e in exports if not (e in seen or seen.add(e))]

    with open("proxy.def", "w") as out:
        out.write('LIBRARY "EOSSDK-Win64-Shipping.dll"\n')
        out.write("EXPORTS\n")
        for name in exports:
            out.write(f"    {name}={ORIG}.{name}\n")

    print(f"proxy.def generated: {len(exports)} forwarders -> {ORIG}.dll")


if __name__ == "__main__":
    main()
