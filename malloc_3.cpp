#include <stdlib.h>
#include <unistd.h>
#include <cstring>
#include <sys/mman.h>

using namespace std;

#define MAX_ORDER 10

struct MallocMetadata {
    size_t size;
    char order;
    MallocMetadata* next;
    MallocMetadata* prev;
    bool is_free;
};

static MallocMetadata* Mem_array[MAX_ORDER + 1];

bool mem_exists = false;

void* orig_addr;
int num_free;
int bytes_free;
int num_taken;
int bytes_taken;

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
    num_free = 32;
    bytes_free = 0x400000 - 32 * sizeof(MallocMetadata);
    num_taken = 0;
    bytes_taken = 0;
    mem_exists = true;
}

void* splitblock (MallocMetadata* meta_ptr, char order) {
    Mem_array[(int)meta_ptr->order] = meta_ptr->next;
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
        num_free++;
        bytes_free -= sizeof(MallocMetadata);
        newaddr->order = meta_ptr->order;
        newaddr->next = Mem_array[(int)meta_ptr->order];
        newaddr->prev = nullptr;
        if (newaddr->next) newaddr->next->prev = newaddr;
        Mem_array[(int)meta_ptr->order] = newaddr;
    }
    num_free--;
    num_taken++;
    bytes_free -= (meta_ptr->size - sizeof(MallocMetadata));
    bytes_taken += (meta_ptr->size - sizeof(MallocMetadata));
    return (void*)meta_ptr;
}

void* joinblock(MallocMetadata* meta_ptr, char max_ord = MAX_ORDER) {
    while(meta_ptr->order < max_ord) {int offset = (char*)meta_ptr - (char*)orig_addr;
        MallocMetadata* complement = (MallocMetadata*)(((char*)orig_addr) + (meta_ptr->size ^ offset));
        
        if (complement->is_free && complement->order == meta_ptr->order) {
            if (complement->next) complement->next->prev = complement->prev;
            if (complement->prev) complement->prev->next = complement->next;
            if (Mem_array[(int)meta_ptr->order] == complement) Mem_array[(int)meta_ptr->order] = complement->next;
            complement->next = nullptr;
            complement->prev = nullptr;
            // merge
            if (complement >= meta_ptr) meta_ptr = meta_ptr;
            else meta_ptr = complement;
            meta_ptr->size *= 2;
            meta_ptr->order++;
            num_free--;
            bytes_free += sizeof(MallocMetadata);
        } else {
            return (void*)meta_ptr;
        }
    }
    return (void*)meta_ptr;
}

void* smalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    if (size > 100000000) {
        return NULL;
    }
    if (!mem_exists) {
        mem_init();
    }

    if (!mem_exists) {
        return NULL;
    }

    char order = -1;
    for (int i = 128, counter = 0; i <= 0x20000; i*=2, counter++) {
        if ((unsigned int)i >= size + sizeof(MallocMetadata)) {
            order = (char)counter;
            break;
        }
    }

    if (order >= 0 && order <= MAX_ORDER) { // normal size file
        for (int i = order; i <= MAX_ORDER; i++) {
            if (Mem_array[i]) {
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

        bytes_taken += (resptr->size - sizeof(MallocMetadata));
        num_taken++;

        return (void*)(((char*)resptr) + sizeof(MallocMetadata));
    }
}

void* scalloc(size_t num, size_t size) {
    void* ptr = smalloc(size * num);
    if (!ptr) return NULL;
    memset(ptr, 0, size * num);
    return ptr;
}

void sfree (void* p) {
    if (!p) return;
    MallocMetadata* act_p = (MallocMetadata*)(((char*)p) - sizeof(MallocMetadata));

    if (act_p->is_free) return;

    if (act_p->order == (char)-1) {
        bytes_taken -= (act_p->size - sizeof(MallocMetadata));
        munmap(act_p, act_p->size);
        num_taken--;

        return;
    } else {
        bytes_taken -= (act_p->size - sizeof(MallocMetadata));
        bytes_free += (act_p->size - sizeof(MallocMetadata));
        act_p = (MallocMetadata*)joinblock(act_p);
        act_p->next = Mem_array[(int)act_p->order];
        act_p->prev = nullptr;
        if (Mem_array[(int)act_p->order]) {
            Mem_array[(int)act_p->order]->prev = act_p;
        }
        Mem_array[(int)act_p->order] = act_p;
        act_p->is_free = true;
        num_free++;
        num_taken--;
        return;
    }
}

void* srealloc(void* oldp, size_t size) {
    if (!oldp) return smalloc(size);

    MallocMetadata* metadata = (MallocMetadata*)(((char*)oldp) - sizeof(MallocMetadata));
    int blk_size = metadata->size;
    if (blk_size - sizeof(MallocMetadata) >= size) return oldp;
    
    // if not enough room, try merging
    char order = (char)-1;
    for (int i = 128, counter = 0; i <= 0x20000; i*=2, counter++) {
        if ((unsigned int)i >= size + sizeof(MallocMetadata)) {
            order = (char)counter;
            break;
        }
    }

    if (metadata->order == (char)-1 || order == (char)-1) {
        void* ptr = smalloc(size);
        if (!ptr) return NULL;

        memmove(ptr, oldp, blk_size - sizeof(MallocMetadata));
        sfree(oldp);
        return ptr;
    }

    //search for a merge path
    char ord = metadata->order;
    char* addr = (char*)metadata;
    int t_size = metadata->size;
    while (ord < order) {
        int offset = addr - (char*)orig_addr;
        MallocMetadata* complement =(MallocMetadata*)(((char*)orig_addr) + (t_size ^ offset));
        if (complement->is_free && complement->order == ord) {
            if ((char*)complement >= addr) addr = addr;
            else addr = (char*)complement;
            t_size *= 2;
            ord++;
        } else {
            break;
        }
    }
    if (ord == order) { // found!
        bytes_free += (blk_size - sizeof(MallocMetadata));
        num_free++;
        MallocMetadata* test_ptr = (MallocMetadata*)joinblock(metadata, order);
        memmove(((char*)test_ptr) + sizeof(MallocMetadata), oldp, blk_size - sizeof(MallocMetadata));
        test_ptr->is_free = false;
        num_free--;
        bytes_free -= (test_ptr->size - sizeof(MallocMetadata));
        bytes_taken += (test_ptr->size - blk_size);
        return (void*)(((char*)test_ptr) + sizeof(MallocMetadata));
    }
    
    // if all else fails, realloc
    void* ptr = smalloc(size);
    if (!ptr) return NULL;

    memmove(ptr, oldp, blk_size - sizeof(MallocMetadata));
    sfree(oldp);
    return ptr;
}

size_t _num_free_blocks() {
    return num_free;
}

size_t _num_free_bytes() {
    return bytes_free;
}

size_t _num_allocated_blocks() {
    return num_free + num_taken;
}

size_t _num_allocated_bytes() {
    return bytes_free + bytes_taken;
}

size_t _num_meta_data_bytes() {
    return (num_free + num_taken) * sizeof(MallocMetadata);
}

size_t _size_meta_data() {
    return sizeof(MallocMetadata);
}