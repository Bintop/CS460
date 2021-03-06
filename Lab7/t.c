#ifndef T_C
#define T_C

#include "type.h"

/**************************************************
  bio.o, queue.o loader.o are in mtxlib
**************************************************/
/* #include "bio.c" */
/* #include "queue.c" */
/* #include "loader.c" */

int scheduler()
{
    if (running->status == READY)
    {
        running->status = READY;
        enqueue(&readyQueue, running);
    }
    running = dequeue(&readyQueue);
    running->status = RUNNING;
    color = 0x000A + (running->pid % 6);
}

int int80h(), s0inth(), s1inth();

int set_vector(u16 vector, u16 handler)
{
  // put_word(word, segment, offset) in mtxlib
     put_word(handler, 0, vector<<2);
     put_word(0x1000,  0,(vector<<2) + 2);
}
            
main()
{
    printf("\nMTX starts in main()\n");
    init();      // initialize and create P0 as running
    set_vector(80, int80h);

    kfork("/bin/u1");     // P0 kfork() P1

    set_vector(12, s0inth);
    sinit();

    while(1)
    {
        printf("P0 running\n");
        
        if (nproc == 2 && proc[1].status != READY)
        {
            printf("No available processes, system halts!\n");
            break;
        }
        
        while(!readyQueue);
        printf("P0 switch process\n\n");
        ktswitch();         // P0 switch to run P1
    }
    printf("all dead, happy ending!\n");
}

#endif
