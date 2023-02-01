
#include <cstdint>
#include <map>
#include <set>

/*
 * this will eventually become the MMU
 * for now, we will just assume everything has access to everything
 * allocate new pages when a new one is needed for read/write
 * never throw any errors or fault conditions
 */

#define PAGE_SIZE 4096
#define PAGE_BIT_WIDTH 12

typedef uint32_t pageAlignedAddress;
#define PAGE_ALIGN(address) static_cast<pageAlignedAddress>(address & 0xfffff000);

struct Page
{
    uint8_t data[PAGE_SIZE];
};

class SystemMemory
{
private:
    std::map<pageAlignedAddress, Page *> pages;
    std::set<uint32_t> activePages;

    public:
        uint8_t ReadByte(uint32_t address);

        void WriteByte(uint32_t address, uint8_t value);

        const std::set<uint32_t> &ActivePages();
};