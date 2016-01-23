#include <gba.h>
#include <stdio.h>

int main(void) {
	irqInit();
	irqEnable(IRQ_VBLANK);

	consoleDemoInit();
	u8 const *save = (void *)0x8020000;
	
	u8 volatile *sram = (void *)SRAM;
	int x;
	for(x = 0; x <= 65536 /2 ; x++) {
		sram[x] = save[x];
	}
	iprintf("AGB_FIRM save injecter\n");
}
