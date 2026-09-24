"""Generates Luthier's factory riff library (spec/riff-library.md 2).

The riffs are written by hand in a compact tab notation, one file per genre,
under Tools/riffs/*.riffdef (grammar: riff-library.md 2.2, and the notes at
the top of each file). This script parses them, validates everything the spec
asks (grammar, bar sums, fret ranges, string counts, technique legality,
coverage, naming and similarity), and writes:

    Resources/Riffs/<Genre>/<id>.luthierriff   one per item
    Resources/Riffs/catalog.json               every item's metadata, no notes
    Resources/Riffs/free.txt                   the Free manifest (section 11)
    Resources/Riffs/strings.en.json            locale strings (section 10)

It is deterministic: a rerun gives byte-identical files. Keys are sorted,
reals are written with at most six decimals, and dates come from the riffdef
(never the clock). CI reruns it and then `git diff --exit-code Resources/Riffs`.

The JSON layout is the canonical riff layout that Source/Riffs/Riff.cpp also
writes, so a factory file loaded and saved by the plugin is byte-identical
(riff-library RL-03). Change one, change both.

Run from the repository root:
    python3 Tools/generate_factory_riffs.py            write Resources/Riffs
    python3 Tools/generate_factory_riffs.py --check    fail if the tree differs
    python3 Tools/generate_factory_riffs.py --only blues [--only jazz]
                                                       validate some genres only
    python3 Tools/generate_factory_riffs.py -v         print every warning
"""

import argparse
import filecmp
import glob
import itertools
import json
import os
import re
import shutil
import sys
import tempfile
from fractions import Fraction

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
RIFFDEFS = os.path.join(HERE, "riffs")
OUT = os.path.join(REPO, "Resources", "Riffs")

sys.path.insert(0, HERE)
try:
    from trademark_scan import matches as trademark_in   # noqa: E402
except Exception:                                         # pragma: no cover
    def trademark_in(_text):
        return None

VERSION = "1.3.0"   # the release the riff library ships in

# ---------------------------------------------------------------------------
# Vocabulary
# ---------------------------------------------------------------------------

GENRES = {
    # id: (folder, display name)
    "rock": ("Rock", "Rock"),
    "blues": ("Blues", "Blues"),
    "metal": ("Metal", "Metal"),
    "funk": ("Funk", "Funk"),
    "country": ("Country", "Country"),
    "jazz": ("Jazz", "Jazz"),
    "folk": ("Folk", "Folk / Acoustic"),
    "reggae": ("Reggae", "Reggae / Ska"),
    "latin": ("Latin", "Latin"),
    "pop": ("Pop", "Pop"),
    "punk": ("Punk", "Punk / Indie"),
    "soul": ("Soul", "Soul / R&B"),
}

TYPES = ["riff", "lick", "strum", "bass"]

# riff-library 2.1: the factory table, exactly.
TABLE = {
    "rock":    (10, 8, 4, 6),
    "blues":   (6, 12, 4, 6),
    "metal":   (12, 6, 2, 6),
    "funk":    (6, 6, 6, 8),
    "country": (4, 10, 4, 6),
    "jazz":    (2, 10, 4, 8),
    "folk":    (4, 6, 10, 4),
    "reggae":  (4, 4, 6, 6),
    "latin":   (4, 4, 8, 6),
    "pop":     (6, 6, 6, 6),
    "punk":    (8, 6, 6, 6),
    "soul":    (6, 8, 6, 8),
}

INSTRUMENTS = {
    # id: (strings, bass family)
    "guitar6": (6, False), "guitar7": (7, False), "guitar12": (6, False),
    "bass4": (4, True), "bass5": (5, True), "bass6": (6, True),
}

# Open-string MIDI notes, highest string first (the PerformanceScore order).
TUNINGS = {
    "guitar": {
        "standard": ("Standard", [64, 59, 55, 50, 45, 40]),
        "drop_d": ("Drop D", [64, 59, 55, 50, 45, 38]),
        "half_down": ("Half Step Down", [63, 58, 54, 49, 44, 39]),
        "drop_c": ("Drop C", [62, 57, 53, 48, 43, 36]),
        "open_g": ("Open G", [62, 59, 55, 50, 43, 38]),
        "open_d": ("Open D", [62, 57, 54, 50, 45, 38]),
        "open_e": ("Open E", [64, 59, 56, 52, 47, 40]),
        "dadgad": ("DADGAD", [62, 57, 55, 50, 45, 38]),
    },
    "guitar7": {
        "standard": ("Standard 7", [64, 59, 55, 50, 45, 40, 35]),
        "drop_a": ("Drop A 7", [64, 59, 55, 50, 45, 40, 33]),
    },
    "bass4": {
        "standard": ("Standard", [43, 38, 33, 28]),
        "drop_d": ("Drop D", [43, 38, 33, 26]),
    },
    "bass5": {
        "standard": ("Standard 5", [43, 38, 33, 28, 23]),
    },
    "bass6": {
        "standard": ("Standard 6", [48, 43, 38, 33, 28, 23]),
    },
}

SCALES = ["ionian", "dorian", "phrygian", "lydian", "mixolydian", "aeolian", "locrian",
          "harmonic_minor", "melodic_minor", "major_pentatonic", "minor_pentatonic",
          "blues", "chromatic"]

