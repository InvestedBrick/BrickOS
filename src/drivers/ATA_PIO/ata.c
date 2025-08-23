#include "ata.h"
#include "../../io/io.h"
#include "../../io/log.h"
#include "../../tables/interrupts.h"

uint32_t addressable_LBA28_sectors = 0;
void await_bsy_clear(uint16_t bus){
    while(inb(ATA_STATUS_PORT(bus)) & 0x80);
}

void await_drq_set(uint16_t bus){
    while(!(inb(ATA_STATUS_PORT(bus)) & 0x08));
}

void ata_poll(uint16_t bus){
    await_bsy_clear(bus);
    await_drq_set(bus);
}
void init_disk_driver(){
    uint8_t old_int_status = get_interrupt_status();
    disable_interrupts();
    uint16_t identify_info[(ATA_SECTOR_SIZE / 2)];
    // select drive 0
    outb(ATA_DRIVE_SELECT_PORT(ATA_PRIMARY_BUS_IO),ATA_INIT_DRIVE_ONE);
    // set sector count and LBA
    outb(ATA_SECTOR_COUNT_PORT(ATA_PRIMARY_BUS_IO),0x0);
    outb(ATA_LBA_LOW_PORT(ATA_PRIMARY_BUS_IO),0x0);
    outb(ATA_LBA_MID_PORT(ATA_PRIMARY_BUS_IO),0x0);
    outb(ATA_LBA_HIGH_PORT(ATA_PRIMARY_BUS_IO),0x0);
    
    // send IDENTIFY
    outb(ATA_COMMAND_PORT(ATA_PRIMARY_BUS_IO),ATA_COMMAND_IDENTIFY);
    
    uint8_t status = inb(ATA_STATUS_PORT(ATA_PRIMARY_BUS_IO));
    
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
        for(uint32_t i = 0; i < (ATA_SECTOR_SIZE / 2);i++){
            identify_info[i] = inw(ATA_DATA_PORT(ATA_PRIMARY_BUS_IO));
        }
    }

    if (identify_info[83] & 0x400) log("Selected Drive supports LBA48");

    addressable_LBA28_sectors = identify_info[61];
    addressable_LBA28_sectors <<= 16;
    addressable_LBA28_sectors |= identify_info[60];
    log("Number of addressable LBA28 sectors: ");
    log_uint(addressable_LBA28_sectors);
    set_interrupt_status(old_int_status);

}

void read_sectors(uint16_t bus,uint8_t n_sectors, unsigned char* buf, uint32_t lba){
    uint8_t old_int_status = get_interrupt_status();
    disable_interrupts();
    outb(ATA_DRIVE_SELECT_PORT(bus), ATA_SELECT_DRIVE_ONE | ((lba >> 24) & 0x0f));
    outb(ATA_FEATURE_PORT(bus),0x00); // waste of CPU time
    outb(ATA_SECTOR_COUNT_PORT(bus),n_sectors);
    outb(ATA_LBA_LOW_PORT(bus),(uint8_t)lba);
    outb(ATA_LBA_MID_PORT(bus),(uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH_PORT(bus),(uint8_t)(lba >> 16));
    outb(ATA_COMMAND_PORT(bus),ATA_COMMAND_READ_SECTORS);

    for (uint32_t sec = 0;  sec < n_sectors;sec++){
        ata_poll(bus);
        for(uint32_t i = 0; i < (ATA_SECTOR_SIZE / 2);i++){
            uint16_t word = inw(ATA_DATA_PORT(bus));
            buf[sec * ATA_SECTOR_SIZE + i * 2] = (uint8_t)word;
            buf[sec * ATA_SECTOR_SIZE + i * 2 + 1] = (uint8_t)(word >> 8);
        }
    }
    
    uint8_t errors = inb(ATA_STATUS_PORT(bus));
    if (errors & 0x1) warn("An error has occured during reading a disk sector");
    set_interrupt_status(old_int_status);
}

void write_sectors(uint16_t bus, uint8_t n_sectors, unsigned char* buf,uint32_t lba){
    uint8_t old_int_status = get_interrupt_status();
    disable_interrupts();
    outb(ATA_DRIVE_SELECT_PORT(bus), ATA_SELECT_DRIVE_ONE | ((lba >> 24) & 0x0f));
    outb(ATA_FEATURE_PORT(bus),0x00); // waste of CPU time
    outb(ATA_SECTOR_COUNT_PORT(bus),n_sectors);
    outb(ATA_LBA_LOW_PORT(bus),(uint8_t)lba);
    outb(ATA_LBA_MID_PORT(bus),(uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH_PORT(bus),(uint8_t)(lba >> 16));
    outb(ATA_COMMAND_PORT(bus),ATA_COMMAND_WRITE_SECTORS);
    
    

    for (uint32_t sec = 0;  sec < n_sectors;sec++){
        ata_poll(bus);
        for(uint32_t i = 0; i < (ATA_SECTOR_SIZE / 2);i++){
            uint32_t word = buf[sec * ATA_SECTOR_SIZE + i * 2 + 1];
            word <<= 8;
            word |= buf[sec * ATA_SECTOR_SIZE + i * 2];
            outw(ATA_DATA_PORT(bus),word);
            asm volatile ("outb %%al, $0x80" : : "a"(0)); // tiny delay

        }
    }

    outb(ATA_COMMAND_PORT(bus),ATA_COMMAND_CACHE_FLUSH);
    await_bsy_clear(bus);

    uint8_t errors = inb(ATA_STATUS_PORT(bus));
    if (errors & 0x1) warn("An error has occured during write or flush of a disk sector");
    set_interrupt_status(old_int_status);
}
