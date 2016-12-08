#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "queue.h"

#define CHILDNUMBER 10
#define QUANTUM 5
#define READY 1
#define WAIT 0
#define MY_KEY 1027
#define PAGESIZE 4096
#define PERPAGEADDR 64000
#define PTETOTAL 1024
#define FREEPAGESIZE 4096
#define FREEPAGENUM 1024

#define OFFSET_MASK 0xFFF
#define VPN_MASK 0xFFFFF000

#define GET_VPN(virtualaddress) ((virtualaddress & VPN_MASK) >> 12)
#define GET_OFFSET(virtualaddress) ((virtualaddress & OFFSET_MASK))
/* Things to be added


In struct page, there exists 'entry point', called PTE
Using PTE,
	1. We can find Physical address of struct page pointed by Virtual address
	2. Using offset from struct page, we can get physical address of each page. (4k* offset + physical base address)
page table is mapping table that maps struct page, which manages real physical address.
virtual address can be considered as 'ID' to find struct page.










Data abort-- 


*/
struct free_list_structure{
	unsigned int frame_number;
	struct free_list_structure *next;
};


struct page_table_entry{
	unsigned int frame_number;
	int validity;
	unsigned int count;
};
struct page_table_entry pte[CHILDNUMBER][PTETOTAL];
// 			pte[page table index][entry]

struct data_to_return{
	unsigned int page_number;
	unsigned int frame_number;
	unsigned int physical_address;		
};

struct child_p {
	long pid;
	int cpu_burst;
	int io_burst;
//	unsigned int physical_addr;
//	unsigned int virtual_addr;
	
};

/*
struct p_msg_node{
	long m_type;
	long pid;
	unsigned int access_request[10];
};
*/
struct msg_node{
	long m_type;
	long pid;
	int io_msg;
	int p_type;
	unsigned int access_request[10];
};

struct process_parent{
	long pid;
	char done;
	unsigned int virtual;
	int work_time;
	int wait_time;
	int remaining_io_time;
};

//added structure



FILE* fp;
pid_t pid;
int global_time;
key_t key, key2;
int msqid; 
struct msg_node msg;
//struct p_msg_node p_msg;
//new var


struct child_p *CP;
struct sigaction sa;
struct sigaction su;
struct queue *run_queue;
struct queue *wait_queue;
struct queue *rt_queue;
struct process_parent process_list[CHILDNUMBER];

void set_pcb(struct node* Node);
void scheduling();
void proc_generate(int i);
void cpu_b_handler();
void do_child();
void do_parent();
void alarm_handler(int sig);
void enable_time();
int Search_process(long ppid);
void time_sign(int sig);



// functions added

unsigned int MMU(struct free_list_structure *free_list, int index, unsigned int VA);

unsigned int get_offset(unsigned int VA);
unsigned int get_PA(unsigned int page_num, unsigned int offset);
struct free_list_structure *create_new_free(unsigned int num, struct free_list_structure *p_structure);
//struct free_list_structure*
unsigned int remove_free(struct free_list_structure *cur_free);
//



struct free_list_structure *create_new_free(unsigned int num, struct free_list_structure *p_structure){
	struct free_list_structure *new;
	struct free_list_structure *temp;
	if(p_structure==NULL){
		p_structure = (struct free_list_structure *)malloc(sizeof(struct free_list_structure));
		p_structure->frame_number=num;
		p_structure->next=NULL;
		return p_structure;
	}
	else{
		new = (struct free_list_structure *)malloc(sizeof(struct free_list_structure));
		new->frame_number=num;
		new->next=p_structure;
		return new;
	}
}

//struct free_list_structure *remove_free

unsigned int remove_free(struct free_list_structure *free_list){
	unsigned int result;
	struct free_list_structure *temp;
	struct free_list_structure *testing;
	struct free_list_structure *last;
/*        testing = free_list;
        while(testing->next != NULL){
                printf(" %d", testing->frame_number);
                testing= testing->next;
        }	printf("\n");
*/



	temp=free_list;
	while(temp->next != NULL){
		last=temp;
		temp=temp->next;
	}
	result = last->frame_number;
	temp->next=NULL;
	free(last);
	return result;
}

