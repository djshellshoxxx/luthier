"""Generates Luthier's body and cabinet impulse-response libraries.

WHAT THESE ARE, HONESTLY
------------------------
These are *synthesised* impulse responses, derived from the same physical models
the engine uses, not measurements of real instruments or cabinets. Shipping real
captures would mean licensing recordings of trademarked instruments and speakers,
which this project cannot do.

What that buys, and what it costs:

  + Every IR is internally consistent with the modal synthesis path, so switching
    a body between Convolution and Modal is a change of CPU cost and of dynamic
    behaviour, not a jump in tone.
  + The whole library regenerates from this one file, so a change to the physics
    propagates everywhere instead of drifting out of sync with the engine.
  - They will not have the fine idiosyncratic detail of a real capture. A user who
    owns commercial IRs can load those instead: the engine takes any WAV.

The body IRs are built from plate and Helmholtz theory (mirroring BodyModels.cpp).
The cabinet IRs are built by designing a magnitude response from the speaker and
microphone models (mirroring CabinetEngine.cpp), converting it to minimum phase
via the real cepstrum, and adding the baffle and cabinet-wall reflections that
give a real cab IR its character.
"""

import math
import os
import struct
import sys

import numpy as np

SR = 48000
BODY_SECONDS = 0.40
CAB_SECONDS = 0.20

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Resources")

SPEED_OF_SOUND = 343.0
POISSON = 0.30

# --- woods: density kg/m3, Young's modulus Pa, loss factor -------------------
WOODS = {
    "sitka_spruce": (430.0, 11.0e9, 0.0080),
    "cedar":        (350.0,  8.0e9, 0.0105),
    "mahogany":     (510.0, 10.0e9, 0.0120),
    "maple":        (650.0, 12.0e9, 0.0090),
    "alder":        (450.0,  9.5e9, 0.0130),
    "ash":          (600.0, 11.5e9, 0.0110),
    "rosewood":     (850.0, 14.0e9, 0.0070),
    "koa":          (610.0, 10.5e9, 0.0110),
    "basswood":     (420.0,  8.5e9, 0.0160),
    "korina":       (540.0, 10.0e9, 0.0120),
    "walnut":       (640.0, 11.0e9, 0.0100),
}

# --- body shapes: lowerBout mm, depth mm, soundhole mm, topThk, backThk, vol L, acoustic
SHAPES = {
    "parlor":            (336.0,  95.0,  92.0, 2.5, 2.6,  9.5, True),
    "concert":           (368.0, 105.0,  98.0, 2.6, 2.7, 12.5, True),
    "auditorium":        (381.0, 110.0, 100.0, 2.7, 2.8, 14.5, True),
    "dreadnought":       (397.0, 121.0, 102.0, 2.8, 2.9, 17.5, True),
    "jumbo":             (432.0, 127.0, 102.0, 2.9, 3.0, 22.0, True),
    "classical":         (368.0, 100.0,  86.0, 2.2, 2.4, 12.0, True),
    "flamenco":          (365.0,  88.0,  86.0, 1.9, 2.1, 10.0, True),
    "resonator":         (390.0, 105.0,   0.0, 0.6, 1.2, 13.0, True),
    "12_string_dread":   (400.0, 122.0, 102.0, 2.9, 3.0, 18.0, True),
    "solid_thin":        (330.0,  38.0,   0.0, 0.0, 0.0,  0.0, False),
    "solid":             (330.0,  45.0,   0.0, 0.0, 0.0,  0.0, False),
    "solid_heavy":       (330.0,  52.0,   0.0, 0.0, 0.0,  0.0, False),
    "semi_hollow":       (406.0,  42.0,   0.0, 4.5, 4.5,  4.0, False),
    "hollow":            (406.0,  75.0,   0.0, 4.0, 4.0,  9.0, False),
    "offset":            (340.0,  42.0,   0.0, 0.0, 0.0,  0.0, False),
    "chambered":         (330.0,  48.0,   0.0, 5.0, 0.0,  1.8, False),
    "bass_solid":        (356.0,  45.0,   0.0, 0.0, 0.0,  0.0, False),
    "bass_hollow":       (400.0,  70.0,  95.0, 3.2, 3.4, 11.0, False),
}

BRACING_STIFFNESS = {
    "parlor": 0.82, "concert": 1.00, "auditorium": 1.00, "dreadnought": 1.00,
    "jumbo": 0.94, "classical": 0.88, "flamenco": 0.88, "resonator": 0.82,
    "12_string_dread": 1.00, "solid_thin": 3.20, "solid": 3.20, "solid_heavy": 3.20,
    "semi_hollow": 1.85, "hollow": 1.20, "offset": 3.20, "chambered": 1.85,
    "bass_solid": 3.20, "bass_hollow": 1.20,
}

