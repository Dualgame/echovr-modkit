"""W23f: w23e's REPORT PLAYER layer (ffa_ac_report) with the PC colours re-fitted in the WHITE lobby.

PC only. Same geometry and op order as w23e; only tint values change on PC. The Quest canvas stays w23e's
(w23e/ac_report_quest.json, built by w23e/quest_twin.sh), so the Quest output is byte-identical to w23e's.

    python3 lib/menu_ui/w23f/gen_ac_report.py            # writes ac_report.json (PC)
    python3 lib/menu_ui/w23f/gen_ac_report.py --check    # exit 1 when the committed file is stale
    python3 lib/menu_ui/w23f/gen_ac_report.py --calib    # print the fit, the targets and the predicted renders

Why. The w23d PC palette was inverted from a tone curve fitted on
s022_w23c_ac/_pick, shot in an ORANGE-lit lobby spot. The PC tablet canvas does not render its tint literally: the response
depends on the spot and on the render mode (`device-observed`, PC 6323983201049540, same w23e palette, tile fill
(0.194, 0.229, 0.245)):
    orange spot, 1536 x 874 shot (s023_w23e_pc_ac)          -> (100, 109, 127)
    white lobby spot, 1536 x 874 shot (s025_xback_report)   -> ( 55,  64,  70)
    white lobby spot, -capturevp2 2560 x 1440 (s027v_pc_12) -> ( 45,  54,  59)   target (87, 104, 117)
The device runs and the R2 doc shots stand at the white spot (Quest is shot there too: 20261105-000018 001121), so the
fit below uses that spot in the -capturevp2 mode (the R2 doc-shot mode). Expect a 1536 x 874 shot there about 20 % lighter
and the orange spot about 2x lighter (`inferred` from the three rows above).

Model (`inferred`, fitted on s027v_pc_12 / _13): the tint is read as LINEAR, the colour keeps its linear chroma and only its
luminance L = 0.2126 r + 0.7152 g + 0.0722 b is tone-mapped: out_lin = tint * T(L) / L. Measured T: footer 0.1665 -> 0.0155,
SUBMIT fill 0.192 -> 0.024, tile 0.223 -> 0.035, CANCEL 0.242 -> 0.044, SUBMIT border grey 0.5 -> 0.337 (157), teal sel
0.545 -> 0.38 (98, 178, 186: the R lift that no per-channel curve explains), white 1.0 -> 0.94 (248). log-log slope 2.9 under
0.242 and 2.8 across 0.242..0.5, so T(L) = 0.337 (L / 0.5)^2.8 up to 0.545, then log-log to (1, 0.94). Forward check on
the w23e tints: footer (33,33,35) vs (33,33,38), SUBMIT (41,42,43) vs (39..44, 41..46, 43..48), CANCEL (58,59,58) vs
(58,59,60), tile (49,53,55) vs (45,54,59), teal by construction. Inversion: T(L_a) = lum(target), tint = target_lin * L_a /
lum(target).

Targets = what Quest shows at the same spot / Coffey's artboard: tile (87,104,117), CANCEL (115,111,112), disabled SUBMIT
(80,80,84), footer band + "<" fill = the artboard footer #2c3039 (Quest's face colour; the w23d PC fit used the orange-spot
stock face (69,62,72)), dots cover BLACK as on Quest (the stock title band renders 4..25 at the white spot, the w23d cover
drew a lighter (34,34,39) strip), picked teal #2fa1ad (47,161,173), Mute & Ghost box (5,200,193).
"""
from __future__ import annotations

import importlib.util
import json
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "ac_report.json"
W23E_PC = HERE.parent / "w23e" / "ac_report.json"


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


W23E = _load("w23e_gen", HERE.parent / "w23e" / "gen_ac_report.py")
W23D, W23B = W23E.W23D, W23E.W23B

KNEE_L, KNEE_T, POW = 0.5, 0.337, 2.8
HI = [(0.545, 0.38), (1.0, 0.94)]
LUM = (0.2126, 0.7152, 0.0722)


def srgb_to_lin(v):
    c = v / 255
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def lin_to_srgb(x):
    x = max(0.0, min(1.0, x))
    return 255 * (12.92 * x if x <= 0.0031308 else 1.055 * x ** (1 / 2.4) - 0.055)


def tone(L):
    if L <= HI[0][0]:
        return KNEE_T * (L / KNEE_L) ** POW
    (l0, t0), (l1, t1) = HI
    return t0 * math.exp(math.log(L / l0) * math.log(t1 / t0) / math.log(l1 / l0))


