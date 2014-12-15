/* Tests cetegorical mutual exclusion with different numbers of threads.
 * Automatic checks only catch severe problems like crashes.
 */
#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"
#include "lib/random.h" //generate random numbers

#define BUS_CAPACITY 3
#define SENDER 0
#define RECEIVER 1
#define NORMAL 0
#define HIGH 1

/*
 *	initialize task with direction and priority
 *	call o
 * */
typedef struct {
	int direction;
	int priority;
} task_t;

struct semaphore busslot, sendPrioHigh, receivePrioHigh, sendPrioLow, receivePrioLow, direction;

void batchScheduler(unsigned int num_tasks_send, unsigned int num_task_receive,
        unsigned int num_priority_send, unsigned int num_priority_receive);

void senderTask(void *);
void receiverTask(void *);
void senderPriorityTask(void *);
void receiverPriorityTask(void *);


void oneTask(task_t task);/*Task requires to use the bus and executes methods below*/
	void getSlot(task_t task); /* task tries to use slot on the bus */
	void transferData(task_t task); /* task processes data on the bus either sending or receiving based on the direction*/
	void leaveSlot(task_t task); /* task release the slot */

static unsigned int highPrio_send;
static unsigned int highPrio_receive;
static int busdir;

/* initializes semaphores */ 
void init_bus(void){ 
 
    random_init((unsigned int)123456789); 
    
    //msg("NOT IMPLEMENTED");
    /* FIXME implement */
    sema_init(&busslot,3);
    sema_init(&sendPrioHigh,1);
    sema_init(&receivePrioHigh,1);
    sema_init(&sendPrioLow,1);
    sema_down(&sendPrioLow);
    sema_init(&receivePrioLow,1);
    sema_down(&receivePrioLow);
    sema_init(&direction,1);
    highPrio_send = 0;
    highPrio_receive = 0;
    busdir = -1;
}

/*
 *  Creates a memory bus sub-system  with num_tasks_send + num_priority_send
    *  sending data to the accelerator and num_task_receive + num_priority_receive tasks
 *  reading data/results from the accelerator.
 *
 *  Every task is represented by its own thread. 
 *  Task requires and gets slot on bus system (1)
 *  process data and the bus (2)
 *  Leave the bus (3).
 */

void batchScheduler(unsigned int num_tasks_send, unsigned int num_tasks_receive,
        unsigned int num_priority_send, unsigned int num_priority_receive)
{

    highPrio_send = num_priority_send;
    highPrio_receive = num_priority_receive;

    if (highPrio_send == 0)
      sema_up(&sendPrioLow);
    if (highPrio_receive == 0)
      sema_up(&receivePrioLow);

    unsigned int i;
    for (i = 0; i < num_priority_send; i++)
      thread_create("sendPrioHigh", PRI_MAX, &senderPriorityTask, NULL);

    for (i = 0; i < num_priority_receive; i++)
      thread_create("receivePrioHigh", PRI_MAX, &receiverPriorityTask, NULL);

    for (i = 0; i < num_tasks_send; i++)
      thread_create("sendPrioLow", PRI_MIN, &senderTask, NULL);

    for (i = 0; i < num_tasks_receive; i++)
      thread_create("receicePrioLow", PRI_MIN, &receiverTask, NULL);    
}

/* Normal task,  sending data to the accelerator */
void senderTask(void *aux UNUSED){
        task_t task = {SENDER, NORMAL};
        oneTask(task);
}

/* High priority task, sending data to the accelerator */
void senderPriorityTask(void *aux UNUSED){
        task_t task = {SENDER, HIGH};
        oneTask(task);
}

/* Normal task, reading data from the accelerator */
void receiverTask(void *aux UNUSED){
        task_t task = {RECEIVER, NORMAL};
        oneTask(task);
}

/* High priority task, reading data from the accelerator */
void receiverPriorityTask(void *aux UNUSED){
        task_t task = {RECEIVER, HIGH};
        oneTask(task);
}

/* abstract task execution*/
void oneTask(task_t task) {
  getSlot(task);
  transferData(task);
  leaveSlot(task);
}


/* task tries to get slot on the bus subsystem */
void getSlot(task_t task) 
{
  //msg("NOT IMPLEMENTED");
    /* FIXME implement */
   
    if (busdir == task.direction || busslot.value == 3) {
      if (task.direction == SENDER) {
	if (task.priority == HIGH) {
	  sema_down(&busslot);
	} else {
	  sema_down(&sendPrioLow);
	  sema_down(&busslot);
	  sema_up(&sendPrioLow);
	}
	// set direction if necessary
	if (busdir != task.direction) {
	  sema_down(&direction);
	  busdir = SENDER;
	  sema_up(&direction);
	}
      } else { 
	if (task.priority == HIGH) {
	  sema_down(&busslot);
	} else {
	  sema_down(&receivePrioLow);
	  sema_down(&busslot);
	  sema_up(&receivePrioLow);
	}
	// set direction if necessary
	if (busdir != task.direction) {
	  sema_down(&direction);
	  busdir = RECEIVER;
	  sema_up(&direction);
	}

      }
    }
    
    if (task.priority == HIGH && task.direction == SENDER) {
      sema_down(&sendPrioHigh);
      highPrio_send--;
      sema_up(&sendPrioHigh);
      if (highPrio_send == 0)
	sema_up(&sendPrioLow);
    }
    if (task.priority == HIGH && task.direction == RECEIVER) {
      sema_down(&receivePrioHigh);
      highPrio_receive--;
      sema_up(&receivePrioHigh);
      if (highPrio_receive == 0)
	sema_up(&receivePrioLow);
    }
    
}

/* task processes data on the bus send/receive */
void transferData(task_t task) 
{
  timer_sleep(random_ulong() % 10);
}

/* task releases the slot */
void leaveSlot(task_t task) 
{
    sema_up(&busslot);
}
