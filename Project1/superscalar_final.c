/**************************************************************/
/* CS/COE 1541				 			
   just compile with gcc -o pipeline pipeline.c			
   and execute using							
   ./pipeline  /afs/cs.pitt.edu/courses/1541/short_traces/sample.tr	  
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
struct trace_item ls_stage[5];		//array to represent load store pipeline
struct trace_item alu_stage[5];		//array to represent alu/branch pipeline
struct trace_item instBuff[2];		//instruction buffer to hold 2 instructions at a time
struct trace_item prevLoad;		//load isntruction from the previous cycle if issued
struct trace_item save_inst;
struct trace_item *tr_entry;
struct trace_item noop;
int trace_view_on = 0;
unsigned int cycle_number;
int save_index = 0;
void add_noop(int buffNum);
int buff_dependence = 0;
int load_dependence = 0;
int prev_flag = 0;
int fetch_again = 0;
int is_branch_predicted = 0;
int branchPredictionMap[128] = {0};

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
	int j = 0;
	printf("\n[Cycle %d] \n", cycle_number);
	printf("Load/Store Pipeline \n");

		/* loop through each stage and print instruction that is currently in that stage */	
		for(i = 0; i < 5; i ++)
		{				
			switch(i)
			{
				case 0:
				printf("IF + ID:  ");
				break;

				case 1:
				printf("REG:  ");
				break;
			
				case 2:
                        	printf("EX:  ");
                        	break;
		
				case 3:
                        	printf("MEM:  ");
                        	break;

				case 4:
                        	printf("WB:  ");
                        	break;
			}

		if (trace_view_on) {/* print the executed instruction if trace_view_on=1 */
      			switch(ls_stage[i].type) {
        			case ti_NOP:
          			printf("NOP: \n");
          			break;
				
        			case ti_RTYPE:
          			printf("RTYPE: ");
				printf("(PC: %x)(sReg_a: %d)(sReg_b: %d)(dReg: %d)\n", ls_stage[i].PC, ls_stage[i].sReg_a, ls_stage[i].sReg_b, ls_stage[i].dReg);
          			break;
				
        			case ti_ITYPE:
          			printf("ITYPE: ");
				printf("(PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n",ls_stage[i].PC, ls_stage[i].sReg_a, ls_stage[i].dReg, ls_stage[i].Addr);
          			break;
				
        			case ti_LOAD:
          			printf("LOAD: ");
					if(i <= 1)
						printf("(PC: %x)(sReg_a: %d)(dReg: %d)\n", ls_stage[i].PC, ls_stage[i].sReg_a, ls_stage[i].dReg);
					if(i > 1)
						printf("(PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", ls_stage[i].PC, ls_stage[i].sReg_a, ls_stage[i].dReg, ls_stage[i].Addr);
					break;
				
        			case ti_STORE:
          			printf("STORE: ");
					if(i <= 1)
						printf("(PC: %x)(sReg_a: %d)(sReg_b: %d)\n", ls_stage[i].PC, ls_stage[i].sReg_a, ls_stage[i].sReg_b);
					if(i > 1)
						printf("(PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", ls_stage[i].PC, ls_stage[i].sReg_a, ls_stage[i].sReg_b, ls_stage[i].Addr);
          			break;
				
        			case ti_BRANCH:
          			printf("BRANCH:");
					if(i <= 1)
						printf("(PC: %x)(sReg_a: %d)(sReg_b: %d)\n", ls_stage[i].PC, ls_stage[i].sReg_a, ls_stage[i].sReg_b);
					if(i > 1)
						printf("(PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", ls_stage[i].PC, ls_stage[i].sReg_a, ls_stage[i].sReg_b, ls_stage[i].Addr);
          			break;
				
        			case ti_JTYPE:
          			printf("JTYPE:");
					if(i <= 0)
						printf("(PC: %x)\n", ls_stage[i].PC);
					if(i > 1)
						printf("(PC: %x)(addr: %x)\n", ls_stage[i].PC, ls_stage[i].Addr);
         			break;
				
        			case ti_SPECIAL:
          			printf("SPECIAL: \n");
					break;
				
        			case ti_JRTYPE:
          			printf("JRTYPE:");
					if(i <= 1)
						printf("(PC: %x)\n", ls_stage[i].PC);
					if(i > 1)
						printf("(PC: %x)(addr: %x)\n", ls_stage[i].PC, ls_stage[i].Addr);
          			break;

          			case ti_SQUASH:
                        printf("[cycle %d]: SQUASH\n", cycle_number);
                        break;

	
			}
		}
}

	printf("ALU/Branch Pipeline \n");
	
	/* loop through each stage and print instruction that is currently in that stage */	
	for(j = 0; j < 5; j ++)
	{				
		switch(j)
		{
			case 0:
			printf("IF + ID:  ");
			break;

			case 1:
			printf("REG:  ");
			break;
			
			case 2:
                        printf("EX:  ");
                        break;
		
			case 3:
                        printf("MEM:  ");
                        break;

			case 4:
                        printf("WB:  ");
                        break;
		}

		if (trace_view_on) {/* print the executed instruction if trace_view_on=1 */
      			switch(alu_stage[j].type) {
        			case ti_NOP:
          			printf("NOP: \n");
          			break;
				
        			case ti_RTYPE:
          			printf("RTYPE: ");
				printf("(PC: %x)(sReg_a: %d)(sReg_b: %d)(dReg: %d)\n", alu_stage[j].PC, alu_stage[j].sReg_a, alu_stage[j].sReg_b, alu_stage[j].dReg);
          			break;
				
        			case ti_ITYPE:
          			printf("ITYPE: ");
				printf("(PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", alu_stage[j].PC, alu_stage[j].sReg_a, alu_stage[j].dReg, alu_stage[j].Addr);
          			break;
				
        			case ti_LOAD:
          			printf("LOAD: ");
					if(j <= 1)
						printf("(PC: %x)(sReg_a: %d)(dReg: %d)\n", alu_stage[j].PC, alu_stage[j].sReg_a, alu_stage[j].dReg);
					if(j > 1)
						printf("(PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", alu_stage[j].PC, alu_stage[j].sReg_a, alu_stage[j].dReg, alu_stage[j].Addr);
					break;
				
        			case ti_STORE:
          			printf("STORE: ");
					if(j <= 1)
						printf("(PC: %x)(sReg_a: %d)(sReg_b: %d)\n", alu_stage[j].PC, alu_stage[j].sReg_a, alu_stage[j].sReg_b);
					if(j > 1)
						printf("(PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", alu_stage[j].PC, alu_stage[j].sReg_a, alu_stage[j].sReg_b, alu_stage[j].Addr);
          			break;
				
        			case ti_BRANCH:
          			printf("BRANCH:");
					if(j <= 1)
						printf("(PC: %x)(sReg_a: %d)(sReg_b: %d)\n", alu_stage[j].PC, alu_stage[j].sReg_a, alu_stage[j].sReg_b);
					if(j > 1)
						printf("(PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", alu_stage[j].PC, alu_stage[j].sReg_a, alu_stage[j].sReg_b, alu_stage[j].Addr);
          			break;
				
        			case ti_JTYPE:
          			printf("JTYPE:");
					if(j <= 1)
						printf("(PC: %x)\n", alu_stage[j].PC);
					if(j > 1)
						printf("(PC: %x)(addr: %x)\n", alu_stage[j].PC, alu_stage[j].Addr);
         			break;
				
        			case ti_SPECIAL:
          			printf("SPECIAL: \n");
				break;
				
        			case ti_JRTYPE:
          			printf("JRTYPE:");
					if(j <= 1)
						printf("(PC: %x)\n", alu_stage[j].PC);
					if(j > 1)
						printf("(PC: %x)(addr: %x)\n", alu_stage[j].PC, alu_stage[j].Addr);
          			break;

          			case ti_SQUASH:
                        printf("[cycle %d]: SQUASH\n", cycle_number);
                        break;
	
			}
		}
	}	
}

