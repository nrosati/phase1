/* ------------------------------------------------------------------------
   phase2.c

   University of Arizona
   Computer Science 452

   ------------------------------------------------------------------------ */

#include "phase1.h"
#include "phase2.h"
#include <usloss.h>
#include "message.h"
#include <stdlib.h>
#include <string.h>

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);
extern int start2 (char *);
void check_kernel_mode(char * procName);
void disableInterrupts();
void enableInterrupts();
void initMailBoxTable();
void initProctable2();
void initMailSlots();
void addWaitList(mailbox box, mboxProcPtr toAdd);
void removeWaitList(mailbox box, mboxProcPtr toRemove);
int availableSlotCount();
/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 1;
int availableSlots = MAXSLOTS;//Dynamic, number of slots not in use
mboxProc procTable2[MAXPROC];
// the mail boxes 
mailbox MailBoxTable[MAXMBOX];

// also need array of mail slots
slotPtr MailSlotTable[MAXSLOTS];
//array of function ptrs to system call handlers, ...
void (*sysCallHandlers[MAXSYSCALLS]) (int dev, void * arg);


//

/* -------------------------- Functions ----------------------------------- */

/* ------------------------------------------------------------------------
   Name - start1
   Purpose - Initializes mailboxes and interrupt vector.
             Start the phase2 test process.
   Parameters - one, default arg passed by fork1, not used here.
   Returns - one to indicate normal quit.
   Side Effects - lots since it initializes the phase2 data structures.
   ----------------------------------------------------------------------- */
int start1(char *arg)
{
    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): at beginning\n");

    check_kernel_mode("start1");

    // Disable interrupts
    disableInterrupts();

    // Initialize the mail box table, slots, & other data structures.
    initMailBoxTable();
    initMailSlots();
    initProctable2();

    int pid = getpid() % 50;
    procTable2[pid].pid = pid;
    //More procTable initialization?

    //Initialize USLOSS_IntVec and system call handlers,

    //allocate mailboxes for interrupt handlers.  Etc... 
    //MboxCreate.....
    enableInterrupts();

    // Create a process for start2, then block on a join until start2 quits
    int status;
    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): fork'ing start2 process\n");
    int kid_pid = fork1("start2", start2, NULL, 4 * USLOSS_MIN_STACK, 1);
    if ( join(&status) != kid_pid ) {
        USLOSS_Console("start2(): join returned something other than ");
        USLOSS_Console("start2's pid\n");
    }

    return 1;//1 to indicate normal quit
} /* start1 */


/* ------------------------------------------------------------------------
   Name - MboxCreate
   Purpose - gets a free mailbox from the table of mailboxes and initializes it 
   Parameters - maximum number of slots in the mailbox and the max size of a msg
                sent to the mailbox.
   Returns - -1 to indicate that no mailbox was created, or a value >= 0 as the
             mailbox id.
   Side Effects - initializes one element of the mail box array. 
   ----------------------------------------------------------------------- */
int MboxCreate(int slots, int slot_size)
{
  check_kernel_mode("MboxCreate");
  if(slot_size > MAX_MESSAGE || slots > availableSlots)//openSlots here may not work
  {
    if(DEBUG2 && debugflag2)
      USLOSS_Console("MboxCreate(): Error\n");
    return -1;
  }
  disableInterrupts();
  int i;
  int id = -1;
  for(i = 0; i < MAXMBOX; i++)
  {
    if(MailBoxTable[i].mboxID == -1)
    {
      MailBoxTable[i].mboxID = i;
      MailBoxTable[i].maxSlots = slots;
      MailBoxTable[i].openSlots = slots;
      MailBoxTable[i].slotSize = slot_size;
      id = i;
      availableSlots -= slots;
      break;
    }
  }
  enableInterrupts();
  return id;//-1 if no slot found, mboxId if otherwise
} /* MboxCreate */


/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size)
{
  check_kernel_mode("MboxSend");
  disableInterrupts();

  enableInterrupts();
  return -1;
} /* MboxSend */


/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size)
{
  check_kernel_mode("MboxRecieve");
  disableInterrupts();

  enableInterrupts();
  return -1;
} /* MboxReceive */

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

void enableInterrupts()
{
  if( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0 ) 
    {
      //not in kernel mode
      USLOSS_Console("Kernel Error: Not in kernel mode, may not ");
      USLOSS_Console("disable interrupts\n");
      USLOSS_Halt(1);
    } 
    else
      // We ARE in kernel mode
      USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);//enable interrupts
}/*enableInterrupts*/

void check_kernel_mode(char *procName)
{
  if((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) != 1)
    {
      USLOSS_Console("%s(): called while in user mode. Halting...\n", procName);
      USLOSS_Halt(1);
    }
}

void initMailBoxTable()
{
  int i;
  for(i = 0; i < MAXMBOX; i++)
  {
    MailBoxTable[i].mboxID = -1;
    MailBoxTable[i].waitListHead = NULL;
    MailBoxTable[i].waitListTail = NULL;
  }
}

void initMailSlots()
{
  int i;
  for(i = 0; i < MAXSLOTS; i++)
  {
    MailSlotTable[i]->mboxID = -1;
    MailSlotTable[i]->status = -1;
  }
}

void initProctable2()
{
  int i;
  for(i = 0; i < MAXPROC; i++)
  {
    procTable2[i].pid = -1;
    procTable2[i].nextMboxProcPtr = NULL;
    procTable2[i].status = -1;
    procTable2[i].mboxID = -1;
  }
}

void addWaitList(mailbox box, mboxProcPtr toAdd)
{
  mboxProcPtr head = box.waitListHead;
  mboxProcPtr tail = box.waitListTail;
  if(head == NULL && tail == NULL)
  {
    head = toAdd;
    tail = toAdd;
  }
  else
  {
    tail->nextMboxProcPtr = toAdd;
    tail = toAdd;
  }
}

void removeWaitList(mailbox box, mboxProcPtr toRemove)
{
  mboxProcPtr head = box.waitListHead;
  mboxProcPtr tail = box.waitListTail;
  //first case want to remove the head
  if(head == toRemove)
  {
    head = toRemove->nextMboxProcPtr;//Move head to the next item on list
    if(tail == toRemove)//If tail is also toRemove, only one item on list
    {
      tail = toRemove->nextMboxProcPtr;//Should set this to NULL
    }
  }
  mboxProcPtr temp = head;//Will this work?  Or have to go from the box again?
  while(temp->nextMboxProcPtr != toRemove)
  {
    temp = temp->nextMboxProcPtr;//Walk the list
  }
  if(tail == toRemove)//If we are removing the tail
  {
    tail = temp;//Set it to the proc before toRemove
  }
  temp = temp->nextMboxProcPtr->nextMboxProcPtr;//cut out the proc to remove
}

int availableSlotCount()
{
  int i;
  int count = 0;
  for(i = 0; i < MAXSLOTS; i++)
  {
    if(MailSlotTable[i]->status == -1)
    {
      count++;
    }
  }
  return count;
}

