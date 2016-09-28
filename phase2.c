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
#include <stdio.h>
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
void clockHandler2(int dev, void *arg);
void termHandler(int dev, void *arg);
void diskHandler(int dev, void *arg);
void syscallHandler(int dev, void *arg);
void nullsys(systemArgs *args);
void resetBox(int mailboxID);
/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;
int availableSlots = MAXSLOTS;//Dynamic, number of slots not in use
mboxProc procTable2[MAXPROC];
// the mail boxes 
mailbox MailBoxTable[MAXMBOX];

// also need array of mail slots
mailSlot MailSlotTable[MAXSLOTS];
//array of function ptrs to system call handlers, ...
void (*systemCallVec[50])(systemArgs *args);

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
    USLOSS_IntVec[USLOSS_CLOCK_INT] = clockHandler2;
    USLOSS_IntVec[USLOSS_TERM_INT] = termHandler;
    USLOSS_IntVec[USLOSS_DISK_INT] = diskHandler;
    USLOSS_IntVec[USLOSS_SYSCALL_INT] = syscallHandler;
    int i;
    for(i = 0; i < 50; i++)
    {
      systemCallVec[i] = nullsys;
    }

    //allocate mailboxes for interrupt handlers.  Etc... 
    //MboxCreate.....
    MboxCreate(0, 50);//Yeah? 0 slot mailbox so doesnt matter I guess
    MboxCreate(0, 50);//Id 1
    MboxCreate(0, 50);
    MboxCreate(0, 50);
    MboxCreate(0, 50);//Id 4
    MboxCreate(0, 50);//Id 5
    MboxCreate(0, 50);//ID 6

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
  disableInterrupts();
  if(slot_size > MAX_MESSAGE || slots < 0 || slot_size < 0)//openSlots here may not work
  {
    if(DEBUG2 && debugflag2)
      USLOSS_Console("MboxCreate(): Error\n");
    enableInterrupts();
    return -1;
  }
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
      //availableSlots -= slots;//Slots are only considered full when they hold a message
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
    enableInterrupts();
    return -1;
  }
  mailboxPtr box = getMbox(mailboxID);
  mboxProcPtr temp = box->waitListHead;
  /*if(temp == NULL)
  {
    enableInterrupts();
    return 0;
  }*/
  box->mboxID = -1;
  while(temp != NULL)
  {
    temp->status = 13;
    int pid = temp->pid;
    unblockProc(pid);
    temp = temp->nextmboxProcPtr;
  }

  resetBox(mailboxID);
  
  if(isZapped())
  {
    enableInterrupts();
    return -3;
  }
  enableInterrupts();
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
  mailboxPtr box = getMbox(mbox_id);
  if(mbox_id < 0 || mbox_id >= MAXMBOX || msg_size > MAX_MESSAGE || box->slotSize < msg_size
    || box->mboxID < 0)
  {
    if(debugflag2 && DEBUG2)
    {
      USLOSS_Console("Sending Parameter errors\n");
    }
    enableInterrupts();
    return -1;
  }
  
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
    if(proc->messageSize < msg_size)
    {
      proc->messageSize = msg_size;
      removeWaitList(mbox_id);
      unblockProc(proc->pid);
      return -1;
    }
    memcpy(proc->procMessagePtr, msg_ptr, msg_size);
    proc->messageSize = msg_size;
    int pid = box->waitListHead->pid;
    removeWaitList(mbox_id);
    unblockProc(pid);
    return 0;
  }
  if(box->openSlots <= 0)//should prolly change this to just == 0
  {
    setProc(getpid(), mbox_id, msg_ptr, msg_size, 12);
    addWaitList(mbox_id, getpid() %50);
    blockMe(12);
    disableInterrupts();
    if(procTable2[getpid() % 50].status == 13)
    {
      enableInterrupts();
      return -3;
    }
    enableInterrupts();
    return 0;//Weve been unblocked, message should already be in a slot iffy here
  }
  //We blocked because no slots, and are now unblocked so we should get a slot
  //This needs to run if we have an open slot, but not for 0 slot mailboxes 
  //coming off a block.  But also should run if we werent blocked and have
  //an open slot
  if(box->maxSlots > 0)
  {
    assignSlot(mbox_id, msg_ptr, msg_size);
    box->openSlots--;
  }
  

  enableInterrupts();
  return 0;
} /* MboxSend */