ROOTS = {"C": 0, "C#": 1, "Db": 1, "D": 2, "D#": 3, "Eb": 3, "E": 4, "F": 5, "F#": 6,
         "Gb": 6, "G": 7, "G#": 8, "Ab": 8, "A": 9, "A#": 10, "Bb": 10, "B": 11}

FEELS = ["straight", "shuffle", "swing", "half_time", "laid_back", "driving"]

# docs/MIDI_EXPORT_LUTHIER_PROFILE.md 6 and MidiPerformance.cpp kTechniques.
TECH_TOKENS = ["bend", "bendrelease", "prebend", "slideup", "slidedown", "slidelegato",
               "slideshift", "slidein", "slideout", "hammer", "pull", "pm", "dead",
               "natural", "pinch", "artificial", "tapharm", "tap", "vibrato", "trill",
               "whammy", "ghost", "accent", "staccato", "letring"]
TECH_MINIMUM = {t: 3 for t in TECH_TOKENS}
TECH_MINIMUM.update({"artificial": 2, "tapharm": 2, "whammy": 2})

BASS_TECH = {"sl": "slap", "po": "pop", "th": "thump", "lh": "lhslap"}

TECH_NAMES = {
    "bend": "Bend", "bendrelease": "Bend Release", "prebend": "Pre-Bend",
    "slideup": "Slide Up", "slidedown": "Slide Down", "slidelegato": "Legato Slide",
    "slideshift": "Shift Slide", "slidein": "Slide In", "slideout": "Slide Out",
    "hammer": "Hammer-On", "pull": "Pull-Off", "pm": "Palm Mute", "dead": "Dead Note",
    "natural": "Natural Harmonic", "pinch": "Pinch Harmonic",
    "artificial": "Artificial Harmonic", "tapharm": "Tap Harmonic", "tap": "Tap",
    "vibrato": "Vibrato", "trill": "Trill", "whammy": "Whammy", "ghost": "Ghost Note",
    "accent": "Accent", "staccato": "Staccato", "letring": "Let Ring",
    "strum": "Strum", "slap": "Slap", "pop": "Pop", "thump": "Double Thump",
    "lhslap": "Left-Hand Slap",
}

NATURAL_NODES = {3, 4, 5, 7, 9, 12, 16, 19, 24}

DURATIONS = {"w": Fraction(4), "h": Fraction(2), "q": Fraction(1), "e": Fraction(1, 2),
             "s": Fraction(1, 4), "t": Fraction(1, 8), "t8": Fraction(1, 3), "t16": Fraction(1, 6)}

DEFAULT_VELOCITY = 0.8
MAX_FRET = 24
MAX_TAP_FRET = 36

# ---------------------------------------------------------------------------
# Canonical JSON (mirrored by Source/Riffs/Riff.cpp: RiffJson)
# ---------------------------------------------------------------------------


def fmt_number(x):
    if isinstance(x, bool):
        return "true" if x else "false"
    if isinstance(x, int):
        return str(x)
    if isinstance(x, Fraction):
        x = float(x)
    s = "%.6f" % x
    s = s.rstrip("0").rstrip(".")
    if s in ("-0", ""):
        s = "0"
    return s


def fmt_string(s):
    return json.dumps(s, ensure_ascii=True)


def is_scalar(v):
    return not isinstance(v, (dict, list))


def emit(v, indent=0, inline=False):
    if isinstance(v, dict):
        keys = sorted(v.keys())
        if inline or not keys:
            return "{" + ",".join(fmt_string(k) + ":" + emit(v[k], 0, True) for k in keys) + "}"
        pad = " " * (indent + 1)
        body = ",\n".join(pad + fmt_string(k) + ": " + emit(v[k], indent + 1, False) for k in keys)
        return "{\n" + body + "\n" + " " * indent + "}"
    if isinstance(v, list):
        if not v:
            return "[]"
        if inline or all(is_scalar(e) or isinstance(e, list) for e in v):
            return "[" + ",".join(emit(e, 0, True) for e in v) + "]"
        pad = " " * (indent + 1)
        return "[\n" + ",\n".join(pad + emit(e, indent + 1, True) for e in v) + "\n" + " " * indent + "]"
    if isinstance(v, str):
        return fmt_string(v)
    if v is None:
        return "null"
    return fmt_number(v)


def canonical(obj):
    return emit(obj) + "\n"


def r6(x):
    """A real as the file will hold it: six decimals, integral values as ints."""
    v = round(float(x), 6)
    if v == int(v):
        return int(v)
    return v


# ---------------------------------------------------------------------------
# Parsing
# ---------------------------------------------------------------------------


class RiffError(Exception):
    pass


HEADER_KEYS = {"type", "instrument", "tuning", "capo", "key", "tempo", "meter", "feel",
               "difficulty", "tags", "free", "chords", "author", "origin", "date", "notes"}

