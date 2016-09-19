
#define DEBUG2 1

typedef struct mailSlot *slotPtr;
typedef struct mailbox   mailbox;
typedef struct mboxProc *mboxProcPtr;
//Mine
typedef struct mboxProc   mboxProc;


struct mailbox {
    int       mboxID;
    // other items as needed...
    mboxProcPtr   waitListHead;
    mboxProcPtr   waitListTail;
    int           maxSlots;
    int           slotSize;
    int           openSlots;
};

struct mailSlot {
    int       mboxID;
    int       status;
    // other items as needed...
    char      message[MAX_MESSAGE];
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
    mboxProcPtr     nextMboxProcPtr;
    int             status;
    int             mboxID;
};