int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size)
{
  check_kernel_mode("MboxCondSend\n");
  disableInterrupts();
  mailboxPtr box = getMbox(mbox_id);
  if(mbox_id < 0 || mbox_id >= MAXMBOX || msg_size > MAX_MESSAGE || 
    box->mboxID < 0) //|| ((box->slotSize > msg_size) && box->maxSlots > 0)
  {
    if(DEBUG2 && debugflag2)
    {
      USLOSS_Console("Cond Send Parameter Errors\n");
    }
    enableInterrupts();
    return -1;
  }
  
  
  //0 slot mailbox with recievers blocked
  if(box->waitListHead != NULL && box->waitListHead->status == 11)
  {

  }
  else if(box->openSlots < 1 || availableSlots < 1)
  {
    return -2;
  }
  if(isZapped())
  {
    return -3;
  }

  return MboxSend(mbox_id, msg_ptr, msg_size);//Should be 0 here
}

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
  mailboxPtr box = getMbox(mbox_id);
  if(mbox_id < 0 || mbox_id >= MAXMBOX || msg_size > MAX_MESSAGE || 
    box->mboxID < 0)
  {
    if(debugflag2 && DEBUG2)
    {
      USLOSS_Console("Recieve Parameter errors\n");
    }
    enableInterrupts();
    return -1;
  }

  if(box->slotListHead != NULL)//We have a slot with a message in it???
  {
    slotPtr slot = box->slotListHead;
    if(slot->messageSize > msg_size)
    {
      return -1;
    }
    memcpy(msg_ptr, slot->slotMessage, msg_size);
    actualSize = slot->messageSize;
    freeSlot(mbox_id, slot);
    //cleanProc()?
    if(box->waitListHead != NULL && box->waitListHead->status == 12)//Check for procs send blocked
    {
      int pid = box->waitListHead->pid;
      removeWaitList(mbox_id);
      unblockProc(pid);
      disableInterrupts();
    }
  }
  //0 slot mailbox
  else if(box->slotListHead == NULL && box->waitListHead != NULL && box->waitListHead->status != 11)//Sender blocked on 0 slot mailbox
  {
    if(DEBUG2 && debugflag2)
    {
      USLOSS_Console("Recieiving on 0 slot mailbox with Sender blocked\n");
    }
    int pid = box->waitListHead->pid;
    actualSize = box->waitListHead->messageSize;
    memcpy(msg_ptr, box->waitListHead->procMessagePtr, actualSize);
    removeWaitList(mbox_id);
    unblockProc(pid);
    disableInterrupts();
  }
  else//add to the wait list and get blocked
  {
    setProc(getpid(), mbox_id, msg_ptr, msg_size, 11);
    addWaitList(mbox_id, getpid() %50);
    blockMe(11);
    disableInterrupts();

    if(procTable2[getpid() % 50].messageSize > msg_size)
    {
      cleanUpProc(getpid() %50);
      enableInterrupts();
      return -1;
    }
    actualSize = procTable2[getpid() % 50].messageSize;
  }
  int status = procTable2[getpid() % 50].status;
  cleanUpProc(getpid() % 50);//Our message has been recieved
  enableInterrupts();
  if(status == 13)
      return -3;
  return actualSize;//Actual message size
} /* MboxReceive */

int MboxCondReceive(int mbox_id, void *msg_ptr, int msg_size)
{
  check_kernel_mode("MboxCondReceive\n");
  disableInterrupts();
  mailboxPtr box = getMbox(mbox_id);
  if(mbox_id < 0 || mbox_id >= MAXMBOX || msg_size > MAX_MESSAGE || box->mboxID < 0)
  {
    if(debugflag2 && DEBUG2)
    {
      USLOSS_Console("MboxCondReceive paramter errors\n");
    }
    enableInterrupts();
    return -1;
  }

  if(box->slotListHead == NULL)//No message in slots
  {
    if(box->waitListHead != NULL && box->waitListHead->status == 12)//Send Blocked 0 slot mbox case
    {
      //Sender blocked on 0 slot mailbox
      //Receive the message
      //Probably gonna have to tweak some stuff in recieve
      MboxReceive(mbox_id, msg_ptr, msg_size);
    }
    else
    {
      enableInterrupts();
      return -2;
    }
  }
  if(isZapped())
  {
    enableInterrupts();
    return -3;
  }

  return MboxReceive(mbox_id, msg_ptr, msg_size);
}


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
    MailBoxTable[i].maxSlots = -1;
    MailBoxTable[i].slotSize = -1;
    MailBoxTable[i].openSlots = -1;
  }
}