SUFFIXES = [
    ("prebend", re.compile(r"pb(\d+(?:\.\d+)?)(r?)")),
    ("pm", re.compile(r"pm((?:0|1)(?:\.\d+)?)?")),
    ("shift", re.compile(r"s([/\\])(\d+)")),
    ("slidein", re.compile(r"/in")),
    ("slidedown", re.compile(r"\\in")),
    ("slideup", re.compile(r"/out")),
    ("slideout", re.compile(r"\\out")),
    ("legato", re.compile(r"([/\\])(\d+)")),
    ("bend", re.compile(r"b(\d+(?:\.\d+)?)(r?)")),
    ("vibrato", re.compile(r"~")),
    ("natural", re.compile(r"<>")),
    ("pinch", re.compile(r"\*")),
    ("tapharm", re.compile(r"tah(\d+)?")),
    ("artificial", re.compile(r"ah(\d+)?")),
    ("trill", re.compile(r"tr(\d+)")),
    ("tap", re.compile(r"T")),
    ("accent", re.compile(r"!")),
    ("staccato", re.compile(r"'")),
    ("letring", re.compile(r"lr")),
    ("ghost", re.compile(r"g")),
    ("velocity", re.compile(r"v(\d+)")),
    ("whammy", re.compile(r"wb(-?\d+(?:\.\d+)?)")),
    ("cv", re.compile(r"cv(\d+)")),
]

EVENT = re.compile(r"^(Dx|D|U|sl|po|th|lh|h|p)?(\[[^\]]*\]|\d+\.(?:x|\d+))(.*)$")
DURATION = re.compile(r"^(t8|t16|w|h|q|e|s|t)(\.?):$")


def tech(ttype, value=0.0, second=0.0, curve=None):
    t = {"type": ttype, "value": r6(value), "second": r6(second)}
    if curve:
        t["curve"] = [[r6(p), r6(v)] for p, v in curve]
    return t


def parse_suffixes(text, where, is_chord):
    """Returns (list of (kind, match)), raising on anything unparsed."""
    out = []
    pos = 0
    while pos < len(text):
        for kind, rx in SUFFIXES:
            m = rx.match(text, pos)
            if m:
                out.append((kind, m))
                pos = m.end()
                break
        else:
            raise RiffError("%s: cannot read '%s'" % (where, text[pos:]))
    return out


def parse_chord(body, nstrings, where):
    """[6.3 5.2 4.0] or a shape string [x32010] / [x-10-12-12-11-10], low to high."""
    inner = body[1:-1].strip()
    notes = []
    if "." in inner:
        for part in inner.split():
            m = re.match(r"^(\d+)\.(x|\d+)$", part)
            if not m:
                raise RiffError("%s: bad chord note '%s'" % (where, part))
            notes.append((int(m.group(1)), m.group(2)))
    else:
        cells = inner.split("-") if "-" in inner else list(inner)
        if len(cells) != nstrings:
            raise RiffError("%s: shape '%s' has %d strings, instrument has %d"
                            % (where, inner, len(cells), nstrings))
        for i, c in enumerate(cells):
            string_number = nstrings - i       # low string first
            if c in ("x", "X"):
                continue
            if not c.isdigit():
                raise RiffError("%s: bad shape cell '%s'" % (where, c))
            notes.append((string_number, c))
    if not notes:
        raise RiffError("%s: empty chord" % where)
    return notes


class Item:
    pass


def parse_header(item, tokens, where):
    i = 0
    while i < len(tokens):
        key = tokens[i]
        if key not in HEADER_KEYS:
            raise RiffError("%s: unknown header word '%s'" % (where, key))
        i += 1
        values = []
        while i < len(tokens) and tokens[i] not in HEADER_KEYS:
            values.append(tokens[i])
            i += 1
        if key in item.header and key != "tags":
            raise RiffError("%s: '%s' given twice" % (where, key))
        if key == "tags":
            item.header.setdefault("tags", []).extend(values)
        else:
            item.header[key] = values


def read_riffdefs(paths):
    items = []
    for path in paths:
        genre = os.path.splitext(os.path.basename(path))[0]
        with open(path, encoding="utf-8") as f:
            lines = f.read().split("\n")
        defaults = {}
        current = None
        in_play = False
        for number, raw in enumerate(lines, 1):
            # A comment is a '#' that starts a word, so F#m7 stays a chord.
            line = re.split(r"(?:^|(?<=\s))#", raw, maxsplit=1)[0].rstrip()
            where = "%s:%d" % (os.path.relpath(path, REPO).replace(os.sep, "/"), number)
            if not line.strip():
                continue
            stripped = line.strip()
            if stripped.startswith("defaults "):
                tokens = stripped.split()[1:]
                for k, v in zip(tokens[0::2], tokens[1::2]):
                    defaults[k] = [v]
                continue
            m = re.match(r'^riff\s+(\S+)\s+"([^"]*)"\s*$', stripped)
            if m:
                current = Item()
                current.id, current.name = m.group(1), m.group(2)
                current.genre = genre
                current.where = where
                current.header = {k: list(v) for k, v in defaults.items()}
                current.header_given = set()
                current.play = []            # (line number, text)
                current.path = path
                items.append(current)
                in_play = False
                continue
            if current is None:
                raise RiffError("%s: text before the first riff" % where)
            if stripped == "play:":
                in_play = True
                continue
            if in_play:
                current.play.append((number, line))
                continue
            # A header line: its words override the file defaults; tags add up.
            own = Item()
            own.header = {}
            parse_header(own, stripped.split(), where)
            for k, v in own.header.items():
                if k == "tags":
                    current.header["tags"] = current.header.get("tags", []) + v \
                        if "tags" in current.header_given else list(v)
                else:
                    if k in current.header_given:
                        raise RiffError("%s: '%s' given twice" % (where, k))
                    current.header[k] = v
                current.header_given.add(k)
    return items


