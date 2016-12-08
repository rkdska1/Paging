#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
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


key_t key;

int Mem[10][1024];

typedef struct free_page{
	int pfn;
	struct free_page *next;
}freepage;

typedef struct _page{
	int pfn;
	int valid;
	}page;

typedef struct _L2_table{
	page *pages;
	}L2_table;

typedef struct L1_table{
	L2_table *L2_tables;
	int valid;	
	}L1table;

struct child_p {
	long pid;
	int cpu_burst;
	int io_burst;
};

struct msg_node{
	long m_type;
	int p_type;
	int rw_type[10];
	pid_t pid;
	int io_msg;
	unsigned int virtual[10];
};

struct process_parent{
	pid_t pid;
	int work_time;
	int wait_time;
	int remaining_io_time;
};

L1table L1pgtbl[10][1024];

freepage *freelist;
FILE* fp;
pid_t pid;
int global_time;
struct child_p *CP;
struct sigaction sa;
struct sigaction su;
struct queue *run_queue;
struct queue *wait_queue;
struct queue *rt_queue;
struct process_parent process_list[CHILDNUMBER];

L2_table* L2_table_allocate();
unsigned int Virtual_to_PA(int i,unsigned int VA );
void init_freepage();
freepage *freepage_malloc(freepage *freepage1, int i);
void L1_table_init();
void set_pcb(struct node* Node);
void scheduling();
void proc_generate(int i);
void cpu_b_handler();
void do_child();
void do_parent();
void alarm_handler(int sig);
void enable_time();
void Memory_init();
freepage *freepage_malloc(freepage *freelist1,int i);
int Search_process(long pid);
void time_sign(int sig);
unsigned int remove_free(freepage *freelist);
void scheduling(){
	printf("SCHEDULING START:::::::::::::::::::::::\n\n");
	if(run_queue ->count != 0){
	push(rt_queue, pop(run_queue->front->pcb->pid, run_queue));
	}
	if(run_queue->count==0){
		printf("PLEASE COMEHERE BOY::::::::::::::::\n\n");
		struct queue *tmp_queue=create_queue();
	        run_queue = rt_queue;
	        rt_queue = tmp_queue;
	}
//	push(rt_queue, pop(run_queue->front->pcb->pid, run_queue));
	run_queue->front->pcb->remain_quantum= QUANTUM;
	
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
}


int main(){
	int i;
	int process_return;	
	int time_tick=0;
	global_time=0;
	fp = fopen("Round_robin_K_J_C.txt","w");	
	
	run_queue= create_queue();
	wait_queue = create_queue();
	rt_queue = create_queue();
	init_freepage();
	L1_table_init();
	Memory_init();
	key = ftok(".",'B');
	
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
	int msqid;
	int i =0;
	struct node *temp_node;
	struct msg_node msg;
//	key_t key;
//	key = ftok(".",'B');
	
	if(-1==(msqid = msgget(key,IPC_CREAT|0644))){
		perror("msgget() failed");
		exit(1);
	}
	else{
		if(CP->cpu_burst !=0){
			CP->cpu_burst--;
			printf("\nCPU BURST REMAINING ::::::::::::::::::::::::::::::::::::::::::::: %d\n", CP->cpu_burst);
			fprintf(fp, "CPU BURST REMAINING : %d\n", CP->cpu_burst);
			if(CP->cpu_burst<=0){
				msg.m_type= 4;
				msg.pid = getpid();
				msg.io_msg = CP->io_burst;
				i=msgsnd(msqid, &msg,sizeof(struct msg_node)-sizeof(long),0);
				return ;
			}			
			for(i=0; i<10; i++){
				msg.virtual[i]=0;
				msg.virtual[i]=((rand()+(getpid())+3)%0xFFFFFFFF);
				printf(":%x ",msg.virtual[i]);
				msg.rw_type[i] = rand()%2;
			}
			msg.m_type = 3;
			msg.pid = getpid();
			msg.io_msg = CP->io_burst;
			
			i=msgsnd(msqid, &msg,sizeof(struct msg_node)-sizeof(long),0);
			return ;
			}
		 else{
			msg.m_type= 4;
			msg.pid = getpid();
			msg.io_msg = CP->io_burst;
			i=msgsnd(msqid, &msg,sizeof(struct msg_node)-sizeof(long),0);
			return ;
			}
		
		}
	}


