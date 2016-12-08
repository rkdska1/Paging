
/*
 * queue.c
 *
 *  Created on: 2011. 4. 23.
 *      Author: Chwang
 */

#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

struct queue* create_queue(){
	struct queue* new_queue=(struct queue*)malloc(sizeof(struct queue));
	new_queue->count=0;
	new_queue->front=NULL;
	return new_queue;
}

void destroy_queue(struct queue* qptr){
	struct node *temp;
	struct process *temp_p;
	while(qptr->count!=0){
		temp = qptr->front;
		temp_p=temp->pcb;
		qptr->front=qptr->front->next;
		free(temp_p);
		free(temp);
	}
	free(qptr);
	printf("Queue Destroy Complete\n");
}



void push(struct queue* Queue,struct node *proc){
	//Create a new node
	struct node *temp;
	int i;
		
	if(Queue->count==0){
		Queue->front = proc;
		proc->next= proc;
	}
	else if(Queue->count==1){
		Queue->front->next = proc;
		proc->next= Queue->front;
	}
	else {
		temp=Queue->front;
		for(i=1;i<Queue->count;i++){
			temp=temp->next;
		}
		temp->next= proc;
		proc->next= Queue->front;
	}	
	Queue->count++;	
}






struct node *pop(long pid, struct queue *Queue){
	struct node *temp;
	struct node *prev;
	temp= Queue->front;
	prev=temp;
	if(Queue->count==0){
		printf("ERROR. NOTHING TO POP\n");
		exit(1);
	}
	else if(Queue->count ==1){
		temp= Queue->front;
		Queue->front = NULL;
		Queue->count--;
		temp->next = NULL;
		return temp;
	}
	
	while (temp->next != Queue->front) {
		if (temp->pcb->pid == pid) {
			// found!!
			Queue->count--;

			if (temp == Queue->front) {
				prev = Queue->front;
				while(prev->next != Queue->front){
					prev=prev->next;
				}
				prev->next= temp->next;
				Queue->front = temp->next;
				temp->next = NULL;
				return temp;
			}
			prev->next = temp->next;
			temp->next = NULL;
			return temp;
		}
		prev = temp;
		temp = temp->next;
	}

	if (temp->pcb->pid == pid) {
		Queue->count--;
		prev->next = temp->next;
		temp->next = NULL;
		return temp;
	}

}

int isEmpty(struct queue *q){
	if(q->count!=0){
		return 1;
	}
	else
		return 0;

}

struct node *find_node(long pid, struct queue *Queue){
	struct node *tmp;
	tmp= Queue->front;
	do{
		if(tmp->pcb->pid==pid){
			return tmp;
		}
		else{
			tmp=tmp->next;
		}


	}while(tmp->next != Queue->front );
	return NULL;
}

