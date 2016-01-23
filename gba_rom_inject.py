#!/usr/bin/python3
import argparse
import struct
parser = argparse.ArgumentParser(description='Injects gba roms.')
#parser.add_argument("gbarom")
parser.add_argument("contentfile")
args = parser.parse_args()
with open(args.contentfile, "rb") as f:
   content = f.read()
magic = bytes(".CAA", "ascii")
footer = content.find(magic)
footer -= 0x34c
print(hex(footer))
romsize = struct.unpack("<i", content[footer: footer + 4])[0]
output = open("output.gba", "wb")
output.write(content[footer - 0x4 - romsize: footer - 0x4 + romsize])