/**
 * @file mm.c
 * @brief A 64-bit struct-based implicit free list memory allocator
 *
 * 15-213: Introduction to Computer Systems
 *
 * TODO: insert your documentation here. :)
 *
 *************************************************************************
 *
 * ADVICE FOR STUDENTS.
 * - Step 0: Please read the writeup!
 * - Step 1: Write your heap checker.
 * - Step 2: Write contracts / debugging assert statements.
 * - Good luck, and have fun!
 *
 *************************************************************************
 *
 * @author Abhishek Hemlani ahemlani@andrew.cmu.edu
 */

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memlib.h"
#include "mm.h"

/* Do not change the following! */

#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mem_memset
#define memcpy mem_memcpy

#endif /* def DRIVER */

/* You can change anything from here onward */

/*
 *****************************************************************************
 * If DEBUG is defined (such as when running mdriver-dbg), these macros      *
 * are enabled. You can use them to print debugging output and to check      *
 * contracts only in debug mode.                                             *
 *                                                                           *
 * Only debugging macros with names beginning "dbg_" are allowed.            *
 * You may not define any other macros having arguments.                     *
 *****************************************************************************
 */
#ifdef DEBUG
/* When DEBUG is defined, these form aliases to useful functions */
#define dbg_requires(expr) assert(expr)
#define dbg_assert(expr) assert(expr)
#define dbg_ensures(expr) assert(expr)
#define dbg_printf(...) ((void)printf(__VA_ARGS__))
#define dbg_printheap(...) print_heap(__VA_ARGS__)
#else
/* When DEBUG is not defined, these should emit no code whatsoever,
 * not even from evaluation of argument expressions.  However,
 * argument expressions should still be syntax-checked and should
 * count as uses of any variables involved.  This used to use a
 * straightforward hack involving sizeof(), but that can sometimes
 * provoke warnings about misuse of sizeof().  I _hope_ that this
 * newer, less straightforward hack will be more robust.
 * Hat tip to Stack Overflow poster chqrlie (see
 * https://stackoverflow.com/questions/72647780).
 */
#define dbg_discard_expr_(...) ((void)((0) && printf(__VA_ARGS__)))
#define dbg_requires(expr) dbg_discard_expr_("%d", !(expr))
#define dbg_assert(expr) dbg_discard_expr_("%d", !(expr))
#define dbg_ensures(expr) dbg_discard_expr_("%d", !(expr))
#define dbg_printf(...) dbg_discard_expr_(__VA_ARGS__)
#define dbg_printheap(...) ((void)((0) && print_heap(__VA_ARGS__)))
#endif

#define NUM_AHEAD 5
#define NUM_CLASS 15

/* Basic constants */

typedef uint64_t word_t;

/** @brief Word and header size (bytes) */
static const size_t wsize = sizeof(word_t);

/** @brief Double word size (bytes) */
static const size_t dsize = 2 * wsize;

/** @brief Minimum block size (bytes) */
static const size_t min_block_size = dsize; //change this to only dsize

/**
 * TODO: explain what chunksize is
 * (Must be divisible by dsize)
 * 
 * chunk is the maximum number of blocks
 
*/
static const size_t chunksize = (1 << 10);
/**
 * TODO: explain what alloc_mask is
 */
static const word_t alloc_mask = 0x1;

static const word_t prev_mask = 0x02;

static const word_t mini_prev_mask = 0x04;

/**
 * TODO: explain what size_mask is
 */
static const word_t size_mask = ~(word_t)0xF;

/** @brief Represents the header and payload of one block in the heap */
typedef struct block {
    /** @brief Header contains size + allocation flag */
    word_t header;

    /**
     * @brief A pointer to the block payload.
     *
     * TODO: feel free to delete this comment once you've read it carefully.
     * We don't know what the size of the payload will be, so we will declare
     * it as a zero-length array, which is a GNU compiler extension. This will
     * allow us to obtain a pointer to the start of the payload. (The similar
     * standard-C feature of "flexible array members" won't work here because
     * those are not allowed to be members of a union.)
     *
     * WARNING: A zero-length array must be the last element in a struct, so
     * there should not be any struct fields after it. For this lab, we will
     * allow you to include a zero-length array in a union, as long as the
     * union is the last field in its containing struct. However, this is
     * compiler-specific behavior and should be avoided in general.
     *
     * WARNING: DO NOT cast this pointer to/from other types! Instead, you
     * should use a union to alias this zero-length array with another struct,
     * in order to store additional types of data in the payload memory.
     */
    
    union {
        struct {
        struct block* next_list;
        struct block* prev_list;
        };        
        char payload[0];
    }; 
    
    /*
     * TODO: delete or replace this comment once you've thought about it.
     * Why can't we declare the block footer here as part of the struct?
     * Why do we even have footers -- will the code work fine without them?
     * which functions actually use the data contained in footers?
     */
} block_t;