def tone_inv(T):
    if T <= HI[0][1]:
        return KNEE_L * (T / KNEE_T) ** (1 / POW)
    (l0, t0), (l1, t1) = HI
    return l0 * math.exp(math.log(T / t0) * math.log(l1 / l0) / math.log(t1 / t0))


def lum(c):
    return sum(w * v for w, v in zip(LUM, c))


def pc_authored(rendered):
    """the PC tint (r, g, b, 1), 3 decimals, that renders `rendered` sRGB at the white lobby spot (-capturevp2)"""
    t = [srgb_to_lin(v) for v in rendered]
    lt = lum(t)
    a = [min(1.0, v * tone_inv(lt) / lt) for v in t]
    return [round(v, 3) for v in a] + [1.0]


def pc_rendered(tint):
    """forward model: PC tint -> predicted sRGB at the white lobby spot"""
    L = lum(tint[:3])
    return tuple(round(lin_to_srgb(v * tone(L) / L)) for v in tint[:3]) if L > 0 else (0, 0, 0)


TARGETS = {
    "slate": (87, 104, 117),
    "cancel": (115, 111, 112),
    "submit": (80, 80, 84),
    "face": (44, 48, 57),
    "sel": (47, 161, 173),
    "teal": (5, 200, 193),
}
PAL = {k: pc_authored(v) for k, v in TARGETS.items()}
PALETTE_PC = {"slate": PAL["slate"], "cancel": PAL["cancel"], "submit": PAL["submit"], "face": PAL["face"],
              "dots": [0, 0, 0, 1]}
TINTED = {"acr_cover", "acr_dots_cover", "acr_back_fill", "acr_mg_box", "acr_cancel_fill", "acr_submit_fill"} | \
    {f"acr_r{i}_{p}" for i in range(1, 7) for p in ("fill", "sel")}


def build():
    bl = W23D.transform(W23B.blocks(json.loads(W23D.SRC.read_text())), PALETTE_PC)
    teal = W23E.TEAL
    W23E.TEAL = {**teal, "pc": PAL["teal"]}
    try:
        out = W23E.transform(bl, "pc", PALETTE_PC)
    finally:
        W23E.TEAL = teal
    for b in out:
        if b["name"].endswith("_sel") and b["name"].startswith("acr_r"):
            W23B.setop(b, "tint", rgba=PAL["sel"])
    ops = W23B.emit(out)
    _same_but_tints(ops, json.loads(W23E_PC.read_text()), {b["name"] for b in out})
    return W23B.render(ops)


def _same_but_tints(ops, ref, names):
    """w23f PC = w23e PC op for op, except the tint values of TINTED prims (geometry, text, uv, order, parents equal)"""
    assert len(ops) == len(ref), "op count differs from w23e"
    prim_name, changed = {}, set()
    for o, r in zip(ops, ref):
        if o["op"] == "clone":
            prim_name[8 + len(prim_name)] = o["names"][0]
        if o["op"] == "tint" and o != r:
            assert o.keys() == r.keys() and o["prim"] == r["prim"], f"tint op shape differs at prim {o.get('prim')}"
            changed.add(prim_name[o["prim"]])
        else:
            assert o == r, f"non-tint op differs from w23e: {o} vs {r}"
    assert changed <= TINTED and TINTED <= names, f"unexpected tint change {sorted(changed - TINTED)}"


def main() -> int:
    a = sys.argv[1:]
    if "--calib" in a:
        print(f"T(L) = {KNEE_T} (L/{KNEE_L})^{POW} to L {HI[0][0]}, then log-log to {HI[1]}")
        for k, v in TARGETS.items():
            print(f"{k:7s} target {v} -> PC tint {PAL[k][:3]} -> predicted {pc_rendered(PAL[k])}")
        for k, v in {"w23e tile": [0.194, 0.229, 0.245], "w23e cancel": [0.236, 0.243, 0.238],
                     "w23e submit": [0.184, 0.194, 0.198], "w23e footer": [0.167, 0.165, 0.179],
                     "w23e sel": [0.184, 0.639, 0.678], "grey 0.5": [0.5, 0.5, 0.5]}.items():
            print(f"check {k:11s} {v} -> predicted {pc_rendered(v)}")
        return 0
    text_out = build()
    if "--check" in a:
        ok = OUT.exists() and OUT.read_text() == text_out
        print(OUT.name, "current" if ok else "STALE")
        return 0 if ok else 1
    OUT.write_text(text_out)
    print(f"wrote {OUT} ({text_out.count(chr(10))} ops)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