PLATE_LAMBDA = [10.2158, 21.2604, 34.8770, 39.7710, 51.0300, 60.8280,
                69.6660, 84.5830, 94.1000, 108.720, 122.440, 139.050]

SIZES = {"small": 0.90, "medium": 1.00, "large": 1.10}
AGES = {"played": 0.45, "vintage": 0.90}


# =============================================================================
#  WAV writing (24-bit PCM, no external dependency)
# =============================================================================
def write_wav24(path, samples, sample_rate=SR):
    os.makedirs(os.path.dirname(path), exist_ok=True)

    peak = float(np.max(np.abs(samples)))
    if peak > 1e-12:
        samples = samples / peak * 0.97

    clipped = np.clip(samples, -1.0, 1.0)
    ints = np.round(clipped * 8388607.0).astype(np.int32)

    frames = bytearray()
    for v in ints:
        v = int(v) & 0xFFFFFF
        frames += bytes((v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF))

    byte_rate = sample_rate * 3
    with open(path, "wb") as f:
        f.write(b"RIFF")
        f.write(struct.pack("<I", 36 + len(frames)))
        f.write(b"WAVEfmt ")
        f.write(struct.pack("<IHHIIHH", 16, 1, 1, sample_rate, byte_rate, 3, 24))
        f.write(b"data")
        f.write(struct.pack("<I", len(frames)))
        f.write(bytes(frames))


# =============================================================================
#  BODY IRs - modal synthesis, matching BodyModels.cpp
# =============================================================================
def plate_mode_hz(lam, thickness_mm, radius_mm, youngs, density):
    t = max(0.3, thickness_mm) * 0.001
    a = max(0.05, radius_mm * 0.001)
    speed = math.sqrt(youngs / (12.0 * density * (1.0 - POISSON ** 2)))
    return (lam / (2.0 * math.pi)) * (t / (a * a)) * speed


def helmholtz_hz(shape, size_scale):
    lower, depth, hole, top_thk, back_thk, volume_l, acoustic = SHAPES[shape]
    if hole < 1.0 or volume_l < 0.1:
        return 0.0

    radius = hole * 0.0005
    area = math.pi * radius * radius
    volume = volume_l * 0.001 * size_scale ** 3
    effective_length = top_thk * 0.001 + 1.7 * radius

    if volume <= 0.0 or effective_length <= 0.0:
        return 0.0

    return min(400.0, max(40.0, (SPEED_OF_SOUND / (2.0 * math.pi))
                          * math.sqrt(area / (volume * effective_length))))


def build_body_modes(shape, top_wood, back_wood, size_scale, age):
    lower, depth, hole, top_thk, back_thk, volume_l, acoustic = SHAPES[shape]
    radius_mm = lower * 0.5 * size_scale

    top_rho, top_e, top_loss = WOODS[top_wood]
    back_rho, back_e, back_loss = WOODS[back_wood]

    stiffness = BRACING_STIFFNESS[shape]
    age_q = 1.0 + age * 0.55

    modes = []

    air = helmholtz_hz(shape, size_scale)
    if air > 0.0:
        modes.append((air, 16.0 * age_q, 1.00))
        modes.append((air * 1.62, 22.0 * age_q, 0.42))
        length_m = lower * 0.0016 * size_scale
        modes.append((SPEED_OF_SOUND / (2.0 * max(0.15, length_m)), 12.0 * age_q, 0.22))
    else:
        modes.append((168.0 / size_scale ** 0.8, 9.0 * age_q, 0.30))
        modes.append((243.0 / size_scale ** 0.8, 11.0 * age_q, 0.22))

    top_count = 10 if acoustic else 5
    for i in range(top_count):
        f = plate_mode_hz(PLATE_LAMBDA[i], max(0.8, top_thk), radius_mm, top_e, top_rho) * stiffness
        q = (1.0 / max(1e-4, top_loss)) * 0.42 * age_q
        modes.append((f, q, 0.95 / (1.0 + 0.55 * i)))

    back_count = 7 if acoustic else 3
    for i in range(back_count):
        f = plate_mode_hz(PLATE_LAMBDA[i], max(0.8, back_thk if back_thk > 0 else top_thk),
                          radius_mm * 0.98, back_e, back_rho) * 1.12
        q = (1.0 / max(1e-4, back_loss)) * 0.35 * age_q
        modes.append((f, q, 0.45 / (1.0 + 0.6 * i)))

    # The dense, irregular thicket above ~1 kHz.
    seed = (hash((shape, top_wood, back_wood)) & 0x7FFFFFFF)
    rng = np.random.default_rng(seed)

    side_base = plate_mode_hz(PLATE_LAMBDA[0], max(1.2, depth * 0.022), radius_mm * 0.55,
                              back_e, back_rho)
    f = max(700.0, side_base)
    scatter = 18 if acoustic else 10

    for _ in range(scatter):
        f *= 1.16 + rng.random() * 0.22
        if f > 11000.0:
            break
        q = (14.0 + rng.random() * 40.0) * age_q
        gain = (0.20 if acoustic else 0.11) * (0.45 + rng.random() * 0.55) / (1.0 + f / 3200.0)
        modes.append((f, q, gain))

    return [(f, q, g) for (f, q, g) in modes if 25.0 < f < SR * 0.47 and g > 1e-5]


