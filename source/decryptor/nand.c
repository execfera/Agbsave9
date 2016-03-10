#include "fs.h"
#include "draw.h"
#include "hid.h"
#include "platform.h"
#include "decryptor/aes.h"
#include "decryptor/sha.h"
#include "decryptor/decryptor.h"
#include "decryptor/nand.h"
#include "fatfs/sdmmc.h"

// see: http://3dbrew.org/wiki/Flash_Filesystem
static PartitionInfo partitions[] = {
    { "TWLN",    {0xE9, 0x00, 0x00, 0x54, 0x57, 0x4C, 0x20, 0x20}, 0x00012E00, 0x08FB5200, 0x3, AES_CNT_TWLNAND_MODE },
    { "TWLP",    {0xE9, 0x00, 0x00, 0x54, 0x57, 0x4C, 0x20, 0x20}, 0x09011A00, 0x020B6600, 0x3, AES_CNT_TWLNAND_MODE },
    { "AGBSAVE", {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 0x0B100000, 0x00030000, 0x7, AES_CNT_CTRNAND_MODE },
    { "FIRM0",   {0x46, 0x49, 0x52, 0x4D, 0x00, 0x00, 0x00, 0x00}, 0x0B130000, 0x00400000, 0x6, AES_CNT_CTRNAND_MODE },
    { "FIRM1",   {0x46, 0x49, 0x52, 0x4D, 0x00, 0x00, 0x00, 0x00}, 0x0B530000, 0x00400000, 0x6, AES_CNT_CTRNAND_MODE },
    { "CTRNAND", {0xE9, 0x00, 0x00, 0x43, 0x54, 0x52, 0x20, 0x20}, 0x0B95CA00, 0x2F3E3600, 0x4, AES_CNT_CTRNAND_MODE }, // O3DS
    { "CTRNAND", {0xE9, 0x00, 0x00, 0x43, 0x54, 0x52, 0x20, 0x20}, 0x0B95AE00, 0x41D2D200, 0x5, AES_CNT_CTRNAND_MODE }  // N3DS
};

static u32 emunand_header = 0;
static u32 emunand_offset = 0;


u32 CheckEmuNand(void)
{
    u8* buffer = BUFFER_ADDRESS;
    u32 nand_size_sectors = getMMCDevice(0)->total_size;
    
    // check the MBR for presence of EmuNAND
    sdmmc_sdcard_readsectors(0, 1, buffer);
    if (nand_size_sectors > getle32(buffer + 0x1BE + 0x8))
        return EMUNAND_NOT_READY;
    
    // check for Gateway type EmuNAND
    sdmmc_sdcard_readsectors(nand_size_sectors, 1, buffer);
    if (memcmp(buffer + 0x100, "NCSD", 4) == 0)
        return EMUNAND_GATEWAY;
    
    // check for RedNAND type EmuNAND
    sdmmc_sdcard_readsectors(1, 1, buffer);
    if (memcmp(buffer + 0x100, "NCSD", 4) == 0)
        return EMUNAND_REDNAND;
        
    // EmuNAND ready but not set up
    return EMUNAND_READY;
}

u32 SetNand(bool set_emunand, bool force_emunand)
{
    if (set_emunand) {
        u32 emunand_state = CheckEmuNand();
        if ((emunand_state == EMUNAND_READY) && force_emunand)
            emunand_state = EMUNAND_GATEWAY;
        switch (emunand_state) {
            case EMUNAND_NOT_READY:
                Debug("SD is not formatted for EmuNAND");
                return 1;
            case EMUNAND_GATEWAY:
                emunand_header = getMMCDevice(0)->total_size;
                emunand_offset = 0;
                Debug("Using EmuNAND @ %06X/%06X", emunand_header, emunand_offset);
                return 0;
            case EMUNAND_REDNAND:
                emunand_header = 1;
                emunand_offset = 1;
                Debug("Using RedNAND @ %06X/%06X", emunand_header, emunand_offset);
                return 0;
            default:
                Debug("EmuNAND is not available");
                return 1;
        }
    } else {
        emunand_header = 0;
        emunand_offset = 0;
        return 0;
    }
}

static inline int ReadNandSectors(u32 sector_no, u32 numsectors, u8 *out)
{
    if (emunand_header) {
        if (sector_no == 0) {
            int errorcode = sdmmc_sdcard_readsectors(emunand_header, 1, out);
            if (errorcode) return errorcode;
            sector_no = 1;
            numsectors--;
            out += 0x200;
        }
        return sdmmc_sdcard_readsectors(sector_no + emunand_offset, numsectors, out);
    } else return sdmmc_nand_readsectors(sector_no, numsectors, out);
}

static inline int WriteNandSectors(u32 sector_no, u32 numsectors, u8 *in)
{
    if (emunand_header) {
        if (sector_no == 0) {
            int errorcode = sdmmc_sdcard_writesectors(emunand_header, 1, in);
            if (errorcode) return errorcode;
            sector_no = 1;
            numsectors--;
            in += 0x200;
        }
        return sdmmc_sdcard_writesectors(sector_no + emunand_offset, numsectors, in);
    } else return sdmmc_nand_writesectors(sector_no, numsectors, in);
}

u32 SetupNandCrypto(u8* ctr, u32 offset)
{
    static bool initial_setup_done = false;
    static u8 CtrNandCtr[16];
    //static u8 TwlNandCtr[16];
    
    if (!initial_setup_done) {
        // CTRNAND
        u8 NandCid[16];
        u8 shasum[32];
        
        sdmmc_get_cid( 1, (uint32_t*) NandCid);
        sha_init(SHA256_MODE);
        sha_update(NandCid, 16);
        sha_get(shasum);
        memcpy(CtrNandCtr, shasum, 16);
        
        /*sha_init(SHA1_MODE);
        sha_update(NandCid, 16);
        sha_get(shasum);
        for(u32 i = 0; i < 16; i++) // little endian and reversed order
            TwlNandCtr[i] = shasum[15-i];
        
        Debug("NAND CID: %08X%08X%08X%08X", getbe32(NandCid), getbe32(NandCid+4), getbe32(NandCid+8), getbe32(NandCid+12));
        
        // part #2: TWL KEY
        // see: https://www.3dbrew.org/wiki/Memory_layout#ARM9_ITCM
        u32* TwlCustId = (u32*) (0x01FFB808+8);
        u8 TwlKeyX[16];
        u8 TwlKeyY[16];
        
        Debug("TWL Customer ID: %08X%08X", TwlCustId[0], TwlCustId[1]);
        
        // see source from https://gbatemp.net/threads/release-twltool-dsi-downgrading-save-injection-etc-multitool.393488/
        const char* nintendo = "NINTENDO";
        u32* TwlKeyXW = (u32*) TwlKeyX;
        TwlKeyXW[0] = (TwlCustId[0] ^ 0xB358A6AF) | 0x80000000;
        TwlKeyXW[3] = TwlCustId[1] ^ 0x08C267B7;
        memcpy(TwlKeyX + 4, nintendo, 8);
        
        // see: https://www.3dbrew.org/wiki/Memory_layout#ARM9_ITCM
        u32 TwlKeyYW3 = 0xE1A00005;
        memcpy(TwlKeyY, (u8*) 0x01FFD3C8, 12);
        memcpy(TwlKeyY + 12, &TwlKeyYW3, 4);
        
        setup_aeskeyX(0x03, TwlKeyX);
        setup_aeskeyY(0x03, TwlKeyY);
        Debug("0x03 KeyX: %08X%08X%08X%08X", getbe32(TwlKeyX), getbe32(TwlKeyX+4), getbe32(TwlKeyX+8), getbe32(TwlKeyX+12));
        Debug("0x03 KeyY: %08X%08X%08X%08X", getbe32(TwlKeyY), getbe32(TwlKeyY+4), getbe32(TwlKeyY+8), getbe32(TwlKeyY+12));
        
        // part #3: CTRNAND N3DS KEY
        while (GetUnitPlatform() == PLATFORM_N3DS) {
            u8 CtrNandKeyY[16];
            
            if (!FileOpen("slot0x05KeyY.bin")) {
                Debug("0x05 KeyY: not set, slot0x05KeyY.bin not found");
                break;
            }
            if (FileRead(CtrNandKeyY, 16, 0) != 16) {
                Debug("0x05 KeyY: not set, bad file");
                FileClose();
                break;
            }
            FileClose();
            
            setup_aeskeyY(0x05, CtrNandKeyY);
            Debug("0x05 KeyY: %08X%08X%08X%08X", getbe32(CtrNandKeyY), getbe32(CtrNandKeyY+4), getbe32(CtrNandKeyY+8), getbe32(CtrNandKeyY+12));
            break;
        }*/
        
        initial_setup_done = true;
    }
    
    // get the correct CTR and increment counter
    memcpy(ctr, CtrNandCtr, 16);
    add_ctr(ctr, offset / 0x10);

    return 0;
}

u32 OutputFileNameSelector(char* filename, const char* basename, char* extension, bool emuname) {
    char bases[3][64] = { 0 };
    char* dotpos = NULL;
    
    // build first base name and extension
    strncpy(bases[0], basename, 63);
    dotpos = strrchr(bases[0], '.');
    
    if (dotpos) {
        *dotpos = '\0';
        if (!extension)
            extension = dotpos + 1;
    }
    
    // build other two base names
    snprintf(bases[1], 63, "%s_%s", bases[0], (emuname) ? "emu" : "sys");
    snprintf(bases[2], 63, "%s%s" , (emuname) ? "emu" : "sys", bases[0]);
    
    u32 fn_id = 0;
    u32 fn_num = 0;
    bool exists = false;
    char extstr[16] = { 0 };
    if (extension)
        snprintf(extstr, 15, ".%s", extension);
    Debug("Use arrow keys and <A> to choose a name");
    while (true) {
        char numstr[2] = { 0 };
        // build and output file name (plus "(!)" if existing)
        numstr[0] = (fn_num > 0) ? '0' + fn_num : '\0';
        snprintf(filename, 63, "%s%s%s", bases[fn_id], numstr, extstr);
        if ((exists = FileOpen(filename)))
            FileClose();
        Debug("\r%s%s", filename, (exists) ? " (!)" : "");
        // user input routine
        u32 pad_state = InputWait();
        if (pad_state & BUTTON_DOWN) { // increment filename id
            fn_id = (fn_id + 1) % 3;
            fn_num = 0;
        } else if (pad_state & BUTTON_UP) { // decrement filename id
            fn_id = (fn_id > 0) ? fn_id - 1 : 2;
        } else if ((pad_state & BUTTON_RIGHT) && (fn_num < 9)) { // increment number
            fn_num++;
        } else if ((pad_state & BUTTON_LEFT) && (fn_num > 0)) { // decrement number
            fn_num--;
        } else if (pad_state & BUTTON_A) {
            Debug("%s%s", filename, (exists) ? " (!)" : "");
            break;
        } else if (pad_state & BUTTON_B) {
            Debug("(cancelled by user)");
            return 2;
        }
    }
    
    // overwrite confirmation
    if (exists) {
        Debug("Press <A> to overwrite existing file");
        while (true) {
            u32 pad_state = InputWait();
            if (pad_state & BUTTON_A) {
                break;
            } else if (pad_state & BUTTON_B) {
                Debug("(cancelled by user)");
                return 2;
            }
        }
    }
    
    return 0;
}

u32 InputFileNameSelector(char* filename, const char* basename, char* extension, u8* magic, u32 msize, u32 fsize) {
    char** fnptr = (char**) 0x20400000; // allow using 0x8000 byte
    char* fnlist = (char*) 0x20408000; // allow using 0x80000 byte
    u32 n_names = 0;
    
    // get the file list - try work directory first
    if (!GetFileList(WORK_DIR, fnlist, 0x80000, false, true, false) && !GetFileList("/", fnlist, 0x800000, false, true, false)) {
        Debug("Failed retrieving the file names list");
        return 1;
    }
    
    // get base name, extension
    char base[64] = { 0 };
    if (basename != NULL) {
        // build base name and extension
        strncpy(base, basename, 63);
        char* dotpos = strrchr(base, '.');
        if (dotpos) {
            *dotpos = '\0';
            if (!extension)
                extension = dotpos + 1;
        }
    }
    
    // limit magic number size
    if (msize > 0x200)
        msize = 0x200;
    
    // parse the file names list for usable entries
    for (char* fn = strtok(fnlist, "\n"); fn != NULL; fn = strtok(NULL, "\n")) {
        u8 data[0x200];
        char* dotpos = strrchr(fn, '.');
        if (strrchr(fn, '/'))
            fn = strrchr(fn, '/') + 1;
        if (strnlen(fn, 128) > 63)
            continue; // file name too long
        if ((basename != NULL) && !strstr(fn, base))
            continue; // basename check failed
        if ((extension != NULL) && (dotpos != NULL) && (strncmp(dotpos + 1, extension, strnlen(extension, 16))))
            continue; // extension check failed
        else if ((extension == NULL) != (dotpos == NULL))
            continue; // extension check failed
        if (!FileOpen(fn))
            continue; // file can't be opened
        if (fsize && (FileGetSize() != fsize)) {
            FileClose();
            continue; // file size check failed
        }
        if (msize) {
            if (FileRead(data, msize, 0) != msize) {
                FileClose();
                continue; // can't be read
            }
            if (memcmp(data, magic, msize) != 0) {
                FileClose();
                continue; // magic number does not match
            }
        }
        FileClose();
        // this is a match - keep it
        fnptr[n_names++] = fn;
        if (n_names * sizeof(char**) >= 0x8000)
            return 1;
    }
    if (n_names == 0) {
        Debug("No usable file found");
        return 1;
    }
    
    u32 index = 0;
    Debug("Use arrow keys and <A> to choose a file");
    while (true) {
        snprintf(filename, 63, "%s", fnptr[index]);
        Debug("\r%s", filename);
        u32 pad_state = InputWait();
        if (pad_state & BUTTON_DOWN) { // next filename
            index = (index + 1) % n_names;
        } else if (pad_state & BUTTON_UP) { // previous filename
            index = (index > 0) ? index - 1 : n_names - 1;
        } else if (pad_state & BUTTON_A) {
            Debug("%s", filename);
            break;
        } else if (pad_state & BUTTON_B) {
            Debug("(cancelled by user)");
            return 2;
        }
    }
    
    return 0;
}

PartitionInfo* GetPartitionInfo(u32 partition_id)
{
    u32 partition_num = 0;
    
    if (partition_id == P_CTRNAND) {
        partition_num = (GetUnitPlatform() == PLATFORM_3DS) ? 5 : 6;
    } else {
        for(; !(partition_id & (1<<partition_num)) && (partition_num < 32); partition_num++);
    }
    
    return (partition_num >= 32) ? NULL : &(partitions[partition_num]);
}

u32 CtrNandPadgen(u32 param)
{
    u32 keyslot;
    u32 nand_size;

    // legacy sizes & offset, to work with 3DSFAT16Tool
    if (GetUnitPlatform() == PLATFORM_3DS) {
        keyslot = 0x4;
        nand_size = 758;
    } else {
        keyslot = 0x5;
        nand_size = 1055;
    }

    Debug("Creating NAND FAT16 xorpad. Size (MB): %u", nand_size);
    Debug("Filename: nand.fat16.xorpad");

    PadInfo padInfo = {
        .keyslot = keyslot,
        .setKeyY = 0,
        .size_mb = nand_size,
        .filename = "nand.fat16.xorpad",
        .mode = AES_CNT_CTRNAND_MODE
    };
    if(GetNandCtr(padInfo.ctr, 0xB930000) != 0)
        return 1;

    return CreatePad(&padInfo);
}

u32 TwlNandPadgen(u32 param)
{
    u32 size_mb = (partitions[0].size + (1024 * 1024) - 1) / (1024 * 1024);
    Debug("Creating TWLN FAT16 xorpad. Size (MB): %u", size_mb);
    Debug("Filename: twlnand.fat16.xorpad");

    PadInfo padInfo = {
        .keyslot = partitions[0].keyslot,
        .setKeyY = 0,
        .size_mb = size_mb,
        .filename = "twlnand.fat16.xorpad",
        .mode = AES_CNT_TWLNAND_MODE
    };
    if(GetNandCtr(padInfo.ctr, partitions[0].offset) != 0)
        return 1;

    return CreatePad(&padInfo);
}

u32 Firm0Firm1Padgen(u32 param)
{
    u32 size_mb = (partitions[3].size + partitions[4].size + (1024 * 1024) - 1) / (1024 * 1024);
    Debug("Creating FIRM0FIRM1 xorpad. Size (MB): %u", size_mb);
    Debug("Filename: firm0firm1.xorpad");

    PadInfo padInfo = {
        .keyslot = partitions[3].keyslot,
        .setKeyY = 0,
        .size_mb = size_mb,
        .filename = "firm0firm1.xorpad",
        .mode = AES_CNT_CTRNAND_MODE
    };
    if(GetNandCtr(padInfo.ctr, partitions[3].offset) != 0)
        return 1;

    return CreatePad(&padInfo);
}

u32 GetNandCtr(u8* ctr, u32 offset)
{
    static const char* versions[] = {"4.x", "5.x", "6.x", "7.x", "8.x", "9.x"};
    static const u8* version_ctrs[] = {
        (u8*)0x080D7CAC,
        (u8*)0x080D858C,
        (u8*)0x080D748C,
        (u8*)0x080D740C,
        (u8*)0x080D74CC,
        (u8*)0x080D794C
    };
    static const u32 version_ctrs_len = sizeof(version_ctrs) / sizeof(u32);
    static u8* ctr_start = NULL;
    
    if (ctr_start == NULL) {
        for (u32 i = 0; i < version_ctrs_len; i++) {
            if (*(u32*)version_ctrs[i] == 0x5C980) {
                Debug("System version %s", versions[i]);
                ctr_start = (u8*) version_ctrs[i] + 0x30;
            }
        }
        
        // If value not in previous list start memory scanning (test range)
        if (ctr_start == NULL) {
            for (u8* c = (u8*) 0x080D8FFF; c > (u8*) 0x08000000; c--) {
                if (*(u32*)c == 0x5C980 && *(u32*)(c + 1) == 0x800005C9) {
                    ctr_start = c + 0x30;
                    Debug("CTR start 0x%08X", ctr_start);
                    break;
                }
            }
        }
        
        if (ctr_start == NULL) {
            Debug("CTR start not found!");
            return 1;
        }
    }
    
    // the ctr is stored backwards in memory
    if (offset >= 0x0B100000) { // CTRNAND/AGBSAVE region
        for (u32 i = 0; i < 16; i++)
            ctr[i] = *(ctr_start + (0xF - i));
    } else { // TWL region
        for (u32 i = 0; i < 16; i++)
            ctr[i] = *(ctr_start + 0x88 + (0xF - i));
    }
    
    // increment counter
    add_ctr(ctr, offset / 0x10);

    return 0;
}

u32 DecryptNandToMem(u8* buffer, u32 offset, u32 size, PartitionInfo* partition)
{
    CryptBufferInfo info = {.keyslot = partition->keyslot, .setKeyY = 0, .size = size, .buffer = buffer, .mode = partition->mode};
    if(SetupNandCrypto(info.ctr, offset) != 0)
        return 1;

    u32 n_sectors = (size + NAND_SECTOR_SIZE - 1) / NAND_SECTOR_SIZE;
    u32 start_sector = offset / NAND_SECTOR_SIZE;
    ReadNandSectors(start_sector, n_sectors, buffer);
    CryptBuffer(&info);

    return 0;
}

u32 DecryptNandToFile(const char* filename, u32 offset, u32 size, PartitionInfo* partition)
{
    u8* buffer = BUFFER_ADDRESS;
    u32 result = 0;

    if (!DebugFileCreate(filename, true))
        return 1;

    for (u32 i = 0; i < size; i += NAND_SECTOR_SIZE * SECTORS_PER_READ) {
        u32 read_bytes = min(NAND_SECTOR_SIZE * SECTORS_PER_READ, (size - i));
        ShowProgress(i, size);
        DecryptNandToMem(buffer, offset + i, read_bytes, partition);
        if(!DebugFileWrite(buffer, read_bytes, i)) {
            result = 1;
            break;
        }
    }

    ShowProgress(0, 0);
    FileClose();

    return result;
}

u32 DumpNand(u32 param)
{
    char filename[64];
    u8* buffer = BUFFER_ADDRESS;
    u32 nand_size = getMMCDevice(0)->total_size * NAND_SECTOR_SIZE;
    u32 result = 0;

    Debug("Dumping %sNAND. Size (MB): %u", (param & N_EMUNAND) ? "Emu" : "Sys", nand_size / (1024 * 1024));
    
    if (OutputFileNameSelector(filename, "NAND.bin", NULL, param & N_EMUNAND) != 0)
        return 1;
    if (!DebugFileCreate(filename, true))
        return 1;

    u32 n_sectors = nand_size / NAND_SECTOR_SIZE;
    for (u32 i = 0; i < n_sectors; i += SECTORS_PER_READ) {
        u32 read_sectors = min(SECTORS_PER_READ, (n_sectors - i));
        ShowProgress(i, n_sectors);
        ReadNandSectors(i, read_sectors, buffer);
        if(!DebugFileWrite(buffer, NAND_SECTOR_SIZE * read_sectors, i * NAND_SECTOR_SIZE)) {
            result = 1;
            break;
        }
    }

    ShowProgress(0, 0);
    FileClose();

    return result;
}

u32 DecryptNandPartition(u32 param)
{
    PartitionInfo* p_info = NULL;
    char filename[64];
    u8 magic[NAND_SECTOR_SIZE];
    
    for (u32 partition_id = P_TWLN; partition_id <= P_CTRNAND; partition_id = partition_id << 1) {
        if (param & partition_id) {
            p_info = GetPartitionInfo(partition_id);
            break;
        }
    }
    if (p_info == NULL) {
        Debug("No partition to dump");
        return 1;
    }
    
    Debug("Dumping & Decrypting %s, size (MB): %u", p_info->name, p_info->size / (1024 * 1024));
    if (DecryptNandToMem(magic, p_info->offset, 16, p_info) != 0)
        return 1;
    if ((p_info->magic[0] != 0xFF) && (memcmp(p_info->magic, magic, 8) != 0)) {
        Debug("Decryption error, please contact us");
        return 1;
    }
    if (OutputFileNameSelector(filename, p_info->name, "bin", param & N_EMUNAND) != 0)
        return 1;
    
    return DecryptNandToFile(filename, p_info->offset, p_info->size, p_info);
}

u32 EncryptMemToNand(u8* buffer, u32 offset, u32 size, PartitionInfo* partition)
{
    CryptBufferInfo info = {.keyslot = partition->keyslot, .setKeyY = 0, .size = size, .buffer = buffer, .mode = partition->mode};
    if(GetNandCtr(info.ctr, offset) != 0)
        return 1;

    u32 n_sectors = (size + NAND_SECTOR_SIZE - 1) / NAND_SECTOR_SIZE;
    u32 start_sector = offset / NAND_SECTOR_SIZE;
    CryptBuffer(&info);
    WriteNandSectors(start_sector, n_sectors, buffer);

    return 0;
}

u32 EncryptFileToNand(const char* filename, u32 offset, u32 size, PartitionInfo* partition)
{
    u8* buffer = BUFFER_ADDRESS;
    u32 result = 0;

    if (!DebugFileOpen(filename))
        return 1;
    
    if (FileGetSize() != size) {
        Debug("%s has wrong size", filename);
        FileClose();
        return 1;
    }

    for (u32 i = 0; i < size; i += NAND_SECTOR_SIZE * SECTORS_PER_READ) {
        u32 read_bytes = min(NAND_SECTOR_SIZE * SECTORS_PER_READ, (size - i));
        ShowProgress(i, size);
        if(!DebugFileRead(buffer, read_bytes, i)) {
            result = 1;
            break;
        }
        EncryptMemToNand(buffer, offset + i, read_bytes, partition);
    }

    ShowProgress(0, 0);
    FileClose();

    return result;
}

u32 RestoreNand(u32 param)
{
    char filename[64];
    u8* buffer = BUFFER_ADDRESS;
    u32 nand_size = getMMCDevice(0)->total_size * NAND_SECTOR_SIZE;
    u32 result = 0;
    u8 magic[4];

    if (!(param & N_NANDWRITE)) // developer screwup protection
        return 1;
        
    // User file select
    if (InputFileNameSelector(filename, "NAND.bin", NULL, NULL, 0, nand_size) != 0)
        return 1;
    
    if (!DebugFileOpen(filename))
        return 1;
    if (nand_size != FileGetSize()) {
        FileClose();
        Debug("NAND backup has the wrong size!");
        return 1;
    };
    if(!DebugFileRead(magic, 4, 0x100)) {
        FileClose();
        return 1;
    }
    if (memcmp(magic, "NCSD", 4) != 0) {
        FileClose();
        Debug("Not a proper NAND backup!");
        return 1;
    }
    
    Debug("Restoring %sNAND. Size (MB): %u", (param & N_EMUNAND) ? "Emu" : "Sys", nand_size / (1024 * 1024));

    u32 n_sectors = nand_size / NAND_SECTOR_SIZE;
    for (u32 i = 0; i < n_sectors; i += SECTORS_PER_READ) {
        u32 read_sectors = min(SECTORS_PER_READ, (n_sectors - i));
        ShowProgress(i, n_sectors);
        if(!DebugFileRead(buffer, NAND_SECTOR_SIZE * read_sectors, i * NAND_SECTOR_SIZE)) {
            result = 1;
            break;
        }
        WriteNandSectors(i, read_sectors, buffer);
    }

    ShowProgress(0, 0);
    FileClose();

    return result;
}

u32 InjectNandPartition(u32 param)
{
    PartitionInfo* p_info = NULL;
    char filename[64];
    u8 magic[NAND_SECTOR_SIZE];
    
    if (!(param & N_NANDWRITE)) // developer screwup protection
        return 1;
    
    for (u32 partition_id = P_TWLN; partition_id <= P_CTRNAND; partition_id = partition_id << 1) {
        if (param & partition_id) {
            p_info = GetPartitionInfo(partition_id);
            break;
        }
    }
    if (p_info == NULL) {
        Debug("No partition to inject to");
        return 1;
    }
    
    Debug("Encrypting & Injecting %s, size (MB): %u", p_info->name, p_info->size / (1024 * 1024));
    // User file select
    if (InputFileNameSelector(filename, p_info->name, "bin",
        p_info->magic, (p_info->magic[0] != 0xFF) ? 8 : 0, p_info->size) != 0)
        return 1;
    
    // Encryption check
    if (DecryptNandToMem(magic, p_info->offset, 16, p_info) != 0)
        return 1;
    if ((p_info->magic[0] != 0xFF) && (memcmp(p_info->magic, magic, 8) != 0)) {
        Debug("Decryption error, please contact us");
        return 1;
    }
    
    // File check
    if (FileOpen(filename)) {
        if(!DebugFileRead(magic, 8, 0)) {
            FileClose();
            return 1;
        }
        if ((p_info->magic[0] != 0xFF) && (memcmp(p_info->magic, magic, 8) != 0)) {
            Debug("Bad file content, won't inject");
            FileClose();
            return 1;
        }
        FileClose();
    }
    
    return EncryptFileToNand(filename, p_info->offset, p_info->size, p_info);
}

uint64_t bswap64(uint64_t a)
{
  a = ((a & 0x00000000000000FFULL) << 56) | 
      ((a & 0x000000000000FF00ULL) << 40) | 
      ((a & 0x0000000000FF0000ULL) << 24) | 
      ((a & 0x00000000FF000000ULL) <<  8) | 
      ((a & 0x000000FF00000000ULL) >>  8) | 
      ((a & 0x0000FF0000000000ULL) >> 24) | 
      ((a & 0x00FF000000000000ULL) >> 40) | 
      ((a & 0xFF00000000000000ULL) >> 56);
  return a;
}

u32 DumpAgbSave(u32 parm)
{
    PartitionInfo* p_info = GetPartitionInfo(P_AGBSAVE);
    u8 Header[NAND_SECTOR_SIZE];
    DecryptNandToMem(Header, p_info->offset, NAND_SECTOR_SIZE, p_info);
    u32 Saveadder;
    memcpy(&Saveadder, Header + (sizeof(u8) * 0x50), sizeof(u32));
    char *Magic = ".SAV";
    if (Saveadder != 0x200 || memcmp(Magic, Header, 4)) {
        Debug("The AGBSAVE partition is corrupted.");
        Debug("Did you run a GBA VC game?");
        return 1;
    }
    u32 Savesize;
    memcpy(&Savesize, Header + (sizeof(u8) * 0x54), sizeof(u32));
    u32 Titleid;
    memcpy(&Titleid, Header + 0x38, 4);
    Debug("Title ID: %08x", Titleid);
    if (Savesize == 32768) {
        Debug("Use save type 0: SRAM");
    } else if (Savesize == 65536) {
	Debug("Use save type 1: Flash");
    } else if (Savesize == 0x2000) {
	Debug("Use save type 2: EEPROM");
    } else {
        Debug("Injection support for this game is not yet ready");
    }
    if (Savesize == 0x2000) {
        u8* buffer = BUFFER_ADDRESS;
        int x;
        DecryptNandToMem(buffer, p_info->offset + Saveadder, Savesize, p_info);
        u64 *u64_buffer = (u64 *)buffer;
        for(x = 0; x <= Savesize / 8; x++) {
            u64_buffer[x] = bswap64(u64_buffer[x]);
        }
        DebugFileCreate("agb_dump.sav", true);
        DebugFileWrite(buffer, Savesize, 0);
        return 0;
    } else {
        return DecryptNandToFile("agb_dump.sav", p_info->offset + Saveadder, Savesize, p_info);
    }
}