unsigned int MMU(struct free_list_structure *free_list, int index, unsigned int VA){
	unsigned int offset=0;
	unsigned int vpn=0;
	unsigned int physical_address=0;
	unsigned int pfn;
	struct free_list_structure *testing;
	struct free_list_structure *temp;
	struct free_list_structure *temp2;
	
	vpn = GET_VPN(VA);
	offset = GET_OFFSET(VA);
	printf("	INDEX IS :: %x		VPN IS ::: %x\n",index, vpn);
	
	if(pte[index][vpn].validity==1){
		pfn= pte[index][vpn].frame_number;
	}
	else{
		pte[index][vpn].validity=1;
		temp=free_list; 
		free_list= free_list->next;
		pfn= remove_free(temp);
		free(temp);
		temp=NULL;
	}
	//if there is no page fault
	physical_address = ((pfn<<12) | offset);
	return physical_address;
}

void scheduling(){
	struct node *temp;
	
	run_queue->front->pcb->remain_quantum= QUANTUM;
	temp=run_queue->front->next;
	push(rt_queue, pop(run_queue->front->pcb->pid, run_queue));
	if(run_queue->count==0){
	               struct queue *tmp_queue;
	               tmp_queue = run_queue;
	               run_queue = rt_queue;
	               rt_queue = tmp_queue;
	               tmp_queue=NULL;
	}
	else
	{
		run_queue->front = temp;
	}
	
}

void set_pcb(struct node* Node){
	struct process *new_process;
	new_process= (struct process *)malloc(sizeof(struct process));
	Node->pcb = new_process;
	Node->pcb->pid=pid;
	Node->pcb->status = READY;
	Node->pcb->remain_quantum = QUANTUM;
}

void proc_generate(int i){
	process_list[i].pid =0;
	process_list[i].work_time=0;
	process_list[i].wait_time=0;
	process_list[i].remaining_io_time=0;
	process_list[i].virtual=0;
	process_list[i].done=0;
}


int main(){
	int i;
	int process_return;	
	int time_tick=0;
	struct free_list_structure *temp;

	global_time=0;
	fp = fopen("Round_robin_K_J_C.txt","w");	
	



	run_queue= create_queue();
	wait_queue = create_queue();
	rt_queue = create_queue();



	
	for(i=0;i<CHILDNUMBER; i++){
                pid=fork();
                if(pid==-1){
                        exit(1);
                }
                else if(pid==0){        //child proces
			do_child();
	                return 0;
                }
                else{           //parent process
			struct node *tmp = (struct node*)malloc(sizeof(struct node));

			proc_generate(i);
			process_list[i].pid=pid;
			set_pcb(tmp);
			push(run_queue,tmp);
			
			

                }
	}
	enable_time();
	do_parent();

	kill(pid, SIGKILL);

	destroy_queue(wait_queue);
	destroy_queue(run_queue);	
	return 0;
}

void do_child(){
	struct sigaction old1;
	struct sigaction old2;
	sigemptyset(&old1.sa_mask);
	sigemptyset(&old2.sa_mask);
	CP = (struct child_p *)malloc(sizeof(struct child_p));	
	srand(time(NULL));
	CP->pid = getpid();

	CP->cpu_burst = (rand()%20+getpid()%9+6);
	CP->io_burst = (rand()%20+getpid()%5 +4);
	

	memset(&su, 0, sizeof(su));
	su.sa_handler = &alarm_handler;
	sigemptyset(&su.sa_mask);
	sigaction(SIGUSR1, &su, 0);
	
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &cpu_b_handler;

	sigemptyset(&sa.sa_mask);
	sigaction(SIGUSR2, &sa, 0);
	while(1);
	sigaction(SIGUSR1, &old1, NULL);
	sigaction(SIGUSR2, &old2, NULL);

}

