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


#ifdef NEXT_FIT
static char *rover;           /* Next fit rover */
#endif




 struct bound {
 	size_t alocd:1;
 	size_t size:31;
 };

 /* struct block representing a free block */
 typedef struct block {
     struct bound head;
     union {
        struct list_elem element;
        char * data[0];
     };
 }block;

/* Function prototypes for internal helper routines */
static void coalesce(block *b);
//struct list * getListAt(int asize);
static size_t getIndex(size_t asize);
static void *freeBlockStart(block *b);
static block *blockHeader(block *b);
static block* findBuddy(block* bp);
static void findFreeBlock(block *elem, struct list *tempList, bool found, size_t asize);
static void findNextBiggest(block *elem, struct list *tempList, bool found, size_t asize);
static size_t breakDown(size_t goal, block *elem, size_t index);
static size_t getListSize(size_t index);

team_t team = {"Allocators", "Broulaye Doumbia", "broulaye", "Peter Maurer", "pdman13"};



struct list freeBlockList[11];/** 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192-INFINITY */
static block *initialBlock;
//static block *prologueBlock;
//static block *epilogueBlock;
static void *heap_listp = 0;    /* Pointer to first block */
//struct list freeBlock2048;
/*
 * mm_init - Initialize the memory manager
 */
/* $begin mminit */
int mm_init(void)
{

//    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) //line:vm:mm:begininit
//        return -1;
//
//    PUT(heap_listp, 0); //padding
//    prologueBlock = (block *) heap_listp+WSIZE;
//    prologueBlock->head.alocd = 1;
//    prologueBlock->head.size = DSIZE;



    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4096)) == (void *)-1) //line:vm:mm:begininit
	return -1;

	for(int i= 0; i < 11; i++) {
        list_init(&freeBlockList[i]);
	}
	initialBlock = (block *) heap_listp;
	initialBlock->head.alocd = 0;
	initialBlock->head.size = (4096);
	list_push_back(&freeBlockList[9], &initialBlock->element);

    return 0;
}
/* $end mminit */

/*
 * mm_malloc - Allocate a block with at least size bytes of payload
 */
/* $begin mmmalloc */
void *mm_malloc(size_t size)
{
    /**(size+8-1) &~ (8-1)*/

    size_t asize = size + 8 ;      /* Adjusted block size */

    while(asize%8 != 0) {
        asize++;
    }



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

	/**
	if(asize > ((CHUNKSIZE/WSIZE))) {
        return NULL;
	}*/

    size_t index = getIndex(asize);
    struct list *tempList;
    //list_init(&tempList);
    tempList = &freeBlockList[index];

    block *tempBlock;

    if(!list_empty(tempList) && index < 10) {
        tempBlock = list_entry(list_pop_back(tempList), block, element);
        tempBlock->head.alocd = 1;

        return freeBlockStart(tempBlock);
    }

    while(list_empty(tempList) && index < 10) {
        index++;
        tempList = &freeBlockList[index];
    }

    if(index == 10) {/**search the last range block*/
        if(!list_empty(tempList)) {
            bool found = false;
            tempBlock = list_entry(list_begin(tempList), block, element);
            findFreeBlock(tempBlock, tempList, found, asize);
            if(found) {/**found so pop it and use it*/
                list_remove(&tempBlock->element);
                //void *header = getHeader(listElem);
                //PUT(header, PACK(asize, 1));
                tempBlock->head.alocd = 1;
                return freeBlockStart(tempBlock);
            }
            else {/**else try to find the next biggest*/
                tempBlock = list_entry(list_begin(tempList), block, element);
                findNextBiggest(tempBlock, tempList, found, asize);
                if(found) {/**found so pop it break it down and use it*/
                    size_t Rindex = breakDown(asize, tempBlock, index);
                    tempBlock = list_entry(list_pop_back(&freeBlockList[Rindex]), block, element);
                    //void *header = getHeader(listElem);
                    //PUT(header, PACK(asize, 1));
                    tempBlock->head.alocd = 1;
                    return freeBlockStart(tempBlock);
                }
                else {/**Not found we will have to request new memory*/
                    index++;
                }

            }

        }
        else {/**last range block is empty*/
            index++;
        }
    }
    if(index == 11) {
        block *newBlock;
        //size_t extendSize = 0;
        if(asize <= 4096){

            size_t index = getIndex(asize);
            size_t sizeToAllocate = getListSize(index) * 2;
            if(sizeToAllocate < 1024) {
                sizeToAllocate = 1024;
            }
            if((newBlock = mem_sbrk(sizeToAllocate)) == NULL) {/**request new memory if you can*/
            /**you can't request more memory, try to coalesce*/
                return NULL;

            }
            else {
                //PUT(newBlock, PACK(asize, 1)); /**set header for new block*/
                //size_t heapSize = mem_heapsize();
                newBlock->head.alocd = 0;
                //newBlock->head.size = heapSize;
                newBlock->head.size = sizeToAllocate;
                index = getIndex(sizeToAllocate);
                list_push_back(&freeBlockList[index], &newBlock->element);
                size_t Rindex = breakDown(asize, newBlock, index);
                tempBlock = list_entry(list_pop_back(&freeBlockList[Rindex]), block, element);
                tempBlock->head.alocd = 1;
                return freeBlockStart(tempBlock);
            }
        }
        else {
             size_t sizeToAllocate = 8192;
             while(asize > sizeToAllocate) {
                sizeToAllocate *= 2;
             }
             if((newBlock = mem_sbrk(sizeToAllocate)) == (void *)-1) {/**request new memory if you can*/
            /**you can't request more memory, try to coalesce*/
                return (void *)-1;

            }
            else {
                //PUT(newBlock, PACK(asize, 1)); /**set header for new block*/
                size_t Rindex;
                newBlock->head.alocd = 0;
                newBlock->head.size = sizeToAllocate;
                list_push_back(&freeBlockList[10], &newBlock->element);
                if(asize == sizeToAllocate  || sizeToAllocate/2 < asize ) {
                    Rindex = 10;
                }
                else {
                    Rindex = breakDown(asize, newBlock, 10);
                }
                tempBlock = list_entry(list_pop_back(&freeBlockList[Rindex]), block, element);
                tempBlock->head.alocd = 1;
                return freeBlockStart(tempBlock);
            }
        }
    }

    else {
        tempBlock = list_entry(list_rbegin(tempList), block, element);
        size_t Rindex = breakDown(asize, tempBlock, index);
        tempBlock = list_entry(list_pop_back(&freeBlockList[Rindex]), block, element);
        //void *header = getHeader(listElem);
        //PUT(header, PACK(asize, 1));
        tempBlock->head.alocd = 1;
        return freeBlockStart(tempBlock);
    }


}