//See if the two instructions in the buffer are dependent on one another
int buffer_dependence()
{

	//check to see if there is a dependence when 1st instruction is r-type
	if(instBuff[0].type == 1)
	{
		if((instBuff[1].type == 2) && (instBuff[0].dReg == instBuff[1].sReg_a))
                        return 1;
                if((instBuff[1].type == 1) && (instBuff[0].dReg == instBuff[1].sReg_a || instBuff[0].dReg == instBuff[1].sReg_b))
                        return 1;
                if((instBuff[1].type == 3) && (instBuff[0].dReg == instBuff[1].sReg_a))
                        return 1;
                if((instBuff[1].type == 4) && (instBuff[0].dReg == instBuff[1].sReg_a || instBuff[0].dReg == instBuff[1].sReg_b))
                        return 1;
                if((instBuff[1].type == 5) && (instBuff[0].dReg == instBuff[1].sReg_a || instBuff[0].dReg == instBuff[1].sReg_b))
                        return 1;
		else
			return 0;
	}

	//check to see if there is a dependence when 1st instruction is i-type
	if(instBuff[0].type == 2)
	{
		if((instBuff[1].type == 2) && (instBuff[0].dReg == instBuff[1].sReg_a))
			return 1;
		if((instBuff[1].type == 1) && (instBuff[0].dReg == instBuff[1].sReg_a || instBuff[0].dReg == instBuff[1].sReg_b))
			return 1;
		if((instBuff[1].type == 3) && (instBuff[0].dReg == instBuff[1].sReg_a))
			return 1;
		if((instBuff[1].type == 4) && (instBuff[0].dReg == instBuff[1].sReg_a || instBuff[0].dReg == instBuff[1].sReg_b))
                        return 1;
		if((instBuff[1].type == 5) && (instBuff[0].dReg == instBuff[1].sReg_a || instBuff[0].dReg == instBuff[1].sReg_b))
                        return 1;
		else
			return 0;

	}

	//check to see if there is  dependence when 1st instruction is a Load
	if(instBuff[0].type == 3)
        {
		
                if((instBuff[1].type ==2) && (instBuff[0].dReg == instBuff[1].sReg_a))
                        return 1;
                if((instBuff[1].type == 1) && (instBuff[0].dReg == instBuff[1].sReg_a || instBuff[0].dReg == instBuff[1].sReg_b))
                        return 1;	
                if((instBuff[1].type == 3) && (instBuff[0].dReg == instBuff[1].sReg_a))
                    	return 1;
                if((instBuff[1].type == 4) && (instBuff[0].dReg == instBuff[1].sReg_a || instBuff[0].dReg == instBuff[1].sReg_b))
        		return 1;
                if((instBuff[1].type == 5) && (instBuff[0].dReg == instBuff[1].sReg_a || instBuff[0].dReg == instBuff[1].sReg_b))
                        return 1;
		else
			return 0;

        }
}


