#include "mm.h"
#include "memlib.h"
#include "list.h"
#include "tree.h"
#include "queue.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


/* $begin mallocmacros */
/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */ //line:vm:mm:beginconst
#define DSIZE       8       /* Doubleword size (bytes) */
#define BLOCK_SZ_WRDS 8;
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


struct bound { 
	int alocd:1;
	int size:31;
}

/* struct block representing a free block */
struct block {
    struct bound head;
    union {
        struct list_elem elem;
        int index;
        size_t size;
	char data[0];
    };
};


static size_t convert_bytes_to_words(size_t b) {
	b = (b + DSIZE - 1) & ~(DSIZE - 1);
	b += 2 * sizeof(struct bound);
	return MAX(BLOCK_SZ_WRDS, bytes/WSIZE);
}

static struct block * find_free(size_t word_size) {
	//search through the list to find a free block
}

static struct block * format(struct block *block_p, size_t word_count) {
	size_t block_size = block_p->head.size;

	if (block_size > word_count && (block_size - word_count) >= BLOCK_SZ_WRDS) {
		printf("split necessary\n");
		mark_alloc(block_p, block_size); // Still need to implement
		mark_free(/*next block */, block_size - word_count);
	} else {
		// no split, use whoe block
		mark_alloc(block_p, block_size);
	}
	return block_p;
}
/* Global variables */
static char *heap_listp = 0;  /* Pointer to first block */
struct list freeBlocks;
int mm_init () {

    /* Initialized free block list
    list_init(&freeBlocks);*/


    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) //line:vm:mm:begininit
	return -1;
    PUT(heap_listp, 0);                          /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));     /* Epilogue header */
    heap_listp += (2*WSIZE);                     //line:vm:mm:endinit

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;


    /* Create a block
    struct block newBlock;
    newBlock.index = heap_listp;
    newBlock.size = GET_SIZE(HDRP(heap_listp));*/

    /* Populate free block list */
    //list_push_back(&freeBlocks, &newBlock.elem);

    return 0;


}


void *mm_malloc (size_t size) {
	size_t word_size; // size adjusted into words
	struct block *block_p; // pointer to block to return

	if (size == 0) return NULL;

	word_size = convert_bytes_to_words(size);

	block_p = find_free(word_size);
	format(block_p, word_size); // set up block to be returnable, split if necessary

	return block_p->data;
}
void mm_free (void *ptr) {
    /* $end mmfree */
    if(bp == 0)
	return;

/* $begin mmfree */
    size_t size = GET_SIZE(HDRP(bp));
/* $end mmfree */
    if (heap_listp == 0){
	mm_init();
    }
/* $begin mmfree */

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}
void *mm_realloc(void *ptr, size_t size) {

}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE; //line:vm:mm:beginextend
    if ((long)(bp = mem_sbrk(size)) == -1)
	return NULL;                                        //line:vm:mm:endextend

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */   //line:vm:mm:freeblockhdr
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */   //line:vm:mm:freeblockftr
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */ //line:vm:mm:newepihdr

    /* Coalesce if the previous block was free */
    return coalesce(bp);                                          //line:vm:mm:returnblock
}

/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 */
/* $begin mmfree */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {            /* Case 1 */
	return bp;
    }

    else if (prev_alloc && !next_alloc) {      /* Case 2 */
	size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size,0));
    }

    else if (!prev_alloc && next_alloc) {      /* Case 3 */
	size += GET_SIZE(HDRP(PREV_BLKP(bp)));
	PUT(FTRP(bp), PACK(size, 0));
	PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
	bp = PREV_BLKP(bp);
    }

    else {                                     /* Case 4 */
	size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
	    GET_SIZE(FTRP(NEXT_BLKP(bp)));
	PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
	PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
	bp = PREV_BLKP(bp);
    }
/* $end mmfree */
#ifdef NEXT_FIT
    /* Make sure the rover isn't pointing into the free block */
    /* that we just coalesced */
    if ((rover > (char *)bp) && (rover < NEXT_BLKP(bp)))
	rover = bp;
#endif
/* $begin mmfree */
    return bp;
}


