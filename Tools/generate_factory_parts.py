"""Generates Luthier's factory parts and guitars (factory-content.md 2-3).

The values come from part-acoustics.md's tables - wood density, stiffness and
damping; bridge mass and coupling; magnet pull; pickup L/R/C - and from the
published specifications of the kinds of hardware each part is named after.
Every part name carries its physical fact ("PAF 57 Alnico 2 7.6k"), and no
name is a trademark (factory-content.md 0.7).

Run from the repository root:  python Tools/generate_factory_parts.py
It rewrites Resources/Parts and Resources/Guitars completely.
"""

import json
import os
import shutil

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Resources")
PARTS = os.path.join(ROOT, "Parts")
GUITARS = os.path.join(ROOT, "Guitars")

FOLDERS = {
    "body": "Bodies", "top": "Tops", "neck": "Necks", "fretboard": "Fretboards",
    "frets": "Frets", "nut": "Nuts", "bridge": "Bridges", "tailpiece": "Tailpieces",
    "tuners": "Tuners", "pickup": "Pickups", "wiring": "Wiring", "strings": "Strings",
    "pickguard": "Pickguards", "slide": "Slides", "pick": "Picks", "capo": "Capos",
}

# part-acoustics.md 1: density kg/m3, E GPa, damping tan(delta) x 1e-3.
WOOD = {
    "alder": (420, 9.5, 8.5), "ash_swamp": (480, 11.0, 7.5), "ash_northern": (680, 13.0, 6.5),
    "basswood": (420, 9.0, 11.0), "mahogany": (550, 10.5, 9.0), "mahogany_african": (530, 9.8, 9.5),
    "maple_hard": (705, 12.6, 6.0), "maple_soft": (545, 10.0, 7.5), "korina": (480, 10.0, 8.5),
    "poplar": (455, 10.9, 10.0), "walnut": (610, 11.5, 7.0), "rosewood": (830, 12.0, 6.0),
    "ebony": (1040, 16.0, 4.5), "pau_ferro": (860, 13.5, 5.5), "spruce": (400, 11.0, 7.0),
    "cedar": (350, 8.0, 9.0), "koa": (610, 10.5, 8.0), "cypress": (510, 9.0, 7.8),
}

parts = []   # (type, name, fields, meta extras)


def part(ptype, name, fields, compat=("electric",), tags=(), default=False, illustration=None):
    parts.append((ptype, name, fields, list(compat), list(tags), default, illustration))
    return "%s/%s.luthierpart" % (FOLDERS[ptype], name)


def wood(key, **extra):
    density = WOOD[key][0]
    d = {"wood": key, "density_kg_m3": density}
    d.update(extra)
    return d


# --- bodies (12) -------------------------------------------------------------------
B = {}
B["alder_dc"] = part("body", "Alder Double-Cut", wood("alder", chambering="solid", thickness_mm=44.5, area_cm2=1150, bracing="none"), default=True, tags=["solid", "alder"])
B["alder_offset"] = part("body", "Alder Offset", wood("alder", chambering="solid", thickness_mm=44.5, area_cm2=1230, bracing="none"), tags=["solid", "offset"])
B["ash_t"] = part("body", "Ash T-Slab", wood("ash_swamp", chambering="solid", thickness_mm=44.5, area_cm2=1100, bracing="none"), tags=["solid", "ash"])
B["mahogany_sc"] = part("body", "Mahogany Single-Cut", wood("mahogany", chambering="chambered", thickness_mm=50.0, area_cm2=1080, bracing="none"), tags=["mahogany", "weight-relieved"])
B["mahogany_thin"] = part("body", "Mahogany SG-Thin", wood("mahogany", chambering="solid", thickness_mm=34.0, area_cm2=1000, bracing="none"), tags=["mahogany", "thin"])
B["basswood_ss"] = part("body", "Basswood Superstrat", wood("basswood", chambering="solid", thickness_mm=40.0, area_cm2=1120, bracing="none"), tags=["basswood", "modern"])
B["swamp_ash_ms"] = part("body", "Swamp Ash Multi-Scale", wood("ash_swamp", chambering="solid", thickness_mm=44.0, area_cm2=1250, bracing="none"), tags=["ash", "extended range"])
B["semi_335"] = part("body", "Semi-Hollow 335 Body", wood("maple_soft", chambering="semi_hollow", thickness_mm=44.5, area_cm2=1500, bracing="none"), tags=["semi-hollow", "laminated maple"])
B["archtop"] = part("body", "Full Hollow Archtop Body", wood("maple_soft", chambering="hollow", thickness_mm=76.0, area_cm2=1800, bracing="x"), compat=("electric", "acoustic"), tags=["hollow", "archtop"])
B["steel_reso"] = part("body", "Steel Resonator Body", {"wood": "steel", "density_kg_m3": 7850, "chambering": "acoustic", "thickness_mm": 80.0, "area_cm2": 1750, "bracing": "none"}, compat=("resonator",), tags=["resonator", "steel"])
B["wood_reso"] = part("body", "Wood Resonator Body", wood("maple_soft", chambering="acoustic", thickness_mm=85.0, area_cm2=1800, bracing="ladder"), compat=("resonator",), tags=["resonator"])
B["bass_p"] = part("body", "Bass P-Style Body", wood("alder", chambering="solid", thickness_mm=44.5, area_cm2=1400, bracing="none"), compat=("bass",), default=False, tags=["bass", "alder"])
# Acoustic bodies: the factory list names the guitars' back and sides; these carry them.
B["dread"] = part("body", "Dreadnought Mahogany Body", wood("mahogany", chambering="acoustic", thickness_mm=120.0, area_cm2=2300, bracing="x"), compat=("acoustic",), tags=["acoustic", "dreadnought"])
B["ga"] = part("body", "Grand Auditorium Rosewood Body", wood("rosewood", chambering="acoustic", thickness_mm=115.0, area_cm2=2150, bracing="x"), compat=("acoustic",), tags=["acoustic"])
B["parlor"] = part("body", "Parlor Mahogany Body", wood("mahogany", chambering="acoustic", thickness_mm=95.0, area_cm2=1600, bracing="ladder"), compat=("acoustic",), tags=["acoustic", "parlor"])
B["jumbo"] = part("body", "Jumbo Maple Body", wood("maple_hard", chambering="acoustic", thickness_mm=125.0, area_cm2=2600, bracing="scalloped_x"), compat=("acoustic",), tags=["acoustic", "jumbo"])
B["classical"] = part("body", "Classical Rosewood Body", wood("rosewood", chambering="acoustic", thickness_mm=100.0, area_cm2=2000, bracing="fan"), compat=("classical",), tags=["classical"])
B["flamenca"] = part("body", "Flamenca Cypress Body", wood("cypress", chambering="acoustic", thickness_mm=90.0, area_cm2=1950, bracing="fan"), compat=("classical",), tags=["flamenco"])
B["selmer"] = part("body", "Selmer-Style Walnut Body", wood("walnut", chambering="acoustic", thickness_mm=100.0, area_cm2=2200, bracing="ladder"), compat=("acoustic",), tags=["gypsy jazz"])
B["hollow_bass"] = part("body", "Thinline Hollow Bass Body", wood("maple_soft", chambering="semi_hollow", thickness_mm=50.0, area_cm2=1700, bracing="none"), compat=("bass",), tags=["bass", "hollow"])

