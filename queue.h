/*
 * queue.h
 *
 *  Created on: 2011. 4. 23.
 *      Author: Chwang
 */

#ifndef QUEUE_H_
#define QUEUE_H_
#define QUEUESIZE 3

struct process {
        long pid;
	int status;
	int remain_quantum;
	int remain_io;
};

struct node{
	struct process *pcb;
	struct node *next;
};

struct queue{
	int count;
	struct node* front;
};

struct queue* create_queue(void);
void destroy_queue(struct queue* qptr);
void add_rear(struct queue* qptr, struct process *proc );
void push(struct queue *queue, struct node *proc);
struct node *pop(long pid, struct queue *Queue);
int remove_front(struct queue*);
int isEmpty(struct queue *q);
struct node *find_node(long pid, struct queue* Queue);



#endif /* QUEUE_H_ */