def render_body_ir(modes, seconds=BODY_SECONDS):
    n = int(SR * seconds)
    t = np.arange(n) / SR
    ir = np.zeros(n)

    for f, q, g in modes:
        # A mode is a damped sinusoid: decay rate follows pi*f/Q.
        decay = math.pi * f / max(1.0, q)
        if decay * seconds > 40.0:
            length = min(n, int(40.0 / decay * SR) + 1)
        else:
            length = n
        tt = t[:length]
        ir[:length] += g * np.exp(-decay * tt) * np.sin(2.0 * math.pi * f * tt)

    # The direct, un-resonated path: a body IR starts with the transient that
    # reaches the listener before any of the plate has moved.
    ir[0] += 0.9

    # Gentle fade so the tail never ends on a step.
    fade = int(SR * 0.02)
    if fade > 0 and fade < n:
        ir[-fade:] *= np.linspace(1.0, 0.0, fade) ** 2

    return ir


# =============================================================================
#  CABINET IRs - designed magnitude, minimum phase, plus baffle reflections
# =============================================================================
# name: lowCorner, bodyHz, bodyDb, presenceHz, presenceDb, topRollHz
SPEAKERS = {
    "celestion_greenback":  (95.0, 200.0, 3.0, 2100.0, 5.5, 4600.0),
    "celestion_vintage_30": (88.0, 180.0, 2.5, 2600.0, 7.0, 5200.0),
    "celestion_g12h":       (82.0, 165.0, 3.5, 2300.0, 6.0, 5000.0),
    "celestion_g12t_75":    (92.0, 210.0, 2.0, 3000.0, 4.5, 5400.0),
    "jensen_c12":          (105.0, 240.0, 2.2, 1900.0, 5.0, 5600.0),
    "alnico_blue":         (110.0, 260.0, 1.8, 2400.0, 6.5, 6200.0),
    "evm_12l":              (70.0, 150.0, 1.2, 3200.0, 3.0, 5800.0),
    "bass_ceramic":         (42.0,  95.0, 2.6, 1400.0, 2.0, 3600.0),
}

# name: proximityHz, proximityDb, presenceHz, presenceDb, topHz
MICS = {
    "shure_sm57":       (140.0, 2.0, 5500.0,  5.0, 14000.0),
    "shure_sm7b":       (120.0, 2.5, 4200.0,  2.5, 15000.0),
    "sennheiser_md421": (130.0, 3.5, 3800.0,  3.5, 16000.0),
    "neumann_u87":       (95.0, 1.5, 9000.0,  3.0, 20000.0),
    "royer_r_121":      (110.0, 3.0, 6000.0, -3.5,  9000.0),
    "akg_c414":          (90.0, 1.2, 10000.0, 3.5, 20000.0),
    "akg_d112":          (70.0, 4.0, 3200.0,  3.0,  9000.0),
}

# name: lowTrim, bodyDb, depth_m (for the wall reflection)
CABS = {
    "1x12_open":    (1.20, -1.0, 0.28),
    "1x12_closed":  (0.95,  1.0, 0.30),
    "2x12_open":    (1.10, -0.5, 0.30),
    "2x12_closed":  (0.90,  1.5, 0.32),
    "4x12":         (0.80,  2.5, 0.36),
    "4x12_vintage": (0.84,  2.0, 0.36),
    "1x15_bass":    (0.60,  3.0, 0.42),
    "4x10_bass":    (0.65,  2.5, 0.40),
    "8x10_bass":    (0.55,  3.5, 0.46),
}