# --- tops (5) --------------------------------------------------------------------------
T = {}
T["flame"] = part("top", "Flame Maple", wood("maple_hard", thickness_mm=16.0, carve="carved"), tags=["maple", "carved"])
T["quilt"] = part("top", "Quilt Maple", wood("maple_hard", thickness_mm=6.0, carve="flat"), tags=["maple"])
T["plain"] = part("top", "Plain Maple", wood("maple_hard", thickness_mm=12.0, carve="carved"), default=True, tags=["maple"])
T["spalted"] = part("top", "Spalted Maple", wood("maple_soft", thickness_mm=6.0, carve="flat"), tags=["maple", "figured"])
T["cedar"] = part("top", "Cedar (acoustic)", wood("cedar", thickness_mm=2.7, carve="flat"), compat=("acoustic", "classical"), tags=["acoustic top"])
T["spruce"] = part("top", "Sitka Spruce (acoustic)", wood("spruce", thickness_mm=2.8, carve="flat"), compat=("acoustic", "classical"), tags=["acoustic top"])

# --- necks (8) -----------------------------------------------------------------------------
N = {}
N["maple_c"] = part("neck", "Maple Bolt-On C", wood("maple_hard", profile="C", scale_length_mm=648.0, joint="bolt", truss="single", strings=6, frets=22), default=True, tags=["bolt-on", "25.5"])
N["maple_v"] = part("neck", "Maple Bolt-On V", wood("maple_hard", profile="V", scale_length_mm=648.0, joint="bolt", truss="single", strings=6, frets=21), tags=["bolt-on", "vintage"])
N["mahogany_set"] = part("neck", "Mahogany Set-Neck", wood("mahogany", profile="59 round", scale_length_mm=628.0, joint="set", truss="single", strings=6, frets=22), tags=["set neck", "24.75"])
N["maple_set"] = part("neck", "Maple Set-Neck", wood("maple_hard", profile="C", scale_length_mm=628.0, joint="set", truss="single", strings=6, frets=22), tags=["set neck"])
N["mahogany_thin"] = part("neck", "Mahogany Thin", wood("mahogany", profile="slim taper", scale_length_mm=628.0, joint="set", truss="single", strings=6, frets=22), tags=["set neck", "thin"])
N["multi"] = part("neck", "Multi-Scale", wood("maple_hard", profile="thin D", scale_length_mm=698.5, joint="through", truss="dual", strings=8, frets=24), tags=["extended range", "fanned"])
N["seven"] = part("neck", "7-String Through-Neck", wood("maple_hard", profile="thin D", scale_length_mm=673.0, joint="through", truss="dual", strings=7, frets=24), tags=["extended range"])
N["baritone"] = part("neck", "Baritone Bolt-On", wood("maple_hard", profile="C", scale_length_mm=686.0, joint="bolt", truss="single", strings=6, frets=24), tags=["baritone"])
N["bass_p"] = part("neck", "Bass P-Style Neck", wood("maple_hard", profile="C", scale_length_mm=864.0, joint="bolt", truss="single", strings=4, frets=20), compat=("bass",), tags=["bass", "34"])
N["bass_j"] = part("neck", "Bass J-Style Neck", wood("maple_hard", profile="slim C", scale_length_mm=864.0, joint="bolt", truss="single", strings=4, frets=20), compat=("bass",), tags=["bass", "34"])
N["bass_5"] = part("neck", "Bass 5-String Neck", wood("maple_hard", profile="D", scale_length_mm=864.0, joint="bolt", truss="dual", strings=5, frets=21), compat=("bass",), tags=["bass"])
N["acoustic"] = part("neck", "Acoustic Mahogany Neck", wood("mahogany", profile="C", scale_length_mm=645.0, joint="set", truss="single", strings=6, frets=20), compat=("acoustic",), tags=["acoustic"])
N["acoustic12"] = part("neck", "12-String Mahogany Neck", wood("mahogany", profile="wide C", scale_length_mm=645.0, joint="set", truss="dual", strings=12, frets=20), compat=("acoustic",), tags=["12-string"])
N["classical"] = part("neck", "Classical Cedar Neck", wood("cedar", profile="D", scale_length_mm=650.0, joint="set", truss="none", strings=6, frets=19), compat=("classical",), tags=["classical"])
N["reso"] = part("neck", "Resonator Round Neck", wood("mahogany", profile="C", scale_length_mm=635.0, joint="set", truss="single", strings=6, frets=19), compat=("resonator",), tags=["resonator"])

# --- fretboards (5) -------------------------------------------------------------------------
F = {}
F["rosewood"] = part("fretboard", "Rosewood", wood("rosewood", radius_mm=305.0, thickness_mm=6.0), compat=("any",), default=True)
F["ebony"] = part("fretboard", "Ebony", wood("ebony", radius_mm=406.0, thickness_mm=6.0), compat=("any",))
F["maple"] = part("fretboard", "Maple", wood("maple_hard", radius_mm=241.0, thickness_mm=6.0), compat=("any",))
F["pau_ferro"] = part("fretboard", "Pau Ferro", wood("pau_ferro", radius_mm=305.0, thickness_mm=6.0), compat=("any",))
F["ebony_bound"] = part("fretboard", "Ebony Bound", wood("ebony", radius_mm=305.0, thickness_mm=6.5), compat=("any",))
F["flat"] = part("fretboard", "Flat Rosewood (classical)", wood("rosewood", radius_mm=10000.0, thickness_mm=6.0), compat=("classical",))

# --- frets (4) --------------------------------------------------------------------------------
FR = {}
FR["mj"] = part("frets", "Medium-Jumbo Nickel-Silver", {"material": "nickel_silver", "height_mm": 1.14, "width_mm": 2.54, "count": 22, "stainless": False}, compat=("any",), default=True)
FR["jumbo_ss"] = part("frets", "Jumbo Stainless", {"material": "stainless", "height_mm": 1.40, "width_mm": 2.79, "count": 24, "stainless": True}, compat=("any",))
FR["vintage"] = part("frets", "Vintage Small Nickel-Silver", {"material": "nickel_silver", "height_mm": 0.89, "width_mm": 2.03, "count": 21, "stainless": False}, compat=("any",))
FR["fretless"] = part("frets", "Fretless (unlined)", {"material": "none", "height_mm": 0.0, "width_mm": 0.0, "count": 0, "stainless": False}, compat=("any",))

