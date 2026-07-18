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
        addr->is_free = true;
        addr->size = 0x20000;
    }
    Mem_array[MAX_ORDER] = (MallocMetadata*) startptr;
    orig_addr = startptr;
    mem_exists = true;
}

void* splitblock (MallocMetadata* meta_ptr, unsigned char order) {
    Mem_array[meta_ptr->order] = meta_ptr->next;
    if (meta_ptr->next) {
        meta_ptr->next->prev = nullptr;
    }
    meta_ptr->next = nullptr;
    meta_ptr->prev = nullptr;
    meta_ptr->is_free = false;
    while (meta_ptr->order > order) {
        MallocMetadata* newaddr = (MallocMetadata*)(((char*)meta_ptr) + meta_ptr->size/2);
        meta_ptr->size /= 2;
        newaddr->size = meta_ptr->size;
        newaddr->is_free = true;
        meta_ptr->order--;
        newaddr->order = meta_ptr->order;
        newaddr->next = Mem_array[meta_ptr->order];
        newaddr->prev = nullptr;
        if (newaddr->next) newaddr->next->prev = newaddr;
        Mem_array[meta_ptr->order] = newaddr;
    }
    return (void*)meta_ptr;
}

void* smalloc(size_t size) {
    if (!mem_exists) {
        mem_init();
    }

    if (!mem_exists) {
        return NULL;
    }

    unsigned char order = -1;
    for (int i = 128, counter = 0; i <= 0x20000; i*=2, counter++) {
        if (i >= size + sizeof(MallocMetadata)) {
            order = (unsigned char)counter;
            break;
        }
    }

    if (order >= 0 && order <= MAX_ORDER) { // normal size file
        for (int i = order; i <= MAX_ORDER; i++) {
            if (Mem_array[i] != nullptr) {
                MallocMetadata* resptr = (MallocMetadata*)splitblock(Mem_array[i], order);
                if ((void*)resptr == (void*)(-1)) {
                    return NULL;
                }
                return (void*)(((char*)resptr) + sizeof(MallocMetadata));
            }
        }
        return NULL;
    } else { // Large file!!!
        MallocMetadata* resptr = (MallocMetadata*)mmap(NULL, size + sizeof(MallocMetadata), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (resptr == MAP_FAILED) return NULL;
        resptr->next = nullptr;
        resptr->prev = nullptr;
        resptr->is_free = false;
        resptr->size = size + sizeof(MallocMetadata);
        resptr->order = order;

        return (void*)(((char*)resptr) + sizeof(MallocMetadata));
    }
}