//check to see if there is a dependence on a load from a previous cycle
int load_use_dependence(int n)
{
	if(prev_flag == 1)
	{
		//compare r-type
		if(prevLoad.dReg == instBuff[n].sReg_a || prevLoad.dReg == instBuff[n].sReg_b )
			return 1;
		//compare i-type
		if(prevLoad.dReg == instBuff[n].sReg_a)
                	return 1;
		//compare load
		if(prevLoad.dReg == instBuff[n].sReg_a)
                	return 1;
		//compare store
		if(prevLoad.dReg == instBuff[n].sReg_a)
                	return 1;
		//compare branch
		if(prevLoad.dReg == instBuff[n].sReg_a)
                	return 1;
		else
			return 0;
	}

	else
	{
		//dont compare
		
	}
			
	
}

int same_types()
{
	if((instBuff[0].type == 1 ||
	   instBuff[0].type == 2 ||
	   instBuff[0].type == 5 ||
	   instBuff[0].type == 6 ||
	   instBuff[0].type == 7 ||
	   instBuff[0].type == 8)&&
	   (instBuff[1].type == 3 ||
	   instBuff[1].type == 4))
		return 0;	//1 alu/branch and 1 load/store

	if((instBuff[1].type == 1 ||
           instBuff[1].type == 2 ||
           instBuff[1].type == 5 ||
           instBuff[1].type == 6 ||
           instBuff[1].type == 7 ||
           instBuff[1].type == 8)&&
           (instBuff[0].type == 3 ||
           instBuff[0].type == 4))
                return 0;       //1 alu/branch and 1 load/store

	else
		return 1;	//same types
}

