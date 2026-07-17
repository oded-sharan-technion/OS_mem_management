#include <stdlib.h>
#include <unistd.h>
#include <cstring>
#include <sys/mman.h>

using namespace std;

#define MAX_ORDER 10

struct MallocMetadata {
    size_t size;
    unsigned char order;
    MallocMetadata* next;
    MallocMetadata* prev;
    bool is_free;
};

static MallocMetadata* Mem_array[MAX_ORDER + 1];

bool mem_exists = false;

void* orig_addr;

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
        addr->size = 0x20000;
    }
    Mem_array[MAX_ORDER] = (MallocMetadata*) startptr;
    orig_addr = startptr;
    mem_exists = true;
}

void* splitblock (MallocMetadata* meta_ptr, unsigned char order) {
    while (meta_ptr->order > order) {
        Mem_array[meta_ptr->order] = meta_ptr->next;
        char* clean_newaddr = ((char*)meta_ptr) + meta_ptr->size/2;
        MallocMetadata* newaddr = (MallocMetadata*) clean_newaddr;
        meta_ptr->size /= 2;
        newaddr->size = meta_ptr->size;
        meta_ptr->order--;
        newaddr->order = meta_ptr->order;
        newaddr->prev = meta_ptr;
        if (meta_ptr->next) meta_ptr->next->prev = meta_ptr->prev;
        if (meta_ptr->prev) meta_ptr->prev->next = meta_ptr->next;
        
    }
}

void* smalloc(size_t size) {
    if (!mem_exists) {
        mem_init();
    }

    if (!mem_exists) {
        return (void*)(-1);
    }

    unsigned char order = -1;
    for (int i = 128, counter = 0; i <= 0x20000; i*=2, counter++) {
        if (i >= size) {
            order = (unsigned char)counter;
            break;
        }
    }

    if (order >= 0 && order <= MAX_ORDER) { // normal size file
        for (int i = order; i <= MAX_ORDER; i++) {
            if (Mem_array[i] != nullptr) {

            }
        }
    } else { // Large file!!!

    }
}