# --- nuts (4) -------------------------------------------------------------------------------------
NU = {}
NU["bone43"] = part("nut", "Bone 43mm", {"material": "bone", "width_mm": 43.0, "slot_depths_mm": [0.45] * 6, "friction": 0.3}, compat=("any",), default=True)
NU["bone42"] = part("nut", "Bone 42mm (Vintage)", {"material": "bone", "width_mm": 42.0, "slot_depths_mm": [0.5] * 6, "friction": 0.35}, compat=("any",))
NU["graphite"] = part("nut", "Graphite Locking 42mm", {"material": "graphite", "width_mm": 42.0, "slot_depths_mm": [0.4] * 6, "friction": 0.1}, compat=("any",))
NU["brass"] = part("nut", "Brass 43mm", {"material": "brass", "width_mm": 43.0, "slot_depths_mm": [0.45] * 6, "friction": 0.4}, compat=("any",))
NU["bass"] = part("nut", "Bone Bass 38mm", {"material": "bone", "width_mm": 38.0, "slot_depths_mm": [0.6] * 5, "friction": 0.3}, compat=("bass",))

# --- bridges (10) -- part-acoustics.md 5's table ---------------------------------------------------
BR = {}
BR["abr1"] = part("bridge", "ABR-1 Tune-o-Matic", {"type": "tune_o_matic", "mass_g": 95, "coupling": 0.55, "has_tremolo": False, "tremolo_type": "none", "spring_count": 0, "piezo": False, "strings": 6})
BR["modern_tom"] = part("bridge", "Modern TOM", {"type": "tune_o_matic", "mass_g": 100, "coupling": 0.58, "has_tremolo": False, "tremolo_type": "none", "spring_count": 0, "piezo": False, "strings": 6})
BR["vintage_trem"] = part("bridge", "Vintage 6-Point Trem", {"type": "vintage_tremolo", "mass_g": 165, "coupling": 0.45, "has_tremolo": True, "tremolo_type": "vintage", "spring_count": 3, "piezo": False, "strings": 6}, default=True)
BR["two_point"] = part("bridge", "Modern 2-Point Trem", {"type": "two_point_tremolo", "mass_g": 150, "coupling": 0.48, "has_tremolo": True, "tremolo_type": "two_point", "spring_count": 3, "piezo": False, "strings": 6})
BR["floyd"] = part("bridge", "Floyd Rose Style", {"type": "floyd_rose", "mass_g": 320, "coupling": 0.30, "has_tremolo": True, "tremolo_type": "floyd", "spring_count": 3, "piezo": False, "strings": 6})
BR["hardtail"] = part("bridge", "Hardtail String-Thru", {"type": "hardtail", "mass_g": 110, "coupling": 0.70, "has_tremolo": False, "tremolo_type": "none", "spring_count": 0, "piezo": False, "strings": 6})
BR["hardtail7"] = part("bridge", "Hardtail 7-String", {"type": "hardtail", "mass_g": 125, "coupling": 0.70, "has_tremolo": False, "tremolo_type": "none", "spring_count": 0, "piezo": False, "strings": 7})
BR["hardtail8"] = part("bridge", "Hardtail 8-String Fanned", {"type": "hardtail", "mass_g": 140, "coupling": 0.70, "has_tremolo": False, "tremolo_type": "none", "spring_count": 0, "piezo": False, "strings": 8})
BR["wrap"] = part("bridge", "Wraparound", {"type": "wraparound", "mass_g": 85, "coupling": 0.60, "has_tremolo": False, "tremolo_type": "none", "spring_count": 0, "piezo": False, "strings": 6})
BR["bigsby"] = part("bridge", "Bigsby-Style Vibrato", {"type": "bigsby", "mass_g": 480, "coupling": 0.35, "has_tremolo": True, "tremolo_type": "bigsby", "spring_count": 1, "piezo": False, "strings": 6})
BR["bass_p"] = part("bridge", "Bass P-Style Bridge", {"type": "hardtail", "mass_g": 120, "coupling": 0.65, "has_tremolo": False, "tremolo_type": "none", "spring_count": 0, "piezo": False, "strings": 4}, compat=("bass",))
BR["bass_badass"] = part("bridge", "Bass BadAss-Style", {"type": "hardtail", "mass_g": 230, "coupling": 0.60, "has_tremolo": False, "tremolo_type": "none", "spring_count": 0, "piezo": False, "strings": 4}, compat=("bass",))
BR["bass_5"] = part("bridge", "Bass 5-String Bridge", {"type": "hardtail", "mass_g": 150, "coupling": 0.63, "has_tremolo": False, "tremolo_type": "none", "spring_count": 0, "piezo": False, "strings": 5}, compat=("bass",))
BR["pin"] = part("bridge", "Acoustic Pin Bridge", {"type": "pin_bridge", "mass_g": 28, "coupling": 0.92, "has_tremolo": False, "tremolo_type": "none", "spring_count": 0, "piezo": True, "strings": 6}, compat=("acoustic",))
BR["pin12"] = part("bridge", "Acoustic 12-String Pin Bridge", {"type": "pin_bridge", "mass_g": 34, "coupling": 0.90, "has_tremolo": False, "tremolo_type": "none", "spring_count": 0, "piezo": True, "strings": 12}, compat=("acoustic",))
BR["tie"] = part("bridge", "Classical Tie Bridge", {"type": "tie_block", "mass_g": 22, "coupling": 0.94, "has_tremolo": False, "tremolo_type": "none", "spring_count": 0, "piezo": False, "strings": 6}, compat=("classical",))
BR["floating"] = part("bridge", "Floating Archtop Bridge", {"type": "floating", "mass_g": 30, "coupling": 0.80, "has_tremolo": False, "tremolo_type": "none", "spring_count": 0, "piezo": False, "strings": 6}, compat=("acoustic", "electric"))
BR["biscuit"] = part("bridge", "Resonator Biscuit Cone", {"type": "resonator_spider", "mass_g": 45, "coupling": 0.88, "has_tremolo": False, "tremolo_type": "none", "spring_count": 0, "piezo": False, "strings": 6}, compat=("resonator",))

# --- tailpieces (3) ------------------------------------------------------------------------------------
TP = {}
TP["stopbar"] = part("tailpiece", "Stopbar", {"type": "stopbar", "mass_g": 60, "break_angle_deg": 14}, default=True)
TP["trapeze"] = part("tailpiece", "Trapeze", {"type": "trapeze", "mass_g": 70, "break_angle_deg": 8}, compat=("electric", "acoustic"))
TP["vibrola"] = part("tailpiece", "Vibrola-Style", {"type": "vibrola", "mass_g": 110, "break_angle_deg": 10})

