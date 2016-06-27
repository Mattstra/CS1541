#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "trace_item.h"
#include "skeleton.c"

#define TRACE_BUFSIZE 1024*1024

static FILE *trace_fd;
static int trace_buf_ptr;
static int trace_buf_end;
static struct trace_item *trace_buf;

// to keep statistics
unsigned int accesses = 0;
unsigned int read_accesses = 0;
unsigned int write_accesses = 0;
unsigned int hits = 0;
unsigned int misses = 0;
unsigned int misses_with_writeback = 0;

int size = 0;                   //cache size as an int
int block_size = 0;             //block size in bytes
double cache_size = 0;          //should be a power of 2 in kilobytes
int associativity = 0;          //associativity of cache sets
int policy;                     //replacement policy: 0 - LRU, 1 - FIFO
unsigned long long cycles = 0;
int load = -1;
int store = -1;

void trace_init()
{
  trace_buf = malloc(sizeof(struct trace_item) * TRACE_BUFSIZE);

  if (!trace_buf) {
    fprintf(stdout, "** trace_buf not allocated\n");
    exit(-1);
  }

  trace_buf_ptr = 0;
  trace_buf_end = 0;
}

void trace_uninit()
{
  free(trace_buf);
  fclose(trace_fd);
}

int trace_get_item(struct trace_item **item)
{
  int n_items;

  if (trace_buf_ptr == trace_buf_end) {
    // get new data
    n_items = fread(trace_buf, sizeof(struct trace_item), TRACE_BUFSIZE, trace_fd);
    if (!n_items) return 0;

    trace_buf_ptr = 0;
    trace_buf_end = n_items;
  }

  *item = &trace_buf[trace_buf_ptr];
  trace_buf_ptr++;

  return 1;
}

int main(int argc, char **argv)
{
  struct trace_item *tr_entry;
  size_t size;
  char *trace_file_name;
  int trace_view_on ;

  if (argc == 1) {
    fprintf(stdout, "\nUSAGE: tv <trace_file> <switch - any character>\n");
    fprintf(stdout, "\n(switch) to turn on or off individual item view.\n\n");
    exit(0);
  }


// here you should extract the cache parameters from the command line
  trace_file_name = argv[1];
  trace_view_on = atoi(argv[2]);
  cache_size = atoi(argv[3]);
  block_size = atoi(argv[4]);
  associativity = atoi(argv[5]);
  policy = atoi(argv[6]);

  double log = log2(cache_size);
  cache_size = pow(2, 10) * pow(2,log );
  size = cache_size;

  printf("size of cache is %d\n block size is %d\n associativity is %d\n", size, block_size, associativity);
  fprintf(stdout, "\n ** opening file %s\n", trace_file_name);

  trace_fd = fopen(trace_file_name, "rb");

  if (!trace_fd) {
    fprintf(stdout, "\ntrace file %s not opened.\n\n", trace_file_name);
    exit(0);
  }

  trace_init();
  // here should call cache_create(cache_size, block_size, associativity, replacement_policy)
  struct cache_t *cp = cache_create(size, block_size, associativity, policy);


  while(1) {
    size = trace_get_item(&tr_entry);

    if (!size) {       /* no more instructions to simulate */
	  printf("+ number of accesses : %d \n", accesses);
      printf("+ number of reads : %d \n", read_accesses);
      printf("+ number of writes : %d \n", write_accesses);
      printf("+ number of hits : %d \n", hits);
      printf("+ number of misses : %d \n", misses);
      printf("+ number of misses with write back : %d \n", misses_with_writeback);

	  break;
    }
    else{              /* process only loads and stores */;
	  if (tr_entry->type == ti_LOAD) {
			if (trace_view_on) printf("LOAD %x \n",tr_entry->Addr) ;
			accesses ++;
			read_accesses++ ;
            load = cache_access(cp, tr_entry->Addr, 0, cycles);



	  }
	  if (tr_entry->type == ti_STORE) {
    		  if (trace_view_on) printf("STORE %x \n",tr_entry->Addr) ;
			accesses ++;
			write_accesses++ ;
			store = cache_access(cp, tr_entry->Addr, 1, cycles);
	  }
	  // based on the value returned, update the statisctics for hits, misses and misses_with_writeback
	  if(load == 0)
      {
          hits++;
          load = -1;
      }

      if(store == 0)
      {
          hits++;
          store = -1;
      }

      if(load == 1)
      {
          misses++;
          load = -1;
      }

      if(store == 1)
      {
          misses++;
          store = -1;
      }

      if(load == 2)
      {
          misses_with_writeback++;
          load = -1;
      }

      if(store == 2)
      {
          misses_with_writeback++;
          store = -1;
      }

    }
    cycles++;
  }

  trace_uninit();

  exit(0);
}