void cpu_b_handler(int sig){
	srand(time(NULL));
	CP->cpu_burst= (rand()%20*getpid()%9+1);
	CP->io_burst=6;	(rand()%20*getpid()%7+1);
}

void enable_time(){
	struct sigaction sa_tt;
	struct itimerval time;
	
	sigemptyset(&sa_tt.sa_mask);
	memset(&sa_tt, 0, sizeof(sa_tt));
	sa_tt.sa_handler = &time_sign;
	sigaction(SIGALRM, &sa_tt, NULL);
	
	time.it_value.tv_sec= 0;
	time.it_value.tv_usec=500000;
	time.it_interval.tv_sec=0;
	time.it_interval.tv_usec=500000;
	
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
/*	if(run_queue->count>0){
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
	temp_number=0;*/
/*	if(wait_queue->count>0){
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
*/




	if(run_queue->count){
		if(global_time>=10000){
			fprintf(fp, "EXITING PROGRAM!!!!!\n");
			for(i=0;i<CHILDNUMBER;i++){
				kill(process_list[i].pid, SIGKILL);
			}
			kill(getpid(), SIGKILL);

		}
		if(run_queue->front->pcb->remain_quantum==0){
			printf("runqueue COUNT %d\n\n",run_queue->count);
			printf("DONTCOMEHEREMAN:::::::::::::\n\n");
			scheduling();
		//	kill(run_queue->front->pcb->pid, SIGUSR1);
		}
		if(run_queue->count){
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
		temp_number = 0 ;
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


		if(run_queue->front->pcb->remain_quantum != 0){
			(run_queue->front->pcb->remain_quantum)--;
			process_list[Search_process(run_queue->front->pcb->pid)].work_time++;
			printf("Process :  %ld running currently\n", run_queue->front->pcb->pid);
			fprintf(fp, "Process :  %ld running currently\n", run_queue->front->pcb->pid);
			kill(run_queue->front->pcb->pid, SIGUSR1);
		}
	if(wait_queue->count>0){
			temp= wait_queue->front;
			for(n=0; n<wait_queue->count;n++){
			temp->pcb->remain_io--;
			process_list[Search_process(temp->pcb->pid)].wait_time++;
			temp2=temp;
			temp=temp->next;
			if(temp2->pcb->remain_io<=0){
				temp2 = pop(temp2->pcb->pid , wait_queue);
				temp2->pcb->status= READY;
				temp2->pcb->remain_quantum=QUANTUM;
				push(run_queue, temp2);
				kill(temp2->pcb->pid, SIGUSR2);
			}
		}
	}
}
	else{
		printf("COME HERE:::::::::::::???\n\n");
		scheduling();
	}
}

void do_parent(){
	int msqid;
	long pid_n;
	int i = 0; int j;
	int receiver;
	int find_pid;
	struct msg_node msg;
	struct process *tmp_process;
	struct node *tmp_node;
	unsigned int P_addr;
//	key_t key;
//	key = ftok(".",'B');
	//	tmp_process = (struct process*)malloc(sizeof(struct process));
	if(-1==(msqid = msgget(key,IPC_CREAT|0644)))
	{
		perror("msgget() failed");
		exit(1);
	}

	while(1){
		receiver = msgrcv(msqid,&msg,sizeof(struct msg_node)-sizeof(long),0,0);
		if(receiver==-1){
			perror("msgrcv");
			continue;
		}
		else{
		find_pid = Search_process(msg.pid);
		if(msg.m_type == 4){
			pid_n = msg.pid;
			if(run_queue->count){
				tmp_node = pop(pid_n, run_queue);
				tmp_node->pcb->remain_io= msg.io_msg;
				tmp_node->pcb->status = WAIT;
				push(wait_queue, tmp_node);
			}
		}
		else if(msg.m_type ==3)
		{
			for(i=0;i<10;i++){
				if(Search_process(msg.pid)==-1){
					exit(1);
						break;
					}
					P_addr= Virtual_to_PA(Search_process(msg.pid),msg.virtual[i]);
					printf("Physical ADDRESS IS :::: %x\n",P_addr);
					if(msg.rw_type[i] ==0){
					//read
					printf("READ DATA : %d\n",Mem[Search_process(msg.pid)][P_addr/4]);
					}
					else if(msg.rw_type[i] == 1){
					Mem[Search_process(msg.pid)][P_addr/4]= 1; //change value
					printf("WRITE DATA :%d\n",Mem[Search_process(msg.pid)][P_addr/4]);
					//write
					}
				}


			}
		}
		
		}
	}


int Search_process(long pid){
	int i=0;
	while(i<CHILDNUMBER){
		if(pid==process_list[i].pid)
		{
			return i;
		}
		i++;
	}
	return -1;
}

void init_freepage(){
	int i;
	freepage *link;
	freepage *temp;
	freepage *temp2;
	link = (freepage*)malloc(sizeof(freepage));
	link->pfn =0;
	freelist =link;
	temp = link;
	
	for(i =0 ; i<100000; i++){
		temp = freepage_malloc(temp,i);
	}
	//	freelist->next = NULL;
}

freepage *freepage_malloc(freepage *freelist1, int i){
	freepage *temp;
	temp = (freepage*)malloc(sizeof(freepage));
	temp->pfn=i;
	freelist1->next = temp;
	temp->next = NULL;
	return temp;
}


L2_table* L2_table_allocate(){
	int i;
	L2_table *L2table = (L2_table*)malloc(sizeof(L2_table));

	page *pages;
	pages = (page*)malloc(sizeof(page)*1024);
	memset(pages,0,(sizeof(pages))*1024);	
	L2table->pages = pages;
	for(i=0; i<1024; i++){
		L2table->pages[i].pfn=i;
	}
	return L2table;
}

unsigned int remove_free(freepage *freelist){
	unsigned int pfn;
	if(freelist == NULL)return -1;
	freepage *temp;
	temp = freelist;
	pfn = temp->pfn;
	temp=temp->next;
	return pfn;
}

void L1_table_init(){
	int i,j,k ;
	for(i=0;i<10;i++){
		for(j=0;j<1024;j++){
			L1pgtbl[i][j].valid =0;
			memset(L1pgtbl,0,sizeof(L1table));
		}
	}
}

unsigned int Virtual_to_PA(int i,unsigned int VA ){
	unsigned int L1index = VA>>22;
	unsigned int L2index = (VA & 0x003ff000)>>12;
	unsigned int offset =(VA & 0x00000fff);
	unsigned int fn=0;
	unsigned int physicaladdr=0;
	unsigned int v_tmp=0;
	int pfn=0;


//	printf("	L1INDEX IS :: %x\n	VPN IS ::: %x\n",L1index,L2index);
	if(L1pgtbl[i][L1index].valid == 0 ){
		L1pgtbl[i][L1index].valid = 1;
		L1pgtbl[i][L1index].L2_tables = L2_table_allocate();
	}
	if(L1pgtbl[i][L1index].L2_tables->pages[L2index].valid == 0){
		v_tmp = remove_free(freelist);
		if(v_tmp == -1)return -1;
		L1pgtbl[i][L1index].L2_tables->pages[L2index].pfn=v_tmp;
		L1pgtbl[i][L1index].L2_tables->pages[L2index].valid = 1;
		fn = L1pgtbl[i][L1index].L2_tables->pages[L2index].pfn;

	}
	else{
		fn = L1pgtbl[i][L1index].L2_tables->pages[L2index].pfn;	
	}
//	printf("offset:::::::::::::::::::::::::::%x\n",offset);



	physicaladdr = ((fn<<12)|(offset));

	return physicaladdr;
}
void Memory_init(){
	int i=0;
	int j=0;
	for(j=0; j<10; j++){
		for(i=0;i<1024;i++){
			Mem[j][i] = i;
		}
	}
}
