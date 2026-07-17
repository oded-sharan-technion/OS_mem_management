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
    if (current == nullptr) {
        void* ptr = sbrk(real_size);
        if (ptr == (void*)(-1)) {
            return NULL;
        }
        current = (MallocMetadata*)(ptr + 1);
        data.size = size;
        data.next = nullptr;
        data.prev = nullptr;
        data.is_free = false;
        *current = data;

        return current + sizeof(MallocMetadata);
    } else {
        if (current->size >= size && current->is_free) {
            current->is_free = false;
            return current + sizeof(MallocMetadata);
        }
        do {
            current = current->next;
            if (current->size >= size && current->is_free) {
                current->is_free = false;
                return current + sizeof(MallocMetadata);
            }
        } while(current->next != nullptr);
        void* ptr = sbrk(real_size);
        if (ptr == (void*)(-1)) {
            return NULL;
        }
        MallocMetadata* metadata = (MallocMetadata*)(ptr + 1);
        data.size = size;
        data.next = nullptr;
        data.prev = current;
        data.is_free = false;
        *metadata = data;
        current->next = metadata;
        // if we want to sort by size as well
        //
        // MallocMetadata* sorter = Mem_list;
        // while(sorter->next != nullptr) {
        //     if (sorter->size > size) {    
        //         data.next = sorter;
        //         if (sorter->prev != nullptr) {
        //             data.prev = sorter->prev;
        //         } else {
        //             data.prev = nullptr;
        //         }
        //         *metadata = data;
        //         sorter->prev = metadata;
        //         break;
        //     }
        // }
        // if (sorter->size <= size) {
        //     data.next = nullptr;
        //     data.prev = sorter;
        //     *metadata = data;
        //     sorter->next = metadata;
        // } else {
        //     data.next = sorter;
        //     if (sorter->prev != nullptr) {
        //         data.prev = sorter->prev;
        //     } else {
        //         data.prev = nullptr;
        //     }
        //     *metadata = data;
        //     sorter->prev = metadata;
        // }
        return metadata + sizeof(MallocMetadata);
    }
}

void* scalloc(size_t size) {
    void* ptr = smalloc(size);
    if (ptr == NULL) return NULL;
    memset(ptr, 0, size);
    return ptr;
}

void sfree (void* p) {
    if (p == nullptr) return;
    p -= sizeof(MallocMetadata);
    MallocMetadata* metadata = (MallocMetadata*)p;
    if (metadata->is_free) return;
    metadata->is_free = true;
}

void* srealloc(void* oldp, size_t size) {
    if (oldp == nullptr) {
        return smalloc(size);
    }
    MallocMetadata* metadata = (MallocMetadata*)(oldp - sizeof(MallocMetadata));
    if (metadata->size <= size) return oldp;
    void* ptr = smalloc(size);
    memmove(ptr, oldp, metadata->size);
    sfree(oldp);
}

// metadata funcs

size_t _num_free_blocks() {
    MallocMetadata* current = Mem_list;
    if (current = nullptr) return 0;
    size_t num = 1;
    do {
        current = current->next;
        if (current->is_free) {
            num++;
        }
    } while (current->next != nullptr);
    return num;
}

size_t _num_free_bytes() {
    MallocMetadata* current = Mem_list;
    if (current = nullptr) return 0;
    size_t num = current->size;
    do {
        current = current->next;
        if (current->is_free) {
            num += current->size;
        }
    } while (current->next != nullptr);
    return num;
}

size_t _num_allocated_blocks() {
    MallocMetadata* current = Mem_list;
    if (current = nullptr) return 0;
    size_t num = 1;
    do {
        current = current->next;
        num++;
    } while (current->next != nullptr);
    return num;
}

size_t _num_allocated_bytes() {
    MallocMetadata* current = Mem_list;
    if (current = nullptr) return 0;
    size_t num = current->size;
    do {
        current = current->next;
        num += current->size;
    } while (current->next != nullptr);
    return num;
}

size_t _num_meta_data_bytes() {
    MallocMetadata* current = Mem_list;
    if (current = nullptr) return 0;
    size_t sum = sizeof(MallocMetadata);
    do {
        current = current->next;
        sum+= sizeof(MallocMetadata);
    } while (current->next != nullptr);
    return sum;
}

size_t _size_meta_data() {
    return sizeof(MallocMetadata);
}
