#!/usr/bin/python3
import struct
import os
import platform
savename = input("Type in the save's name: ")
outname = input("Type in the output cia's name: ")
savetype = int(input("Type in the save type: "))
titleid = int(input("Type in the title id of the game you wish to inject: "), 16)
titleid = struct.pack("<i", titleid)
with open("exheader.bin", "rb") as f:
    exheader = bytearray(f.read())
exheader[0x200 : 0x200 + 4] = titleid
with open("exheader.bin", "wb") as f:
    f.write(exheader)
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
if platform.system() == "Linux":
    makeromname = "./makerom_linux"
elif platform.system() == "Windows":
    makeromname = "makerom.exe"
if platform.system() == "Linux":
    dstoolname = "./3dstool_linux"
elif platform.system() == "Windows":
    dstoolname = "3dstool.exe"
os.system(dstoolname + " -c --header exefshead.bin --exefs-dir exefs --type exefs --file exefs.bin")
os.system(dstoolname + " -c --extendedheader exheader.bin " +
"--romfs romfs.bin --file gba.cxi --header header.bin --type cxi --exefs exefs.bin")
os.system(makeromname + " -content gba.cxi:0:0 -f cia -o "+ outname)