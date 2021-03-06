#ifndef KERNAL_C
#define KERNAL_C

#include "types.h"

// function to create a process DYNAMICALLY
PROC *kfork()
{
    int i; // will be used for the kstack initialization...
    // get the proc...
    PROC *p = get_proc();
    if (p == 0) return 0; // if there were no procs, report kfork's failure
    
    // initialize the proc...
    p->status = READY; // it must be ready to run...
    p->priority = 1; // it has no particular preference on when to run...
    p->ppid = running->pid; // its parent is the current processor, of course!
    p->parent = running;
    
    // now to setup the kstack!
    // first things first, lets clean up the registers by setting them to 0.
    for (i = 1; i < 10; i++)
        p->kstack[SSIZE - i] = 0;
    p->kstack[SSIZE - 1] = (int)body; // now we need to make sure to call tswitch from body when the proc runs...
    p->ksp = &(p->kstack[SSIZE - 9]); // set the ksp to point to the top of the stack
    
    // enter the proc into the readyQueue, since it's now ready for primetime!
    enqueue(&readyQueue, p);
    
    // return the new proc!!!
    return p;
}

// changes running process status to ZOMBIE,
// then call tswitch() to give up CPU;
bool kzombie()
{
    printf("\nProcess [%d] is now undead!", running->pid);

    running->status = ZOMBIE;
    do_tswitch();
    return true;
}

// go through each process and print info on it
bool kps()
{
    int i;
    bool showmoreinfo = false;
    bool showevent = false;
    bool showexitcode = false;
    PROC *p;
    
    printf("=======================================================================\n");
    printf("   name       status       pid       ppid       event       exitcode\n");
    printf("-----------------------------------------------------------------------\n");
    
    for (i = 0; i < NPROC; i++)
    {   // initialize all procs
        showmoreinfo = true;
        showevent = false;
        showexitcode = false;
        p = &proc[i];
        printf("              ");
        
        // write the status and set the information vars!
        switch (p->status)
        {
            case FREE:
                printf("FREE         ");
                showmoreinfo = false;
                showexitcode = true;
                break;
            case READY:
                printf("READY        ");
                break;
            case RUNNING:
                printf("RUNNING      ");
                break;
            case STOPPED:
                printf("STOPPED      ");
                showexitcode = true;
                break;
            case SLEEP:
                printf("SLEEP        ");
                showevent = true;
                break;
            case ZOMBIE:
                printf("ZOMBIE       ");
                showexitcode = true;
                showevent = true;
                break;
        }
        
        // show pid and ppid?
        if (showmoreinfo == true)
            printf("%d         %d         ", p->pid, p->ppid);
        else
            printf("                    ");
            
        // show event num?
        if (showevent == true)
            printf("%d           ", p->event);
        else
            printf("              ");
        
        // show exit code?
        if (showexitcode == true)
            printf("%d\n", p->exitCode);
        else
            printf("\n");
    }
    printf("-----------------------------------------------------------------------\n");
    return true;
}

// sleep the running process and give it the event!
bool ksleep(int event)
{
    // set running process event to event, set running to sleep,
    //     enqueue it onto the sleep list and then switch procs 
    //     to a running one.
    running->event = event;
    running->status = SLEEP;
    enqueue(&sleepList, running);
    tswitch();
    return true;
}

// wakeup a process!
int kwakeup(int event)
{
    int i, counter = 0;
    PROC *p = sleepList;
    
    // CASE 1: no sleep list...ergo, no processes to wake up!
    if (p == null)
    {
        return -1;
    }
    // CASE 2: sleep list. go through each process in the list and wake up ones with the same event
    else while(p)
    {
        if (p->event == event)
        {
            p = dequeue(&sleepList);
            p->status = READY;
            enqueue(&readyQueue, p);
            counter++;
        }
        else
        {
            p = p->next;
        }
    }
    return counter;
}

// murder teh running process!
int kexit(int exitvalue)
{
    int i;
    // CASE 1: Trying to kill Proc 1
    if (running->pid == DEFAULTPROCESS)
    {
        // we can only kill P1 if it has no children, so lets check if it does
        for (i = DEFAULTPROCESS + 1; i < NPROC; i++)
        {
            if (proc[i].status != FREE && proc[i].ppid == proc[DEFAULTPROCESS].pid)
            {
                return i;
            }
        }
    }
    // DEFAULT CASE: Trying to kill any other process, or P1 has no children
    running->exitCode = exitvalue;
    running->status = ZOMBIE;
    for (i = 0; i < NPROC; i++)
    {
        if (proc[i].ppid == running->pid)
        {
            proc[i].ppid = proc[DEFAULTPROCESS].pid;
        }
    }
    
    kwakeup(running->ppid);
    tswitch();
    return -1;
}

