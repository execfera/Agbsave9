#include <gba.h>
#include <stdio.h>
#include <string.h>

u8 const *save = (void *)0x8020000;

void sram() {
	u8 volatile *sram = (void *)SRAM;
	int x;
	for(x = 0; x <= 0x7FFE; x++) {
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
	u16 buffer[73];
	memset(buffer, 0, sizeof(u16) * 73);
	u16 x;
	int z;
	for(x = 0; x <= 512; x += 8) {
		for(z = 0; z <= 64; z++) {
			if ((save[x + (z / 8)] & 1 << z % 8) != 0) {
				buffer[z + 1] = 1;
			}
		}
		for(z = 65; z <= 71; z++) {
			buffer[z] = x & 1 << z % 8;
		}
		buffer[72] = 1;
		dmaCopy(buffer, (void *)0xDFFFF00, 73 << 1);
		while((*(u8 *)0xDFFFF00 & 1) != 1); // Wait for data to be written.
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
