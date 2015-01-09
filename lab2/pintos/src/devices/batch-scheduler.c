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


struct semaphore busslot;              
struct semaphore sendPrioLow;          /* Used to indicate, if high priority sender tasks are remaining */
struct semaphore receivePrioLow;       /* Used to indicate, if high priority receiver tasks are remaining */
struct semaphore varHighPrio_send;     /* Used to prevent race-conditions for variable 'highPrio_receive */
struct semaphore varHighPrio_receive;  /* Used to prevent race-conditions for variable 'highPrio_send' */
struct semaphore varBusdir;            /* Used to prevent race-conditions for variable 'busdir' */

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

void setDirection(int);


static unsigned int highPrio_send;      /* Number of high priority sender tasks */
static unsigned int highPrio_receive;   /* Number of high priority receiver tasks */
static int busdir;                      /* Direction of the bus */

/* initializes semaphores */ 
void init_bus(void){ 
 
    random_init((unsigned int)123456789); 

    sema_init(&busslot,BUS_CAPACITY);

    sema_init(&varHighPrio_send,1);
    sema_init(&varHighPrio_receive,1);

    /* The semaphores get downed, because we assume that there will be high priority tasks.
       If there are none, we up the semaphores before we create all threads in the function 
       'batch-scheduler' */
    sema_init(&sendPrioLow,1);
    sema_down(&sendPrioLow);
    sema_init(&receivePrioLow,1);
    sema_down(&receivePrioLow);

    sema_init(&varBusdir,1);

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

    /* If no high priority sender tasks are to be created/remaining, use semaphore 
	 'sendPrioLow' to indicate that */
    if (highPrio_send == 0)
      sema_up(&sendPrioLow);
    /* If no high priority receiver tasks are to be created/remaining, use semaphore 
	 'receivePrioLow' to indicate that */
    if (highPrio_receive == 0)
      sema_up(&receivePrioLow);

    unsigned int i;
    /* Create threads for high priority sender-tasks */
    for (i = 0; i < num_priority_send; i++)
      thread_create("sendPrioHigh", PRI_MAX, &senderPriorityTask, NULL);

    /* Create threads for high priority receiver-tasks */
    for (i = 0; i < num_priority_receive; i++)
      thread_create("receivePrioHigh", PRI_MAX, &receiverPriorityTask, NULL);

    /* Create threads for low priority sender-tasks */
    for (i = 0; i < num_tasks_send; i++)
      thread_create("sendPrioLow", PRI_MIN, &senderTask, NULL);

    /* Create threads for low priority receiver tasks */
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

    /* Need to add here.
       if bus-direction is not right, nothing happens.
       Need to put a thread, that is waiting for the right direction, to wait */
    if (busdir != task.direction && busslot.value < BUS_CAPACITY) {
      /* make task wait until direction is ok
	 probably use a semaphore or 2 to do this */
    }

    /* From here on, it's safe for the task to use the bus,
       because the direction is either right or the bus is unused */
    
    if (task.direction == SENDER) {
      if (task.priority == HIGH) {
	sema_down(&busslot);
      } else {
	sema_down(&sendPrioLow);
	sema_down(&busslot);
	sema_up(&sendPrioLow);
      }
      /* Set the bus-direction to SENDER if it isn't */
      if (busdir != task.direction) {
	setDirection(SENDER);
      }
    } else { 
      if (task.priority == HIGH) {
	sema_down(&busslot);
      } else {
	sema_down(&receivePrioLow);
	sema_down(&busslot);
	sema_up(&receivePrioLow);
      }
      /* Set the bus-direction to RECEIVER if it isn't */
      if (busdir != task.direction) {
	setDirection(RECEIVER);
      }

    }
    

    
    if (task.priority == HIGH && task.direction == SENDER) {
      /* Update the number of remaining high priority sender tasks */
      sema_down(&varHighPrio_send);
      highPrio_send--;
      sema_up(&varHighPrio_send);

      /* If no high priority sender tasks are remaining, use semaphore 
	 'sendPrioLow' to indicate that */
      if (highPrio_send == 0)
	sema_up(&sendPrioLow);
    }
    
    if (task.priority == HIGH && task.direction == RECEIVER) {
      /* Update the number of remaining high priority receiver tasks */
      sema_down(&varHighPrio_receive);
      highPrio_receive--;
      sema_up(&varHighPrio_receive);

      /* If no high priority receiver tasks are remaining, use semaphore 
	 'receivePrioLow' to indicate that */
      if (highPrio_receive == 0)
	sema_up(&receivePrioLow);
    }
    
}

/* task processes data on the bus send/receive */
void transferData(task_t task) 
{
  /* Printing a message makes the tests fail.
     So we simply put a thread to sleep for a random number of ticks */
  timer_sleep(random_ulong() % 10);
}

/* task releases the slot */
void leaveSlot(task_t task) 
{
  sema_up(&busslot);
}

/* Set the direction of the bus */
void setDirection(int dir)
{
  sema_down(&varBusdir);
  busdir = dir;
  sema_up(&varBusdir);
}
