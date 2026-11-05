"""`csymbol64(name)` under the module name the menu stage scripts import it from."""
from csymbol64 import hash_name


def csymbol64(name: str) -> int:
    return hash_name(name) & 0xFFFFFFFFFFFFFFFF