def one(item, key, required=True):
    v = item.header.get(key)
    if v is None:
        if required:
            raise RiffError("%s: %s has no '%s'" % (item.where, item.id, key))
        return None
    if len(v) != 1:
        raise RiffError("%s: '%s' wants one value, got %s" % (item.where, key, " ".join(v)))
    return v[0]


def compile_item(item):
    """Fills item.json (the .luthierriff) and item.catalog. Raises RiffError."""
    w = item.where
    rid = item.id
    if not re.match(r"^[a-z]+\.(riff|lick|strum|bass)\.[a-z0-9]+(?:-[a-z0-9]+)*$", rid):
        raise RiffError("%s: bad id '%s'" % (w, rid))
    genre, rtype = rid.split(".")[0], rid.split(".")[1]
    if genre != item.genre:
        raise RiffError("%s: id '%s' is not in genre file '%s'" % (w, rid, item.genre))
    if genre not in GENRES:
        raise RiffError("%s: unknown genre '%s'" % (w, genre))
    if one(item, "type") != rtype:
        raise RiffError("%s: type '%s' does not match id '%s'" % (w, one(item, "type"), rid))

    instrument = one(item, "instrument")
    if instrument not in INSTRUMENTS:
        raise RiffError("%s: unknown instrument '%s'" % (w, instrument))
    nstrings, bass_family = INSTRUMENTS[instrument]
    if (rtype == "bass") != bass_family:
        raise RiffError("%s: a %s item cannot use %s" % (w, rtype, instrument))

    tuning_key = one(item, "tuning", required=False) or "standard"
    family = instrument if instrument in TUNINGS else ("guitar" if not bass_family else "bass4")
    if instrument in ("guitar6", "guitar12"):
        family = "guitar"
    if tuning_key not in TUNINGS[family]:
        raise RiffError("%s: tuning '%s' is not known for %s" % (w, tuning_key, instrument))
    tuning_name, tuning = TUNINGS[family][tuning_key]

    capo = int(one(item, "capo", required=False) or "0")
    if not 0 <= capo <= 12:
        raise RiffError("%s: capo out of range" % w)

    key = item.header.get("key")
    if not key or len(key) != 2 or key[0] not in ROOTS or key[1] not in SCALES:
        raise RiffError("%s: key wants '<root> <scale>', got %s" % (w, key))

    tempo = int(one(item, "tempo"))
    if not 30 <= tempo <= 300:
        raise RiffError("%s: tempo %d out of 30-300" % (w, tempo))

    m = re.match(r"^(\d+)/(\d+)$", one(item, "meter"))
    if not m:
        raise RiffError("%s: bad meter" % w)
    num, den = int(m.group(1)), int(m.group(2))
    if den not in (2, 4, 8, 16) or not 1 <= num <= 15:
        raise RiffError("%s: bad meter %d/%d" % (w, num, den))
    bar = Fraction(num * 4, den)

    feel = one(item, "feel")
    if feel not in FEELS:
        raise RiffError("%s: unknown feel '%s'" % (w, feel))

    difficulty = int(one(item, "difficulty"))
    if not 1 <= difficulty <= 5:
        raise RiffError("%s: difficulty out of 1-5" % w)

    tags = item.header.get("tags", [])
    for t in tags:
        if not re.match(r"^[a-z0-9]+(?:-[a-z0-9]+)*$", t):
            raise RiffError("%s: bad tag '%s'" % (w, t))

    free = (one(item, "free", required=False) or "no") == "yes"
    author = one(item, "author")
    origin = one(item, "origin")
    if origin != "original":
        raise RiffError("%s: origin must be 'original' (riff-library 12)" % w)
    if not re.match(r"^[A-Z]{1,3}$", author):
        raise RiffError("%s: author must be initials" % w)
    date = one(item, "date")
    if not re.match(r"^\d{4}-\d{2}-\d{2}$", date):
        raise RiffError("%s: bad date" % w)

    chords = []
    for c in item.header.get("chords", []):
        mm = re.match(r"^(\d+(?:\.\d+)?):(\S+)$", c)
        if not mm:
            raise RiffError("%s: bad chord symbol '%s'" % (w, c))
        chords.append({"beat": r6(float(mm.group(1))), "symbol": mm.group(2)})

    # ---- the play body -----------------------------------------------------
    notes = []          # dicts under construction
    strums = []
    bass_tech = []
    last_event = []     # the notes the last event started, for '~'
    beat = Fraction(0)
    bar_start = Fraction(0)
    duration = Fraction(1)
    bars = 0
    source_parts = []

    def fail(msg, number, col):
        raise RiffError("%s:%d col %d: %s" % (os.path.relpath(item.path, REPO).replace(os.sep, "/"),
                                                number, col, msg))

    for number, line in item.play:
        source_parts.append(line.strip())
        # A chord may hold spaces ([6.3 5.2 4.0]); it is one token with its
        # prefix and suffixes.
        for token_match in re.finditer(r"[^\s\[|]*\[[^\]]*\][^\s|]*|\||[^\s|]+", line):
            tok = token_match.group(0)
            col = token_match.start() + 1
            if tok == "|":
                if beat - bar_start != bar:
                    fail("bar adds up to %s beats, the meter needs %s" % (beat - bar_start, bar), number, col)
                bar_start = beat
                bars += 1
                continue
            dm = DURATION.match(tok)
            if dm:
                duration = DURATIONS[dm.group(1)] * (Fraction(3, 2) if dm.group(2) else 1)
                continue
            if tok == "r":
                beat += duration
                last_event = []
                continue
            if tok == "~":
                if not last_event:
                    fail("'~' has no note to hold", number, col)
                for n in last_event:
                    n["_dur"] += duration
                beat += duration
                continue
            em = EVENT.match(tok)
            if not em:
                fail("cannot read '%s'" % tok, number, col)
            prefix, body, suffix = em.group(1) or "", em.group(2), em.group(3)
            where = "%s:%d col %d" % (os.path.relpath(item.path, REPO).replace(os.sep, "/"), number, col)
            try:
                sfx = parse_suffixes(suffix, where, body.startswith("["))
            except RiffError as e:
                raise
            if body.startswith("["):
                cells = parse_chord(body, nstrings, where)
                if prefix not in ("", "D", "U", "Dx"):
                    fail("prefix '%s' does not apply to a chord" % prefix, number, col)
            else:
                s, f = body.split(".")
                cells = [(int(s), f)]
                if prefix in ("D", "U", "Dx"):
                    fail("strum prefix on a single note", number, col)

            made = []
            mask = 0
            cv = 200
            for string_number, fret_text in cells:
                if not 1 <= string_number <= nstrings:
                    fail("string %d on a %d-string instrument" % (string_number, nstrings), number, col)
                sidx = string_number - 1
                dead = fret_text == "x" or prefix == "Dx"
                fret = 0 if fret_text == "x" else int(fret_text)
                n = {"_start": beat, "_dur": duration, "str": sidx, "fret": fret,
                     "vel": DEFAULT_VELOCITY, "tech": [], "_line": number, "_col": col,
                     "_tap": False}
                if dead:
                    n["tech"].append(tech("dead"))
                if prefix == "h":
                    n["tech"].append(tech("hammer"))
                elif prefix == "p":
                    n["tech"].append(tech("pull"))
                for kind, sm in sfx:
                    if kind == "bend":
                        amount = float(sm.group(1))
                        if sm.group(2):
                            n["tech"].append(tech("bendrelease", amount, 0,
                                                  [(0, 0), (0.25, amount), (0.6, amount), (1, 0)]))
                        else:
                            n["tech"].append(tech("bend", amount, 0, [(0, 0), (0.25, amount), (1, amount)]))
                    elif kind == "prebend":
                        amount = float(sm.group(1))
                        if sm.group(2):
                            n["tech"].append(tech("prebend", amount, 0, [(0, amount), (0.4, amount), (1, 0)]))
                        else:
                            n["tech"].append(tech("prebend", amount, amount, [(0, amount), (1, amount)]))
                    elif kind == "pm":
                        n["tech"].append(tech("pm", float(sm.group(1)) if sm.group(1) else 0.0))
                    elif kind == "shift":
                        n["tech"].append(tech("slideshift", int(sm.group(2))))
                    elif kind == "legato":
                        n["tech"].append(tech("slidelegato", int(sm.group(2))))
                    elif kind == "slidein":
                        n["tech"].append(tech("slidein", max(0, fret - 3)))
                    elif kind == "slidedown":
                        n["tech"].append(tech("slidedown", min(MAX_FRET, fret + 3)))
                    elif kind == "slideout":
                        n["tech"].append(tech("slideout", max(1, fret - 5)))
                    elif kind == "slideup":
                        n["tech"].append(tech("slideup", min(MAX_FRET, fret + 5)))
                    elif kind == "vibrato":
                        n["tech"].append(tech("vibrato", 5.5, 20 if bass_family else 30))
                    elif kind == "natural":
                        n["tech"].append(tech("natural", fret))
                    elif kind == "pinch":
                        n["tech"].append(tech("pinch"))
                    elif kind == "artificial":
                        n["tech"].append(tech("artificial", int(sm.group(1) or 12)))
                    elif kind == "tapharm":
                        n["tech"].append(tech("tapharm", int(sm.group(1) or 12)))
                    elif kind == "trill":
                        n["tech"].append(tech("trill", int(sm.group(1))))
                    elif kind == "tap":
                        n["tech"].append(tech("tap"))
                        n["_tap"] = True
                    elif kind == "accent":
                        n["tech"].append(tech("accent"))
                    elif kind == "staccato":
                        n["tech"].append(tech("staccato"))
                    elif kind == "letring":
                        n["tech"].append(tech("letring"))
                    elif kind == "ghost":
                        n["tech"].append(tech("ghost"))
                    elif kind == "velocity":
                        v = int(sm.group(1))
                        if not 1 <= v <= 127:
                            fail("velocity out of 1-127", number, col)
                        n["vel"] = r6(v / 127.0)
                    elif kind == "whammy":
                        amount = float(sm.group(1))
                        n["tech"].append(tech("whammy", amount, 0,
                                              [(0, 0), (0.3, amount), (0.6, amount), (1, 0)]))
                    elif kind == "cv":
                        cv = int(sm.group(1))
                        if prefix not in ("D", "U", "Dx"):
                            fail("cv without a strum", number, col)
                types = [t["type"] for t in n["tech"]]
                if len(types) != len(set(types)):
                    fail("a technique is given twice", number, col)
                mask |= 1 << sidx
                made.append(n)

            # Several notes on one string at once cannot be played.
            strs = [n["str"] for n in made]
            if len(strs) != len(set(strs)):
                fail("two notes on one string", number, col)

            if prefix in BASS_TECH:
                bass_tech.append({"beat": r6(beat), "str": made[0]["str"], "tech": BASS_TECH[prefix],
                                  "pos": 0.5, "force": 0.8})
            if prefix in ("D", "U", "Dx"):
                strums.append({"beat": r6(beat), "dir": "down" if prefix in ("D", "Dx") else "up",
                               "cv": cv, "mask": mask, "striker": "pick",
                               "mute": 1 if prefix == "Dx" else 0})
            notes.extend(made)
            last_event = made
            beat += duration

    if not item.play:
        raise RiffError("%s: %s has no play body" % (w, rid))
    if beat != bar_start:
        raise RiffError("%s: the last bar is not closed with '|'" % w)
    length = beat
    if not 1 <= bars <= 8 or length > 64:
        raise RiffError("%s: %d bars / %s beats (1-8 bars, at most 64 beats)" % (w, bars, length))
    if len(notes) > 4096:
        raise RiffError("%s: too many notes" % w)

    # ---- legality (riff-library 2.3) ----------------------------------------
    by_string = {}
    for n in sorted(notes, key=lambda n: (n["_start"], n["str"])):
        by_string.setdefault(n["str"], []).append(n)
    max_bend = 1.5 if bass_family else 3.0
    for n in notes:
        where = "%s:%d col %d" % (os.path.relpath(item.path, REPO).replace(os.sep, "/"), n["_line"], n["_col"])
        types = [t["type"] for t in n["tech"]]
        limit = MAX_TAP_FRET if ("tap" in types or "tapharm" in types) else MAX_FRET
        if not 0 <= n["fret"] <= limit:
            raise RiffError("%s: fret %d out of range" % (where, n["fret"]))
        if "pm" in types and "tap" in types:
            raise RiffError("%s: a palm mute and a tap on one note" % where)
        for t in n["tech"]:
            if t["type"] in ("bend", "bendrelease", "prebend") and not 0.5 <= t["value"] <= max_bend:
                raise RiffError("%s: bend of %s semitones (0.5-%s)" % (where, t["value"], max_bend))
            if t["type"] == "natural" and n["fret"] not in NATURAL_NODES:
                raise RiffError("%s: natural harmonic off a node fret" % where)
            if t["type"] in ("slidelegato", "slideshift") and t["value"] == n["fret"]:
                raise RiffError("%s: slide to the fret it starts on" % where)
            if t["type"] == "trill" and t["value"] == n["fret"]:
                raise RiffError("%s: trill to its own fret" % where)
        if "hammer" in types or "pull" in types:
            chain = by_string[n["str"]]
            k = chain.index(n)
            prev = chain[k - 1] if k > 0 else None
            if prev is None or prev["_start"] + prev["_dur"] != n["_start"] \
                    or any(t["type"] == "dead" for t in prev["tech"]):
                raise RiffError("%s: a hammer-on or pull-off needs a sounding note before it on the same string"
                                % where)
            if "hammer" in types and n["fret"] <= prev["fret"] and "tap" not in [t["type"] for t in prev["tech"]]:
                raise RiffError("%s: a hammer-on must go up the neck" % where)
            if "pull" in types and n["fret"] >= prev["fret"]:
                raise RiffError("%s: a pull-off must go down the neck" % where)

    # ---- out ------------------------------------------------------------------
    notes.sort(key=lambda n: (n["_start"], n["str"]))
    note_json = []
    used = set()
    for n in notes:
        tl = sorted(n["tech"], key=lambda t: TECH_TOKENS.index(t["type"]))
        used.update(t["type"] for t in tl)
        note_json.append({"beat": r6(n["_start"]), "dur": r6(n["_dur"]), "str": n["str"],
                          "fret": n["fret"], "vel": n["vel"], "tech": tl})
    techniques = sorted(used, key=TECH_TOKENS.index)
    if strums:
        techniques.append("strum")
    for b in ["slap", "pop", "thump", "lhslap"]:
        if any(x["tech"] == b for x in bass_tech):
            techniques.append(b)

    stamp = date + "T00:00:00Z"
    doc = {
        "schema": 1,
        "magic": "luthier.riff",
        "meta": {"id": rid, "name": item.name, "author": author, "origin": origin, "tags": tags,
                 "created": stamp, "modified": stamp, "version_created": VERSION,
                 "version_modified": VERSION, "notes": " ".join(item.header.get("notes", []))},
        "riff": {"type": rtype, "genre": genre, "instrument": instrument, "tuning": tuning,
                 "tuning_name": tuning_name, "capo": capo,
                 "key": {"root": key[0], "scale": key[1]}, "tempo_bpm": tempo, "meter": [num, den],
                 "length_beats": r6(length), "feel": feel, "difficulty": difficulty,
                 "techniques": techniques, "chords": chords},
        "notes": note_json,
        "strums": strums,
        "bass_tech": bass_tech,
        "source": " ".join(source_parts),
    }
    item.json = doc
    item.free = free
    item.bars = bars
    item.tuning_key = tuning_key
    item.techniques = techniques
    item.difficulty = difficulty
    item.type = rtype
    item.instrument = instrument
    item.tempo = tempo
    item.length = float(length)
    item.pitches = [(float(n["_start"]), tuning[n["str"]] + capo + n["fret"], float(n["_dur"]))
                    for n in notes if not any(t["type"] == "dead" for t in n["tech"])]
    item.file = "%s/%s.luthierriff" % (GENRES[genre][0], rid)
    item.catalog = {
        "id": rid, "name": item.name, "file": item.file, "type": rtype, "genre": genre,
        "instrument": instrument, "tuning_name": tuning_name, "tuning": tuning, "capo": capo,
        "key": {"root": key[0], "scale": key[1]}, "tempo_bpm": tempo, "meter": [num, den],
        "length_beats": r6(length), "feel": feel, "difficulty": difficulty,
        "techniques": techniques, "tags": tags, "free": free, "author": author,
        "origin": origin, "notes": len(note_json),
    }