POSITIONS = {
    "on_axis_centre": (2.5, 1.15),
    "cap_edge":       (0.0, 1.00),
    "off_axis_45":   (-2.5, 0.80),
    "cone_edge":     (-4.5, 0.65),
    "rear":          (-6.0, 0.55),
}

DISTANCES = {"close": 1.00, "medium": 0.45, "far": 0.10}

# Which speakers belong in which cabinets.
CAB_SPEAKERS = {
    "1x12_open":    ["jensen_c12", "alnico_blue", "celestion_greenback"],
    "1x12_closed":  ["celestion_vintage_30", "celestion_g12h", "evm_12l"],
    "2x12_open":    ["celestion_greenback", "jensen_c12", "alnico_blue"],
    "2x12_closed":  ["celestion_vintage_30", "celestion_g12t_75", "celestion_g12h"],
    "4x12":         ["celestion_greenback", "celestion_vintage_30", "celestion_g12t_75"],
    "4x12_vintage": ["celestion_greenback", "celestion_g12h", "alnico_blue"],
    "1x15_bass":    ["bass_ceramic"],
    "4x10_bass":    ["bass_ceramic"],
    "8x10_bass":    ["bass_ceramic"],
}


def db_to_gain(db):
    return 10.0 ** (db / 20.0)


def peaking_response(freqs, centre, q, gain_db):
    """Magnitude of an RBJ peaking filter, evaluated on a frequency grid."""
    a = 10.0 ** (gain_db / 40.0)
    w = freqs / max(1.0, centre)
    # A standard analogue peaking magnitude, which is all we need for a design.
    num = 1.0 + (w / max(0.05, q)) ** 2 * a * a - 2.0 * 0 + (w ** 2 - 1.0) ** 2
    den = 1.0 + (w / max(0.05, q)) ** 2 / (a * a) + (w ** 2 - 1.0) ** 2
    return np.sqrt(np.maximum(1e-12, num / den))


def design_cab_magnitude(freqs, cab, speaker, mic, position, distance, speaker_age):
    low_corner, body_hz, body_db, presence_hz, presence_db, top_roll = SPEAKERS[speaker]
    prox_hz, prox_db, mic_pres_hz, mic_pres_db, mic_top = MICS[mic]
    low_trim, cab_body_db, _ = CABS[cab]
    axis_db, top_trim = POSITIONS[position]
    prox_scale = DISTANCES[distance]

    low_corner *= (1.0 - speaker_age * 0.12) * low_trim
    presence_db *= (1.0 - speaker_age * 0.22)

    f = np.maximum(1.0, freqs)
    mag = np.ones_like(f)

    # Highpass: below the corner a cone stops moving air, steeply.
    mag *= 1.0 / np.sqrt(1.0 + (low_corner / f) ** 4)

    # Low shelf for the cabinet's body.
    shelf_gain = db_to_gain(body_db + cab_body_db)
    mag *= (1.0 + (shelf_gain - 1.0) / (1.0 + (f / (body_hz * 1.5)) ** 2))

    # Proximity bump from the microphone.
    mag *= peaking_response(f, prox_hz, 0.9, prox_db * prox_scale)

    # Cone breakup: the bite.
    mag *= peaking_response(f, presence_hz, 1.3, presence_db + axis_db + mic_pres_db * 0.35)

    # The brick wall above the breakup region, which is the most recognisable
    # feature of any guitar cabinet.
    roll = min(top_roll * top_trim, mic_top)
    mag *= 1.0 / np.sqrt(1.0 + (f / roll) ** 8)

    # Fine comb structure from the cone's own modes: this is what stops a
    # synthesised IR from sounding smooth and lifeless.
    seed = (hash((cab, speaker, mic, position, distance)) & 0x7FFFFFFF)
    rng = np.random.default_rng(seed)

    for _ in range(14):
        centre = 400.0 * (1.0 + rng.random() * 18.0)
        q = 3.0 + rng.random() * 9.0
        gain = (rng.random() - 0.5) * 5.0
        mag *= peaking_response(f, centre, q, gain)

    return mag