void issue_instructions()
{
	if(buff_dependence == 0)
	{
		if(load_dependence == 0)
		{
			int s = same_types();

			//check to see if one instruction is load/store and other is alu/branch
			//If so then issue both instructions
			if(s == 0)
			{ 
				if(instBuff[0].type == 3 || instBuff[0].type == 4)
					ls_stage[1] = instBuff[0];
				if(instBuff[1].type == 1 || instBuff[1].type == 2 || instBuff[1].type == 5 || instBuff[1].type == 6 || instBuff[1].type == 7 || instBuff[1].type == 8)
					alu_stage[1] = instBuff[1];
				if(instBuff[1].type == 3 || instBuff[1].type == 4)
					ls_stage[1] = instBuff[1];
				if(instBuff[0].type == 0 || instBuff[0].type == 1 || instBuff[0].type == 2 || instBuff[0].type == 5 || instBuff[0].type == 6 || instBuff[0].type == 7 || instBuff[0].type == 8)
					alu_stage[1] = instBuff[0];
			}

			else
			{
				//two instructions for 1 pipeline so according to rule 3, issue 2 noops
				add_noop(0);
				add_noop(1);
			}
		}
		//when there is a load/use dependence
		else
		{
			add_noop(0);
			add_noop(1);
			load_dependence = 1;
		}
	}
	//when there is a buffer dependence, issue two noops according to rule 3
	else
	{
		add_noop(0);
                add_noop(1);
		buff_dependence = 0;
	}
	
}

void add_noop(int buffNum)
{
	noop.type = 0;
	noop.sReg_a = 0;
	noop.sReg_b = 0;
	noop.dReg = 0;
	noop.PC = 0;
	noop.Addr = 0;

	//select pipeline to get noop
	if(buffNum == 0)
		ls_stage[1] = noop;
	if(buffNum == 1)
		alu_stage[1] = noop;
}

int mispredicted_branch_this_cycle(){
    if (alu_stage[2].type == ti_BRANCH){
        if (is_branch_predicted == 0) {
            if (alu_stage[2].Addr == alu_stage[1].PC || alu_stage[2].Addr == ls_stage[1].PC) {
                return 1;
            }

        } else if (is_branch_predicted == 1) {
            unsigned int hashable_index = alu_stage[2].Addr; 
            unsigned int bits_4_10 = (hashable_index & 0x7F0) >> 4;

            if (branchPredictionMap[bits_4_10] == 0 && (alu_stage[2].Addr == alu_stage[1].PC || alu_stage[2].Addr == ls_stage[1].PC)){
                branchPredictionMap[bits_4_10] = 1;
                return 1;
            }

            else if (branchPredictionMap[bits_4_10] == 1 && (alu_stage[2].Addr != alu_stage[1].PC && alu_stage[2].Addr != ls_stage[1].PC)){
                branchPredictionMap[bits_4_10] = 0;
                return 1;
            }

            else {
                return 0;
            }            
        }
    }

    else{
        return 0;
    }
}

void perform_two_squash_cycles() {
    for (int i = 0; i < 2; i++) {
    	if (trace_view_on == 1) {
    		printStages();	
    	}
        

        cycle_number++;
        ls_stage[4] = ls_stage[3];
        ls_stage[3] = ls_stage[2];
        ls_stage[2].type = ti_SQUASH;

        alu_stage[4] = alu_stage[3];
        alu_stage[3] = alu_stage[2];
        alu_stage[2].type = ti_SQUASH;
    }
}