# ---------------------------------------------------------------------------
# Analysis (mirrors Source/Riffs/RiffAnalysis.cpp)
# ---------------------------------------------------------------------------


def estimate_difficulty(item):
    onsets = sorted(set(round(n["beat"], 6) for n in item.json["notes"]))
    seconds = item.length * 60.0 / item.tempo
    nps = len(onsets) / seconds if seconds > 0 else 0.0
    distinct = len(item.techniques)
    frets = [n["fret"] for n in item.json["notes"] if n["fret"] > 0]
    span = (max(frets) - min(frets)) if frets else 0
    est = round(1 + nps / 3.0 + distinct / 3.0 + max(0, span - 4) / 4.0)
    return max(1, min(5, int(est)))


def grams(item, n=6):
    """Interval-and-rhythm n-grams of the top line (riff-library 12)."""
    top = {}
    for start, pitch, dur in item.pitches:
        key = round(start, 6)
        if key not in top or pitch > top[key][0]:
            top[key] = (pitch, dur)
    line = [top[k] for k in sorted(top)]
    seq = [(line[i][0] - line[i - 1][0], round(line[i][1], 4)) for i in range(1, len(line))]
    return set(tuple(seq[i:i + n]) for i in range(0, len(seq) - n + 1))


# ---------------------------------------------------------------------------
# Rules
# ---------------------------------------------------------------------------


