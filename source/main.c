#include "common.h"
#include "draw.h"
#include "fs.h"
#include "menu.h"
#include "i2c.h"
#include "decryptor/game.h"
#include "decryptor/nand.h"
#include "decryptor/nandfat.h"
#include "decryptor/titlekey.h"

#define SUBMENU_START 1


MenuInfo menu[] =
{
    {
        "Agbsave9", 1,
        {
            { "Save Dump",           &DumpAgbSave,           0 }
        }
    },
/*
    {
        "EmuNAND Options", 9,
        {
            { "EmuNAND Backup",               &DumpNand,              N_EMUNAND },
            { "EmuNAND Restore",              &RestoreNand,           N_NANDWRITE | N_EMUNAND | N_FORCENAND },
            { "Partition Dump...",            NULL,                   SUBMENU_START + 1 },
            { "Partition Inject...",          NULL,                   SUBMENU_START + 3 },
            { "File Dump...",                 NULL,                   SUBMENU_START + 5 },
            { "File Inject...",               NULL,                   SUBMENU_START + 7 },
            { "Health&Safety Dump",           &DumpHealthAndSafety,   N_EMUNAND },
            { "Health&Safety Inject",         &InjectHealthAndSafety, N_NANDWRITE | N_EMUNAND },
            { "Update SeedDB",                &UpdateSeedDb,          N_EMUNAND }
        }
    },
    {
        NULL, 0, {}, // empty menu to signal end
    }
};


void Reboot()
{
    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 2);
    while(true);
}


void PowerOff()
{
    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 0);
    while (true);
}


int main()
{
    ClearScreenFull(true, true);
    InitFS();

    u32 menu_exit = ProcessMenu(menu, SUBMENU_START);
    
    DeinitFS();
    (menu_exit == MENU_EXIT_REBOOT) ? Reboot() : PowerOff();
    return 0;
}