// wait for child procs to die
int kwait(int *status)
{
    int i;
    
    // first lets figure out if the process has any children...
    for (i = 0; i < NPROC; i++)
    {
        if (proc[i].ppid == running->pid) break;
    }
    if (i == NPROC) return -1; // if it has no children, stop before we start
    
    while (1)
    {
        // find a zombie!
        for (i = 0; i < NPROC; i++)
        {
            // if a zombie exists, stop waiting!
            if (proc[i].status == ZOMBIE && proc[i].ppid == running->pid)
            {
                *status = proc[i].exitCode;
                proc[i].status = FREE;
                enqueue(&freeList, proc[i]);
                return proc[i].pid;
            }
        }
        // sleep!
        ksleep(running->pid);
    }
}

// free the running process!
int kfree(int exitvalue)
{
    int i;
    // CASE 1: Trying to free Proc 1
    if (running->pid == DEFAULTPROCESS)
    {
        // we can only free P1 if it has no children, so lets check if it does
        for (i = DEFAULTPROCESS + 1; i < NPROC; i++)
        {
            if (proc[i].status != FREE && proc[i].ppid == proc[DEFAULTPROCESS].pid)
            {
                return i;
            }
        }
    }
    // DEFAULT CASE: Trying to free any other process, or P1 has no children
    running->exitCode = exitvalue;
    running->status = FREE;
    for (i = 0; i < NPROC; i++)
    {
        if (proc[i].ppid == running->pid)
        {
            proc[i].ppid = proc[DEFAULTPROCESS].pid;
        }
    }
    
    // finish freeing this proc!
    kwakeup(running->ppid);
    running->ppid = 0;
    put_proc(running);
    tswitch();
    return -1;
}

// resurrect all of the zombie procs!
int kresurrect()
{
    int i, counter = 0;
    PROC *p;
    for (i = 0; i < NPROC; i++)
    {
        p = &proc[i];
        if (p->status == ZOMBIE)
        {
            p->status = FREE;
            p->ppid = 0;
            put_proc(p);
            counter++;
        }
    }
    return counter;
}

#endif

// FORK NOTES
/****************************************************************
Instead of creating ALL the PROCs at once, write a
           PROC *kfork() 
   function to create a process DYNAMICALLY.

    PROC *kfork()
    {  
       
      (1). PROC *p = get_proc(); to get a FREE PROC from freeList;
                     if none, return 0 for FAIL;

      (2). Initialize the new PROC p with
             --------------------------
             status   = READY;
             priority = 1;
             ppid = running pid;
             parent = running;
            --------------------------

          *********** THIS IS THE MAIN PART OF THE ASSIGNMENT!!!***********
          INITIALIZE p's kstack to make it start from body() when it runs.

          To do this, PRETNED that the process called tswitch() from the 
          the entry address of body() and executed the SAVE part of tswitch()
          to give up CPU before. 
          Initialize its kstack[ ] and ksp to comform to these.
  
          enter p into readyQueue;
          *****************************************************************

          return p;
    }
*****************************************************************/

// WAIT NOTES
/****************************************************************
           int wait(int *status)
           {
              if (no child)
                 return -1 for ERROR;
              while(1){
                 if (found a ZOMBIE child){
                    copy child's exitValue to *status; save its pid;
                    free the ZOMBIE child PROC (enter it into freeList);
                    return dead child's pid;
                 }
                 sleep(running);    // sleep at its own &PROC
              }
            }
*****************************************************************/

// SLEEP NOTES
/****************************************************************
       sleep(int event)
       {
          running->event = event;      //Record event in PROC
          running->status = SLEEP;     // mark itself SLEEP
// For fairness, put running into a FIFO sleepList so that they will wakeup in order
          tswitch();                   // not in readyQueue anymore
       } 
*****************************************************************/

// WAKEUP NOTES
/****************************************************************
       wakeup(int event)
       {
          for every PROC do{
              if (PROC.status==SLEEP && PROC.event==event){
                 // remove p from sleepList if you implement a sleepList
                  p->status = READY;       // make it READY
                 enqueue(&readyQueue, p)   // enter p into readyQueue (by pri)
              }
          }
       }
*****************************************************************/

// ZOMBIE NOTES
/****************************************************************
which changes running process status to ZOMBIE,
                         then call tswitch() to give up CPU;
*****************************************************************/

// PS NOTES
/****************************************************************
write C code to print ALL PROC's pid, ppid, status;
*****************************************************************/

// EXIT NOTES
/****************************************************************
             ask for an exitValue (value), e.g. 12
             kexit(exitValue);
             
                             Algorithm of kexit(value):
       Record value in its PROC.exitValue;
       Give away children (dead or alive) to P1. 
       Make sure P1 does not die if other procs still exist.
       Issue wakeup(parent) to wake up its parent;
             Wake up P1 also if it has sent any children to P1;
       Mark itself a ZOMBIE;
       Call tswitch(); to give up CPU;
     
     When a proc dies, its PROC is marked as ZOMBIE, contains an exit value, but
     the PROC is NOT yet freed. It will be freed by the parent via the wait() 
     operation.

*****************************************************************/

//  NOTES
/****************************************************************
*****************************************************************/
