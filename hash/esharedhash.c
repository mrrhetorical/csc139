/* `````````````````````````````````````````````````````````````````````
 * Multi-Process Huffman Tree Construction with Shared Memory Allocator
 *
 * This program demonstrates correct concurrent programming with processes
 * sharing a custom memory allocator. The key challenge: after fork(),
 * child processes inherit pointers but must coordinate access to shared
 * data structures.
 *
 * Architecture:
 *   - Parent reads file in 1KB chunks
 *   - Each chunk is processed by a separate child process
 *   - All processes share a single memory allocator (protected by semaphore)
 *   - Results are collected in order via pipes
 *
 * Performance Characteristics:
 *   This implementation is CORRECT but INEFFICIENT. The global semaphore
 *   protecting the allocator creates severe lock contention - with N
 *   parallel children, most time is spent waiting for the lock rather
 *   than doing useful work. This is intentional: students will optimize
 *   this in later assignments by using per-thread pools or lock-free
 *   data structures.
 *
 * Key Design Decisions Explained:
 *
 * 1. Why free_list_ptr is a pointer-to-pointer in shared memory:
 *    After fork(), each process gets a copy of global variables. If
 *    free_list were a regular pointer, each child would have a stale
 *    copy of its value (pointing to wherever the head was when that
 *    child forked). By storing the pointer itself in mmap'd memory,
 *    all processes see the same memory location and can observe each
 *    other's updates to where the list head points.
 *
 * 2. Why semaphore initialization is conditional:
 *    Single-process mode doesn't need synchronization overhead. The
 *    use_multiprocess flag avoids initializing (and acquiring) the
 *    semaphore when running sequentially, eliminating unnecessary cost.
 *
 * 3. Why buffers are allocated via umalloc():
 *    Passing stack buffer pointers to children is unsafe - the parent's
 *    stack could be modified while children are reading. By allocating
 *    in shared memory, each child gets a stable copy. This also exercises
 *    the allocator, making lock contention observable.
 *
 * 4. Why three-phase execution:
 *    To achieve true parallelism, we separate forking (Phase 1) from
 *    result collection (Phase 2) from cleanup (Phase 3). This allows
 *    all children to execute simultaneously, contending for the allocator,
 *    which exposes the performance bottleneck students will optimize.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <pthread.h>

#define BLOCK_SIZE 1024
#define SYMBOLS 256
#define LARGE_PRIME 2147483647
#define UMEM_SIZE (2 * 1024 * 1024)   // 2 MB: large enough for ~1000 concurrent blocks

void *umalloc(size_t size);
void ufree(void *ptr);
unsigned long process_block(const unsigned char *buf, size_t len);
int run_single(const char *filename);
int run_threads(const char *filename);

/* =======================================================================
   PROVIDED CODE — DO NOT MODIFY
   ======================================================================= */

#define MAGIC 0xDEADBEEFLL

typedef struct {
    long size;
    long magic;
} header_t;

typedef struct __node_t {
    long size;
    struct __node_t *next;
} node_t;


// Declaration of free list
static node_t* free_list = NULL;

#define ALIGNMENT 16
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

// Declaration of locks, thread variables, and constants for num threads and pool size
pthread_mutex_t mLock = PTHREAD_MUTEX_INITIALIZER;
int use_multiprocess = 0;

__thread char* pool_start = NULL;
__thread char* pool_current = NULL;

#define NUM_THREADS 1024
#define POOL_SIZE 1024

// thread heap where mostly unmanaged sections of memory live for each thread to do as they wish using pooling
char* thread_heap;

#ifdef DEBUG
// These are used for me to debug how many lock aquisitions are actually getting bypassed
int bypassAccesses = 0;
int freeLockAccess = 0;
int mallLockAccess = 0;
#endif

