"""Swap the `sprite` row of one card element (OWNER on PAGE) in a mnu_master CR15UIPage2CR file. 8 bytes.
usage: card_sprite.py IN OUT OWNER PAGE SPRITE   (SPRITE must be a T2 sprite of ui_mnu_card)"""
import struct, sys
from pathlib import Path
REPO = Path(__import__('os').environ['MAIN_MENU_A_ROOT'])
sys.path.insert(0, str(REPO / 'lib')); sys.path.insert(0, str(REPO / 'lib/menu_ui'))
import page_heap
from csymbol64 import hash_name
from game_store import Basis
from resource_io import cuicanvasresource as canvas
src, dst, owner, page, sprite = sys.argv[1], sys.argv[2], int(sys.argv[3], 0), int(sys.argv[4]), sys.argv[5]
KEY, NEW = hash_name('sprite'), hash_name(sprite)
b = Basis.pc_final()
card = canvas.read(b.read('primary', 'CUICanvasResource', 'ui_mnu_card'), platform='win10_dx12')
if NEW not in {struct.unpack_from('<Q', r, 0)[0] for r in card.records(2)}:
    sys.exit(f'{sprite}: not a ui_mnu_card sprite')
data = Path(src).read_bytes()
_, arrays, elements = page_heap.decode(data)
els = [e for e in elements if e['page'] == page and int(e['word0'], 16) == owner]
if len(els) != 1:
    sys.exit(f'0x{owner:016x}: {len(els)} elements on page {page}; need 1')
arr = [a for a in arrays if a['page'] == page and a['element'] == els[0]['element'] and a['slot'] == 80]
hits = []
for a in arr:
    for k in range(a['count']):
        o = a['payload_file_offset'] + k * 16
        if struct.unpack_from('<Q', data, o)[0] == KEY:
            hits.append(o)
if len(hits) != 1:
    sys.exit(f'{len(hits)} sprite rows; need 1')
out = bytearray(data); old = struct.unpack_from('<Q', data, hits[0] + 8)[0]
struct.pack_into('<Q', out, hits[0] + 8, NEW)
out = bytes(out)
assert page_heap.decode(out)[0] == out
assert sum(x != y for x, y in zip(data, out)) <= 8 and len(out) == len(data)
Path(dst).parent.mkdir(parents=True, exist_ok=True); Path(dst).write_bytes(out)
print(f'card 0x{owner:016x} p{page}: sprite {b.name_of(old) or hex(old)} -> {sprite} @+0x{hits[0]+8:x}')