# --- tuners (5) --------------------------------------------------------------------------------------------
TU = {}
TU["kluson"] = part("tuners", "Kluson 15 to 1", {"ratio": 15, "mass_g": 30, "stability": 0.7, "locking": False}, compat=("any",), default=True)
TU["sealed"] = part("tuners", "Modern Sealed 18 to 1", {"ratio": 18, "mass_g": 40, "stability": 0.85, "locking": False}, compat=("any",))
TU["locking"] = part("tuners", "Locking 21 to 1", {"ratio": 21, "mass_g": 42, "stability": 0.95, "locking": True}, compat=("any",))
TU["open"] = part("tuners", "Vintage Open-Back", {"ratio": 14, "mass_g": 22, "stability": 0.6, "locking": False}, compat=("any",))
TU["bass"] = part("tuners", "Bass 20 to 1", {"ratio": 20, "mass_g": 70, "stability": 0.85, "locking": False}, compat=("bass",))

# --- pickups (18) --------------------------------------------------------------------------------------------
def pickup(name, family, L, R, C, magnet, turns, pole, cover, output, compat=("electric",), default=False):
    return part("pickup", name, {"family": family, "inductance_h": L, "dc_resistance_k": R, "capacitance_pf": C,
                                 "magnet": magnet, "coil_turns": turns, "pole_piece_material": pole,
                                 "cover": cover, "output_dbfs_reference": output}, compat=compat, default=default)

P = {}
P["paf57"] = pickup("PAF 57 Alnico 2 7.6k", "humbucker", 4.5, 7.6, 150, "alnico2", 5000, "steel", "nickel", -6.2)
P["paf59"] = pickup("PAF 59 Alnico 5 8.1k", "humbucker", 4.8, 8.1, 160, "alnico5", 5200, "steel", "nickel", -5.4, default=True)
P["hot"] = pickup("Modern High-Output Ceramic 15k", "humbucker", 8.2, 15.0, 190, "ceramic", 8000, "steel", "none", -1.5)
P["mini"] = pickup("Mini-Humbucker Alnico 5 6.8k", "mini_humbucker", 3.2, 6.8, 120, "alnico5", 4400, "steel", "chrome", -7.0)
P["firebird"] = pickup("Firebird-Style Mini", "mini_humbucker", 2.6, 6.0, 110, "alnico5", 3600, "alnico", "chrome", -8.0)
P["sc54"] = pickup("Vintage 54 Alnico 3 5.8k", "single_coil", 2.3, 5.8, 110, "alnico3", 7600, "alnico", "plastic", -8.5)
P["noiseless"] = pickup("Modern Noiseless 6.5k", "single_coil", 2.9, 6.5, 130, "alnico5", 8200, "alnico", "plastic", -7.8)
P["t_neck"] = pickup("T-Style Neck 7.5k", "single_coil", 2.8, 7.5, 150, "alnico5", 8200, "alnico", "chrome", -8.2)
P["t_bridge"] = pickup("T-Style Bridge 8.5k", "single_coil", 3.3, 8.5, 140, "alnico5", 9200, "alnico", "none", -7.0)
P["p90"] = pickup("P90 Alnico 5 8.2k", "p90", 4.0, 8.2, 200, "alnico5", 10000, "steel", "plastic", -6.0)
P["emg81"] = pickup("EMG-Style 81 Active", "active", 1.2, 10.0, 60, "ceramic", 1500, "steel", "plastic", -2.0)
P["emg60"] = pickup("EMG-Style 60 Active", "active", 1.1, 10.0, 60, "ceramic", 1500, "steel", "plastic", -3.0)
P["split_p"] = pickup("Split-P Alnico 5 11k", "split_coil", 7.0, 11.0, 220, "alnico5", 10000, "alnico", "plastic", -4.0, compat=("bass",))
P["j_neck"] = pickup("J-Neck Alnico 5 7.5k", "single_coil", 3.5, 7.5, 170, "alnico5", 8600, "alnico", "plastic", -6.5, compat=("bass",))
P["j_bridge"] = pickup("J-Bridge Alnico 5 8.0k", "single_coil", 3.8, 8.0, 175, "alnico5", 9000, "alnico", "plastic", -6.0, compat=("bass",))
P["mm"] = pickup("Music Man-Style Ceramic Humbucker 13k", "humbucker", 6.5, 13.0, 200, "ceramic", 7500, "steel", "plastic", -3.0, compat=("bass",))
P["piezo"] = pickup("Under-Saddle Piezo", "piezo", 0.0, 1.0, 0.0, "none", 0, "none", "none", -9.0, compat=("acoustic", "classical"))
P["soundhole"] = pickup("Soundhole Magnetic", "soundhole", 5.0, 7.5, 220, "alnico5", 6000, "steel", "none", -8.0, compat=("acoustic",))
P["floating_hb"] = pickup("Floating Jazz Humbucker", "humbucker", 5.5, 9.0, 170, "alnico5", 5600, "steel", "none", -7.5, compat=("electric", "acoustic"))

# --- wiring (8) --------------------------------------------------------------------------------------------------
W = {}
def wiring(name, vol, tone, cap, taper, bleed, switching, active=False, compat=("electric",), default=False):
    return part("wiring", name, {"volume_pot_ohm": vol, "tone_pot_ohm": tone, "tone_cap_f": cap, "taper": taper,
                                 "treble_bleed": bleed, "switching": switching, "active": active}, compat=compat, default=default)

W["lp50"] = wiring("50s LP Wiring", 500e3, 500e3, 22e-9, "fifties", "none", "3way_independent")
W["lp_modern"] = wiring("Modern LP Wiring", 500e3, 500e3, 22e-9, "audio", "none", "3way_independent")
W["strat_vintage"] = wiring("Vintage Strat Wiring", 250e3, 250e3, 47e-9, "audio", "none", "5way", default=True)
W["strat_modern"] = wiring("Modern Strat Wiring", 250e3, 250e3, 22e-9, "audio", "kinman", "5way")
W["t_style"] = wiring("T-Style Wiring", 250e3, 250e3, 47e-9, "audio", "fender", "3way")
W["emg"] = wiring("Active EMG Wiring", 25e3, 25e3, 47e-9, "audio", "none", "3way", True)
W["bass_passive"] = wiring("Bass Passive 2V1T", 250e3, 250e3, 47e-9, "audio", "none", "independent_volumes", compat=("bass",))
W["bass_active"] = wiring("Bass 3-Band Active Preamp", 25e3, 25e3, 22e-9, "audio", "none", "blend", True, compat=("bass",))
W["acoustic"] = wiring("Acoustic Preamp", 25e3, 25e3, 22e-9, "audio", "none", "single", True, compat=("acoustic", "classical", "resonator"))