/* Global variables */
/** @brief Pointer to head of a seg list*/
//static block_t *head = NULL;
//static block_t *tail = NULL;
/** @brief Pointer to first block in the heap */
block_t *heap_start = NULL;

/**@brief Pointer to seglist class sizes */
block_t *seg_list[NUM_CLASS]; //do we change this to make it fewer class sizes/



/*
 *****************************************************************************
 * The functions below are short wrapper functions to perform                *
 * bit manipulation, pointer arithmetic, and other helper operations.        *
 *                                                                           *
 * We've given you the function header comments for the functions below      *
 * to help you understand how this baseline code works.                      *
 *                                                                           *
 * Note that these function header comments are short since the functions    *
 * they are describing are short as well; you will need to provide           *
 * adequate details for the functions that you write yourself!               *
 *****************************************************************************
 */

/*
 * ---------------------------------------------------------------------------
 *                        BEGIN SHORT HELPER FUNCTIONS
 * ---------------------------------------------------------------------------
 */

/**
 * @brief Returns the maximum of two integers.
 * @param[in] x
 * @param[in] y
 * @return `x` if `x > y`, and `y` otherwise.
 */
static size_t max(size_t x, size_t y) {
    return (x > y) ? x : y;
}

/**
 * @brief Rounds `size` up to next multiple of n
 * @param[in] size
 * @param[in] n
 * @return The size after rounding up
 */
static size_t round_up(size_t size, size_t n) {
    return n * ((size + (n - 1)) / n);
}

/**
 * @brief Packs the `size` and `alloc` of a block into a word suitable for
 *        use as a packed value.
 *
 * Packed values are used for both headers and footers.
 *
 * The allocation status is packed into the lowest bit of the word.
 *
 * @param[in] size The size of the block being represented
 * @param[in] alloc True if the block is allocated
 * @return The packed value
 */
static word_t pack(size_t size, bool alloc, bool prev_alloc, bool mini_prev) {
    word_t word = size;
    if (alloc) {
        word |= alloc_mask;
    }
    if (prev_alloc){ //if it is true, it is allocated 
        word |= prev_mask;
    }
    if(mini_prev){
        word |= mini_prev_mask;
    }
    return word;
}

/**
 * @brief Extracts the size represented in a packed word.
 *
 * This function simply clears the lowest 4 bits of the word, as the heap
 * is 16-byte aligned.
 *
 * @param[in] word
 * @return The size of the block represented by the word
 */
static size_t extract_size(word_t word) {
    return (word & size_mask);
}

/**
 * @brief Extracts the size of a block from its header.
 * @param[in] block
 * @return The size of the block
 */
static size_t get_size(block_t *block) 
{
    dbg_requires(block != NULL);

    return extract_size(block->header);
}

/**
 * @brief Given a payload pointer, returns a pointer to the corresponding
 *        block.
 * @param[in] bp A pointer to a block's payload
 * @return The corresponding block
 */
static block_t *payload_to_header(void *bp) {
    return (block_t *)((char *)bp - offsetof(block_t, payload));
}

/**
 * @brief Given a block pointer, returns a pointer to the corresponding
 *        payload.
 * @param[in] block
 * @return A pointer to the block's payload
 * @pre The block must be a valid block, not a boundary tag.
 */
static void *header_to_payload(block_t *block) {
    dbg_requires(get_size(block) != 0);
    return (void *)(block->payload);
}

/**
 * @brief Given a block pointer, returns a pointer to the corresponding
 *        footer.
 * @param[in] block
 * @return A pointer to the block's footer
 * @pre The block must be a valid block, not a boundary tag.
 */
static word_t *header_to_footer(block_t *block) {
    dbg_requires(get_size(block) != 0 &&
                 "Called header_to_footer on the epilogue block");
    return (word_t *)(block->payload + get_size(block) - dsize);
}

