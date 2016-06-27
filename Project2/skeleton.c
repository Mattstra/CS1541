#ifndef __SKELETON_H__
#define __SKELETON_H__

///////////////////////////////////////////////////////////////////////////////
//
// CS 1541 Introduction to Computer Architecture
// You may use this skeleton code to create a cache instance and implement cache operations.
// Feel free to add new variables in the structure if needed.
//
///////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <stdio.h>

int nsets;
int nblocks;
int associativity;
int block_num = 0;
int count_num = 0;

struct cache_blk_t {
  unsigned long tag;
  char valid;
  char dirty;
  int counter;
  int address;
};

enum cache_policy {
  LRU,
  FIFO
};

struct cache_t {
  int nsets;		// # sets
  int bsize;		// block size
  int assoc;		// associativity

  enum cache_policy policy;       // cache replacement policy
  struct cache_blk_t **blocks;    // cache blocks in the cache
};

struct cache_t *
cache_create(int size, int blocksize, int assoc, enum cache_policy policy)
{

  int i;
  nblocks = size / blocksize; // number of blocks in the cache
  nsets = size / (assoc * blocksize);   // number of sets (entries) in the cache
  associativity = assoc;
  count_num = associativity -1;

  struct cache_t *C = (struct cache_t *)calloc(1, sizeof(struct cache_t));

  C->nsets = nsets;
  C->bsize = blocksize;
  C->assoc = assoc;
  C->policy = policy;

  C->blocks= (struct cache_blk_t **)calloc(nsets, sizeof(struct cache_blk_t));

	for(i = 0; i < nsets; i++)
    {
		C->blocks[i] = (struct cache_blk_t *)calloc(assoc, sizeof(struct cache_blk_t));
	}

  return C;
}

int cache_access(struct cache_t *cp, unsigned long address,
             char access_type, unsigned long long now)
{
	//////////////////////////////////////////////////////////////////////
  //
  // Your job:
  // based on address determine the set to access in cp
  // examine blocks in the set to check hit/miss
  // if miss, determine the victim in the set to replace
  // if update the block list based on the replacement policy
  // return 0 if a hit, 1 if a miss or 2 if a miss_with_write_back
  //
	//////////////////////////////////////////////////////////////////////

    int i;
    int j;
    int set = address % nsets;
    int tag = address / nsets;
    int is_dirty;
    int found = 0;
    int hit = 0;

    //check for a hit
    for(i = 0; i < associativity; i++)
    {
        if(cp->blocks[set][i].tag == tag && cp->blocks[set][i].valid == 1)
        {
            //cache hit
            hit = 1;
            if(access_type == 1)
                cp->blocks[set][i].dirty = 1;

            is_dirty = -1;
            printf("hit!!!\n");
            //reset LRU counter for block
            cp->blocks[set][i].counter = -1;
            for(j = 0; j < associativity; j++)
            {
                cp->blocks[set][j].counter++;
            }
        }
    }

    if(hit == 0)
    {
        //cache miss
        //see if there are any unoccupied blocks in set first
        for(i = 0; i < associativity; i++)
        {
            if(cp->blocks[set][i].valid != 1)
            {
                //found available block
                cp->blocks[set][i].tag = tag;
                cp->blocks[set][i].valid = 1;
                cp->blocks[set][i].dirty = 1;

                //set counter value for LRU
                //accounts for when associativity is 2 or lower power of 2
                if(associativity < 3)
                {
                    int lru_val = (associativity % (i+1));

                    if(lru_val == 0)
                    {
                        if((i+1) == associativity)
                            lru_val = 0;
                        else
                            lru_val = associativity -1;
                    }

                    cp->blocks[set][i].counter = lru_val;
                }
                else
                {
                    int lru_val;
                    if(i == 0)
                    {
                        lru_val = associativity;
                    }
                    else
                    {
                        lru_val = associativity % i;
                        if(lru_val == 0)
                            lru_val = associativity - i;
                    }
                    cp->blocks[set][i].counter = lru_val;
                }
                is_dirty = 0;
                found = 1;
                printf("found in set %d\n", set);
                break;
            }
        }

        if(found == 0)
        {
            printf("Not found\n");
            //no available blocks so have to replace based on policy

            //FIFO
            if(cp->policy == 1)
            {
                //replace blocks in order
                if(cp->blocks[set][block_num].dirty == 0)
                {
                    cp->blocks[set][block_num].dirty = 1;
                    is_dirty = 0;
                }
                else
                {
                    is_dirty = 1;
                }

                cp->blocks[set][block_num].tag = tag;
                cp->blocks[set][block_num].valid = 1;
            }

            //LRU
            else
            {
                int lru_array[associativity];
                //zero out array
                for(i = 0; i < associativity; i++)
                {
                    lru_array[i] = 0;
                }

                for(i = 0; i < associativity; i++)
                {
                    lru_array[i] = cp->blocks[set][i].counter;
                }

                //now compare and see which is largest
                int largest = 0;
                int temp = 0;
                int index = 0;
                largest = lru_array[0];

                for(i = 0; i < associativity; i++)
                {
                    if(lru_array[i] > largest)
                    {
                        largest = lru_array[i];
                        index = i;
                    }
                }
                //increment all counter values
                for(i = 0; i < associativity; i++)
                {
                    cp->blocks[set][i].counter = cp->blocks[set][i].counter+1;
                }

                //replace block stored at index in set

                if(cp->blocks[set][index].dirty == 1)
                    is_dirty = 1;

                cp->blocks[set][index].tag = tag;
                cp->blocks[set][index].valid = 1;
                cp->blocks[set][index].counter = 0;

            }

            if(block_num == associativity -1)
                block_num = 0;
            else
                block_num++;
        }
    }

        if(is_dirty == 1)
        {
            is_dirty = 0;
            return 2;
        }
        if(is_dirty == 0)
        {
            return 1;
        }
        if(is_dirty == -1)
        {
            is_dirty = 0;
            return 0;
        }
}

#endif
