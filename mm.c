/*
 * Simple, 32-bit and 64-bit clean allocator based on implicit free
 * lists, first fit placement, and boundary tag coalescing, as described
 * in the CS:APP2e text. Blocks must be aligned to doubleword (8 byte)
 * boundaries. Minimum block size is 16 bytes.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mm.h"
#include "memlib.h"
#include "list.h"

/*
 * If NEXT_FIT defined use next fit search, else use first fit search
 */
#define NEXT_FITx

/* $begin mallocmacros */
/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */ //line:vm:mm:beginconst
#define DSIZE       8       /* Doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */  //line:vm:mm:endconst

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc)) //line:vm:mm:pack

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))            //line:vm:mm:get
#define PUT(p, val)  (*(unsigned int *)(p) = (val))    //line:vm:mm:put

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)                   //line:vm:mm:getsize
#define GET_ALLOC(p) (GET(p) & 0x1)                    //line:vm:mm:getalloc

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)                      //line:vm:mm:hdrp
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) //line:vm:mm:ftrp

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) //line:vm:mm:nextblkp
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) //line:vm:mm:prevblkp
/* $end mallocmacros */

/* Global variables */
static char *heap_listp = 0;    /* Pointer to first block */
#ifdef NEXT_FIT
static char *rover;           /* Next fit rover */
#endif
typedef struct block block;

struct block
{
  short alloc;
  short size;
  //long padding;
  struct list_elem element;
};
/* Function prototypes for internal helper routines */
static void coalesce(block *b);
struct list * getListAt(int asize);
static int getIndex(int asize);
static void *freeBlockStart(block *b);
static void *blockHeader(block *b);
static block* findBuddy(block* bp);

team_t team = {"Allocators", "Broulaye Doumbia", "broulaye", "Peter Maurer", "pdman13"};



struct list freeBlock8;
struct list freeBlock16;
struct list freeBlock32;
struct list freeBlock64;
struct list freeBlock128;
struct list freeBlock256;
struct list freeBlock512;
struct list freeBlock1024;
static block *initialBlock;
//struct list freeBlock2048;
/*
 * mm_init - Initialize the memory manager
 */
/* $begin mminit */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(CHUNKSIZE/WSIZE)) == (void *)-1) //line:vm:mm:begininit
	return -1;
	list_init(&freeBlock8);
	list_init(&freeBlock16);
	list_init(&freeBlock32);
	list_init(&freeBlock64);
	list_init(&freeBlock128);
	list_init(&freeBlock256);
	list_init(&freeBlock512);
	list_init(&freeBlock1024);
	initialBlock = (block *) heap_listp;
	initialBlock->alloc = 0;
	initialBlock->size = (CHUNKSIZE/WSIZE);
	list_push_back(&freeBlock1024, &initialBlock->element);

    return 0;
}
/* $end mminit */

/*
 * mm_malloc - Allocate a block with at least size bytes of payload
 */
/* $begin mmmalloc */
void *mm_malloc(size_t size)
{
    size_t asize = size + sizeof(block);      /* Adjusted block size */

/* $end mmmalloc */
    if (heap_listp == 0){
        mm_init();
    }
/* $begin mmmalloc */
    /* Ignore spurious requests */
    if (size == 0)
	return NULL;

    /* Adjust block size to include overhead and alignment reqs.
    if (size <= DSIZE)
	asize = 2*DSIZE;
    else
	asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE); */

	if(asize > ((CHUNKSIZE/WSIZE))) {
        return NULL;
	}
    int index = getIndex(asize);
    struct list *tempList;
    //list_init(&tempList);
    tempList = getListAt(index);

    block *allocBlock;

    if(!list_empty(tempList)) {
        allocBlock = (block *)list_pop_back(tempList);
        allocBlock->alloc = 1;

        return freeBlockStart(allocBlock);
    }

    while(list_empty(tempList) && index < 8) {
        index++;
        tempList = getListAt(index);
    }

    if(index == 8) {
        block *newBlock;
        if((newBlock = mem_sbrk(CHUNKSIZE/WSIZE)) == (void *)-1) {
            return (void *)-1;
        }

        newBlock->alloc = 0;
        newBlock->size = 1024;
        list_push_back(&freeBlock1024, &newBlock->element);
        index--;

    }

    block *currentBlock;
    block *newBlock;

    int goal = getIndex(asize);
    while(index != goal) {
        tempList = getListAt(index);
        currentBlock = (block *)list_pop_front(tempList);
        currentBlock->size = currentBlock->size/2;


        newBlock = (block *) ((char *) currentBlock + currentBlock->size);
        newBlock->size = currentBlock->size;

        index--;

        tempList = getListAt(index);

        list_push_back(tempList, &currentBlock->element);
        list_push_back(tempList, &newBlock->element);

    }

    block *result = (block *)list_pop_front(tempList);

    return freeBlockStart(result);
}


struct list * getListAt(int index) {
  switch(index) {
    case 0:
        return &freeBlock8;
    case 1:
        return &freeBlock16;
    case 2:
        return &freeBlock32;
    case 3:
        return &freeBlock64;
    case 4:
        return &freeBlock128;
    case 5:
        return &freeBlock256;
    case 6:
        return &freeBlock512;
    default:
        return &freeBlock1024;
  }
}

int getIndex(int asize) {
    int index = 0;
    int size = 8;
    while (size < asize) {
        size *= 2;
        index++;
    }
    return index;
}

void *freeBlockStart(block *b) {
    char *Bptr = (char *)b;
    return Bptr + sizeof(block);
}

void *blockHeader(block *b) {
    char *Bptr = (char *)b;
    return Bptr - sizeof(block);
}


/* $end mmmalloc */

/*
 * mm_free - Free a block
 */
/* $begin mmfree */
void mm_free(void *bp)
{
/* $end mmfree */
    if(bp == 0)
	return;



/* $begin mmfree */
    //size_t size = GET_SIZE(HDRP(bp));
/* $end mmfree */
    if (heap_listp == 0){
        mm_init();
    }


    block *b = blockHeader(bp);
    b->alloc = 0;

    coalesce(b);

}


block* findBuddy(block* bp){
  int buddy = (int) bp ^ bp->size;
  block *b = (block *) buddy;
  if (bp->size == b->size) return b;
  else return NULL;
}

/* $end mmfree */
/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 */
/* $begin mmfree */
static void coalesce(block *b)
{
    block *buddy = findBuddy(b);

    while(buddy && (buddy->alloc == 0)){
        list_remove(&buddy->element);

        if(buddy < b) {
            b = buddy;
        }

        b->size = b->size * 2;
        buddy = findBuddy(b);
    }

    int index = getIndex(b->size);
    struct list * tempList;
    //list_init(&tempList);
    tempList = getListAt(index);

    list_push_back(tempList, &b->element);
}
/* $end mmfree */

/*
 * mm_realloc - Naive implementation of realloc
 */
void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize;
    void *newptr;

    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0) {
	mm_free(ptr);
	return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if(ptr == NULL) {
	return mm_malloc(size);
    }

    newptr = mm_malloc(size);

    /* If realloc() fails the original block is left untouched  */
    if(!newptr) {
	return 0;
    }

    /* Copy the old data. */
    block *b = blockHeader(ptr);
    oldsize = b->size;
    if(size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize- sizeof(block));

    /* Free the old block. */
    mm_free(ptr);

    return newptr;
}