/**
 * @brief Given a block footer, returns a pointer to the corresponding
 *        header.
 * 
 * The header is found by subtracting the block size from
 * the footer and adding back wsize. 
 *
 * If the prologue is given, then the footer is return as the block.
 *
 * @param[in] footer A pointer to the block's footer
 * @return A pointer to the start of the block
 */
static block_t *footer_to_header(word_t *footer) {
    size_t size = extract_size(*footer);
    if (size == 0){
        return (block_t *)footer;
    }
    return (block_t *)((char *)footer + wsize - size);
}

/**
 * @brief Returns the payload size of a given block.
 *
 * The payload size is equal to the entire block size minus the sizes of the
 * block's header and footer.
 *
 * @param[in] block
 * @return The size of the block's payload
 */
static size_t get_payload_size(block_t *block) {
    size_t asize = get_size(block);
    return asize - wsize; //need to change this to asize - wsize; 
}

/**
 * @brief Returns the allocation status of a given header value.
 *
 * This is based on the lowest bit of the header value.
 *
 * @param[in] word
 * @return The allocation status correpsonding to the word
 */
static bool extract_alloc(word_t word) {
    return (bool)(word & alloc_mask);
}

/**
 * @brief Returns the allocation status of a block, based on its header.
 * @param[in] block
 * @return The allocation status of the block
 */
static bool get_alloc(block_t *block) {
    return extract_alloc(block->header);
}

/**
 * @brief Writes an epilogue header at the given address.
 *
 * The epilogue header has size 0, and is marked as allocated.
 *
 * @param[out] block The location to write the epilogue header
 */
static void write_epilogue(block_t *block, bool is_mini) {
    dbg_requires(block != NULL);
    dbg_requires((char *)block == (char *)mem_heap_hi() - 7);
    block->header = pack(0, true, false, is_mini);
}



/**
 * @brief Finds the next consecutive block on the heap.
 *
 * This function accesses the next block in the "implicit list" of the heap
 * by adding the size of the block.
 *
 * @param[in] block A block in the heap
 * @return The next consecutive block on the heap
 * @pre The block is not the epilogue
 */
static block_t *find_next(block_t *block) {
    dbg_requires(block != NULL);
    dbg_requires(get_size(block) != 0 &&
                 "Called find_next on the last block in the heap");

    return (block_t *)((char *)block + get_size(block));
}

/**
 * @brief Finds the footer of the previous block on the heap.
 * @param[in] block A block in the heap
 * @return The location of the previous block's footer
 */
static word_t *find_prev_footer(block_t *block) {
    // Compute previous footer position as one word before the header
    return &(block->header) - 1;
}

/**
 * @brief finds the footer of the previous block which is a mini block 
 * @return the location of the previous block's footer
*/
static block_t *find_prev_mini(block_t *block){
    return (block_t*)(&(block->header) - 2);
}

//create a find prev mini block and it is &(block->header) -2 
//use the above find_mini_prev in the coalesce function 
//create a separate field that tells us if the previous block is a miniblock or not
//when writeblock is called, we need to check if it is dsize or not, but the payload is 8 bytes, not 16 bytes. 
/**
 * @brief Finds the previous consecutive block on the heap.
 *
 * This is the previous block in the "implicit list" of the heap.
 *
 * The position of the previous block is found by reading the previous
 * block's footer to determine its size, then calculating the start of the
 * previous block based on its size.
 *
 * @param[in] block A block in the heap
 * @return The previous consecutive block in the heap.
 * @pre The block is not the prologue
 */
static block_t *find_prev(block_t *block) {
    dbg_requires(block != NULL);
    
    dbg_requires(get_size(block) != 0 &&
                 "Called find_prev on the first block in the heap");
    word_t *footerp = find_prev_footer(block); //find prev footer
    block_t* val = footer_to_header(footerp); //convert the footer to a header*/
 
    return val;
}

static bool get_prev_alloc(block_t* block){
    word_t header = block->header & prev_mask;
    if(header == 0){
        return false;
    }
    return true;
}
/**
 * @brief this function returns true if the prev block is a mini_block 
*/
static bool get_mini_prev(block_t* block){ 
    word_t header = block->header & mini_prev_mask;
    if(header == 0){
        return false;
    }
    return true; 
}
/**
 * @brief Writes a block starting at the given address.
 *
 * This function writes both a header and footer, where the location of the
 * footer is computed in relation to the header.
 *
 * TODO: Are there any preconditions or postconditions?
 *
 * @param[out] block The location to begin writing the block header
 * @param[in] size The size of the new block
 * @param[in] alloc The allocation status of the new block
 */