# --- strings (12) --------------------------------------------------------------------------------------------------
S = {}
def strings(name, gauges, winding, material, core, compat=("electric",), default=False):
    return part("strings", name, {"gauges_in": gauges, "winding": winding, "winding_material": material, "core": core},
                compat=compat, default=default)

S["8"] = strings("8-38 Extra Light NPS", [0.008, 0.010, 0.015, 0.021, 0.030, 0.038], "round", "nickel_plated_steel", "hex")
S["9"] = strings("9-42 Super Light NPS", [0.009, 0.011, 0.016, 0.024, 0.032, 0.042], "round", "nickel_plated_steel", "hex")
S["10"] = strings("10-46 Regular NPS", [0.010, 0.013, 0.017, 0.026, 0.036, 0.046], "round", "nickel_plated_steel", "hex", default=True)
S["11"] = strings("11-49 Medium NPS", [0.011, 0.014, 0.018, 0.028, 0.038, 0.049], "round", "nickel_plated_steel", "hex")
S["10_52"] = strings("10-52 Skinny-Top Heavy-Bottom", [0.010, 0.013, 0.017, 0.030, 0.042, 0.052], "round", "nickel_plated_steel", "hex")
S["7str"] = strings("10-59 Seven-String NPS", [0.010, 0.013, 0.017, 0.026, 0.036, 0.046, 0.059], "round", "nickel_plated_steel", "hex")
S["8str"] = strings("10-74 Eight-String NPS", [0.010, 0.013, 0.017, 0.026, 0.036, 0.046, 0.059, 0.074], "round", "nickel_plated_steel", "hex")
S["baritone"] = strings("14-68 Baritone NPS", [0.014, 0.018, 0.026, 0.036, 0.050, 0.068], "round", "nickel_plated_steel", "hex")
S["pb"] = strings("12-53 Phosphor Bronze", [0.012, 0.016, 0.024, 0.032, 0.042, 0.053], "round", "phosphor_bronze", "hex", compat=("acoustic", "resonator"))
S["8020"] = strings("12-54 80-20 Bronze", [0.012, 0.016, 0.025, 0.032, 0.042, 0.054], "round", "bronze_8020", "hex", compat=("acoustic", "resonator"))
S["silk"] = strings("11-52 Silk & Steel", [0.011, 0.015, 0.022, 0.030, 0.042, 0.052], "round", "silk_steel", "round", compat=("acoustic",))
S["pb12"] = strings("10-47 12-String Phosphor Bronze", [0.010, 0.010, 0.014, 0.014, 0.023, 0.008, 0.030, 0.012, 0.039, 0.018, 0.047, 0.027], "round", "phosphor_bronze", "hex", compat=("acoustic",))
S["nylon_n"] = strings("Normal Tension Nylon", [0.028, 0.032, 0.040, 0.029, 0.035, 0.043], "round", "nylon", "round", compat=("classical",))
S["nylon_h"] = strings("High Tension Nylon", [0.029, 0.033, 0.041, 0.030, 0.036, 0.044], "round", "nylon", "round", compat=("classical",))
S["bass_n"] = strings("45-105 Nickel Roundwound", [0.045, 0.065, 0.085, 0.105], "round", "nickel_plated_steel", "hex", compat=("bass",))
S["bass_ss"] = strings("45-105 Stainless Roundwound", [0.045, 0.065, 0.085, 0.105], "round", "stainless", "hex", compat=("bass",))
S["bass_flat"] = strings("50-105 Flatwound", [0.050, 0.070, 0.085, 0.105], "flat", "stainless", "round", compat=("bass",))
S["bass_5"] = strings("45-130 Five-String Nickel", [0.045, 0.065, 0.085, 0.105, 0.130], "round", "nickel_plated_steel", "hex", compat=("bass",))

# --- pickguards (6) ---------------------------------------------------------------------------------------------------
PG = {}
PG["cream"] = part("pickguard", "Cream 3-Ply", {"material": "pvc", "plies": 3, "mass_g": 45}, compat=("any",))
PG["black"] = part("pickguard", "Black 3-Ply", {"material": "pvc", "plies": 3, "mass_g": 45}, compat=("any",), default=True)
PG["white"] = part("pickguard", "White 1-Ply", {"material": "pvc", "plies": 1, "mass_g": 25}, compat=("any",))
PG["tortoise"] = part("pickguard", "Tortoise 3-Ply", {"material": "celluloid", "plies": 3, "mass_g": 48}, compat=("any",))
PG["mint"] = part("pickguard", "Mint Green", {"material": "celluloid", "plies": 3, "mass_g": 46}, compat=("any",))
PG["none"] = part("pickguard", "None", {"material": "none", "plies": 0, "mass_g": 0}, compat=("any",))

# --- accessories: picks (6), slides (5), capos (3) --------------------------------------------------------------------
part("pick", "Celluloid 0.73 Standard", {"material": "celluloid", "thickness_mm": 0.73, "tip_radius_mm": 1.0, "bevel": 0.2, "wear": 0.1}, compat=("any",), default=True)
part("pick", "Nylon 0.60 Standard", {"material": "nylon", "thickness_mm": 0.60, "tip_radius_mm": 1.0, "bevel": 0.2, "wear": 0.1}, compat=("any",))
part("pick", "Delrin 1.0 Standard", {"material": "delrin", "thickness_mm": 1.0, "tip_radius_mm": 1.0, "bevel": 0.2, "wear": 0.05}, compat=("any",))
part("pick", "Ultex 1.5 Sharp", {"material": "ultex", "thickness_mm": 1.5, "tip_radius_mm": 0.5, "bevel": 0.1, "wear": 0.0}, compat=("any",))
part("pick", "Wooden 3mm Jazz", {"material": "wood", "thickness_mm": 3.0, "tip_radius_mm": 1.5, "bevel": 0.5, "wear": 0.2}, compat=("any",))
part("pick", "Thumbpick Standard", {"material": "celluloid", "thickness_mm": 1.0, "tip_radius_mm": 1.2, "bevel": 0.2, "wear": 0.1}, compat=("any",))

part("slide", "Glass Delta 22mm", {"material": "glass", "mass_g": 65, "length_mm": 70, "diameter_mm": 22}, compat=("any",), default=True)
part("slide", "Brass Heavy 25mm", {"material": "brass", "mass_g": 150, "length_mm": 65, "diameter_mm": 25}, compat=("any",))
part("slide", "Steel Bar Lap Steel", {"material": "steel", "mass_g": 210, "length_mm": 80, "diameter_mm": 24}, compat=("any",))
part("slide", "Ceramic 20mm", {"material": "ceramic", "mass_g": 55, "length_mm": 60, "diameter_mm": 20}, compat=("any",))
part("slide", "Chrome Standard 22mm", {"material": "steel", "mass_g": 90, "length_mm": 60, "diameter_mm": 22}, compat=("any",))

