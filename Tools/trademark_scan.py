"""Scans user-visible names for trademarks (factory-content.md 0.1, qa-polish.md 11).

Checks string literals in Source/ (not tests, not comments) and the names of the
factory resource files. Exit status 1 if anything matches, so it can run in CI.
The same list is held by the TrademarkScan test (Source/Tests/TrademarkTests.cpp).
"""

import glob
import os
import re
import sys

BRANDS = [
    "Fender", "Gibson", "Stratocaster", "Strat", "Telecaster", "Tele", "Les Paul", "ES-335", "ES335",
    "Explorer", "Flying V", "Firebird", "Thunderbird", "Jazzmaster", "Jaguar", "Precision", "Jazz Bass",
    "Rickenbacker", "Ibanez", "Music Man", "StingRay", "EMG", "Seymour", "Duncan", "DiMarzio",
    "Floyd Rose", "Bigsby", "Kluson", "Grover", "Schaller", "Gotoh", "BadAss", "Badass",
    "Tune-o-Matic", "Tune-o-matic", "ABR-1", "Marshall", "Vox", "Mesa", "Boogie", "Peavey", "Orange",
    "Hiwatt", "Soldano", "Bogner", "Diezel", "Ampeg", "SVT", "JCM", "AC30", "Deluxe", "Champ",
    "Bassman", "Twin Reverb", "Celestion", "Greenback", "Jensen", "EVM", "Shure", "SM57", "SM7B", "Royer", "Neumann",
    "Sennheiser", "MD421", "AKG", "C414", "D112", "U87", "Tube Screamer", "Big Muff", "Klon", "Boss",
    "Fuzz Face",
    "MXR", "Electro-Harmonix", "Uni-Vibe", "Leslie", "D'Addario", "NYXL", "Ernie Ball", "Elixir",
    "Selmer", "Dobro", "Gretsch", "Epiphone", "PRS", "Danelectro", "TransTrem", "Kinman", "Alnico Blue",
]

LITERAL = re.compile(r'"((?:[^"\\]|\\.)*)"')


def matches(text):
    for brand in BRANDS:
        if re.search(r"(?<![A-Za-z])" + re.escape(brand) + r"(?![a-z])", text):
            return brand
    return None


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(root)
    found = []

    for path in glob.glob("Source/**/*.cpp", recursive=True) + glob.glob("Source/**/*.h", recursive=True):
        norm = path.replace(os.sep, "/")
        if "/Tests/" in norm:
            continue
        with open(path, encoding="utf-8", errors="ignore") as f:
            for number, line in enumerate(f, 1):
                stripped = line.strip()
                if stripped.startswith(("//", "*", "/*")) or "legacy name" in line:
                    continue
                for m in LITERAL.finditer(line):
                    brand = matches(m.group(1))
                    if brand:
                        found.append(f"{norm}:{number}: {brand}: \"{m.group(1)[:80]}\"")

    for path in glob.glob("Resources/**/*.luthier*", recursive=True):
        brand = matches(os.path.basename(path))
        if brand:
            found.append(f"{path.replace(os.sep, '/')}: {brand}")

    for line in found:
        print(line)

    print(f"{len(found)} trademark hits", file=sys.stderr)
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