void alarm_handler(int sig){
	int i =0;
	int msqid;
	struct node *temp_node;

	key = ftok(".",'B');
	srand(time(NULL));
	
	if(-1==(msqid = msgget(key,IPC_CREAT|0644))){
		perror("msgget() failed");
		exit(1);
	}

			else{
				CP->cpu_burst--;
                       		printf("CPU BURST REMAINING : %d\n", CP->cpu_burst);
            			fprintf(fp, "CPU BURST REMAINING : %d\n", CP->cpu_burst);
	
				
	                        for(i=0;i<10;i++){
					msg.access_request[i]=0;
       	                	        msg.access_request[i]=((rand()+(getpid())+3)%0xFFFF);
       	           	        	printf(":%x", msg.access_request[i]);
       		                }


				printf("\n");
				msg.p_type=1;
				msg.m_type=4;
				msg.pid = getpid();
				msg.io_msg= CP->io_burst;
				printf("		SENDING MESSAGE!!!\n");

                      if(CP->cpu_burst<1){
                                msg.m_type= 4;
                                msg.p_type=0;
                                msg.pid = getpid();
                                msg.io_msg = CP->io_burst;
                        }
	

		i=msgsnd(msqid, &msg, sizeof(struct msg_node), IPC_NOWAIT);
	}
	
}

void cpu_b_handler(int sig){
	srand(time(NULL));
	CP->cpu_burst= (rand()%20*getpid()%9+1);
	CP->io_burst= (rand()%20*getpid()%7+1);
}

void enable_time(){
	struct sigaction sa_tt;
	struct itimerval time;
	
	sigemptyset(&sa_tt.sa_mask);
	memset(&sa_tt, 0, sizeof(sa_tt));
	sa_tt.sa_handler = &time_sign;
	sigaction(SIGALRM, &sa_tt, NULL);
	
	time.it_value.tv_sec= 0;
	time.it_value.tv_usec=100000;
	time.it_interval.tv_sec=0;
	time.it_interval.tv_usec=100000;
	
	setitimer(ITIMER_REAL, &time, NULL);

}

void time_sign(int sig){
	int i; int j; int n; int temp_number=0;
	struct node *temp_node; struct node *temp_node2; struct node *temp_node3; struct node *temp_node4;
	struct node *temp;
	struct node *temp2;
	global_time++;





	printf("-------------------------------------------------\nTime : %d\n", global_time);
	fprintf(fp, "---------------------------------------------------\nTime : %d\n", global_time);
	if(run_queue->count>0){
		temp_node=run_queue->front;
		printf("***RUN QUEUE***\nPID : ");
		fprintf(fp, "***RUN QUEUE***\nPID : ");
		while(run_queue->count!=temp_number){
			printf(" (%ld)", temp_node->pcb->pid);
			fprintf(fp, " (%ld)", temp_node->pcb->pid);
			temp_node= temp_node->next;
			temp_number++;		
		}
		printf("\n***END OF RUN QUEUE***\n");
		fprintf(fp, "\n***END OF RUN QUEUE***\n");
	}
	temp_number=0;
	if(wait_queue->count>0){
		temp_node2=wait_queue->front;
		printf("***WAIT QUEUE***\n PID : ");
		fprintf(fp, "***WAIT QUEUE***\n PID :");
		while(wait_queue->count!=temp_number){
			printf(" (%ld)", temp_node2->pcb->pid);
			fprintf(fp, " (%ld)", temp_node2->pcb->pid);
			temp_node2=temp_node2->next;
			temp_number++;
		}
		printf("\n***END OF WAIT QUEUE***\n");
		fprintf(fp, "\n***END OF WAIT QUEUE***\n");
	}
	temp_number=0;
	if(rt_queue->count>0){
		temp_node4=rt_queue->front;
		printf("***RETIRE QUEUE***\n PID : ");
		fprintf(fp, "***RETIRE QUEUE***\n PID : ");
		while(rt_queue->count!=temp_number){
			printf(" (%ld)", temp_node4->pcb->pid);
			fprintf(fp, " (%ld)", temp_node4->pcb->pid);
			temp_node4=temp_node4->next;
			temp_number++;
		}
		printf("\n***END OF RETIRE QUEUE***\n");
		fprintf(fp, "\n***END OF RETIRE QUEUE***\n");
	}
	temp_number=0;







	if(run_queue->count){
		(run_queue->front->pcb->remain_quantum)--;
		process_list[Search_process(run_queue->front->pcb->pid)].work_time++;
		printf("Process :  %ld running currently\n", run_queue->front->pcb->pid);
		fprintf(fp, "Process :  %ld running currently\n", run_queue->front->pcb->pid);
		
		if(global_time>=10000){
			fprintf(fp, "EXITING PROGRAM!!!!!\n");
			for(i=0;i<CHILDNUMBER;i++){
				kill(process_list[i].pid, SIGKILL);
			}
			kill(getpid(), SIGKILL);
	
		}
		if(run_queue->front->pcb->remain_quantum==0){
			scheduling();
			kill(run_queue->front->pcb->pid, SIGUSR1);
		}
		else{
			kill(run_queue->front->pcb->pid, SIGUSR1);
		}
	}
	if(wait_queue->count>0){
		temp= wait_queue->front;
		for(n=0; n<wait_queue->count;n++){
		//	process_list[Search_process(temp->pcb->pid)].remaining_io_time--;
			temp->pcb->remain_io--;
			process_list[Search_process(temp->pcb->pid)].wait_time++;
			temp2=temp;
			temp=temp->next;
			if(temp2->pcb->remain_io<=0){
				temp2 = pop(temp2->pcb->pid , wait_queue);
				temp2->pcb->status= READY;
				temp2->pcb->remain_quantum=QUANTUM;
				printf("**********IO_BURST DONE **** NODE %ld BACK IN RUN QUEUE !\n", temp->pcb->pid);
				push(run_queue, temp2);
				kill(temp2->pcb->pid, SIGUSR2);
			}
		}
	}
}