void resetBox(int mailboxID)
  {
    mailboxPtr box = getMbox(mailboxID);
    box->mboxID = -1;
    box->waitListHead = NULL;
    box->waitListTail = NULL;
    box->slotListHead = NULL;
    box->slotListTail = NULL;
    box->maxSlots = -1;
    box->slotSize = -1;
    box->openSlots = -1;
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
  mboxProcPtr proc = box->waitListHead;
  if(box->waitListHead == box->waitListTail)//if only one on waitlist
  {
    box->waitListHead = box->waitListHead->nextmboxProcPtr;
    box->waitListTail = box->waitListTail->nextmboxProcPtr;
  }
  else
    box->waitListHead = box->waitListHead->nextmboxProcPtr;
  if(box->openSlots > 0)
  {
    assignSlot(mbox_id, proc->procMessagePtr, proc->messageSize);
  }
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
  int i;
  for(i = 0; i < 7; i++)
  {
    if(MailBoxTable[i].waitListHead != NULL)
    {
      if(debugflag2 && DEBUG2)
      {
        USLOSS_Console("Check_io found a process on mailbox %d waiting\n", i);
      }
      return 1;
    }
  }
  return 0;
}

// type = interrupt device type, unit = # of device (when more than one),
// status = where interrupt handler puts device's status register.
int waitDevice(int type, int unit, int *status)
{
  if(DEBUG2 && debugflag2)
  {
    //dumpProcesses();
  }
  switch(type)
  {
    case USLOSS_CLOCK_INT:
      MboxReceive(0, status, 50);
      break;
    case USLOSS_TERM_INT:
      MboxReceive(unit + 1, status, 50);//units are 0-3
      break;
    case USLOSS_DISK_INT:
      MboxReceive(unit + 5, status, 50);//If unit is 0 and 1 this will go to box 5 or 6
      break;
    case USLOSS_SYSCALL_INT:
      USLOSS_Console("SYScall wait device??\n");
      break;
    default:
      USLOSS_Console("DOH!!!\n");
  }
  if(isZapped())
  {
    return -1;
  }
  return 0;
}

/* -------------------------- Interrupt Handlers ------------------------------------- */

void nullsys(systemArgs *args)
{
    USLOSS_Console("nullsys(): Invalid syscall %d. Halting...\n", args->number);
    USLOSS_Halt(1);
} /* nullsys */


void clockHandler2(int dev, void *arg)
{
   if (DEBUG2 && debugflag2)
      USLOSS_Console("clockHandler2(): called\n");
    //dumpProcesses();
    if(dev != USLOSS_CLOCK_INT)
    { 
      USLOSS_Console("Clock Handler called on non clock device\n");
      USLOSS_Halt(1);
    }

    //if(USLOSS_Clock() - readCurStart_time() > 80000)//undefined reference to start time?
      timeSlice();
    static int count = 0;
    count++;
    if(DEBUG2 && debugflag2)
    {
      USLOSS_Console("Clock Count = %d\n", count);
    }
    if(count == 5)
    {
      int status;
      count = 0;
      USLOSS_DeviceInput(dev, 0, &status);
      MboxCondSend(0, &status, sizeof(status));//Box 0
    }

} /* clockHandler */


void diskHandler(int dev, void *arg)//Dev should be 2
{
   if (DEBUG2 && debugflag2)
      USLOSS_Console("diskHandler(): called\n");
    int unit = *((int *) arg);//Yeah this doesnt work
    if(dev != USLOSS_DISK_INT || unit > 2)
    {
      USLOSS_Console("Disk Handler called on non clock device\n");
      USLOSS_Halt(1);
    }
    int status;
    
    USLOSS_DeviceInput(dev, unit, &status);
    if(unit == 0)
      MboxCondSend(5, &status, sizeof(status));//Boxes 5-6
    else
      MboxCondSend(6, &status, sizeof(status));
} /* diskHandler */


void termHandler(int dev, void *arg)//Is arg unit?
{
   if (DEBUG2 && debugflag2)
      USLOSS_Console("termHandler(): called\n");
    long temp = *((long *)arg);//This doesnt work either
    USLOSS_Console("%ld\n", temp);
    if(dev != USLOSS_TERM_INT)
    {
      USLOSS_Console("Terminal Handler called on non clock device\n");
      USLOSS_Halt(1);
    }
    int status;
    
    USLOSS_DeviceInput(dev, 1, &status);//second parameter is unit
    int box;
    switch(1)
    {
      case 0:
        box = 1;
        break;
      case 1:
        box = 2;
        break;
      case 2:
        box = 3;
        break;
      case 3:
        box = 4;
        break;
      default:
        USLOSS_Console("Error bad Unit in Term Handler\n");
    }
    MboxCondSend(box, &status, sizeof(status));//Boxes 1-4*/

} /* termHandler */


void syscallHandler(int dev, void *arg)
{
   if (DEBUG2 && debugflag2)
      USLOSS_Console("syscallHandler(): called\n");
    if(dev != USLOSS_SYSCALL_INT)
    {
      USLOSS_Console("Syscall Handler called on non clock device\n");
      USLOSS_Halt(1);
    }
    //systemArgs sArgs = *((systemArgs*)arg);
    //systemCallVec[sArgs.number].(sArgs);//So how do we invoke the handler?

} /* syscallHandler */
