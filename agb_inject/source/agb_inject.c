#include <gba.h>
#include <stdio.h>
#include <string.h>

void sram() {
	u8 const *save = (void *)0x8020000;
	
	u8 volatile *sram = (void *)SRAM;
	int x;
	for(x = 0; x <= 65536 /2 ; x++) {
		sram[x] = save[x];
	}
}

void flash() {
	u8 const *save = (void *)0x8020000;
	u8 volatile *flash = (void *)0xE000000;
	u8 volatile *command1 = (void *)0xE005555;
	u8 volatile *command2 = (void *)0xE002AAA;
	*command1 = 0xAA; // Enter id mode
	*command2 = 0x55;
	*command1 = 0x90;
	flash[0]; // read manufacuer
	flash[1]; // read dev
	*command1 = 0xAA; // Exit id mode
	*command2 = 0x55;
	*command1 = 0xF0;
	*command1 = 0xAA; //erase command
	*command2 = 0x55;
	*command1 = 0x80;
	*command1 = 0xAA; //erase entire flash
	*command2 = 0x55;
	*command1 = 0x10;
	while(*flash != 0xFF);
	int x;
	for(x = 0; x <= 65536 ; x++) {
		*command1 = 0xAA; // Write byte
		*command2 = 0x55;
		*command1 = 0xA0;
		flash[x] = save[x];
		while(flash[x] != save[x]);
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
	iprintf("AGB_FIRM save injecter\n");
}
