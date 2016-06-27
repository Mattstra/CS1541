/**************************************************************/
/* CS/COE 1541				 			
   just compile with gcc -o pipeline pipeline.c			
   and execute using							
   ./pipeline  /afs/cs.pitt.edu/courses/1541/short_traces/sample.tr	0  
***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include "trace_item.h" 

#define TRACE_BUFSIZE 1024*1024

static FILE *trace_fd;
static int trace_buf_ptr;
static int trace_buf_end;
static struct trace_item *trace_buf;
struct trace_item stage[5];		//array to represent 5 stage pipeline
struct trace_item *tr_entry;
int trace_view_on = 0;
unsigned int cycle_number = 0;


int is_big_endian(void)
{
    union {
        uint32_t i;
        char c[4];
    } bint = {0x01020304};

    return bint.c[0] == 1; 
}

uint32_t my_ntohl(uint32_t x)
{
  u_char *s = (u_char *)&x;
  return (uint32_t)(s[3] << 24 | s[2] << 16 | s[1] << 8 | s[0]);
}

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

  if (trace_buf_ptr == trace_buf_end) {	/* if no more unprocessed items in the trace buffer, get new data  */
    n_items = fread(trace_buf, sizeof(struct trace_item), TRACE_BUFSIZE, trace_fd);
    if (!n_items) return 0;				/* if no more items in the file, we are done */

    trace_buf_ptr = 0;
    trace_buf_end = n_items;			/* n_items were read and placed in trace buffer */
  }

  *item = &trace_buf[trace_buf_ptr];	/* read a new trace item for processing */
  trace_buf_ptr++;
  
  if (is_big_endian()) {
    (*item)->PC = my_ntohl((*item)->PC);
    (*item)->Addr = my_ntohl((*item)->Addr);
  }

  return 1;
}



void printStages()
{
	/* keep track of instructions in the pipeline by using an array
	to represent a 5 stage pipeline
	*/
	int i = 0;
	printf("[Cycle %d] \n", cycle_number);
	stage[i] = *tr_entry;

	/* loop through each stage and print instruction that is currently in that stage */	
	for(i = 0; i < 5; i ++)
	{				
		switch(i)
		{
			case 0:
			printf("IF:  ");
			break;

			case 1:
			printf("ID:  ");
			break;
			
			case 2:
                        printf("EX:  ");
                        break;
		
			case 3:
                        printf("MEM: ");
                        break;

			case 4:
                        printf("WB:  ");
                        break;
		}

		if (trace_view_on) {/* print the executed instruction if trace_view_on=1 */
      			switch(stage[i].type) {
        			case ti_NOP:
          			printf("NOP: \n");
          			break;
				
        			case ti_RTYPE:
          			printf("RTYPE: ");
				if(i == 0)
					printf("(PC: %x)\n", stage[i].PC);
				if(i != 0)
					printf("(PC: %x)(sReg_a: %d)(sReg_b: %d)(dReg: %d)\n", stage[i].PC, stage[i].sReg_a, stage[i].sReg_b, stage[i].dReg);
          			break;
				
        			case ti_ITYPE:
          			printf("ITYPE: ");
					if(i == 0)
						printf("(PC: %x)\n", stage[i].PC);
					if(i != 0)
						printf("(PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n",stage[i].PC,stage[i].sReg_a, stage[i].dReg, stage[i].Addr);
          			break;
				
        			case ti_LOAD:
          			printf("LOAD: ");
					if(i == 0)
						printf("(PC: %x)\n", stage[i].PC);
					if(i == 1)
						printf("(PC: %x)(sReg_a: %d)(dReg: %d)\n",stage[i].PC,stage[i].sReg_a, stage[i].dReg);
					if(i > 1)
						printf("(PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n",stage[i].PC, stage[i].sReg_a, stage[i].dReg, stage[i].Addr);
					break;
				
        			case ti_STORE:
          			printf("STORE: ");
					if(i == 0)
						printf("(PC: %x)\n", stage[i].PC);
					if(i == 1)
						printf("(PC: %x)(sReg_a: %d)(sReg_b: %d)\n", stage[i].PC, stage[i].sReg_a, stage[i].sReg_b);
					if(i > 1)
						printf("(PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n",stage[i].PC, stage[i].sReg_a, stage[i].sReg_b, stage[i].Addr);
          			break;
				
        			case ti_BRANCH:
          			printf("BRANCH:");
					if(i == 0)
						printf("(PC: %x)\n", stage[i].PC);
					if(i == 1)
						printf("(PC: %x)(sReg_a: %d)(sReg_b: %d)\n",stage[i].PC, stage[i].sReg_a, stage[i].sReg_b);
					if(i > 1)
						printf("(PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n",stage[i].PC, stage[i].sReg_a, stage[i].sReg_b, stage[i].Addr);
          			break;
				
        			case ti_JTYPE:
          			printf("JTYPE:");
					if(i == 0)
						printf("(PC: %x)\n", stage[i].PC);
					if(i > 1)
						printf("(PC: %x)(addr: %x)\n", stage[i].PC, stage[i].Addr);
         			break;
				
        			case ti_SPECIAL:
          			printf("SPECIAL: \n");
				break;
				
        			case ti_JRTYPE:
          			printf("JRTYPE:");
					if(i == 0)
						printf("(PC: %x)\n", stage[i].PC);
					if(i > 1)
						printf("(PC: %x)(addr: %x)\n",stage[i].PC, stage[i].Addr);
          			break;
	
			}
		}
	}	
}





int main(int argc, char **argv)
{
  size_t size;
  char *trace_file_name;
  

  if (argc == 1) {
    fprintf(stdout, "\nUSAGE: tv <trace_file> <switch - any character>\n");
    fprintf(stdout, "\n(switch) to turn on or off individual item view.\n\n");
    exit(0);
  }
    
  trace_file_name = argv[1];
  if (argc == 3) trace_view_on = atoi(argv[2]) ;

  fprintf(stdout, "\n ** opening file %s\n", trace_file_name);

  trace_fd = fopen(trace_file_name, "rb");

  if (!trace_fd) {
    fprintf(stdout, "\ntrace file %s not opened.\n\n", trace_file_name);
    exit(0);
  }

  trace_init();

	//simulation loops until there are no more instructions to be fetched
  while(1) {
    	size = trace_get_item(&tr_entry);
   
    	if (!size) {       /* no more instructions (trace_items) to simulate */
      		printf("+ Simulation terminates at cycle : %u\n", cycle_number);
      		break;
    	}

    	else{              /* parse the next instruction to simulate */
        	//move instructions through pipeline each cycle
        	cycle_number++;
		stage[4] = stage[3];
		stage[3] = stage[2];
		stage[2] = stage[1];
		stage[1] = stage[0];	  
		
	
    	    }
	printStages();	
  }

  trace_uninit();

  exit(0);
}