static void write_block(block_t *block, size_t size, bool alloc, bool prev_alloc, bool mini_prev) {
    dbg_requires(block != NULL);
    dbg_requires(size > 0);
    //read the allocation status of prev block
    bool is_mini = false;
    if(size <= dsize){ //checks if current block is a mini block 
        is_mini = true;
    }
    
    block->header = pack(size, alloc,prev_alloc, mini_prev);

    if(alloc == false && size > dsize){ //need to add a condition for footers only in payload > 16
        word_t *footerp = header_to_footer(block);
        *footerp = pack(size, alloc, prev_alloc, mini_prev);
    }
    //call pack on the next header
    //read next block header adn size and alloc
    
    block_t* next_block = find_next(block);
    word_t next_size = get_size(next_block);
    bool next_alloc = get_alloc(next_block); 
    find_next(block)->header = pack(next_size, next_alloc, alloc, is_mini); //packs the current information into the next block

}




/*
 * ---------------------------------------------------------------------------
 *                        END SHORT HELPER FUNCTIONS
 * ---------------------------------------------------------------------------
 */

/******** The remaining content below are helper and debug routines ********/

/**
 * @brief this function prints the contents of the heap 
*/

static void print_heap(){

    //print size, allocation, and loc of prologue and epilogue 
    block_t *pro = (block_t *) ((char*) mem_heap_lo());
    printf("\n************************\n");
    printf("\033[0;34m");
    printf("Prologue %lu\n", pro->header);
    printf("\033[0m");
    //print header, allocation status, next and prev pointer, and payload
   block_t* block; 
    
    for (block = find_next(heap_start); get_size(block) > 0; block = find_next(block)){
        printf("the header is %lu \n", block->header);
        if(get_alloc(block) == true){
            printf("Allocation status: allocated\n");

        }
        else{
            
            printf("Allocation status: free\n");
            if(get_size(block) > dsize){
                if(block->prev_list != NULL){
                    printf("Prev pointer is %lu\n", get_size(block->prev_list));
                }
                else{
                    printf("Prev pointer is NULL\n");
                }
            }

                if(block->next_list != NULL){
                    printf("Next pointer is %lu\n", block->next_list->header);
                }
                else{
                    printf("Next pointer is NULL\n");
                }

                printf("---------------\n");

                if(get_prev_alloc(block) == true ){
                    printf("Previous block is allocated\n");
                }
                else{
                    printf("Previous block is free\n");
                }

                if(get_mini_prev(block) == true){
                    printf("Previous block is a mini block\n");
                }
                else{
                    printf("Previous block is not a mini block\n");
                }
            
            
        }

       
    }

        //print each seg_list

        for(size_t i = 0; i< NUM_CLASS; i++){
            block_t* current = seg_list[i];
            printf("\n seg_list at index %zu", i);
            printf("\n+++++++++++++++++\n");
            while(current != NULL){
                printf("the header is %lu \n", get_size(current));
                if(current->next_list != NULL){
                    printf("Next pointer is %lu\n", get_size(current->next_list));
                }
                else{
                    printf("Next pointer is NULL\n");
                }
                if(i != 0){
                if(current->prev_list != NULL){
                    printf("Prev pointer is %lu\n", get_size(current->prev_list));
                }
                else{
                    printf("Prev pointer is NULL\n");
                }
                }

                if(get_prev_alloc(current) == true ){
                    printf("Previous block is allocated\n");
                }
                else{
                    printf("Previous block is free\n");
                }

                if(get_mini_prev(current) == true){
                    printf("Previous block is a mini block\n");
                }
                else{
                    printf("Previous block is not a mini block\n");
                }

            
                current = current->next_list;
                printf("+++++++++++++++++\n");
            }
        }

        printf("--------------------\n");
    block_t *epi = (block_t *)((char *) mem_heap_hi() -7);
    printf("\033[0;34m");
    printf("Epilogue %lu\n", epi->header);
    printf("\033[0m");
    printf("************************\n");
    //printf("--------------------\n");
}

/**
 * @brief this calculates the log_2 index of the size of the block
 * @return returns the index 
*/
static size_t log_2(size_t num){
    long res = -1;
    while( num > 0){
        num >>= 1;
        res++;
    }
    return (size_t) res;

}
/**
 * @brief this function adds the block to the seg list 
*/