int main(int argc, char **argv)
{
  size_t size;
  char *trace_file_name;
  int i = 0;

  if (argc == 1) {
    fprintf(stdout, "\nUSAGE: tv <trace_file> <switch - any character>\n");
    fprintf(stdout, "\n(switch) to turn on or off individual item view.\n\n");
    exit(0);
  }
  
  trace_file_name = argv[1];

  if (argc == 2) {
    is_branch_predicted = 0;
    trace_view_on = 0;
  }
  
  if (argc == 3) {
  	is_branch_predicted = atoi(argv[2]);
  	trace_view_on = 0;
  } 

  if (argc == 4){
    is_branch_predicted = atoi(argv[2]); 
    trace_view_on = atoi(argv[3]);
  } 

  fprintf(stdout, "\n ** opening file %s\n", trace_file_name);

  trace_fd = fopen(trace_file_name, "rb");

  if (!trace_fd) {
    fprintf(stdout, "\ntrace file %s not opened.\n\n", trace_file_name);
    exit(0);
  }

  trace_init();
	//simulation loops until there are no more instructions to be fetched
  while(1){
	
	if(fetch_again == 0)
	{
		size = trace_get_item(&tr_entry);
		ls_stage[0] = *tr_entry;
		instBuff[0] = ls_stage[0];
		size = trace_get_item(&tr_entry);
        alu_stage[0] = *tr_entry;
		instBuff[1] = alu_stage[0];
	}

	//fetch saved instruction from previous cycle
	else
	{
		size = trace_get_item(&tr_entry);
                ls_stage[0] = save_inst;
                instBuff[0] = ls_stage[0];
                size = trace_get_item(&tr_entry);
                alu_stage[0] = *tr_entry;
                instBuff[1] = alu_stage[0];
		fetch_again = 0;
	}

   
    	if (!size) {       /* no more instructions (trace_items) to simulate */
      		printf("+ Simulation terminates at cycle : %u\n", cycle_number);
      		break;
    	}

    	else{              /* parse the next instruction to simulate */
        	//move instructions through pipeline each cycle
		
    	if (mispredicted_branch_this_cycle() == 1) {perform_two_squash_cycles();}

    	if (trace_view_on == 1) {
    		printStages();	
    	}
		
		
		//keep track of load from previous instruction
		if(ls_stage[1].type == 3)
		{
			prevLoad = ls_stage[1];
			prev_flag = 1;
		}

		else
		{
			prevLoad.type = 0;
			prevLoad.sReg_a = 0;
			prevLoad.sReg_b = 0;
			prevLoad.dReg = 0;
			prevLoad.PC = 0;
			prevLoad.Addr = 0;
			prev_flag = 0;
		}
	
        	cycle_number++;
		ls_stage[4] = ls_stage[3];
		ls_stage[3] = ls_stage[2];
		ls_stage[2] = ls_stage[1];
		ls_stage[1].type = 0;

		alu_stage[4] = alu_stage[3];
                alu_stage[3] = alu_stage[2];
                alu_stage[2] = alu_stage[1];	  
		
		//check dependencies
		int b = buffer_dependence();
		if(b == 0)
		{
			 //check load use dependence
			if((load_use_dependence(0) == 0) && (load_use_dependence(1)== 0) )
			{
				//no load use dependence
				issue_instructions();
			}

			//see which instruction has the load dependence
			else
			{
				//see if the first instruction has a load dependence
				if(load_use_dependence(0) == 1)
				{
					if(instBuff[0].type !=3 || instBuff[0].type != 4)
					{
						alu_stage[1] = instBuff[0];
						add_noop(0);
						
					}	
					else
					{
						ls_stage[1] = instBuff[0];
						fetch_again = 1;
						save_inst = instBuff[1];
						add_noop(1);
					}		
				}

				//now check to see if the second one has the dependence
				else
				{
					if(load_use_dependence(1) == 1)
                                	{
						if(instBuff[1].type !=3 || instBuff[1].type != 4)
						{
                                                	alu_stage[1] = instBuff[1];
                                                	add_noop(0);
						}
                                		else
                        	        	{
                	                        	ls_stage[1] = instBuff[1];
        	                                	add_noop(1);
	                                	}
					}
				}
			}
		}
		//when there is a buffer dependence
		else
		{
			if(same_types == 0)
			{
				printf("Buffer dependence between two types\n");
				buff_dependence = 1;
				issue_instructions();
			}

			else
				issue_instructions();
		}					
    	    }	
  }

  trace_uninit();

  exit(0);
}