void findFreeBlock(block *newBlock, struct list *tempList, bool found, size_t asize) {
    struct list_elem *elem = &newBlock->element;
    while(elem != list_tail(tempList)){
        if(newBlock->head.size != asize) {
            elem = list_next(elem);
            newBlock = list_entry(elem, block, element);
        }
        else {
            found = true;
            break;
        }
    }
}

void findNextBiggest(block *newBlock, struct list *tempList, bool found, size_t asize) {
    struct list_elem *elem = &newBlock->element;
    while( elem != list_tail(tempList)){
        if(newBlock->head.size < asize) {
            elem = list_next(elem);
            newBlock = list_entry(elem, block, element);
        }
        else {
            found = true;
            break;
        }
    }
}


size_t breakDown(size_t goal, block *Block, size_t index) {
    size_t Currentsize = Block->head.size; /**get size of current block*/

    if(Currentsize/2 < goal) {
        list_remove(&Block->element); /**remove the current block from it's list*/
        list_push_back(&freeBlockList[index], &Block->element);
        return index;
    }

    while(Currentsize > 4096) {

        list_remove(&Block->element); /**remove the current block from it's list*/
        Currentsize /= 2;/**half the size of the current free block*/

        Block->head.size = Currentsize;

        block *newBlock = (block *)(((char *)Block) + Currentsize); /**create a new block at size offset from current block*/
        newBlock->head.size = Currentsize;
        newBlock->head.alocd = 0;

        if(Currentsize > 4096) { /**current size still greater than 4096*/
            list_push_back(&freeBlockList[10], &Block->element);
            list_push_back(&freeBlockList[10], &newBlock->element);
            if(Currentsize >= goal && (Currentsize/2) < goal) {
                return 10;
            }
        }
        else { /**Current size not greater than 4096*/
            list_push_back(&freeBlockList[9], &Block->element);
            list_push_back(&freeBlockList[9], &newBlock->element);
            index = 9;
            if(Currentsize == goal || Currentsize/2 < goal) {
                return 9;
            }
        }

    }

    size_t indexGoal = getIndex(goal);
    while(index != indexGoal) {
        Block = list_entry(list_pop_back(&freeBlockList[index]), block, element);
        Currentsize = Block->head.size; /**get size of current block*/

        Currentsize /= 2; /**half the size of the current free block*/

        Block->head.size = Currentsize;

        block *newBlock = (block *)(((char *)Block) + Currentsize); /**create a new block at size offset from current block*/
        newBlock->head.size = Currentsize;
        newBlock->head.alocd = 0;

        index--;

        /**push it into a smaller list*/
        list_push_back(&freeBlockList[index], &Block->element);
        list_push_back(&freeBlockList[index], &newBlock->element);


    }

    return index;

}





size_t getListSize(size_t index) {
  switch(index) {
    case 0:
        return 8;
    case 1:
        return 16;
    case 2:
        return 32;
    case 3:
        return 64;
    case 4:
        return 128;
    case 5:
        return 256;
    case 6:
        return 512;
    case 7:
        return 1024;
    case 8:
        return 2048;
    default:
        return 4096;
  }
}

size_t getIndex(size_t asize) {
    size_t index = 0;
    size_t size = 8;
    while (size < asize && index < 10) {
        size *= 2;
        index++;
    }
    return index;
}

void *freeBlockStart(block *b) {
    //char *Bptr = (char *)b;
    return (void *)((char *)b->data + 4);
}


block *blockHeader(block *b) {
    char *Bptr = (char *)b;
    return (block *)(Bptr - 8);
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
    if(b->head.alocd == 0) {
        printf("Calling free on an already free pointer\n");
        return;
    }
    b->head.alocd = 0;

    coalesce(b);

}


block* findBuddy(block* bp){
  int buddy = (int) bp ^ bp->head.size;
  block *b = (block *) buddy;

  if((char *)b-1 > (char *)mem_heap_hi() || (char *)b-1 < (char *)mem_heap_lo())
    return NULL;

  if (bp->head.size == b->head.size) return b;
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

    while(buddy && (buddy->head.alocd == 0)){
        list_remove(&buddy->element);

        if(buddy < b) {
            b = buddy;
        }

        b->head.size = b->head.size * 2;
        buddy = findBuddy(b);
    }

    int index = getIndex(b->head.size);


    list_push_back(&freeBlockList[index], &b->element);
}
/* $end mmfree */

/*
 * mm_realloc - Naive implementation of realloc
 */
void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize;
    block *newptr;

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
    oldsize = b->head.size;
    if(size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize- sizeof(block));

    /* Free the old block. */
    mm_free(ptr);

    return newptr;
}


