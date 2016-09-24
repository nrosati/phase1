
#define DEBUG2 1

typedef struct mailSlot *slotPtr;
typedef struct mailbox   mailbox;
typedef struct mboxProc *mboxProcPtr;
//Mine
typedef struct mboxProc   mboxProc;
typedef struct mailSlot   mailSlot;
typedef struct mailbox   *mailboxPtr;


struct mailbox {
    int       mboxID;
    // other items as needed...
    mboxProcPtr   waitListHead;
    mboxProcPtr   waitListTail;
    int           maxSlots;//Number of slots mailbox can have
    int           slotSize;//Max size of message
    int           openSlots;//Number of slots mailbox has available
    slotPtr       slotListHead;
    slotPtr       slotListTail;
};

struct mailSlot {
    int       mboxID;
    int       status;
    // other items as needed...
    char      slotMessage[MAX_MESSAGE];
    slotPtr   nextSlotPtr;
    int       messageSize;
};

struct psrBits {
    unsigned int curMode:1;
    unsigned int curIntEnable:1;
    unsigned int prevMode:1;
    unsigned int prevIntEnable:1;
    unsigned int unused:28;
};

union psrValues {
    struct psrBits bits;
    unsigned int integerPart;
};

/**********MINE*************/
struct mboxProc {
    int             pid;
    mboxProcPtr     nextmboxProcPtr;
    int             status;
    int             mboxID;
    void            *procMessagePtr;
    int             messageSize;
};