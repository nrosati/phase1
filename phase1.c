/* ------------------------------------------------------------------------
   phase1.c

   University of Arizona
   Computer Science 452
   Fall 2015

   ------------------------------------------------------------------------ */

#include "phase1.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "kernel.h"

/* ------------------------- Prototypes ----------------------------------- */
int sentinel (char *);
extern int start1 (char *);
void dispatcher(void);
void launch();
static void checkDeadlock();


/* -------------------------- Globals ------------------------------------- */

// Patrick's debugging global variable...
int debugflag = 1;

// the process table
procStruct ProcTable[MAXPROC];

// Process lists
static procPtr ReadyList[6];

// current process ID
procPtr Current;
// the next pid to be assigned
unsigned int nextPid = SENTINELPID;


/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes process lists and clock interrupt vector.
             Start up sentinel process and the test process.
   Parameters - none, called by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   ----------------------------------------------------------------------- */
void startup()
{
    int result; // value returned by call to fork1()
    int i;
    // initialize the process table
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing process table, ProcTable[]\n");
      for(i = 0; i < MAXPROC; i++)
      {
        ProcTable[i].status = 00;//Initialized status
      }
    // Initialize the Ready list, etc.
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing the Ready list\n");
    
    /* Ready list can be a single queue with all the priorities or multiple queues one
    for each priority.  I think the multiple quesues with one priority each will be
    easier.  Something he said on Friday made me think that but dont remember what*/
    
    for(i = 0; i < 6; i++)
    {
      ReadyList[i] = NULL;
    }


    // startup a sentinel process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for sentinel\n");
    result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
                    SENTINELPRIORITY);
    if (result < 0) {
        if (DEBUG && debugflag) {
            USLOSS_Console("startup(): fork1 of sentinel returned error, ");
            USLOSS_Console("halting...\n");
        }
        USLOSS_Halt(1);
    }
    
    // start the test process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for start1\n");
    result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
    if (result < 0) {
        USLOSS_Console("startup(): fork1 for start1 returned an error, ");
        USLOSS_Console("halting...\n");
        USLOSS_Halt(1);
    }

    USLOSS_Console("startup(): Should not see this message! ");
    USLOSS_Console("Returned from fork1 call that created start1\n");

    return;
} /* startup */

/* ------------------------------------------------------------------------
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish()
{
    if (DEBUG && debugflag)
        USLOSS_Console("in finish...\n");
} /* finish */