def minimum_phase_ir(mag, n_fft):
    """Real cepstrum method: fold the anticausal part onto the causal one."""
    mag = np.maximum(mag, 1e-9)
    log_mag = np.log(mag)

    # Build the full symmetric spectrum.
    full = np.concatenate([log_mag, log_mag[-2:0:-1]])
    cep = np.real(np.fft.ifft(full))

    window = np.zeros(n_fft)
    window[0] = 1.0
    window[1:n_fft // 2] = 2.0
    window[n_fft // 2] = 1.0

    min_phase_spectrum = np.exp(np.fft.fft(cep * window))
    return np.real(np.fft.ifft(min_phase_spectrum))


def render_cab_ir(cab, speaker, mic, position, distance, speaker_age, seconds=CAB_SECONDS):
    n_fft = 8192
    freqs = np.fft.rfftfreq(n_fft, 1.0 / SR)

    mag = design_cab_magnitude(freqs, cab, speaker, mic, position, distance, speaker_age)
    ir = minimum_phase_ir(mag, n_fft)

    n = int(SR * seconds)
    out = np.zeros(n)
    take = min(n, len(ir))
    out[:take] = ir[:take]

    # Cabinet wall reflection: the back of the box returns a delayed, filtered,
    # polarity-inverted copy. This is a real and audible part of a cab IR.
    _, _, depth_m = CABS[cab]
    delay = int(SR * (2.0 * depth_m) / SPEED_OF_SOUND)
    if 0 < delay < n:
        reflection = np.zeros(n)
        reflection[delay:] = -0.22 * out[:n - delay]
        # The reflection has bounced off wood, so it has lost its top end.
        kernel = np.ones(6) / 6.0
        reflection = np.convolve(reflection, kernel)[:n]
        out += reflection

    # Room floor bounce, present in every close-miked capture.
    floor_delay = int(SR * 0.004 * (1.0 / max(0.15, DISTANCES[distance])))
    if 0 < floor_delay < n:
        out[floor_delay:] += 0.05 * out[:n - floor_delay]

    fade = int(SR * 0.02)
    if 0 < fade < n:
        out[-fade:] *= np.linspace(1.0, 0.0, fade) ** 2

    return out


# =============================================================================
def generate_bodies():
    count = 0
    folder = os.path.join(OUT, "BodyIRs")

    # Woods worth offering per shape. Acoustic tops and backs vary; a solid body
    # barely cares, so it gets one.
    # Two top/back pairings per family. The engine falls back to modal synthesis
    # for any combination that has no IR, so exhaustive coverage is unnecessary -
    # what matters is that the common choices are here.
    acoustic_pairs = [
        ("sitka_spruce", "rosewood"),
        ("cedar", "mahogany"),
    ]
    electric_pairs = [
        ("alder", "alder"),
        ("mahogany", "mahogany"),
    ]

    for shape, props in SHAPES.items():
        acoustic = props[6]
        pairs = acoustic_pairs if acoustic else electric_pairs

        for size_name, size_scale in SIZES.items():
            for age_name, age in AGES.items():
                for top, back in pairs:
                    if top not in WOODS or back not in WOODS:
                        continue

                    modes = build_body_modes(shape, top, back, size_scale, age)
                    if not modes:
                        continue

                    ir = render_body_ir(modes)
                    path = os.path.join(folder, shape,
                                        "%s_%s_%s.wav" % (size_name, top, age_name))

                    # One file per (size, top wood, age); the back wood varies with
                    # the top in the pairing, which is how real instruments are built.
                    if os.path.exists(path):
                        continue

                    write_wav24(path, ir)
                    count += 1

    return count


def generate_cabs():
    count = 0
    folder = os.path.join(OUT, "CabIRs")

    for cab, speakers in CAB_SPEAKERS.items():
        for speaker in speakers:
            bass_cab = "bass" in cab
            mics = ["akg_d112", "shure_sm57", "sennheiser_md421", "neumann_u87"] if bass_cab \
                else ["shure_sm57", "sennheiser_md421", "royer_r_121", "neumann_u87"]

            for mic in mics:
                for position in ["on_axis_centre", "cap_edge", "off_axis_45"]:
                    for distance in ["close", "medium"]:

                        ir = render_cab_ir(cab, speaker, mic, position, distance, 0.4)
                        path = os.path.join(folder, cab,
                                            "%s_%s_%s_%s.wav" % (speaker, mic, position, distance))

                        if os.path.exists(path):
                            continue

                        write_wav24(path, ir)
                        count += 1

    return count


def main():
    print("Generating body IRs...")
    bodies = generate_bodies()
    print("  wrote %d body IRs" % bodies)

    print("Generating cabinet IRs...")
    cabs = generate_cabs()
    print("  wrote %d cabinet IRs" % cabs)

    total_bytes = 0
    for root, _, files in os.walk(OUT):
        for name in files:
            if name.endswith(".wav"):
                total_bytes += os.path.getsize(os.path.join(root, name))

    print("Total IR library: %.1f MB" % (total_bytes / (1024.0 * 1024.0)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
