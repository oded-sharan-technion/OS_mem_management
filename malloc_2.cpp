#include <stdlib.h>
#include <unistd.h>
#include <cstring>

using namespace std;

struct MallocMetadata {
    size_t size;
    MallocMetadata* next;
    MallocMetadata* prev;
    bool is_free;
};

static MallocMetadata* Mem_list = nullptr;

void* smalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    if (size > 100000000) {
        return NULL;
    }
    MallocMetadata* current = Mem_list;
    MallocMetadata data;
    int real_size = size + sizeof(MallocMetadata);
    if (!current) {
        void* ptr = sbrk(real_size);
        if (ptr == (void*)(-1)) {
            return NULL;
        }
        current = (MallocMetadata*)(ptr);
        data.size = size + sizeof(MallocMetadata);
        data.next = nullptr;
        data.prev = nullptr;
        data.is_free = false;
        *current = data;

        Mem_list = current;
        return (void*)(((char*)current) + sizeof(MallocMetadata));
    } else {
        if (current->size >= size + sizeof(MallocMetadata) && current->is_free) {
            current->is_free = false;
            return (void*)(((char*)current) + sizeof(MallocMetadata));
        }
        while(current->next) {
            current = current->next;
            if (current->size >= size + sizeof(MallocMetadata) && current->is_free) {
                current->is_free = false;
                return (void*)(((char*)current) + sizeof(MallocMetadata));
            }
        }
        void* ptr = sbrk(real_size);
        if (ptr == (void*)(-1)) {
            return NULL;
        }
        MallocMetadata* metadata = (MallocMetadata*)ptr;
        data.size = size + sizeof(MallocMetadata);
        data.next = nullptr;
        data.prev = current;
        data.is_free = false;
        *metadata = data;
        current->next = metadata;

        return (void*)(((char*)metadata) + sizeof(MallocMetadata));
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
    MallocMetadata* metadata = (MallocMetadata*)(((char*)p) - sizeof(MallocMetadata));
    if (metadata->is_free) return;
    metadata->is_free = true;
}

void* srealloc(void* oldp, size_t size) {
    if (!oldp) return smalloc(size);
    MallocMetadata* metadata = (MallocMetadata*)(((char*)oldp) - sizeof(MallocMetadata));
    if (metadata->size - sizeof(MallocMetadata) >= size) return oldp;
    void* ptr = smalloc(size);
    memmove(ptr, oldp, metadata->size - sizeof(MallocMetadata));
    sfree(oldp);
    return ptr;
}

// metadata funcs

size_t _num_free_blocks() {
    MallocMetadata* current = Mem_list;
    if (!current) return 0;
    size_t num = 0;
    if (current->is_free) {
        num++;
    }
    while (current->next ) {
        current = current->next;
        if (current->is_free) {
            num++;
        }
    }
    return num;
}

size_t _num_free_bytes() {
    MallocMetadata* current = Mem_list;
    if (!current) return 0;
    size_t num = 0;
    if (current->is_free) {
        num += current->size - sizeof(MallocMetadata);
    }
    while (current->next ) {
        current = current->next;
        if (current->is_free) {
            num += current->size - sizeof(MallocMetadata);
        }
    }
    return num;
}

size_t _num_allocated_blocks() {
    MallocMetadata* current = Mem_list;
    if (!current) return 0;
    size_t num = 1;
    while (current->next) {
        current = current->next;
        num++;
    }
    return num;
}

size_t _num_allocated_bytes() {
    MallocMetadata* current = Mem_list;
    if (!current) return 0;
    size_t num = current->size - sizeof(MallocMetadata);
    while (current->next) {
        current = current->next;
        num += current->size - sizeof(MallocMetadata);
    }
    return num;
}

size_t _num_meta_data_bytes() {
    MallocMetadata* current = Mem_list;
    if (!current) return 0;
    size_t sum = sizeof(MallocMetadata);
    while (current->next) {
        current = current->next;
        sum+= sizeof(MallocMetadata);
    }
    return sum;
}

size_t _size_meta_data() {
    return sizeof(MallocMetadata);
}
