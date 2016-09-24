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
void addWaitList(int mbox_id, int procPID);
void removeWaitList(int mbox_id);
int availableSlotCount();
void assignSlot(int mbox_id, void *msg_ptr, int msg_size);
mailboxPtr getMbox(int mbox_id);
void addSlotList(int mbox_id, slotPtr slot);
void removeSlotList(int mbox_id);
void freeSlot(int mbox_id, slotPtr slot);
void setProc(int procPID, int mbox_id, void *msg_ptr, int size, int status);
void cleanUpProc(int pid);
/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;
int availableSlots = MAXSLOTS;//Dynamic, number of slots not in use
mboxProc procTable2[MAXPROC];
// the mail boxes 
mailbox MailBoxTable[MAXMBOX];

// also need array of mail slots
mailSlot MailSlotTable[MAXSLOTS];
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
  if(slot_size > MAX_MESSAGE)//openSlots here may not work
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

int MboxRelease(int mailboxID)
{
  check_kernel_mode("MboxRelease\n");
  disableInterrupts();
  if(MailBoxTable[mailboxID].mboxID == -1)
  {
    return -1;
  }
  mailboxPtr box = getMbox(mailboxID);
  mboxProcPtr temp = box->waitListHead;
  if(temp == NULL)
  {
    return 0;
  }
  while(temp != NULL)
  {
    temp->status = 13;
    int pid = temp->pid;
    unblockProc(pid);
    temp = temp->nextmboxProcPtr;
  }

  if(isZapped())
  {
    return -3;
  }
  return 0;
}
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
  if(mbox_id < 0 || mbox_id > MAXMBOX || msg_size > MAX_MESSAGE)
  {
    if(debugflag2 && DEBUG2)
    {
      USLOSS_Console("Sending Parameter errors\n");
    }
    return -1;
  }
  mailboxPtr box = getMbox(mbox_id);
  /*Im thinking we may want two wait lists, one for blocked senders
  and one for blocked recievers, at the same time though I feel like
  we should never have both, if we have blocked recievers its because the 
  slots are full so as long as someone calls recieve there will never be a blocked
  reciever.  Conversly if there are blocked recievers, senders should never block.  
  So it seems weird to only have one list buttttt theoritically I think it will work.
  */
  if(box->waitListHead != NULL && box->waitListHead->status == 11)//recieve blocked
  {
    mboxProcPtr proc = box->waitListHead;
    memcpy(proc->procMessagePtr, msg_ptr, msg_size);
    proc->messageSize = msg_size;
    int pid = box->waitListHead->pid;
    removeWaitList(mbox_id);
    unblockProc(pid);
  }
  if(box->openSlots <= 0)
  {
    setProc(getpid(), mbox_id, msg_ptr, msg_size, 12);
    addWaitList(mbox_id, getpid() %50);
    blockMe(12);
    disableInterrupts();
    if(procTable2[getpid() % 50].status == 13)
    {
      return -3;
    }
  }
  assignSlot(mbox_id, msg_ptr, msg_size);
  box->openSlots--;

  enableInterrupts();
  return 0;
} /* MboxSend */

void assignSlot(int mbox_id, void *msg_ptr, int msg_size)
{
  if(availableSlots <= 0)
  {
    if(DEBUG2 && debugflag2)
    {
      USLOSS_Console("Out of Slots\n");
    }
    USLOSS_Halt(1);
  }
  int i;
  for(i = 0; i < MAXSLOTS; i++)
  {
    if(MailSlotTable[i].mboxID == -1)
    {
      MailSlotTable[i].mboxID = mbox_id;
      MailSlotTable[i].messageSize = msg_size;
      memcpy(&MailSlotTable[i].slotMessage, msg_ptr, msg_size);
      availableSlots--;
      addSlotList(mbox_id, &MailSlotTable[i]);
      cleanUpProc(getpid() % 50);//Message is in a slot so we dont need it in proc table?
      return;
    }
  }
}
void addSlotList(int mbox_id, slotPtr slot)
{
  mailboxPtr box = getMbox(mbox_id);
  if(box->slotListHead == NULL && box->slotListTail == NULL)
  {
    box->slotListHead = slot;//want it to be head = slot
    box->slotListTail = slot;
  }
  else
  {
    box->slotListTail->nextSlotPtr = slot;
    box->slotListTail = slot;
  }
}

void removeSlotList(int mbox_id)
{
  mailboxPtr box = getMbox(mbox_id);
  if(box->slotListHead == box->slotListTail)//if only one on waitlist
  {
    box->slotListHead = box->slotListHead->nextSlotPtr;
    box->slotListTail = box->slotListTail->nextSlotPtr;
  }
  else
    box->slotListHead = box->slotListHead->nextSlotPtr;
}