part("capo", "Trigger Capo", {"type": "full", "mass_g": 60, "string_mask": [True] * 6, "pressure": 0.7}, compat=("any",), default=True)
part("capo", "Screw Capo", {"type": "full", "mass_g": 45, "string_mask": [True] * 6, "pressure": 0.5}, compat=("any",))
part("capo", "Partial 3-String Capo", {"type": "partial", "mass_g": 35, "string_mask": [False, True, True, True, False, False], "pressure": 0.6}, compat=("any",))

# ======================================================================================================================
#  Guitars
# ======================================================================================================================
guitars = []


def guitar(folder, name, family, style, parts_map, pickups, setup, finish, seed, tags=()):
    guitars.append((folder, name, family, style, parts_map, pickups, setup, finish, seed, list(tags)))


def pu(ref, pos, ht, hb):
    return {"reference": ref, "position_mm": pos, "height_treble_mm": ht, "height_bass_mm": hb}


def setup(t, b, relief, nut=0.45, n=6):
    return {"action_treble_mm": t, "action_bass_mm": b, "relief_mm": relief, "nut_slot_depths_mm": [nut] * n, "intonation_mm": [0.0] * n}


def finish(kind, a, b="#000000", shape="radial", gloss=0.85, aging=0.0):
    return {"type": kind, "color_a": a, "color_b": b, "burst_shape": shape, "gloss": gloss, "aging": aging}


def bill(body, neck, fretboard, frets, nut, bridge, tuners, wiring_ref, strings_ref, top=None, tailpiece=None, pickguard=None):
    return {"body": body, "top": top, "neck": neck, "fretboard": fretboard, "frets": frets, "nut": nut,
            "bridge": bridge, "tailpiece": tailpiece, "tuners": tuners, "wiring": wiring_ref,
            "strings": strings_ref, "pickguard": pickguard}


# factory-content.md 2's fifteen, then the build's other guitar types (guitar-workshop.md 0.6).
guitar("Electric", "Vintage Single-Cut", "electric", "single_cutaway_arched",
       bill(B["mahogany_sc"], N["mahogany_set"], F["rosewood"], FR["mj"], NU["bone43"], BR["abr1"], TU["kluson"], W["lp50"], S["10"], top=T["flame"], tailpiece=TP["stopbar"], pickguard=PG["cream"]),
       {"neck": pu(P["paf57"], 152, 2.8, 3.1), "middle": None, "bridge": pu(P["paf59"], 38, 2.4, 2.6)},
       setup(1.6, 2.0, 0.2, 0.9), finish("burst", "#7A2E1B", "#F2C441", "diagonal", 0.9, 0.15), 428913, ["vintage", "humbucker"])
guitar("Electric", "Vintage Double-Cut", "electric", "double_cutaway_offset",
       bill(B["alder_dc"], N["maple_c"], F["rosewood"], FR["vintage"], NU["bone42"], BR["vintage_trem"], TU["kluson"], W["strat_vintage"], S["9"], pickguard=PG["white"]),
       {"neck": pu(P["sc54"], 159, 2.4, 2.8), "middle": pu(P["sc54"], 99, 2.4, 2.8), "bridge": pu(P["sc54"], 41, 2.0, 2.4)},
       setup(1.6, 2.0, 0.2), finish("burst", "#2B1A0F", "#E3A33C", "radial", 0.9, 0.1), 115003, ["vintage", "single coil"])
guitar("Electric", "Classic T-Style", "electric", "single_cutaway_slab",
       bill(B["ash_t"], N["maple_c"], F["maple"], FR["vintage"], NU["bone42"], BR["hardtail"], TU["kluson"], W["t_style"], S["10"], pickguard=PG["black"]),
       {"neck": pu(P["t_neck"], 165, 2.4, 2.4), "middle": None, "bridge": pu(P["t_bridge"], 36, 1.8, 2.0)},
       setup(1.5, 1.8, 0.2), finish("solid", "#E8C07A", gloss=0.8, aging=0.2), 227004, ["twang"])
guitar("Electric", "Semi-Hollow 335", "electric", "double_cutaway_semi",
       bill(B["semi_335"], N["mahogany_set"], F["rosewood"], FR["mj"], NU["bone43"], BR["abr1"], TU["kluson"], W["lp_modern"], S["10"], tailpiece=TP["stopbar"], pickguard=PG["black"]),
       {"neck": pu(P["paf57"], 150, 2.6, 3.0), "middle": None, "bridge": pu(P["paf57"], 40, 2.2, 2.6)},
       setup(1.6, 1.9, 0.25), finish("solid", "#8C1C13", gloss=0.9), 335005, ["semi-hollow"])
guitar("Electric", "Full Hollow Archtop", "electric", "archtop",
       bill(B["archtop"], N["maple_set"], F["ebony"], FR["mj"], NU["bone43"], BR["floating"], TU["kluson"], W["lp_modern"], S["11"], top=T["spruce"], tailpiece=TP["trapeze"], pickguard=PG["tortoise"]),
       {"neck": pu(P["floating_hb"], 170, 3.5, 3.8), "middle": None, "bridge": None},
       setup(1.9, 2.3, 0.2), finish("burst", "#3B1F10", "#D89B45", "radial", 0.9, 0.2), 175006, ["jazz", "hollow"])
guitar("Electric", "Offset Modern", "electric", "offset",
       bill(B["alder_offset"], N["maple_c"], F["rosewood"], FR["mj"], NU["bone43"], BR["two_point"], TU["sealed"], W["strat_modern"], S["10"], pickguard=PG["tortoise"]),
       {"neck": pu(P["mini"], 150, 2.4, 2.6), "middle": None, "bridge": pu(P["mini"], 42, 2.2, 2.4)},
       setup(1.6, 1.9, 0.2), finish("solid", "#8FD1C1", gloss=0.85), 606007, ["offset"])
guitar("Electric", "7-String Modern", "electric", "superstrat",
       bill(B["mahogany_sc"], N["seven"], F["ebony"], FR["jumbo_ss"], NU["graphite"], BR["hardtail7"], TU["locking"], W["emg"], S["7str"], top=T["quilt"]),
       {"neck": pu(P["emg60"], 150, 2.8, 3.2), "middle": None, "bridge": pu(P["emg81"], 40, 2.6, 3.0)},
       setup(1.5, 1.9, 0.15, 0.45, 7), finish("solid", "#161616", gloss=0.2), 707008, ["metal", "extended range"])
