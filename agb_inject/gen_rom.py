#!/usr/bin/python3
import struct
import os
import platform
import shutil
import sys
import subprocess
ScriptPath = os.path.dirname(os.path.realpath(sys.argv[0]))
if platform.system() == "Windows":
	ScriptPath = ScriptPath + "\\"
else:
	ScriptPath = ScriptPath + "/"
if sys.version_info[0] < 3:
    print("Use Python3.")
savename = input("Type in the save's name: ")
outname = input("Type in the output cia's name: ")
savetype = int(input("Type in the save type: "))
titleid = input("Type in the title id of the game you wish to inject: ")
os.mkdir(ScriptPath + "tmp")
try:
   titleid = titleid[1:6]
   footername = ScriptPath + "agb_inject_mb_" + str(savetype) + ".ftr"
   with open(footername, "rb") as f:
       footer = f.read()
   with open(ScriptPath + "agb_inject_mb.gba", "rb") as f:
       rom = f.read()
   padding = struct.unpack("<i", footer[4:8])[0]
   padding -= len(rom)
   rom = bytearray(rom + bytes(padding) + footer)
   with open(savename, "rb") as f:
       sav = f.read()
   rom[0x1ffff] = savetype
   rom[0x20000 : 0x20000 + len(sav)] = sav
   with open(ScriptPath + "tmp/code.bin", "wb") as f:
       f.write(rom)
   with open(ScriptPath + "agb_inject_mb.rsf", "rb") as f:
       rsf = bytearray(f.read())
   rsf[0x180 : 0x185] = bytes(titleid, "ascii")
   with open(ScriptPath + "tmp/agb_inject_mb.rsf", "wb") as f:
       f.write(rsf)
   if platform.system() == "Linux":
       makeromname = ScriptPath + "makerom_linux"
   elif platform.system() == "Windows":
       makeromname = ScriptPath + "makerom.exe"
   subprocess.call([makeromname, "-icon", ScriptPath + "exefs/icon.bin",
   "-banner", ScriptPath + "exefs/banner.bin", "-code", ScriptPath + "tmp/code.bin",
   "-exheader", ScriptPath + "exheader.bin", "-romfs", ScriptPath + "romfs.bin", 
   "-rsf", ScriptPath + "tmp/agb_inject_mb.rsf", "-f", "cxi", "-o", ScriptPath + "tmp/gba.cxi"])
   subprocess.call([makeromname, "-content", ScriptPath + "tmp/gba.cxi:0:0", "-f", "cia", "-o", outname])
finally:
   shutil.rmtree(ScriptPath + "tmp")