// modified a lot
void *init_umem(void) {
    void *base = malloc(UMEM_SIZE);
    if (!base) {
        perror("malloc");
        exit(1);
    }

    // I need to reserve 1mb for thread pools and I'm not sure how else I can do this other than pre-allocating before
    // I make the free list. I think some threads will go over their fill and I need some left over to make sure it's
    size_t total_reserved = NUM_THREADS * POOL_SIZE;
    size_t remaining = UMEM_SIZE - total_reserved;
    free_list = (node_t *)((char*)base + total_reserved);
    free_list->size = remaining - sizeof(node_t);
    free_list->next = NULL;

    thread_heap = base;

    return base;
}

// unmodified
static void coalesce(void) {
    node_t *curr = free_list;
    while (curr && curr->next) {
        char *end = (char *)curr + sizeof(node_t) + ALIGN(curr->size);
        if (end == (char *)curr->next) {
            curr->size += sizeof(node_t) + ALIGN(curr->next->size);
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}


// unmodified
void *_umalloc(size_t size) {
    if (size == 0) return NULL;

    size = ALIGN(size);
    node_t *prev = NULL;
    node_t *curr = free_list;

    while (curr) {
        if (curr->size >= (long)size) {
            char *alloc_start = (char *)curr;
            long remaining = curr->size - (long)size;
            node_t *next_free = curr->next;

            header_t *hdr = (header_t *)alloc_start;
            hdr->size = size;
            hdr->magic = MAGIC;
            void *user_ptr = alloc_start + sizeof(header_t);

            if (remaining > (long)sizeof(node_t)) {
                node_t *new_free = (node_t *)(alloc_start + sizeof(header_t) + size);
                new_free->size = remaining - sizeof(node_t);
                new_free->next = next_free;
                if (prev)
                    prev->next = new_free;
                else
                    free_list = new_free;
            } else {
                if (prev)
                    prev->next = next_free;
                else
                    free_list = next_free;
            }

            return user_ptr;
        }
        prev = curr;
        curr = curr->next;
    }

    return NULL;
}

// unmodified
void _ufree(void *ptr) {
    if (!ptr) return;

    header_t *hdr = (header_t *)((char *)ptr - sizeof(header_t));
    if (hdr->magic != MAGIC) {
        fprintf(stderr, "Error: invalid free detected.\n");
        abort();
    }

    node_t *node = (node_t *)hdr;
    node->size = ALIGN(hdr->size);
    node->next = NULL;

    if (!free_list || node < free_list) {
        node->next = free_list;
        free_list = node;
    } else {
        node_t *curr = free_list;
        while (curr->next && curr->next < node)
            curr = curr->next;
        node->next = curr->next;
        curr->next = node;
    }

    coalesce();
}


// Modified version of the provided umalloc_fast method provided in instructions
void* umalloc_fast(size_t size) {
    size = ALIGN(size);
    if (pool_current != NULL) {
        if (pool_current + size <= pool_start + POOL_SIZE) {
            void* ptr = pool_current;
            pool_current += size;
			#ifdef DEBUG
			bypassAccesses++;
			#endif
			return ptr;
        }
    }

    pthread_mutex_lock(&mLock);
	#ifdef DEBUG
	mallLockAccess++;
    #endif
	void* ptr = _umalloc(size);
    pthread_mutex_unlock(&mLock);
    return ptr;
}

// Modified to be an alias for umalloc_fast
void *umalloc(size_t size) {
    return umalloc_fast(size);
}

// Modified ufree because in order to support larger files I have to still have some free space
// for larger trees to be able to use space. This makes sure though that if any ufree is called
// with a pointer to some place in the thread heap space then it just returns
void ufree(void *ptr) {
    if (ptr == NULL)
        return;

    // No-op on thread heaps
    if (thread_heap != NULL) {
        char *c = (char *)ptr;
        if (c >= thread_heap && c < thread_heap + NUM_THREADS * POOL_SIZE) {
			#ifdef DEBUG
			bypassAccesses++;
			#endif
            return;
        }
    }



    if (use_multiprocess) {
        pthread_mutex_lock(&mLock);
	}

	#ifdef DEBUG
	freeLockAccess++;
	#endif
	
	_ufree(ptr);

	if (use_multiprocess) {
        pthread_mutex_unlock(&mLock);
	}
}

/* =======================================================================
   Huffman Tree Construction (Given)
   ======================================================================= */

typedef struct Node {
    unsigned char symbol;
    unsigned long freq;
    struct Node *left, *right;
} Node;

typedef struct {
    Node **data;
    int size;
    int capacity;
} MinHeap;

MinHeap *heap_create(int capacity) {
    MinHeap *h = umalloc(sizeof(MinHeap));
    h->data = umalloc(sizeof(Node *) * capacity);
    h->size = 0;
    h->capacity = capacity;
    return h;
}

void heap_swap(Node **a, Node **b) {
    Node *tmp = *a; *a = *b; *b = tmp;
}

void heap_push(MinHeap *h, Node *node) {
    int i = h->size++;
    h->data[i] = node;
    while (i > 0) {
        int p = (i - 1) / 2;
        if (h->data[p]->freq < h->data[i]->freq) break;
        heap_swap(&h->data[p], &h->data[i]);
        i = p;
    }
}

Node *heap_pop(MinHeap *h) {
    if (h->size == 0) return NULL;
    Node *min = h->data[0];
    h->data[0] = h->data[--h->size];
    int i = 0;
    while (1) {
        int l = 2 * i + 1, r = l + 1, smallest = i;
        if (l < h->size && h->data[l]->freq < h->data[smallest]->freq) smallest = l;
        if (r < h->size && h->data[r]->freq < h->data[smallest]->freq) smallest = r;
        if (smallest == i) break;
        heap_swap(&h->data[i], &h->data[smallest]);
        i = smallest;
    }
    return min;
}

void heap_free(MinHeap *h) {
    ufree(h->data);
    ufree(h);
}

Node *new_node(unsigned char sym, unsigned long freq, Node *l, Node *r) {
    Node *n = umalloc(sizeof(Node));
    n->symbol = sym;
    n->freq = freq;
    n->left = l;
    n->right = r;
    return n;
}

void free_tree(Node *n) {
    if (!n) return;
    free_tree(n->left);
    free_tree(n->right);
    ufree(n);
}

Node *build_tree(unsigned long freq[SYMBOLS]) {
    MinHeap *h = heap_create(SYMBOLS);
    for (int i = 0; i < SYMBOLS; i++)
        if (freq[i] > 0)
            heap_push(h, new_node((unsigned char)i, freq[i], NULL, NULL));
    if (h->size == 0) {
        heap_free(h);
        return NULL;
    }
    while (h->size > 1) {
        Node *a = heap_pop(h);
        Node *b = heap_pop(h);
        Node *p = new_node(0, a->freq + b->freq, a, b);
        heap_push(h, p);
    }
    Node *root = heap_pop(h);
    heap_free(h);
    return root;
}

unsigned long hash_tree(Node *n, unsigned long hash) {
    if (!n) return hash;
    hash = (hash * 31 + n->freq + n->symbol) % LARGE_PRIME;
    hash = hash_tree(n->left, hash);
    hash = hash_tree(n->right, hash);
    return hash;
}

/* =======================================================================
   Output Functions
   ======================================================================= */

void print_intermediate(int block_num, unsigned long hash, pid_t pid) {
#ifdef DEBUG
#  if DEBUG == 2
    printf("[PID %d] Block %d hash: %lu\n", pid, block_num, hash);
#  elif DEBUG == 1
    printf("Block %d hash: %lu\n", block_num, hash);
#  endif
#else
    (void)block_num;
    (void)hash;
    (void)pid;
#endif
}

void print_final(unsigned long final_hash) {
    printf("Final signature: %lu\n", final_hash);
}

/* `````````````````````````````````````````````````````````````````````
 * Main Entry Point
 *
 * Parses arguments to determine execution mode, initializes the shared
 * memory allocator, then dispatches to either single-process or
 * threaded execution.
 */

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file> [-t]\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    use_multiprocess = (argc >= 3 && strcmp(argv[2], "-m") == 0) || (argc >= 3 && strcmp(argv[2], "-t") == 0);

    init_umem();

    if (use_multiprocess)
        return run_threads(filename);
    else
        return run_single(filename);
}

// unmodified
unsigned long process_block(const unsigned char *buf, size_t len) {
    unsigned long freq[SYMBOLS] = {0};
    for (size_t i = 0; i < len; i++)
        freq[buf[i]]++;

    Node *root = build_tree(freq);
    unsigned long h = hash_tree(root, 0);
    free_tree(root);
    return h;
}

// unmodified
int run_single(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    unsigned char buf[BLOCK_SIZE];
    unsigned long final_hash = 0;
    int block_num = 0;

    while (!feof(fp)) {
        size_t n = fread(buf, 1, BLOCK_SIZE, fp);
        if (n == 0) break;
        unsigned long h = process_block(buf, n);
        print_intermediate(block_num++, h, getpid());
        final_hash = (final_hash + h) % LARGE_PRIME;
    }

    fclose(fp);
    print_final(final_hash);
    return 0;
}

// Multi-threaded stuff

// Thread argument structure
typedef struct {
    int block_id;
    unsigned char *block_buf;
    size_t block_len;
    unsigned long *results;
} thread_arg_t;

// Initializes the thread pool for the thread id by an offset
void thread_pool_init(int tid) {
    size_t offset = tid * POOL_SIZE;
    pool_start = (char*) thread_heap + offset;
    pool_current = pool_start;
}

// Worker thread function initializes the thread pool and process the block
void *worker_thread(void *arg) {
    thread_arg_t *targ = (thread_arg_t *)arg;

    thread_pool_init(targ->block_id);

    unsigned long h = process_block(targ->block_buf, targ->block_len);
    ufree(targ->block_buf);
    targ->results[targ->block_id] = h;
    ufree(targ);
    return NULL;
}

int run_threads(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    unsigned char buf[BLOCK_SIZE];
    unsigned long final_hash = 0;
    unsigned long results[1024];
    pthread_t threads[1024];
    int num_blocks = 0;

    // Spawn the threads in the loop
    while (!feof(fp)) {
        size_t n = fread(buf, 1, BLOCK_SIZE, fp);
        if (n == 0) break;

        if (num_blocks >= 1024) {
            fprintf(stderr, "Error: file too large (max 1024 blocks)\n");
            fclose(fp);
            return 1;
        }

        // Not sure if malloc was banned but my implementation supports using umalloc anyways
        unsigned char *block_buf = umalloc(n);
        if (!block_buf) {
            fprintf(stderr, "umalloc failed for block %d\n", num_blocks);
            fclose(fp);
            return 1;
        }
        memcpy(block_buf, buf, n);

        // Samesies with the malloc -> umalloc in case it's not allowed
        // I could've maybe done this with a stack pointer but this feels cleaner
        thread_arg_t *args = umalloc(sizeof(thread_arg_t));
        args->block_id = num_blocks;
        args->block_buf = block_buf;
        args->block_len = n;
        args->results = results;
        int r = pthread_create(&threads[num_blocks], NULL, worker_thread, args);

        if (r) {
            perror("pthread_create");
            ufree(block_buf);
            ufree(args);
            fclose(fp);
            return 1;
        }

        num_blocks++;
    }

    fclose(fp);

    // Waiting for all the threads to finish with pthread_join and add to total

    for (int i = 0; i < num_blocks; i++) {
        pthread_join(threads[i], NULL);
        unsigned long h = results[i];
        print_intermediate(i, h, i);
        final_hash = (final_hash + h) % LARGE_PRIME;
    }

    print_final(final_hash);
#ifdef DEBUG
	printf("Malloc lock accesses: %d\nFree lock accesses: %d\nTotal bypassed: %d\nPercent bypassed: %0.2f", mallLockAccess, freeLockAccess, bypassAccesses, (float) (bypassAccesses / (float) (bypassAccesses + mallLockAccess + freeLockAccess)));
#endif
	return 0;
}
