#include <gba.h>
#include <stdio.h>
#include <string.h>

void sram() {
	u8 const *save = (void *)0x8020000;
	
	u8 volatile *sram = (void *)SRAM;
	int x;
	for(x = 0; x <= 32768; x++) {
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
	u8 const *save = (void *)0x8020000;
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

int main() {
	u8 savetype = *(u8 *)0x801FFFF;
	irqInit();
	irqEnable(IRQ_VBLANK);

	consoleDemoInit();
	
	if (savetype == 0) {
		sram();
	} else {
		flash();
	}
	iprintf("AGB_FIRM save injector\n");
}
