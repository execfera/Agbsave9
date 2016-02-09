#include <gba.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

u8 const *save = (void *)0x8020000;

void sram() {
	u8 volatile *sram = (void *)SRAM;
	int x;
	for(x = 0; x <= 0x7FFF; x++) {
		sram[x] = save[x];
	}
}

void inline flash_command(u8 command) {
	u8 volatile *command_addr1 = (void *)0xE005555;
	u8 volatile *command_addr2 = (void *)0xE002AAA;
	*command_addr1 = 0xAA;
	*command_addr2 = 0x55;
	*command_addr1 = command;
}

void flash() {
	u8 volatile *flash = (void *)0xE000000;
	flash_command(0x90); // Enter id mode
	flash[0]; // read manufacuer
	flash[1]; // read dev
	flash_command(0xF0); // Exit id mode
	flash_command(0x80); // Erase command
	flash_command(0x10); // Erase entire flash
	while(*flash != 0xFF); // Wait for flash to be erased
	int x;
	for(x = 0; x <= 0xFFFF ; x++) {
		flash_command(0xA0); // Write byte
		flash[x] = save[x];
		while(flash[x] != save[x]); // Wait for byte to be written
	}
} 

void eeprom() {
	u16 buffer[81];
	int x;
	int z;
	int i;
	int volatile v;
	buffer[0] = 1; // Read from eeprom
	buffer[1] = 1;
	dmaCopy(buffer, (void *)0xDFFFF00, 17 << 1);
	dmaCopy((void *)0xDFFFF00, buffer, 68 << 1);
	for(x = 0; x <= 0x1FFF; x += 8) {
		memset(buffer, 0, sizeof(u16) * 81);
		buffer[0] = 1;
		for(z = 0; z <= 13; z++) {
			if (((x / 8) & (1 << (13 - z))) != 0)
				buffer[z + 2] = 1;
		}
		for(i = 0; i <= 7; i++) {
			for(z = 0; z <= 7; z++) {
				if ((save[x + i] & (1 << (7 - z))) != 0) {
					buffer[z + 16 + (i * 8)] = 1;
				}
			}
		}
		dmaCopy(buffer, (void *)0xDFFFF00, 81 << 1);
		for(v = 0; v <= 27092; v++); // Wait until eeprom should be written
		if (x % 200 == 0) {
			iprintf("%d: Bytes written\n", x);
		}
	}
}

int main() {
	u8 savetype = *(u8 *)0x801FFFF;
	irqInit();
	irqEnable(IRQ_VBLANK);

	consoleDemoInit();
	
	if (savetype == 0) {
		sram();
	} else if (savetype == 1) {
		flash();
	} else if (savetype == 2) {
		eeprom();
	}
	iprintf("AGB_FIRM save injector\n");
	while(1);
}
