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
#include <usloss.h>
#include "kernel.h"

/* ------------------------- Prototypes ----------------------------------- */
int sentinel (char *);
extern int start1 (char *);
void dispatcher(void);
void launch();
static void checkDeadlock();

void addReadyList(procPtr Parent);
void addQuitList(procPtr Parent);
void disableInterrupts();
void clock_handler(int dev, void *arg);
void dumpProcesses();
int getpid();
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
        ProcTable[i].procTime = 0;//Time on processor
      }
    // Initialize the Ready list, etc.
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing the Ready list\n");
    
    //Initializing ReadyLists
    for(i = 0; i < 6; i++)
    {
      ReadyList[i] = NULL;
    }
    //Initializing clock interrupt handler //
    USLOSS_IntVec[USLOSS_CLOCK_INT] = clock_handler;

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

/*
  For clock interrupts, returns void, takes two integer
  arguments.  Checks if the current process has exceeded its time slice.  Call 
  dispatcher if necessary Time Slice is 80 milliseconds.  USLOSS_CLOCK returns 
  time in microseconds. 1,000 microseconds for 1 milisecond.  So the time 
  slice is 80,000 microseconds.
*/
void clock_handler(int dev, void *arg)
{
  if(DEBUG && debugflag)
    USLOSS_Console("Clock Handler called\n");
  int currentTime = USLOSS_Clock();
  int procTime = Current->timeSlice;
  if (DEBUG && debugflag)
    USLOSS_Console("%s proc Time: %d\n", Current->name, currentTime - procTime);
  if(currentTime - procTime >= 80000)
  {
    Current->status = 3;
    dispatcher();//Need to add some functionality to dispatcher now
    //Need to set the time for process going on in dispatcher and check
    //if Current process has exceeded time slice call addReadyList on it
  }
}
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
    disableInterrupts();
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
        While loop that runs 50 times, maxprocs, if the nextPid
        mod 50 fits in the tablle we set proc slot to that value, 
        increment pid, and break out of the loop.  If that slot
        is not open we increment i to move to the next slot and
        move to the next pid.
      */
    int i = 1;
    if (DEBUG && debugflag)
      USLOSS_Console("nextPID: %d\n", nextPid);
    while(i <= MAXPROC)
    {
      if(ProcTable[nextPid % 50].status == 00)
      {
        procSlot = nextPid % 50;
        if (DEBUG && debugflag)
          USLOSS_Console("%s pid: %d\n", name, nextPid);
        ProcTable[procSlot].pid = nextPid++;
        break;
      }
      else
      {
        i++;
        nextPid++;
      }
    }
    if(procSlot == -1)
      return procSlot;
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
    
    
    /*
      Here is where we deal with the children.  If the current process, which
      should be the parent doesnt have a child set the child to the process
      we are making.  If it has a child, set a temp to that child then
      look at the sibling pointer.  If it has a sibling keep advancing 
      down that list of siblings until we find one without a sibling.  
      Set the next available sibling to the process we are making.
    */

    //See if we have a child
    if(Current != NULL)
    {
      
      ProcTable[procSlot].parentProcPtr = &ProcTable[Current->pid % 50];
      //
      if(Current->childProcPtr == NULL)
      {//If not set it here
        //USLOSS_Console("%s, %d", ProcTable[procSlot].name, ProcTable[procSlot].pid);
        Current->childProcPtr = &(ProcTable[procSlot]);
        if (DEBUG && debugflag)
          USLOSS_Console("Setting Child\n");
      }
      else//if we do
      {//Look at that childs sibling
        procPtr temp = Current->childProcPtr;
        while(temp->nextSiblingPtr != NULL)//If we have a sibling
        {
          //We got an infinite loop here some how
          temp = temp->nextSiblingPtr;//Move to the sibling until we dont
        }
        temp->nextSiblingPtr = &(ProcTable[procSlot]);//Set the sibling
      }//End of Sibling else
    }//End of Current Null Check
   
    //USLOSS_Console("Am I making it to here?\n");
    ProcTable[procSlot].priority = priority;//priority
    ProcTable[procSlot].status = 1;//status is it ready(1) at start up?
    
    // Initialize context for this process, but use launch function pointer for
    // the initial value of the process's program counter (PC)

    //USLOSS_Console("Am I making it to here?\n");
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
        USLOSS_Console("%s %d returned to launch\n", Current->name, Current->pid);

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
  disableInterrupts();
  if (DEBUG && debugflag)
        USLOSS_Console("%s called join\n", Current->name);
  int quitPID;
  procPtr child; 
  //Check to see if we have any children
  if(Current->childProcPtr == NULL && Current->quitChildPtr == NULL)
  {
    if(DEBUG && debugflag)
      USLOSS_Console("Error: No Children\n");
    return -2;
  }
  //Check to see if we have any children who have quit
  child = Current->quitChildPtr;
  if(Current->quitChildPtr != NULL)
  {
    if(DEBUG && debugflag)
      USLOSS_Console("Child on quit list: \n");
    //child = Current->quitChildPtr;
    *status = child->quitStatus;
    quitPID = child->pid;
    child->status = 00;//Mark slot as open
    //ProcTable[child->pid % 50].status = 00;//Mark slot as open
    //free(child->stack);
    //I think we can reuse nextProcPtr, once a process quits
    //Its off the table and shouldnt point to the next process
    //on ready list, I think, so instead lets have it point
    //the next quit child, kind of like a quit ready list
    //That of course will all be handled in quit
    child->nextSiblingPtr = NULL;
    Current->quitChildPtr = Current->quitChildPtr->nextProcPtr;
    //Current->childProcPtr = child->nextSiblingPtr;
    return quitPID;
  }
  //No child has quit we have to block
  else
  {
    if(DEBUG && debugflag)
      USLOSS_Console("Join Blocking\n");
    //Pull the parent off
    Current->status = 2;//joinBlock
    //Move off the ready list here
    ReadyList[Current->priority -1] = ReadyList[Current->priority -1]->nextProcPtr;
    //dipatcher needs to check if current is going to run again and just return
    dispatcher();
  } 
  child = Current->quitChildPtr;
  *status = child->quitStatus;
  child->status = 00;
  quitPID = child->pid;
  //free(child->stack);
  if(Current->childProcPtr == child)
  {
    Current->childProcPtr = child->nextSiblingPtr;
  }
  child->nextSiblingPtr = NULL;
  Current->quitChildPtr = child->nextProcPtr;
  return quitPID;  // PID of Child.
} /* join */