def load_deny():
    words = []
    path = os.path.join(RIFFDEFS, "deny.txt")
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.split("#", 1)[0].strip()
            if line:
                words.append(line.lower())
    return words


def denied(text, deny):
    low = text.lower().replace("-", " ")
    for d in deny:
        if re.search(r"(?<![a-z0-9])" + re.escape(d.replace("-", " ")) + r"(?![a-z0-9])", low):
            return d
    return None


def check_names(items, deny, errors):
    seen_ids, seen_names = {}, {}
    for it in items:
        if it.id in seen_ids:
            errors.append("%s: id '%s' also at %s" % (it.where, it.id, seen_ids[it.id]))
        seen_ids[it.id] = it.where
        if it.name.lower() in seen_names:
            errors.append("%s: name '%s' also at %s" % (it.where, it.name, seen_names[it.name.lower()]))
        seen_names[it.name.lower()] = it.where
        if len(it.name) >= 32:
            errors.append("%s: name '%s' is 32 characters or more" % (it.where, it.name))
        if not re.match(r"^[A-Z][A-Za-z0-9'&\-]*(?: [A-Za-z0-9'&\-]+)* [1-9][0-9]*$", it.name):
            errors.append("%s: name '%s' is not '[Descriptor] [Figure] [n]'" % (it.where, it.name))
        if "style" in it.name.lower():
            errors.append("%s: name '%s' says 'style'" % (it.where, it.name))
        for text in [it.name] + it.header.get("tags", []):
            d = denied(text, deny)
            if d:
                errors.append("%s: '%s' matches deny-list entry '%s'" % (it.where, text, d))
            b = trademark_in(text)
            if b:
                errors.append("%s: '%s' uses the trademark '%s'" % (it.where, text, b))