/* ------------------------------------------------------------------------
   Name - fork1
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */
int fork1(char *name, int (*startFunc)(char *), char *arg,
          int stacksize, int priority)
{
    int procSlot = -1;

    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): creating process %s\n", name);

    // test if in kernel mode; halt if in user mode
    if((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) != 1)
      USLOSS_Halt(1);
    // Return if stack size is too small
      if(stacksize < USLOSS_MIN_STACK)
        return -2;
    // find an empty slot in the process table
      /*
        We set i to be <= so we can get one extra iteration in the loop
        that way we can see if i == maxproc then all the spots are full
        and before we check the status of the table we can just return -1
      */
      int i;
     for(i = 0; i <= MAXPROC; i++)
     {
      if(i == MAXPROC)//if i == maxproc no empty slots go ahead and return error
        return -1;
      if(ProcTable[i].status == 00)
      {
        //Insert New Process
        procSlot = i;
        break;
      }
     }//end of procTable loop

    // fill-in entry in process table */
    if ( strlen(name) >= (MAXNAME - 1) ) {
        USLOSS_Console("fork1(): Process name is too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    strcpy(ProcTable[procSlot].name, name);//name
    ProcTable[procSlot].startFunc = startFunc;//start function
    if ( arg == NULL )
        ProcTable[procSlot].startArg[0] = '\0';
    else if ( strlen(arg) >= (MAXARG - 1) ) {
        USLOSS_Console("fork1(): argument too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    else
        strcpy(ProcTable[procSlot].startArg, arg);//arg
    ProcTable[procSlot].stackSize = stacksize;
    ProcTable[procSlot].stack = malloc(stacksize * sizeof(char));
    //gotta walk the ready list and set the previous proc, next proc ptr to this proc

    if(ReadyList[priority -1] == NULL)
    {
      ReadyList[priority -1] = &ProcTable[procSlot];
      ProcTable[procSlot].nextProcPtr = NULL;//Next Proc Ptr
    }
    else
    {
      procPtr temp = ReadyList[priority -1];
      while(temp->nextProcPtr != NULL)
      {
          temp = temp->nextProcPtr;
      }
      temp->nextProcPtr = &ProcTable[procSlot];
      ProcTable[procSlot].nextProcPtr = NULL;
    }
    
    //ProcTable[procSlot].childProcPtr;
    //ProcTable[procSlot].nextSiblingPtr;
    ProcTable[procSlot].pid = nextPid++;
    ProcTable[procSlot].priority = priority;//priority
    ProcTable[procSlot].status = 1;//status is it ready(1) at start up?
    
    // Initialize context for this process, but use launch function pointer for
    // the initial value of the process's program counter (PC)


    USLOSS_ContextInit(&(ProcTable[procSlot].state), USLOSS_PsrGet(),
                       ProcTable[procSlot].stack,
                       ProcTable[procSlot].stackSize,
                       launch);
    

    // More stuff to do here... Dispatcher? Ready list? Launch?
    if(priority != 6)//Disaptcher not called with Sentinel 
    {
      dispatcher();
    }
    

    return ProcTable[procSlot].pid;  // -1 is not correct! Here to prevent warning. should be PID of child
} /* fork1 */

/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void launch()
{
    int result;

    if (DEBUG && debugflag)
        USLOSS_Console("launch(): started\n");

    // Enable interrupts
      //PSRset() set second bit to 1
      USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    // Call the function passed to fork1, and capture its return value
    result = Current->startFunc(Current->startArg);

    if (DEBUG && debugflag)
        USLOSS_Console("Process %d returned to launch\n", Current->pid);

    quit(result);

} /* launch */


/* ------------------------------------------------------------------------
   Name - join
   Purpose - Wait for a child process (if one has been forked) to quit.  If 
             one has already quit, don't wait.
   Parameters - a pointer to an int where the termination code of the 
                quitting process is to be stored.
   Returns - the process id of the quitting child joined on.
             -1 if the process was zapped in the join
             -2 if the process has no children
   Side Effects - If no child process has quit before join is called, the 
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *status)
{
    return -1;  // -1 is not correct! Here to prevent warning.
} /* join */


/* ------------------------------------------------------------------------
   Name - quit
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents child completion code
             list.
   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int status)
{
    p1_quit(Current->pid);
} /* quit */


/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context of the machine is changed

   From an email response I got from Dr. Homer
      The dispatcher will figure out what process to run and then make
      that process to be current.  

      Then in dispatcher set the new value of current, remember the 
      previous value of current .  When you call context switch the first
      argument will be the previous pid and the second argument will be 
      the current pid
   ----------------------------------------------------------------------- */
void dispatcher(void)
{
    procPtr nextProcess = NULL;//next process in ready list
    int i;
    //Look through our priority queues
    for(i = 0; i < 6; i++)
    {
      //If its empty go to the next priority
      if(ReadyList[i] == NULL)
      {
        continue;
      }
      //If its not empty
      else
      {
        //The next process should be pointed to by the ReadyList[i]
        nextProcess = ReadyList[i];
        USLOSS_Console("Process found pid: %d\n", nextProcess->pid);
        //Move to next item on ReadyList
        ReadyList[i] = ReadyList[i]->nextProcPtr;
        break;
      }
    }

    if(Current == NULL)
    {
      USLOSS_Console("Current is Null setting to nextProcess\n");
      Current = nextProcess;
      USLOSS_Console("Current pid %d\n", Current->pid);
      USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
      p1_switch(0, Current->pid);
      USLOSS_ContextSwitch(NULL, &(Current->state));
    }
    else
    {
      USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
      p1_switch(Current->pid, nextProcess->pid);
      USLOSS_ContextSwitch(&Current->state, &(nextProcess->state));
    }
    
    //USLOSS_Console("Updated ReadyList pid: %d\n", ReadyList[i]->pid);
    //First argument is previous, second argument is new
    
} /* dispatcher */


/* ------------------------------------------------------------------------
   Name - sentinel
   Purpose - The purpose of the sentinel routine is two-fold.  One
             responsibility is to keep the system going when all other
             processes are blocked.  The other is to detect and report
             simple deadlock states.
   Parameters - none
   Returns - nothing
   Side Effects -  if system is in deadlock, print appropriate error
                   and halt.
   ----------------------------------------------------------------------- */
int sentinel (char *dummy)
{
    if (DEBUG && debugflag)
        USLOSS_Console("sentinel(): called\n");
    while (1)
    {
        checkDeadlock();
        USLOSS_WaitInt();
    }
} /* sentinel */


/* check to determine if deadlock has occurred... */
static void checkDeadlock()
{
} /* checkDeadlock */


/*
 * Disables the interrupts.
 */
void disableInterrupts()
{
    // turn the interrupts OFF iff we are in kernel mode
    if( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0 ) {
        //not in kernel mode
        USLOSS_Console("Kernel Error: Not in kernel mode, may not ");
        USLOSS_Console("disable interrupts\n");
        USLOSS_Halt(1);
    } else
        // We ARE in kernel mode
        USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_INT );
} /* disableInterrupts */
