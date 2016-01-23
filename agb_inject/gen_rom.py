#!/usr/bin/python3
with open("agb_inject_mb.gba", "rb") as f:
    rom = f.read()
size = len(rom)
size = 196608 - size
rom = bytearray(rom + bytes(size))
with open("inject.sav", "rb") as f:
    sav = f.read()
rom[0x1ffff] = 1
rom[0x20000 : 0x20000 + 65536] = sav
out = open("out.gba", "wb")
out.write(rom)
out.close()