static void add(block_t *block)
 {
  dbg_requires(block != NULL); 
    size_t size = get_size(block);
    
    if(size <= dsize){ // insert into mini_free list fo rmini_blocks
        size_t in = log_2((size-1));
        if(seg_list[in] == NULL){ //either the list is empty 
            seg_list[in] = block;
            seg_list[in]->next_list = NULL;
        }
        else{
            block->next_list = seg_list[in]; //list is not empty
            seg_list[in] = block;
        }
    }
    else{ //insert into seg list 
        size_t index = log_2((size-1));
        if(index > NUM_CLASS-1){
            index = NUM_CLASS-1; 
        }        
        
        if(seg_list[index] != NULL){
      
            block->next_list = seg_list[index];
            seg_list[index]->prev_list = block;
            seg_list[index] = block;
            seg_list[index]->prev_list = NULL;
        }
        else{
            seg_list[index] = block;
            seg_list[index]->next_list = NULL;
            seg_list[index]->prev_list = NULL;
        }

    }

}

/**
 * @brief deletes node from double linked list 
*/
static void delete(block_t *block) {

    size_t size = get_size(block);
    if(size == dsize){
        size_t in = log_2((size-1));
        if(seg_list[in] == block){ //if head of mini list is block to delete
            seg_list[in] = seg_list[in]->next_list;
            block->next_list = NULL;
        }
        else{ //the block is somewhere in the list 
            block_t* cur = seg_list[in];
            while(cur->next_list!= block){
                cur = cur->next_list;
            }
            cur->next_list = block->next_list;
            block->next_list = NULL;
        }
    }
    else
    { 
        size_t index = log_2(size-1);
        if(index > NUM_CLASS-1){
            index = NUM_CLASS-1; 
        }

        if(seg_list[index] == block){
            
            seg_list[index] = block->next_list;
            if(block->next_list != NULL){

                block->next_list->prev_list = NULL;
            }
            return;
        }
        

        if(block->prev_list!= NULL && block->next_list == NULL){
            block->prev_list->next_list = NULL;
            block->prev_list = NULL;
            return;
        }

          else{
            block->prev_list->next_list = block->next_list;
            block->next_list->prev_list = block->prev_list;
            block->prev_list = NULL;
            block->next_list = NULL; 
        }
    }
}

    


/**
 * @brief
 *
 * <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 *
 * @param[in] block
 * @return
 */
static block_t *coalesce_block(block_t *block) {
    //dbg_requires(mm_checkheap(__LINE__));
    /*
     * TODO: delete or replace this comment once you're done.
     *
     * Before you start, it will be helpful to review the "Dynamic Memory
     * Allocation: Basic" lecture, and especially the four coalescing
     * cases that are described.
     *
     * The actual content of the function will probably involve a call to
     * find_prev(), and multiple calls to write_block(). For examples of how
     * to use write_block(), take a look at split_block().
     *
     * Please do not reference code from prior semesters for this, including
     * old versions of the 213 website. We also discourage you from looking
     * at the malloc code in CS:APP and K&R, which make heavy use of macros
     * and which we no longer consider to be good style.
     */

   
    block_t* next = find_next(block);
    bool prev_alloc = get_prev_alloc(block);
    block_t* prev;
    
    //case 1: both prev and next are allocated
    if(prev_alloc == true && get_alloc(next) == true){ //find prev alloc on current block
        add(block);
        dbg_ensures(mm_checkheap(__LINE__));
        return block;
    }

    //case 3: only prev is free
    if(prev_alloc == false  && get_alloc(next) == true){
        if(get_mini_prev(block) == true){
            prev = find_prev_mini(block);
        }
        else{
            prev = find_prev(block); 
        }

        delete(prev);

        write_block(prev,get_size(prev)+ get_size(block),false,get_prev_alloc(prev),get_mini_prev(prev));
        add(prev);
        dbg_ensures(mm_checkheap(__LINE__));
        return prev;
    }
    //case 2: only next is free
    if(prev_alloc == true  && get_alloc(next) == false){
         delete(next);
        write_block(block,get_size(next)+ get_size(block),false, get_prev_alloc(block), get_mini_prev(block));
        
        
        add(block);
         dbg_ensures(mm_checkheap(__LINE__));
         return block;
    }
    
    //case 4: both prev and next are free 
     if(prev_alloc == false  && get_alloc(next) == false){
       if(get_mini_prev(block) == true){
            prev = find_prev_mini(block);
        }
        else{
            prev = find_prev(block); 
        } 

    
        delete(next);

        delete(prev);
         write_block(prev,get_size(next)+ get_size(block) + get_size(prev),false, get_prev_alloc(prev), get_mini_prev(prev));
         
         add(prev);
         dbg_ensures(mm_checkheap(__LINE__));
         return prev;
    }
dbg_ensures(mm_checkheap(__LINE__));
    return NULL;
    //case 1: 


    
}
    