//test
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
  disableInterrupts();
    if (DEBUG && debugflag)
        USLOSS_Console("%s called quit\n", Current->name);
  //in Quit take current off ready list
    //Status as dead,gone,empty etc
    //run dispatcher at end of quit
    //Call dispatcher
   
    int index = Current->priority -1;
    ReadyList[index] = ReadyList[index]->nextProcPtr;//Take off ready list
    Current->status = 6;//Mark dead
    Current->quitStatus = status;//Save status for parent
    procPtr parent = Current->parentProcPtr;//Parent pointer
    if(parent == NULL)
    {
      p1_quit(Current->pid);
      Current = NULL;
      dispatcher();
    }
    addQuitList(parent);
    if(Current->parentProcPtr->status == 2)//Parent is join blocked
    {
      addReadyList(parent);
    }
    p1_quit(Current->pid);
    Current = NULL;
    dispatcher();
} /* quit */

void addQuitList(procPtr parent)
{
  procPtr temp = parent->quitChildPtr;//Start of the list
  if(temp == NULL)
  {
    parent->quitChildPtr = Current;
    Current->nextProcPtr = NULL;//Going to use next procPtr instead of a new 1
  }
  else
  {
    while(temp->nextProcPtr != NULL)
    {
      temp = temp->nextProcPtr;
    }
    temp->nextProcPtr = Current;
    Current->nextProcPtr = NULL;
  }
}
void addReadyList(procPtr parent)
{
  int index = parent->priority -1;
  parent->status = 1;//If we're putting it on the ready list status has to be Ready
  procPtr temp = ReadyList[index];
  if(temp == NULL)
  {
    ReadyList[index] = parent;
    parent->nextProcPtr = NULL;
  }
  else
  {
    while(temp->nextProcPtr != NULL)
    {
      temp = temp->nextProcPtr;
    }
    temp->nextProcPtr = parent;
    parent->nextProcPtr = NULL;
  }
  
}

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
    disableInterrupts();
    if (DEBUG && debugflag)
        USLOSS_Console("Dispatcher: started\n");
    procPtr nextProcess = NULL;//next process in ready list
    int i;

    //if(Current->status != 00)
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
        if (DEBUG && debugflag)
          USLOSS_Console("Process found pid: %d\n", nextProcess->pid);
        break;
      }
    }
    USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);//enable interrupts
    /*
    With help from the TA, we came up with this code that got start1 
    switching and running.
    */

    if(Current == nextProcess && nextProcess != NULL)//safety check 
    {
      if (DEBUG && debugflag)
        USLOSS_Console("Current is nextProcess\n");
      if(Current->status == 3)//Time Sliced
      {
        if (DEBUG && debugflag)
          USLOSS_Console("Time Sliced\n");
          addReadyList(Current);
          Current = NULL;
      }
      else//This process is going to keep running
      {
        return;
      }
        
    }
    if(Current == NULL)
    {
      if (DEBUG && debugflag)
        USLOSS_Console("Current is Null setting to nextProcess\n");
      Current = nextProcess;
      Current->timeSlice = USLOSS_Clock();
      //USLOSS_Console("Current pid %d\n", Current->pid);
      p1_switch(0, Current->pid);
      USLOSS_ContextSwitch(NULL, &(Current->state));
    }
    /*
    Despite the vairable names, Current has to be the nextProcess in context
    switch and there has to be another variable holding what is currently
    in current.  These names came with the skeleton.
    */
    else
    {
      if (DEBUG && debugflag)
        USLOSS_Console("Current is not Null\n");
      //USLOSS_Console("Current pid: %d Next pid: %d\n", Current->pid, nextProcess->pid);
      //I think this is the only place we need this line
      Current->procTime += USLOSS_Clock() - Current->timeSlice;//Current time - start time
      procPtr old = Current;//Save old status
      Current = nextProcess;//Make Current the new
      Current->timeSlice = USLOSS_Clock();
      //Since this process is about to be put on the processor
      //Move the ReadyList pointer to the next process
      p1_switch(old->pid, Current->pid);
      USLOSS_ContextSwitch(&(old->state), &(Current->state));
    }
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
  /* checkDeadlock */
  int i;
  for(i = 0; i < 5; i++)
  {
    if(ReadyList[i] != NULL)
    {
      if (DEBUG && debugflag)
        USLOSS_Console("Processes Remain shouldn't be here\n");
      USLOSS_Halt(1);
    }
    else
      USLOSS_Console("All processes completed.\n");
    USLOSS_Halt(0);
  }
}
int getpid()
{
  return Current->pid;
}
void dumpProcesses()
{
  if(DEBUG && debugflag)
    USLOSS_Console("Dumping Processes\n");
}
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