guitar("Electric", "8-String Modern", "electric", "superstrat",
       bill(B["swamp_ash_ms"], N["multi"], F["ebony"], FR["jumbo_ss"], NU["graphite"], BR["hardtail8"], TU["locking"], W["emg"], S["8str"]),
       {"neck": pu(P["emg60"], 160, 2.8, 3.4), "middle": None, "bridge": pu(P["emg81"], 45, 2.6, 3.2)},
       setup(1.5, 2.1, 0.15, 0.45, 8), finish("oil", "#C9A26B", gloss=0.1), 808009, ["metal", "fanned"])
guitar("Acoustic", "Dreadnought", "acoustic", "dreadnought",
       bill(B["dread"], N["acoustic"], F["rosewood"], FR["mj"], NU["bone43"], BR["pin"], TU["open"], W["acoustic"], S["pb"], top=T["spruce"], pickguard=PG["tortoise"]),
       {"neck": None, "middle": None, "bridge": pu(P["piezo"], 0, 0, 0)},
       setup(2.0, 2.5, 0.2), finish("natural", "#E7C78F", gloss=0.9), 111010, ["acoustic"])
guitar("Acoustic", "Grand Auditorium", "acoustic", "grand_auditorium",
       bill(B["ga"], N["acoustic"], F["ebony"], FR["mj"], NU["bone43"], BR["pin"], TU["sealed"], W["acoustic"], S["pb"], top=T["spruce"], pickguard=PG["tortoise"]),
       {"neck": None, "middle": None, "bridge": pu(P["piezo"], 0, 0, 0)},
       setup(1.9, 2.4, 0.2), finish("natural", "#EBCB94", gloss=0.3), 111011, ["acoustic", "fingerstyle"])
guitar("Acoustic", "Parlor", "acoustic", "parlor",
       bill(B["parlor"], N["acoustic"], F["rosewood"], FR["vintage"], NU["bone42"], BR["pin"], TU["open"], W["acoustic"], S["pb"], top=T["cedar"]),
       {"neck": None, "middle": None, "bridge": None},
       setup(2.0, 2.5, 0.25), finish("natural", "#D9AE72", gloss=0.6, aging=0.3), 111012, ["acoustic", "blues"])
guitar("Acoustic", "12-String Jumbo", "acoustic", "jumbo",
       bill(B["jumbo"], N["acoustic12"], F["rosewood"], FR["mj"], NU["bone43"], BR["pin12"], TU["sealed"], W["acoustic"], S["pb12"], top=T["spruce"], pickguard=PG["tortoise"]),
       {"neck": None, "middle": None, "bridge": pu(P["piezo"], 0, 0, 0)},
       setup(2.1, 2.6, 0.25, 0.45, 12), finish("burst", "#2C170C", "#E4B06A", "radial", 0.9), 121013, ["12-string"])
guitar("Classical", "Classical", "classical", "classical",
       bill(B["classical"], N["classical"], F["flat"], FR["vintage"], NU["bone43"], BR["tie"], TU["open"], W["acoustic"], S["nylon_n"], top=T["cedar"]),
       {"neck": None, "middle": None, "bridge": None},
       setup(2.8, 3.4, 0.25, 0.6), finish("natural", "#D8A865", gloss=0.8), 131014, ["nylon"])
guitar("Classical", "Flamenca Blanca", "classical", "classical",
       bill(B["flamenca"], N["classical"], F["flat"], FR["vintage"], NU["bone43"], BR["tie"], TU["open"], W["acoustic"], S["nylon_h"], top=T["spruce"]),
       {"neck": None, "middle": None, "bridge": None},
       setup(2.2, 2.8, 0.2, 0.5), finish("natural", "#F0D7A4", gloss=0.5), 131015, ["flamenco"])
guitar("Resonator", "Resonator Steel", "resonator", "resonator",
       bill(B["steel_reso"], N["reso"], F["rosewood"], FR["vintage"], NU["bone43"], BR["biscuit"], TU["open"], W["acoustic"], S["pb"]),
       {"neck": None, "middle": None, "bridge": None},
       setup(2.8, 3.2, 0.3, 0.8), finish("metal", "#B9BEC4", gloss=0.95, aging=0.3), 141016, ["slide", "resonator"])
guitar("Acoustic", "Selmer-Style", "acoustic", "gypsy_jazz",
       bill(B["selmer"], N["acoustic"], F["ebony"], FR["vintage"], NU["bone43"], BR["floating"], TU["open"], W["acoustic"], S["silk"], top=T["spruce"], tailpiece=TP["trapeze"]),
       {"neck": None, "middle": None, "bridge": None},
       setup(2.0, 2.6, 0.2), finish("natural", "#E0B97E", gloss=0.8, aging=0.2), 151017, ["gypsy jazz"])
guitar("Bass", "P-Style Bass", "bass", "bass_offset",
       bill(B["bass_p"], N["bass_p"], F["rosewood"], FR["vintage"], NU["bass"], BR["bass_p"], TU["bass"], W["bass_passive"], S["bass_n"], pickguard=PG["tortoise"]),
       {"neck": None, "middle": pu(P["split_p"], 170, 2.8, 3.2), "bridge": None},
       setup(2.0, 2.4, 0.3, 0.6, 4), finish("burst", "#2B1A0F", "#E3A33C", "radial", 0.9), 161018, ["bass"])
guitar("Bass", "J-Style Bass", "bass", "bass_offset",
       bill(B["bass_p"], N["bass_j"], F["rosewood"], FR["mj"], NU["bass"], BR["bass_badass"], TU["bass"], W["bass_passive"], S["bass_n"], pickguard=PG["tortoise"]),
       {"neck": pu(P["j_neck"], 160, 2.6, 3.0), "middle": None, "bridge": pu(P["j_bridge"], 42, 2.4, 2.8)},
       setup(2.0, 2.4, 0.3, 0.6, 4), finish("solid", "#F2EEDF", gloss=0.85, aging=0.1), 171019, ["bass", "funk"])
guitar("Bass", "Full Hollow Bass", "bass", "bass_hollow",
       bill(B["hollow_bass"], N["bass_p"], F["rosewood"], FR["mj"], NU["bass"], BR["bass_p"], TU["bass"], W["bass_passive"], S["bass_flat"], tailpiece=TP["trapeze"]),
       {"neck": pu(P["mm"], 150, 3.0, 3.4), "middle": None, "bridge": None},
       setup(2.2, 2.6, 0.3, 0.6, 4), finish("burst", "#3B1F10", "#D89B45", "radial", 0.9), 181020, ["bass", "hollow"])