def check_coverage(items, genres, errors, warnings):
    by_genre = {}
    for it in items:
        by_genre.setdefault(it.genre, []).append(it)
    for g in genres:
        its = by_genre.get(g, [])
        counts = tuple(sum(1 for it in its if it.type == t) for t in TYPES)
        if counts != TABLE[g]:
            errors.append("%s: counts riff/lick/strum/bass are %s, the table says %s" % (g, counts, TABLE[g]))
        if len(its) < 20:
            errors.append("%s: fewer than 20 items" % g)
        for d in range(1, 5):
            if not any(it.difficulty == d for it in its):
                errors.append("%s: no item at difficulty %d" % (g, d))
        free = [it for it in its if it.free]
        if len(free) != 5:
            errors.append("%s: %d free items, needs 5" % (g, len(free)))
        if not any(it.type == "bass" for it in free):
            errors.append("%s: no bass line in the free set" % g)
        for it in free:
            if it.difficulty > 3:
                errors.append("%s: free item %s is above difficulty 3" % (it.where, it.id))
        if g == "metal":
            if sum(1 for it in its if it.tuning_key == "drop_d") < 4:
                errors.append("metal: fewer than 4 Drop D items")
            if sum(1 for it in its if it.instrument == "guitar7") < 2:
                errors.append("metal: fewer than 2 7-string items")
        if g in ("funk", "soul"):
            slappers = [it for it in its if it.type == "bass"
                        and ("slap" in it.techniques or "pop" in it.techniques)]
            if len(slappers) < 2:
                errors.append("%s: fewer than 2 slap / pop bass lines" % g)

    if set(genres) == set(GENRES):
        easy = sum(1 for it in items if it.difficulty <= 2)
        if easy * 100 < 40 * len(items):
            errors.append("difficulty 1-2 is %d of %d, needs 40%%" % (easy, len(items)))
        for t, least in TECH_MINIMUM.items():
            n = sum(1 for it in items if t in it.techniques)
            if n < least:
                errors.append("technique '%s' is in %d items, needs %d" % (t, n, least))
        if sum(1 for it in items if it.instrument == "bass5") < 6:
            errors.append("fewer than 6 5-string bass items")
        if len(items) < 300:
            errors.append("%d items, needs 300" % len(items))

    for it in items:
        est = estimate_difficulty(it)
        if abs(est - it.difficulty) > 2:
            warnings.append("%s: %s difficulty %d, the estimate is %d" % (it.where, it.id, it.difficulty, est))