void do_parent(){
	long pid_n;
	int i = 0; int j;
	int receiver=-1;
	unsigned int physical_address=0;
	struct free_list_structure *free_list;
	struct free_list_structure *testing;
	struct free_list_structure *curr;









        free_list=(struct free_list_structure *)malloc(sizeof(struct free_list_structure));
        free_list->frame_number=0; free_list->next=NULL;
        for(i=1;i<FREEPAGENUM;i++){
		free_list=create_new_free(i, free_list);
        }
	
	key = ftok(".",'B');
	// IF i need to use separate receiver, then use this/


	struct process *tmp_process;
	struct node *tmp_node;
//	tmp_process = (struct process*)malloc(sizeof(struct process));
	
	


        msqid = msgget(key,IPC_CREAT|0644);
	while(1){
	if(msqid==-1)
	{
                perror("msgget() failed");
                exit(1);
        }
	else{
		receiver = msgrcv(msqid, &msg, sizeof(struct msg_node), 4,0);
		if(receiver ==-1){
		;	
		}
			
//		else if(msg.m_type==3)
		else if(msg.p_type==1)
		{
			for(i=0;i<10;i++){
				physical_address = MMU(free_list, Search_process(msg.pid), msg.access_request[i]);
			}
		}
	
//		else if(msg.m_type==4)
		else if(msg.p_type==0)
		{
//				printf("parent receive pid:[%ld],CPU_burst:%d,IO_burst:%d\n",msg.pid,msg.cpu_burst,msg.io_burst);
			if(-1== (j =Search_process(msg.pid)))
			{
				break;
			}
			pid_n = msg.pid;
			for(i=0;i<9;i++)	printf(" -	-\n");	
			if(run_queue->count){//meaningless. just in case
				printf("Putting runqueue into wait queue\n");
				tmp_node = pop(pid_n, run_queue);
				tmp_node->pcb->remain_io= msg.io_msg;
				tmp_node->pcb->status = WAIT;
				printf("**********CPU_BURST DONE **** PUSHING NODE %ld into WAIT QUEUE !\n", tmp_node->pcb->pid);
				push(wait_queue, tmp_node);
			}
		}

		}
	}
}
				

int Search_process(long ppid){
	int i=0;
	while(i<CHILDNUMBER){
	if(ppid==process_list[i].pid)
	{
		return i;
	}
	i++;
	}
		return -1;
}