# The build's other guitar types (guitar-workshop.md 0.6): every enum entry has a file.
guitar("Electric", "Double-Cut Devil", "electric", "double_cutaway_thin",
       bill(B["mahogany_thin"], N["mahogany_thin"], F["rosewood"], FR["mj"], NU["bone43"], BR["abr1"], TU["kluson"], W["lp_modern"], S["10"], tailpiece=TP["stopbar"], pickguard=PG["black"]),
       {"neck": pu(P["paf57"], 150, 2.6, 2.9), "middle": None, "bridge": pu(P["paf59"], 38, 2.2, 2.5)},
       setup(1.5, 1.8, 0.2), finish("solid", "#6B1A12", gloss=0.9), 191021, ["rock"])
guitar("Electric", "Angular Korina", "electric", "angular",
       bill(B["mahogany_sc"], N["mahogany_set"], F["rosewood"], FR["mj"], NU["bone43"], BR["abr1"], TU["sealed"], W["lp_modern"], S["10"], tailpiece=TP["stopbar"]),
       {"neck": pu(P["paf59"], 150, 2.4, 2.7), "middle": None, "bridge": pu(P["hot"], 40, 2.0, 2.3)},
       setup(1.5, 1.8, 0.2), finish("natural", "#E3C48B", gloss=0.9), 191022, ["rock"])
guitar("Electric", "Superstrat Floyd", "electric", "superstrat",
       bill(B["basswood_ss"], N["maple_c"], F["rosewood"], FR["jumbo_ss"], NU["graphite"], BR["floyd"], TU["locking"], W["strat_modern"], S["9"]),
       {"neck": pu(P["paf59"], 150, 2.4, 2.8), "middle": pu(P["noiseless"], 99, 2.4, 2.8), "bridge": pu(P["hot"], 38, 2.0, 2.4)},
       setup(1.3, 1.6, 0.15, 0.4), finish("solid", "#1C3F8A", gloss=0.9), 191023, ["shred"])
guitar("Electric", "Baritone Electric", "electric", "offset",
       bill(B["alder_offset"], N["baritone"], F["rosewood"], FR["mj"], NU["bone43"], BR["hardtail"], TU["sealed"], W["t_style"], S["baritone"], pickguard=PG["black"]),
       {"neck": pu(P["noiseless"], 170, 2.6, 3.0), "middle": None, "bridge": pu(P["t_bridge"], 40, 2.2, 2.6)},
       setup(1.8, 2.3, 0.25), finish("solid", "#2C2C2C", gloss=0.6), 191024, ["baritone"])
guitar("Acoustic", "Jumbo", "acoustic", "jumbo",
       bill(B["jumbo"], N["acoustic"], F["rosewood"], FR["mj"], NU["bone43"], BR["pin"], TU["sealed"], W["acoustic"], S["8020"], top=T["spruce"], pickguard=PG["tortoise"]),
       {"neck": None, "middle": None, "bridge": pu(P["piezo"], 0, 0, 0)},
       setup(2.1, 2.6, 0.25), finish("burst", "#2C170C", "#E4B06A", "radial", 0.9), 191025, ["acoustic"])
guitar("Bass", "Hollow Violin-Style Bass", "bass", "bass_violin",
       bill(B["hollow_bass"], N["bass_p"], F["rosewood"], FR["mj"], NU["bass"], BR["bass_p"], TU["bass"], W["bass_passive"], S["bass_flat"]),
       {"neck": pu(P["mm"], 120, 3.0, 3.4), "middle": None, "bridge": pu(P["split_p"], 40, 2.6, 3.0)},
       setup(2.0, 2.4, 0.3, 0.6, 4), finish("burst", "#40170A", "#C9722C", "radial", 0.9), 191026, ["bass"])
guitar("Bass", "Five-String Bass", "bass", "bass_offset",
       bill(B["bass_p"], N["bass_5"], F["ebony"], FR["jumbo_ss"], NU["bass"], BR["bass_5"], TU["bass"], W["bass_active"], S["bass_5"]),
       {"neck": pu(P["j_neck"], 160, 2.6, 3.0), "middle": None, "bridge": pu(P["mm"], 45, 2.4, 2.8)},
       setup(2.0, 2.5, 0.3, 0.6, 5), finish("solid", "#1B1B1B", gloss=0.3), 191027, ["bass", "extended range"])
guitar("Bass", "Fretless Bass", "bass", "bass_offset",
       bill(B["bass_p"], N["bass_j"], F["ebony"], FR["fretless"], NU["bass"], BR["bass_badass"], TU["bass"], W["bass_passive"], S["bass_flat"]),
       {"neck": pu(P["j_neck"], 160, 2.6, 3.0), "middle": None, "bridge": pu(P["j_bridge"], 42, 2.4, 2.8)},
       setup(1.8, 2.2, 0.2, 0.6, 4), finish("natural", "#C9A26B", gloss=0.3), 191028, ["bass", "fretless"])


def write_json(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(data, f, indent=2)
        f.write("\n")


def main():
    for folder in (PARTS, GUITARS):
        if os.path.isdir(folder):
            shutil.rmtree(folder)

    seen = set()

    # Part names become file names, on every platform the installer covers.
    for ptype, name, *_ in parts:
        bad = set(name) & set(r'<>:"/\|?*')
        assert not bad, "part name %r has characters a file name cannot: %s" % (name, "".join(sorted(bad)))

    for ptype, name, fields, compat, tags, default, illustration in parts:
        key = (ptype, name)
        assert key not in seen, key
        seen.add(key)

        meta = {"name": name, "part_type": ptype, "author": "Factory", "tags": tags, "compatibility": compat}
        if default:
            meta["is_default"] = True

        body = {"schema": 1, "magic": "luthier.part", "meta": meta, "fields": fields}
        if illustration:
            body["illustration"] = illustration

        write_json(os.path.join(PARTS, FOLDERS[ptype], name + ".luthierpart"), body)

    for folder, name, family, style, bill_map, pickups, setup_map, finish_map, seed, tags in guitars:
        parts_obj = {}
        for slot, ref in bill_map.items():
            parts_obj[slot] = {"reference": "Factory/" + ref} if ref else None
        parts_obj["pickups"] = {k: (dict(v, reference="Factory/" + v["reference"]) if v else None) for k, v in pickups.items()}
        parts_obj["hardware_color"] = "nickel" if family != "bass" else "chrome"
        parts_obj["finish"] = finish_map

        body = {
            "schema": 1, "magic": "luthier.guitar",
            "meta": {"name": name, "family": family, "body_style": style, "author": "Factory", "tags": tags,
                     "created": "2026-09-23T00:00:00Z", "modified": "2026-09-23T00:00:00Z"},
            "parts": parts_obj,
            "setup": setup_map,
            "character_seed": str(seed),
        }
        write_json(os.path.join(GUITARS, folder, name + ".luthierguitar"), body)

    print("%d parts, %d guitars" % (len(parts), len(guitars)))


if __name__ == "__main__":
    main()