/**
 * @brief
 * 
 * <What does this function do?> -> extend heap if there is not enough space for a malloc call 
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 *
 * @param[in] size
 * @return
 */
static block_t *extend_heap(size_t size) {
    void *bp;

    // Allocate an even number of words to maintain alignment
    size = round_up(size, dsize);
    if ((bp = mem_sbrk((intptr_t)size)) == (void *)-1) {
        return NULL;
    }

    /*
     * TODO: delete or replace this comment once you've thought about it.
     * Think about what bp represents. Why do we write the new block
     * starting one word BEFORE bp, but with the same size that we
     * originally requested?
     */

    // Initialize free block header/footer
    block_t *block = payload_to_header(bp);
    write_block(block, size, false, get_prev_alloc(block), get_mini_prev(block));

    // Create new epilogue header
    block_t *block_next = find_next(block);
    if(size <= dsize){
        write_epilogue(block_next, true);
    }
    else{
        write_epilogue(block_next, false);
    }

    // Coalesce in case the previous block was free
    block = coalesce_block(block);

    return block;
}



/**
 * @brief
 *
 * <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 *
 * @param[in] block
 * @param[in] asize
 */
static void split_block(block_t *block, size_t asize) { // how does the free list change in this case
    dbg_requires(get_alloc(block));
    /* TODO: Can you write a precondition about the value of asize? */

    size_t block_size = get_size(block);

    if ((block_size - asize) >= min_block_size) { 
        block_t *block_next; 

        delete(block);
        write_block(block, asize, true, get_prev_alloc(block), get_mini_prev(block)) ;

        block_next = find_next(block); 
       
        write_block(block_next, block_size - asize, false, true, asize==dsize);
        add(block_next);
        
    }
    else{
        delete(block);
    }

    dbg_ensures(get_alloc(block));
}

/**
 * @brief
 *
 * <What does this function do?>
 * <What are the function's arguments?>  
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 *
 * @param[in] asize
 * @return
 */
static block_t *find_fit(size_t asize) {

    block_t *block;
   
    if(asize == dsize){
        block = seg_list[0];
        if(block != NULL ){
            return block;
        }
    }
    //printf("Finding the block..\n");

    size_t index = log_2((asize-1));
    if(index > NUM_CLASS-1){
        index = NUM_CLASS-1; 
    }   
    for(size_t i = index; i < NUM_CLASS; i++){
        block = seg_list[i];
        while(block!= NULL){
            if (asize == get_size(block)) {
                return block;
            }
            if(asize < get_size(block)){
                block_t* temp = block;
                size_t num = NUM_AHEAD;
                size_t temp_size = get_size(block);

                block = block->next_list;
                while(num>0 && block !=NULL){
                    if(get_size(block) < temp_size && get_size(block) >= asize){
                        temp_size = get_size(block);
                        temp = block;
                    }
                    num = num - 1;
                    block = block->next_list;
                }
                dbg_assert(block != temp);
                dbg_assert(num ==0 || block == NULL);
                return temp;
            }
            block = block->next_list;
        }
    }
    return NULL;// no fit found
    
}
/**
 * @brief this function finds a matching block in the free list
 * @param takes in a free block;
*/


static bool find_block(block_t* target){
    size_t size = get_size(target);
    block_t* block = seg_list[0];
   if(size > dsize) {
        size_t index = log_2((size-1));
        
        if(index > NUM_CLASS-1){
            index = NUM_CLASS-1; 
            block = seg_list[index];
        }
        else{
            block = seg_list[index];
        }
    }
    while(block!= NULL){
           
            if(get_size(block) == get_size(target) && get_alloc(block) == false){
            return true;
            }
            block = block->next_list;
        }
    return false;

}

/**
 * @brief
 *
 * <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 *
 * @param[in] line
 * @return
 */
