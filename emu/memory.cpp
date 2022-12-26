#include <string.h>
#include <stdio.h>

#include "memory.h"

uint8_t SystemMemory::ReadByte(uint32_t address)
{
    uint32_t pageAddr = PAGE_ALIGN(address);
    if(this->pages.count(pageAddr) == 0)
    {
        Page *p = new Page();
        memset(p->data, 0, PAGE_SIZE);
        this->pages[pageAddr] = p;
        this->activePages.insert(pageAddr >> PAGE_BIT_WIDTH);
    }
    // printf("Read from address %08x\n", address);
    return this->pages[pageAddr]->data[address & 0xfff];
}

void SystemMemory::WriteByte(uint32_t address, uint8_t value)
{
    uint32_t pageAddr = PAGE_ALIGN(address);
    if(this->pages.count(pageAddr) == 0)
    {
        Page *p = new Page();
        memset(p->data, 0, PAGE_SIZE);
        this->pages[pageAddr] = p;
        this->activePages.insert(pageAddr >> PAGE_BIT_WIDTH);
    }
    // printf("Write to address %08x\n", address);
    this->pages[pageAddr]->data[address & 0xfff] = value;
}

const std::set<uint32_t> &SystemMemory::ActivePages()
{
    return this->activePages;
}
