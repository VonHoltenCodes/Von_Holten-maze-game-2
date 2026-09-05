#!/usr/bin/env python3
"""Replace the DJGPP stub on a .EXE with CWSDSTUB so the program carries its own DPMI server.
usage: exe2coff.py IN.EXE STUB.EXE OUT.EXE"""
import struct, sys
src, stub, dst = sys.argv[1:4]
data = open(src, "rb").read()
assert data[:2] == b"MZ", "not an MZ executable"
e_cblp, e_cp = struct.unpack_from("<HH", data, 2)
mz_size = (e_cp - 1) * 512 + (e_cblp if e_cblp else 512)
coff = data[mz_size:]
assert coff[:2] == b"\x4c\x01", "COFF image not found after the stub"
open(dst, "wb").write(open(stub, "rb").read() + coff)
print(f"{dst}: stub {len(open(stub,'rb').read())} B + COFF {len(coff)} B (old stub was {mz_size} B)")