def check_similarity(items, errors, limit=0.8):
    sets = [(it, grams(it)) for it in items]
    sets = [(it, g) for it, g in sets if g]
    for (a, ga), (b, gb) in itertools.combinations(sets, 2):
        shared = len(ga & gb)
        if shared and shared > limit * min(len(ga), len(gb)):
            errors.append("%s and %s share %d of their %d interval-and-rhythm 6-grams (limit 80%%)"
                          % (a.id, b.id, shared, min(len(ga), len(gb))))


# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------


def write_tree(items, root):
    if os.path.isdir(root):
        shutil.rmtree(root)
    os.makedirs(root)
    for it in items:
        path = os.path.join(root, it.file)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(canonical(it.json))
    ordered = sorted(items, key=lambda it: it.id)
    catalog = {"schema": 1, "magic": "luthier.riffcatalog", "version": VERSION,
               "count": len(items), "items": [it.catalog for it in ordered]}
    with open(os.path.join(root, "catalog.json"), "w", encoding="utf-8", newline="\n") as f:
        f.write(canonical(catalog))
    with open(os.path.join(root, "free.txt"), "w", encoding="utf-8", newline="\n") as f:
        for it in ordered:
            if it.free:
                f.write(it.file + "\n")
    strings = {}
    for it in ordered:
        strings["riff.%s.name" % it.id] = it.name
    for g, (_folder, display) in GENRES.items():
        strings["riff.genre.%s" % g] = display
    for t in TYPES:
        strings["riff.type.%s" % t] = t.capitalize()
    for t, name in TECH_NAMES.items():
        strings["riff.tech.%s" % t] = name
    for tag in sorted(set(tag for it in items for tag in it.header.get("tags", []))):
        strings["riff.tag.%s" % tag] = tag.replace("-", " ")
    with open(os.path.join(root, "strings.en.json"), "w", encoding="utf-8", newline="\n") as f:
        f.write(canonical(strings))


def trees_equal(a, b):
    def listing(root):
        out = []
        for dirpath, _dirs, files in os.walk(root):
            for name in files:
                out.append(os.path.relpath(os.path.join(dirpath, name), root))
        return sorted(out)
    la, lb = listing(a), listing(b)
    if la != lb:
        return False, "file lists differ: %s" % sorted(set(la) ^ set(lb))[:10]
    for rel in la:
        if not filecmp.cmp(os.path.join(a, rel), os.path.join(b, rel), shallow=False):
            return False, "%s differs" % rel
    return True, ""


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--check", action="store_true", help="compare with Resources/Riffs, write nothing")
    ap.add_argument("--only", action="append", default=[], help="validate these genres only")
    ap.add_argument("--out", default=OUT, help="output folder")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args(argv)

    genres = args.only or sorted(GENRES)
    for g in genres:
        if g not in GENRES:
            print("unknown genre '%s'" % g, file=sys.stderr)
            return 2
    paths = [os.path.join(RIFFDEFS, g + ".riffdef") for g in genres]
    missing = [p for p in paths if not os.path.exists(p)]
    if missing:
        print("missing: %s" % ", ".join(missing), file=sys.stderr)
        return 1

    errors, warnings = [], []
    try:
        items = read_riffdefs(paths)
    except RiffError as e:
        print("error: %s" % e, file=sys.stderr)
        return 1
    good = []
    for it in items:
        try:
            compile_item(it)
            good.append(it)
        except RiffError as e:
            errors.append(str(e))
    if not errors:
        check_names(good, load_deny(), errors)
        check_coverage(good, genres, errors, warnings)
        check_similarity(good, errors)

    if args.verbose:
        for wline in warnings:
            print("warning: %s" % wline, file=sys.stderr)
    elif warnings:
        print("%d difficulty warnings (-v to list)" % len(warnings), file=sys.stderr)
    for e in errors:
        print("error: %s" % e, file=sys.stderr)
    if errors:
        print("%d errors" % len(errors), file=sys.stderr)
        return 1

    if args.only:
        print("%d items in %s: OK" % (len(good), ", ".join(genres)))
        return 0

    if args.check:
        tmp = tempfile.mkdtemp()
        try:
            write_tree(good, os.path.join(tmp, "Riffs"))
            same, why = trees_equal(os.path.join(tmp, "Riffs"), args.out)
        finally:
            shutil.rmtree(tmp)
        if not same:
            print("Resources/Riffs is stale: %s. Rerun Tools/generate_factory_riffs.py" % why, file=sys.stderr)
            return 1
        print("%d riffs: Resources/Riffs is up to date" % len(good))
        return 0

    write_tree(good, args.out)
    print("%d riffs written to %s" % (len(good), os.path.relpath(args.out, REPO)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
