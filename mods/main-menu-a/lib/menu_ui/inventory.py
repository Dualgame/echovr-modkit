"""The constants and helpers `page_heap` / `script_heap` import (their offline inventory commands are not shipped)."""
import csv
import hashlib
import os
from pathlib import Path

BUILD = '6323983201049540'
ROOT = Path(os.environ.get('MENU_UI_ROOT', '.'))
CAP = 64 * 1024 * 1024


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def tsv(path, rows, fields):
    with path.open('w', newline='') as f:
        w = csv.DictWriter(f, fields, delimiter='\t')
        w.writeheader()
        w.writerows(rows)