bool mm_checkheap(int line) {

    //check for epilogue and prologue
    
    block_t *epi = (block_t *)((char *) mem_heap_hi() -7);
    if(get_size(epi) != 0 || get_alloc(epi) == false){ // check epilogue header is correct
        dbg_printf("\n epi is wrong \n");
        return false; 
    }
    //char* adn 
    block_t *pro = (block_t *) ((char*) mem_heap_lo());
    if(get_size(pro) != 0 || get_alloc(pro) == false){//check the prologue header is correct
        dbg_printf("\n pro is wrong\n");
        return false;
    }

    block_t *block;

    //int count_free_block = 0; 
    //printf("checkheap output:\n");
    for (block = heap_start; get_size(block) > 0; block = find_next(block)) { //iterate through the heap
   // printf("\ncurrent blockbt in loop is %lu\n",block->header);
        if(get_alloc(block) == false){
                if(find_block(block) == false){ // check free blocks in heap arein free list
                    printf("the current size of block is %lu\n", get_size(block));
                    dbg_printf("\nThe blocks did not match\n");
                    return false;
        
                }
            if(get_size(block) > dsize){
                    if(extract_size((*header_to_footer(block))) != get_size(block) || extract_alloc(*header_to_footer(block)) != get_alloc(block)){ //check header and footer are the same 
                    //printf("here header not footer\n");
                    dbg_printf("\n header != footer\n");
                    return false;

                }
            }
            
        }
       
        if(get_size(block) % wsize != 0 || get_payload_size(block) % wsize != 0 || ((uintptr_t) block->payload) % dsize != 0){ //check the address alignment of each block
            dbg_printf("\n not address aligned\n");
            return false;
        }
        if(get_size(block) != 0){ 
            if(get_alloc(block) == false && get_alloc(find_next(block)) == false){ //no two consecutive free blocks in heap
                dbg_printf("\n no two consec free block\n");
                return false;
            }
        }
    }

    //checks for the seg_list
    for(size_t i = 0; i<NUM_CLASS; i++){
        size_t min_size = (size_t) 1<<i; //2^i
        size_t max_size = (size_t) 1<<(i+1);//2^(i+1)

        block_t* cur = seg_list[i];

  
        while(cur != NULL ){
           
            //check that the free list pointers are between lo and hi
            if(((void *)cur) <  mem_heap_lo() || ((void *)cur) > mem_heap_hi()){ 

                dbg_printf("block is out of bounds \n");
                return false;
            }

            //check that pointers are consistent
             if(i > 0 && cur != NULL && cur->prev_list != NULL && cur->next_list != NULL && cur->next_list->prev_list != cur && cur->prev_list->next_list != cur){
                dbg_printf("block is not consistant\n");
                return false;
            }

            size_t cur_size = get_size(cur); 
            if(min_size > cur_size || max_size < cur_size ){//check that the blocks in each bucket are within the bucket size range (2^i, 2^(i+1)) 
                if((i!= 0 && i != NUM_CLASS-1) || cur_size < max_size ){
                dbg_printf("the size of block was greater or less than the size range of bucket\n");
                return false;
                }
                
            }
            cur = cur->next_list;
        }

    }

    return true;
}

/**
 * @brief
 *
 * Initializes the heap
 * 
 * Returns True
 * 
 *
 * @return
 */
bool mm_init(void) {
    // Create the initial empty heap
    word_t *start = (word_t *)(mem_sbrk(2 * wsize));

    if (start == (void *)-1) {
        return false;
    }


    /*
     * TODO: delete or replace this comment once you've thought about it.
     * Think about why we need a heap prologue and epilogue. Why do
     * they correspond to a block footer and header respectively?
     */

    start[0] = pack(0, true, true, false); // Heap prologue (block footer)
    start[1] = pack(0, true,true, false); // Heap epilogue (block header)

    // Heap starts with first "block header", currently the epilogue
    heap_start = (block_t *)&(start[1]);
    for(size_t i = 0; i < NUM_CLASS; i++){
        
        seg_list[i] = NULL;
    
    } 
    // Extend the empty heap with a free block of chunksize bytes
    if (extend_heap(chunksize) == NULL) {
        return false;
    }

    size_t index = log_2(get_size(heap_start)-1);
    seg_list[index] = heap_start;
    seg_list[index]->next_list = NULL;

    return true;
}

/**
 * @brief
 *
 * <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 *
 * @param[in] size
 * @return
 */
