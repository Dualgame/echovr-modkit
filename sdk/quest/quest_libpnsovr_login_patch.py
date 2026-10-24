"""libpnsovr.so login-prerequisite bypass for Echo VR Quest 6528386917200980 (login needed the call at 0x1ecf0c).
A step of the Quest APK builder. Every site's stock bytes are asserted."""
from __future__ import annotations

PNSOVR_CODE_PATCHES = {
    # CNSOVRUser::UpdateInternal prerequisite gates (fn at 0x1ed9e8).
    0x001EDA14: (bytes.fromhex("000a0054"), bytes.fromhex("1f2003d5")),
    0x001EDA34: (bytes.fromhex("000900b4"), bytes.fromhex("1f2003d5")),
    0x001EDB50: (bytes.fromhex("80020034"), bytes.fromhex("14000014")),
    # OVR callback error gates (org-id and user-proof paths) plus the org-id source retarget.
    # 0x1ece7c: GotLoggedInUserOrgIdCb (0x1ece60) error branch.
    0x001ECE7C: (bytes.fromhex("40040036"), bytes.fromhex("22000014")),
    # 0x1ecf0c: retarget ovr_OrgScopedID_GetID -> ovr_GetLoggedInUserID (both stock PLT stubs) so a
    # private server sees a non-zero platform user id. Login needs this call.
    0x001ECF0C: (bytes.fromhex("51eefe97"), bytes.fromhex("7df0fe97")),
    # 0x1ed59c/678/6c0: GotUserProofCB (0x1ed578) error / crypto / serial-compare branches.
    0x001ED59C: (bytes.fromhex("e0040036"), bytes.fromhex("27000014")),
    0x001ED678: (bytes.fromhex("a0010034"), bytes.fromhex("0d000014")),
    0x001ED6C0: (bytes.fromhex("400200b4"), bytes.fromhex("12000014")),
}


def patch_libpnsovr(data: bytes) -> bytes:
    patched = bytearray(data)
    for off, (old, new) in PNSOVR_CODE_PATCHES.items():
        if data[off : off + len(old)] != old:
            got = data[off : off + len(old)].hex()
            raise ValueError(f"libpnsovr.so patch stock bytes mismatch at 0x{off:x}: got {got}")
        patched[off : off + len(new)] = new
        print(f"patched libpnsovr.so at 0x{off:x}: {old.hex()} -> {new.hex()}")
    return bytes(patched)
