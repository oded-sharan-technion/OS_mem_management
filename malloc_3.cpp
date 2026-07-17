#include <stdlib.h>
#include <unistd.h>
#include <cstring>
#include <sys/mman.h>

using namespace std;

#define MAX_ORDER 10

struct MallocMetadata {
    unsigned char order;
    MallocMetadata* next;
    MallocMetadata* prev;
    bool is_free;
};

static MallocMetadata* Mem_array[MAX_ORDER + 1];

bool mem_exists = false;

void mem_init() {
    for (auto& ptr : Mem_array) {
        ptr = nullptr;
    }
    void* startptr = mmap(NULL, 0x400000, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (startptr == MAP_FAILED) return;
    size_t offset = 0;
    for (int i = 0; i < 32; i++) {
        offset = 0x20000 * i;
        char* clean_addr = ((char*)startptr) + offset;
        MallocMetadata* addr = (MallocMetadata*)clean_addr;
        addr->order = MAX_ORDER;
        if (i >= 1) {
            addr->prev = (MallocMetadata*)(clean_addr - 0x20000);
            addr->prev->next = addr;
        }
        else addr->prev = nullptr;
        if (i == 31) addr->next = nullptr;
    }
    Mem_array[MAX_ORDER] = (MallocMetadata*) startptr;
    mem_exists = true;
}

void* smalloc(size_t size) {
    if (!mem_exists) {
        mem_init();
    }

}