void *malloc(size_t size) {
    dbg_requires(mm_checkheap(__LINE__));
    //print_heap();

    size_t asize;      // Adjusted block size
    size_t extendsize; // Amount to extend heap if no fit is found
    block_t *block;
    void *bp = NULL;

    // Initialize heap if it isn't initialized
    if (heap_start == NULL) {
        if (!(mm_init())) {
            dbg_printf("Problem initializing heap. Likely due to sbrk");
            return NULL;
        }
    }

    // Ignore spurious request
    if (size == 0) {
        dbg_ensures(mm_checkheap(__LINE__));
        return bp;
    }

    // Adjust block size to include overhead and to meet alignment requirements
    asize = round_up(size + wsize, dsize); //adjust block size for removing footers
    asize = max(asize, min_block_size); 
    
    block = find_fit(asize);
   
    // If no fit is found, request more memory, and then and place the block
    if (block == NULL) {
    
        extendsize = max(asize, chunksize);
        block = extend_heap(extendsize);
        // extend_heap returns an error
        if (block == NULL) {
            return bp;
        }
    }


    // The block should be marked as free
    dbg_assert(!get_alloc(block));

    // Mark block as allocated
    size_t block_size = get_size(block);
    write_block(block, block_size, true, get_prev_alloc(block), get_mini_prev(block));
    // Try to split the block if too large
    split_block(block, asize);

    bp = header_to_payload(block);

    dbg_ensures(mm_checkheap(__LINE__));
    return bp;
}

/**
 * @brief
 *
 * <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 *
 * @param[in] bp
 */
void free(void *bp) {
    dbg_requires(mm_checkheap(__LINE__));
    

    if (bp == NULL) {
        return;
    }

    block_t *block = payload_to_header(bp);
    size_t size = get_size(block);

    // The block should be marked as allocated
    dbg_assert(get_alloc(block));
    // Mark the block as free
   
    
    write_block(block, size, false, get_prev_alloc(block), get_mini_prev(block));



    // Try to coalesce the block with its neighbors
    coalesce_block(block);


    dbg_ensures(mm_checkheap(__LINE__));
}

/**
 * @brief
 *
 * <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 *
 * @param[in] ptr
 * @param[in] size
 * @return
 */
void *realloc(void *ptr, size_t size) {
    block_t *block = payload_to_header(ptr);
    size_t copysize;
    void *newptr;

    // If size == 0, then free block and return NULL
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    // If ptr is NULL, then equivalent to malloc
    if (ptr == NULL) {
        return malloc(size);
    }

    // Otherwise, proceed with reallocation
    newptr = malloc(size);

    // If malloc fails, the original block is left untouched
    if (newptr == NULL) {
        return NULL;
    }

    // Copy the old data
    copysize = get_payload_size(block); // gets size of old payload
    if (size < copysize) {
        copysize = size;
    }
    memcpy(newptr, ptr, copysize);

    // Free the old block
    free(ptr);

    return newptr;
}

/**
 * @brief
 *
 * <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 *
 * @param[in] elements
 * @param[in] size
 * @return
 */
void *calloc(size_t elements, size_t size) {
    void *bp;
    size_t asize = elements * size;

    if (elements == 0) {
        return NULL;
    }
    if (asize / elements != size) {
        // Multiplication overflowed
        return NULL;
    }

    bp = malloc(asize);
    if (bp == NULL) {
        return NULL;
    }

    // Initialize all bits to 0
    memset(bp, 0, asize);

    return bp;
}

/*
 *****************************************************************************
 * Do not delete the following super-secret(tm) lines!                       *
 *                                                                           *
 * 53 6f 20 79 6f 75 27 72 65 20 74 72 79 69 6e 67 20 74 6f 20               *
 *                                                                           *
 * 66 69 67 75 72 65 20 6f 75 74 20 77 68 61 74 20 74 68 65 20               *
 * 68 65 78 61 64 65 63 69 6d 61 6c 20 64 69 67 69 74 73 20 64               *
 * 6f 2e 2e 2e 20 68 61 68 61 68 61 21 20 41 53 43 49 49 20 69               *
 *                                                                           *
 * 73 6e 27 74 20 74 68 65 20 72 69 67 68 74 20 65 6e 63 6f 64               *
 * 69 6e 67 21 20 4e 69 63 65 20 74 72 79 2c 20 74 68 6f 75 67               *
 * 68 21 20 2d 44 72 2e 20 45 76 69 6c 0a c5 7c fc 80 6e 57 0a               *
 *                                                                           *
 *****************************************************************************
 */