mailboxPtr getMbox(int mbox_id)
{
  return &MailBoxTable[mbox_id];
}
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
  int actualSize;
  if(mbox_id < 0 || mbox_id > MAXMBOX || msg_size > MAX_MESSAGE || msg_size < 0)
  {
    if(debugflag2 && DEBUG2)
    {
      USLOSS_Console("Sending Parameter errors\n");
    }
    return -1;
  }
  mailboxPtr box = getMbox(mbox_id);

  if(box->slotListHead != NULL)//We have a slot with a message in it???
  {
    slotPtr slot = box->slotListHead;
    memcpy(msg_ptr, slot->slotMessage, msg_size);
    actualSize = slot->messageSize;
    freeSlot(mbox_id, slot);
    //cleanProc()?
    if(box->waitListHead != NULL && box->waitListHead->status == 12)//Check for procs send blocked
    {
      int pid = box->waitListHead->pid;
      removeWaitList(mbox_id);
      unblockProc(pid);
    }
  }
  else if(box->slotListHead == NULL && box->waitListHead != NULL)//0 slot mailbox
  {
    if(DEBUG2 && debugflag2)
    {
      USLOSS_Console("Recieiving on 0 slot mailbox\n");
    }
  }
  else
  {
    setProc(getpid(), mbox_id, msg_ptr, msg_size, 11);
    addWaitList(mbox_id, getpid() %50);
    blockMe(11);
    disableInterrupts();
    actualSize = procTable2[getpid() % 50].messageSize;
  }
  int status = procTable2[getpid() % 50].status;
  cleanUpProc(getpid() % 50);//Our message has been recieved
  enableInterrupts();
  if(status == 13)
      return -3;
  return actualSize;//Actual message size
} /* MboxReceive */
void freeSlot(int mbox_id, slotPtr slot)
{
  mailboxPtr box = getMbox(mbox_id);
  slot->mboxID = -1;
  strcpy(slot->slotMessage, "");
  availableSlots++;
  box->openSlots++;
  removeSlotList(mbox_id);
}


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
    MailBoxTable[i].slotListHead = NULL;
    MailBoxTable[i].slotListTail = NULL;
  }
}

void initMailSlots()
{
  int i;
  for(i = 0; i < MAXSLOTS; i++)
  {
    MailSlotTable[i].mboxID = -1;
    MailSlotTable[i].status = -1;
    MailSlotTable[i].messageSize = -1;
    strcpy(MailSlotTable[i].slotMessage, "");
    MailSlotTable[i].nextSlotPtr = NULL;
  }
}

void initProctable2()
{
  int i;
  for(i = 0; i < MAXPROC; i++)
  {
    procTable2[i].pid = -1;
    procTable2[i].nextmboxProcPtr = NULL;
    procTable2[i].status = -1;
    procTable2[i].mboxID = -1;
    procTable2[i].procMessagePtr = NULL;
    procTable2[i].messageSize = -1;
  }
}
//Save the message from this proc in its entry in procTable2
void setProc(int procPID, int mbox_id, void *msg_ptr, int size, int status)
{
  mboxProcPtr proc = &procTable2[procPID % 50];
  proc->pid = procPID % 50;
  proc->mboxID = mbox_id;
  proc->procMessagePtr = msg_ptr;
  proc->messageSize = size;
  proc->status = status;

}

void cleanUpProc(int pid)
{
  mboxProcPtr proc = &procTable2[pid];
  proc->pid  = -1;
  proc->mboxID = -1;
  proc->procMessagePtr = NULL;
  proc->messageSize = -1;
  proc->status = -1;
}
void addWaitList(int mbox_id, int procPID)
{
  mailboxPtr box = getMbox(mbox_id);
  mboxProcPtr proc = &procTable2[procPID % 50];
  if(box->waitListHead == NULL && box->waitListTail == NULL)
  {
    box->waitListHead = proc;
    box->waitListTail = proc;
  }
  else
  {
    box->waitListTail->nextmboxProcPtr = proc;
    box->waitListTail = proc;
  }
}

void removeWaitList(int mbox_id)
{
  mailboxPtr box = getMbox(mbox_id);
  if(box->waitListHead == box->waitListTail)//if only one on waitlist
  {
    box->waitListHead = box->waitListHead->nextmboxProcPtr;
    box->waitListTail = box->waitListTail->nextmboxProcPtr;
  }
  else
    box->waitListHead = box->waitListHead->nextmboxProcPtr;
}

int availableSlotCount()
{
  int i;
  int count = 0;
  for(i = 0; i < MAXSLOTS; i++)
  {
    if(MailSlotTable[i].status == -1)
    {
      count++;
    }
  }
  return count;
}

int check_io()
{//Supposed to see if io mailboxes have anyone waiting
  return 0;//Tmemporary
}