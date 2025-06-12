#include "ata.h"
#include "../../io/io.h"
#include "../../io/log.h"

void await_bsy_clear(unsigned short bus){
    while(inb(ATA_STATUS_PORT(bus)) & 0x80);
}

void await_drq_set(unsigned short bus){
    while(!(inb(ATA_STATUS_PORT(bus)) & 0x08));
}

void ata_poll(unsigned short bus){
    await_bsy_clear(bus);
    await_drq_set(bus);
}
void init_disk_driver(){

    unsigned short identify_info[(ATA_SECTOR_SIZE / 2)];
    // select drive 0
    outb(ATA_DRIVE_SELECT_PORT(ATA_PRIMARY_BUS_IO),ATA_INIT_DRIVE_ONE);
    // set sector count and LBA
    outb(ATA_SECTOR_COUNT_PORT(ATA_PRIMARY_BUS_IO),0x0);
    outb(ATA_LBA_LOW_PORT(ATA_PRIMARY_BUS_IO),0x0);
    outb(ATA_LBA_MID_PORT(ATA_PRIMARY_BUS_IO),0x0);
    outb(ATA_LBA_HIGH_PORT(ATA_PRIMARY_BUS_IO),0x0);
    
    // send IDENTIFY
    outb(ATA_COMMAND_PORT(ATA_PRIMARY_BUS_IO),ATA_COMMAND_IDENTIFY);
    
    unsigned char status = inb(ATA_STATUS_PORT(ATA_PRIMARY_BUS_IO));
    
    if(!status){
        error("Selected drive does not exist");
        return;
    }
    
    await_bsy_clear(ATA_PRIMARY_BUS_IO);

    // check LBAmid and LBAhi
    if (inb(ATA_LBA_MID_PORT(ATA_PRIMARY_BUS_IO) + inb(ATA_LBA_HIGH_PORT(ATA_PRIMARY_BUS_IO) != 0))){
        warn("Selected drive is not ATA");
        return;
    }

    await_drq_set(ATA_PRIMARY_BUS_IO);

    // check for ERR clear and read the IDENTIFY data
    if(!(inb(ATA_STATUS_PORT(ATA_PRIMARY_BUS_IO) & 0x1))){
        for(unsigned int i = 0; i < (ATA_SECTOR_SIZE / 2);i++){
            identify_info[i] = inw(ATA_DATA_PORT(ATA_PRIMARY_BUS_IO));
        }
    }

    if (identify_info[83] & 0x400) log("Selected Drive supports LBA48");

    unsigned int addressable_LBA28_sectors = identify_info[61];
    addressable_LBA28_sectors <<= 16;
    addressable_LBA28_sectors |= identify_info[60];
    log("Number of addressable LBA28 sectors: ");
    log_uint(addressable_LBA28_sectors);

}

void read_sectors(unsigned short bus,unsigned char n_sectors, unsigned char* buf, unsigned int lba){

    outb(ATA_DRIVE_SELECT_PORT(bus), ATA_SELECT_DRIVE_ONE | ((lba >> 24) & 0x0f));
    outb(ATA_FEATURE_PORT(bus),0x00); // waste of CPU time
    outb(ATA_SECTOR_COUNT_PORT(bus),n_sectors);
    outb(ATA_LBA_LOW_PORT(bus),(unsigned char)lba);
    outb(ATA_LBA_MID_PORT(bus),(unsigned char)(lba >> 8));
    outb(ATA_LBA_HIGH_PORT(bus),(unsigned char)(lba >> 16));
    outb(ATA_COMMAND_PORT(bus),ATA_COMMAND_READ_SECTORS);

    for (unsigned int sec = 0;  sec < n_sectors;sec++){
        ata_poll(bus);
        for(unsigned int i = 0; i < (ATA_SECTOR_SIZE / 2);i++){
            unsigned short word = inw(ATA_DATA_PORT(bus));
            buf[sec * ATA_SECTOR_SIZE + i * 2] = (unsigned char)word;
            buf[sec * ATA_SECTOR_SIZE + i * 2 + 1] = (unsigned char)(word >> 8);
        }
    }
    
    unsigned char errors = inb(ATA_STATUS_PORT(bus));
    if (errors & 0x1) warn("An error has occured during reading a disk sector");

}

void write_sectors(unsigned short bus, unsigned char n_sectors, unsigned char* buf,unsigned int lba){
    outb(ATA_DRIVE_SELECT_PORT(bus), ATA_SELECT_DRIVE_ONE | ((lba >> 24) & 0x0f));
    outb(ATA_FEATURE_PORT(bus),0x00); // waste of CPU time
    outb(ATA_SECTOR_COUNT_PORT(bus),n_sectors);
    outb(ATA_LBA_LOW_PORT(bus),(unsigned char)lba);
    outb(ATA_LBA_MID_PORT(bus),(unsigned char)(lba >> 8));
    outb(ATA_LBA_HIGH_PORT(bus),(unsigned char)(lba >> 16));
    outb(ATA_COMMAND_PORT(bus),ATA_COMMAND_WRITE_SECTORS);
    
    

    for (unsigned int sec = 0;  sec < n_sectors;sec++){
        ata_poll(bus);
        for(unsigned int i = 0; i < (ATA_SECTOR_SIZE / 2);i++){
            unsigned int word = buf[sec * ATA_SECTOR_SIZE + i * 2 + 1];
            word <<= 8;
            word |= buf[sec * ATA_SECTOR_SIZE + i * 2];
            outw(ATA_DATA_PORT(bus),word);
            asm volatile ("outb %%al, $0x80" : : "a"(0)); // tiny delay

        }
    }

    outb(ATA_COMMAND_PORT(bus),ATA_COMMAND_CACHE_FLUSH);
    await_bsy_clear(bus);

    unsigned char errors = inb(ATA_STATUS_PORT(bus));
    if (errors & 0x1) warn("An error has occured during write or flush of a disj sector");
}
