"""Emit HELP picture-tile canvas copies (from ui_mni_quit_info) and render offline previews.
usage: build.py OUTDIR [--only key]   (light, single stream)"""
import json, struct, subprocess, sys, tempfile
from pathlib import Path
REPO = Path(__import__('os').environ['MAIN_MENU_A_ROOT'])
sys.path.insert(0, str(REPO / 'lib')); sys.path.insert(0, str(REPO / 'lib/menu_ui'))
from resource_io import cuicanvasresource as canvas
import canvas_edit as ce
from csymbol64 import hash_name
HERE = Path(__file__).resolve().parent
OUT = Path(sys.argv[1]); TEX = HERE / 'tex'
OPS = Path(tempfile.mkdtemp(prefix='help_tiles_ops_'))
W, H = 640, 280
FRAME = {'small': ([0.78125, 0.77295, 0.9375, 0.82422], [0.625, 0.77295, 0.78125, 0.82422]),
         'half': ([0.78125, 0.66211, 0.9375, 0.73926], [0.625, 0.66211, 0.78125, 0.73926])}
TILES = json.loads((HERE / 'tiles.json').read_text())

def ops(t):
    scrim, hil = FRAME[t.get('frame', 'half')]
    o = [{"op": "design", "size": [W, H], "rt": [1024, 512]},
         {"op": "rect", "prim": 0, "rect": [0, 0, W, H]},
         {"op": "rect", "prim": 1, "rect": t['rect']},
         {"op": "rect", "prim": 2, "rect": [0, 0, W, H]}, {"op": "uv", "prim": 2, "uv": scrim},
         {"op": "rect", "prim": 3, "rect": [0, 0, W, H]}, {"op": "uv", "prim": 3, "uv": hil},
         {"op": "name", "prim": 3, "name": "buttonhighlight"},
         {"op": "name", "prim": 4, "name": "buttontext"},
         {"op": "text", "prim": 4, "text": t['label']},
         {"op": "textstyle", "prim": 4, "size": 50}]
    if t['texture'] != 'ui_mnu_early_quit_info':
        o.append({"op": "texture", "prim": 1, "texture": t['texture']})
    o.append({"op": "uv", "prim": 1, "uv": t['uv']})
    return o

def render(path, label, png):
    from PIL import Image, ImageDraw, ImageFont
    c = canvas.read(path.read_bytes(), platform='win10_dx12')
    recs, rr = c.records(0), [x[0] for x in ce.computed_rects(c)]
    tx = {p.stem: p for p in TEX.glob('*.png')}
    def tex(sym): return Image.open(tx[f'{sym:016x}']).convert('RGBA')
    def crop(im, uv, box):
        w, h = im.size
        x0, y0, x1, y1 = box
        return im.crop((round(uv[0]*w), round(uv[1]*h), round(uv[2]*w), round(uv[3]*h))).resize((max(1, round(x1-x0)), max(1, round(y1-y0))), Image.LANCZOS)
    S = 1
    out = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    mask_r = rr[0]; m = tex(struct.unpack_from('<Q', recs[0], 0x78)[0]).convert('L')
    mask = Image.new('L', (W, H), 0); mask.paste(m.resize((round(mask_r[2]-mask_r[0]), round(mask_r[3]-mask_r[1]))), (round(mask_r[0]), round(mask_r[1])))
    img_layer = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    r1 = rr[1]; im = crop(tex(struct.unpack_from('<Q', recs[1], 0x88)[0]), struct.unpack_from('<4f', recs[1], 0x90), r1)
    img_layer.paste(im, (round(r1[0]), round(r1[1])))
    a = Image.composite(img_layer, Image.new('RGBA', (W, H), (0, 0, 0, 0)), mask)
    out.alpha_composite(a)
    r2 = rr[2]; fr = crop(tex(struct.unpack_from('<Q', recs[2], 0x88)[0]), struct.unpack_from('<4f', recs[2], 0x90), r2)
    out.alpha_composite(fr, (round(r2[0]), round(r2[1])))
    d = ImageDraw.Draw(out)
    try: f = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf', 44)
    except OSError: f = ImageFont.load_default()
    r4 = rr[4]; tw = d.textlength(label, font=f)
    d.text(((r4[0]+r4[2]-tw)/2, (r4[1]+r4[3])/2 - 26), label, font=f, fill=(255, 255, 255, 255))
    bg = Image.new('RGBA', (W + 40, H + 40), (70, 82, 100, 255)); bg.alpha_composite(out, (20, 20))
    bg.convert('RGB').save(png)

only = sys.argv[3] if len(sys.argv) > 3 and sys.argv[2] == '--only' else None
for key, t in TILES.items():
    if only and key != only: continue
    oj = OPS / f'ops_{key}.json'; oj.write_text(json.dumps(ops(t), indent=1))
    r = subprocess.run([sys.executable, str(REPO / 'lib/menu_ui/canvas_edit.py'), 'emit', 'ui_mni_quit_info',
                        '--edits', str(oj), '--out', str(OUT), '--as', t['name']], capture_output=True, text=True)
    if r.returncode: print(r.stdout[-1500:], r.stderr[-1500:]); sys.exit(1)
    rep = json.loads(r.stdout[:r.stdout.index('\n}') + 2])
    f = next(OUT.glob(f"primary/*/*/{hash_name(t['name']):016x}"))
    print(key, t['name'], f'0x{hash_name(t["name"]):016x}', 'gates', all(rep['gates'].values()), 'bytes', rep['bytes'], f.relative_to(OUT))
    if TEX.is_dir(): render(f, t['label'], HERE / f'preview_{key}.png')
