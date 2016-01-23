#!/usr/bin/python3
import struct
import os
import platform
savename = input("Type in the save's name: ")
outname = input("Type in the output cia's name: ")
savetype = int(input("Type in the save type: "))
titleid = input("Type in the title id of the game you wish to inject: ")
titleid = titleid[1:6]
footername = "agb_inject_mb_" + str(savetype) + ".ftr"
with open(footername, "rb") as f:
    footer = f.read()
with open("agb_inject_mb.gba", "rb") as f:
    rom = f.read()
padding = struct.unpack("<i", footer[4:8])[0]
padding -= len(rom)
rom = bytearray(rom + bytes(padding) + footer)
with open(savename, "rb") as f:
    sav = f.read()
rom[0x1ffff] = savetype
rom[0x20000 : 0x20000 + len(sav)] = sav
with open("exefs/code.bin", "wb") as f:
    f.write(rom)
with open("agb_inject_mb.rsf", "rb") as f:
    rsf = bytearray(f.read())
rsf[0x180 : 0x185] = bytes(titleid, "ascii")
with open("agb_inject_mb.rsf", "wb") as f:
    f.write(rsf)
if platform.system() == "Linux":
    makeromname = "./makerom_linux"
elif platform.system() == "Windows":
    makeromname = "makerom.exe"
os.system(makeromname + " -icon exefs/icon.bin " +
"-banner exefs/banner.bin -code exefs/code.bin " +
"-exheader exheader.bin -romfs romfs.bin -rsf agb_inject_mb.rsf -o gba.cxi")
os.system(makeromname + " -contents gba.cxi:0:0 -o "+ outname)