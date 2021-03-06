#ifndef PIPE_C
#define PIPE_C

#include "type.h"

// creates a pipe and the file descriptors
int kpipe(int pd[2])
{
    OFT *read_ptr = 0, *write_ptr = 0;
    PIPE *pipe_ptr;
    int i = 0, j = 0;
    
    // get empty pipes, if they exist
    read_ptr = get_free_fd();
    read_ptr->refCount = 1;
    write_ptr = get_free_fd();
    pipe_ptr = get_free_pipe();
    while (i < NFD && running->fd[i] != 0) { i++; }
    while (j < NFD && (j == i || running->fd[j] != 0)) { j++; }
    
    // if we don't have a complete set of pipes, call it quits :(
    if (read_ptr == NULL || write_ptr == NULL ||
        pipe_ptr == NULL || i == NFD || j == NFD)
    {
    
        read_ptr->refCount = 0;
        return(-1);
    }
    
    // Assign the pipes!
    running->fd[i] = read_ptr;
    put_word(i, running->uss, pd++);
    running->fd[j] = write_ptr;
    put_word(j, running->uss, pd);
    
    // Initialize the pipe
    pipe_ptr->head = 0;
    pipe_ptr->tail = 0;
    pipe_ptr->data = 0;
    pipe_ptr->room = PSIZE;
    pipe_ptr->nwriter = 1;
    pipe_ptr->nreader = 1;
    pipe_ptr->busy = 1;
    
    // setup read pipe
    read_ptr->mode = READ_PIPE;
    read_ptr->pipe_ptr = pipe_ptr;
    
    // setup write pipe
    write_ptr->mode = WRITE_PIPE;
    write_ptr->refCount++;
    write_ptr->pipe_ptr = pipe_ptr;
    
    // return!
    return(0);
}

// gets a free file descriptor
OFT *get_free_fd()
{
    OFT *ptr = 0;
    int i = 0;
    
    while (i < NFD)
    {
        ptr = &oft[i++];
        if (ptr->refCount == 0)
            return(ptr);
    }
    
    printf("No free fds :(\n");
    return(0);
}

// gets a free pipe
PIPE *get_free_pipe()
{
    PIPE *ptr;
    int i = 0;
    
    while (i < NPIPE)
    {
        ptr = &pipe[i++];
        if (ptr->busy == NULL)
            return(ptr);
    }
    
    printf("No free pipes :(\n");
    return(0);
}

// prints file descriptor information
int pfd()
{
    int i;
    OFT *ptr;
    
    // print running process' opened file descriptors
    printf("Proc %d file descriptors:\n", running->pid);
    printf("-----------------------------------\n");
    printf("  FD | Mode  |   pipe    | refCount\n");
    printf("-----------------------------------\n");
    for(i=0; i<NFD; i++)
    {
        ptr = running->fd[i];
        printf("- %u | ", i);
        if (ptr != 0)
        {
            switch(ptr->mode)
            {
                case(READ_PIPE): printf("READ  | "); break;
                case(WRITE_PIPE): printf("WRITE | "); break;
                default: printf("  | "); break;
            }
            printf("0x%x | %d", ptr->pipe_ptr, ptr->refCount);
        }
        printf("\n");
    }
    printf("-----------------------------------\n");
}

// prints pipe information
int show_pipe(PIPE *ptr)
{
    int i, j = ptr->tail;
    
    printf("Pipe 0x%x\n", ptr);
    printf("Head: %u, Tail: %u, Data: %u, Room: %u\n", ptr->head, ptr->tail, ptr->data, ptr->room);
    printf("Readers: %u, Writers: %u\n", ptr->nreader, ptr->nwriter);
    
    printf("------------ PIPE CONTENETS ------------\n");
    for (i=ptr->data; i>0; i--)
    {
        if (i > PSIZE)
        {
            printf("Invalid pipe address!\n");
            break;
        }
        if (j == PSIZE) j = 0;
        printf("%c", ptr->buf[j++]);
    }
    printf("\n----------------------------------------\n");
    
    return(0);
}

// closes a pipe
int close_pipe(int fd)
{
    OFT *op; PIPE *pp;

    // grab the running procs fd and clear it
    op = running->fd[fd];
    running->fd[fd] = 0;
    pp = op->pipe_ptr;

    // CASE 1: read pipe
    if (op->mode == READ_PIPE)
    {
        // decrement the reader by 1
        pp->nreader--;
        
        // if this is the last reader and we have no more writers free the pipe
        if (--op->refCount == 0 && pp->nwriter <= 0)
        {
            pp->busy = 0;
            return;
        }
        
        // wakeup procs
        kwakeup(&pp->room); 
        return;
    }
    // CASE 2: write pipe
    else if (op->mode == WRITE_PIPE)
    {
        // decrement the writer by 1
        pp->nwriter--;
        
        // if this is the last writer and we have no more readers free the pipe
        if (--op->refCount == 0 && pp->nreader <= 0)
        {
            pp->busy = 0;
            return;
        }
        
        // wakeup procs
        kwakeup(&pp->room); 
        return;
    }
}

// reads from a pipe
int read_pipe(int ifd, char *buf, int n)
{
    PIPE *ptr = running->fd[ifd]->pipe_ptr;
    int nRead = 0;
    
    if (running->fd[ifd] == 0)
    {
        printf("Invalid file handle index(%d)\n", ifd);
        return(-1);
    }
    if (running->fd[ifd]->mode != READ_PIPE)
    {
        printf("Invalid file handle mode: write-only pipe\n", ifd);
        return(-1);
    }
    if (n == 0)
    {
        n = ptr->data;
    }
    
    // read data while asked to
    while (n > 0)
    {
        // if we have data to read...
        if (ptr->data)
        {
            put_byte(ptr->buf[ptr->tail++], running->uss, buf++);
            
            if (ptr->tail == PSIZE) ptr->tail = 0;
            
            ptr->room++;
            ptr->data--;
            nRead++;
            n--;
            kwakeup(&ptr->room);
        }
        // if we have no more data to read...
        else
        {
            if (ptr->nwriter > 0)
            {
                kwakeup(&ptr->room);
                ksleep(&ptr->room);
            }
            else
            {
                printf("No more data or writers, closing pipe!\n", ptr);
                close_pipe(ifd);
                break;
            }
        }
    }
    
    return(nRead);
}

// writes to a pipe
int write_pipe(int ifd, char *buf, int n)
{
    PIPE *ptr = running->fd[ifd]->pipe_ptr;
    char str[1024];
    int i = 0;
    
    if (running->fd[ifd] == 0)
    {
        printf("Invalid file handle index(%d)\n", ifd);
        return(-1);
    }
    if (running->fd[ifd]->mode != WRITE_PIPE)
    {
        printf("Invalid file handle mode: read-only pipe\n", ifd);
        return(-1);
    }
    if (ptr->nreader <= 0)
    {
        printf("Broken pipe: No readers, closing pipe\n");
        close_pipe(ifd);
        return(-1);
    }
    get_str(buf, str, n);
    
    // write to the pipe
    while (n > 0)
    {
        // if we have room to write to...
        if (ptr->room)
        {
            ptr->buf[ptr->head++] = str[i++];
            
            if (ptr->head == PSIZE) ptr->head = 0;
            
            ptr->room--;
            ptr->data++;
            n--;
            kwakeup(&ptr->room);
        }
        // if we're out of writing room
        else
        {
            ksleep(&ptr->room);
        }
    }
    
    return(i);
}

#endif

