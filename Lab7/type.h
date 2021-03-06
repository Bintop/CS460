#ifndef TYPE_H
#define TYPE_H

//////////////////////////////////////// DEFINITIONS ////////////////////////////////////////

typedef unsigned char   u8;
typedef unsigned short u16;
typedef unsigned long  u32;

#define MTXSEG 0x1000

#define NPROC    9
#define SSIZE 1024

#define NULL 0     // defines the value for null.

#define INPUTBUFFER 64

/******* PROC status ********/
#define FREE     0
#define READY    1
#define RUNNING  2
#define STOPPED  3
#define SLEEP    4
#define ZOMBIE   5
#define BLOCK    6

/******* Pipe stuffs ********/
#define READ_PIPE  4
#define WRITE_PIPE 5
#define NOFT       20
#define NFD        10
#define PSIZE      10
#define NPIPE      10

/**************** CONSTANTS ***********************/
#define INBUFLEN    80
#define OUTBUFLEN   80
#define EBUFLEN     10
#define NULLCHAR     0
#define BEEP         7
#define BACKSPACE 0x7F

#define NR_STTY      2    /* number of serial ports */

/* offset from serial ports base */
#define DATA         0   /* Data reg for Rx, Tx   */
#define DIVL         0   /* When used as divisor  */
#define DIVH         1   /* to generate baud rate */
#define IER          1   /* Interrupt Enable reg  */
#define IIR          2   /* Interrupt ID rer      */
#define LCR          3   /* Line Control reg      */
#define MCR          4   /* Modem Control reg     */
#define LSR          5   /* Line Status reg       */
#define MSR          6   /* Modem Status reg      */

/**** The serial terminal data structure ****/

typedef struct Oft {
    int   mode;
    int   refCount;
    struct pipe *pipe_ptr;
} OFT;

typedef struct pipe {
    char  buf[PSIZE];
    int   head, tail, data, room;
    int   nreader, nwriter;
    int   busy;
}PIPE;

// PROC definition
typedef struct proc{
    struct proc *next;
    int    *ksp;               // at offset 2
    int    uss, usp;           // at offsets 4,6
    int    inkmode;
    int    pid;                // add pid for identify the proc
    int    status;             // status = FREE|READY|RUNNING|SLEEP|ZOMBIE    
    int    ppid;               // parent pid
    struct proc *parent;
    int    priority;
    int    event;
    int    exitCode;
    char   name[32];
    
    OFT    *fd[NFD];
    
    int    kstack[SSIZE];      // per proc stack area
}PROC;

struct semaphore{
    int value;
    PROC *queue;
};

struct stty {
    /* input buffer */
    char inbuf[INBUFLEN];
    int inhead, intail;
    struct semaphore inchars, inmutex;

    /* output buffer */
    char outbuf[OUTBUFLEN];
    int outhead, outtail;
    struct semaphore outroom, outmutex;

    int tx_on;

    /* echo buffer */
    char ebuf[EBUFLEN];
    int ehead, etail, e_count;

    /* Control section */
    char echo;   /* echo inputs */
    char ison;   /* on or off */
    char erase, kill, intr, quit, x_on, x_off, eof;

    /* I/O port base address */
    int port;
} stty[NR_STTY];

// Bool definition
typedef enum { false, true } bool;

// COMMAND TABLE
typedef struct command_table{
	char key;
	char *name;
	bool (*f)();
	char *help;
} COMMANDTABLE;

//////////////////////////////////////// VARIABLES ////////////////////////////////////////
PROC proc[NPROC], *running, *freeList, *readyQueue, *sleepList;
int procSize = sizeof(PROC);
int nproc = 0;
int color;
char *pname[] = { "Odin", "Thor", "Freya",  "Baldur", "Tyr", "Loki", "Heimdall", "Frigg", "Njordur" };
PIPE pipe[NPIPE];
OFT oft[NOFT];
char *MODE[] = {"READ_PIPE ", "WRITE_PIPE"};

//////////////////////////////////////// FUNCTIONS ////////////////////////////////////////
////////// int.c //////////
int kcinth();
int kgetpid();
int kpd();
int kchname(char *name);
int kkfork();
int ktswitch();
int kkwait(int *status);
int kkexit(int value);
int kkmode();
int kexec(char* filename);
int ksin(char *y);
int ksout(char *y);

////////// kernal.c //////////
int kfork(char *filename);
bool kcopy(u16 parent, u16 child, u16 size);
u8 get_byte(u16 segment, u16 offset);
int get_word(u16 segment, u16 offset);
int put_byte(u8 byte, u16 segment, u16 offset);
int put_word(u16 word, u16 segment, u16 offset);

////////// t.c //////////
int init();
int scheduler();
int int80h();
int set_vector(u16 vector, u16 handler);

////////// wait.c //////////
int kwait(int *status);
int ksleep(int event);
int kwakeup(int event);
int kexit(int exitvalue);

////////// misc_functions.c //////////
int body();
int help();
int shorthelp();
int get_str(char *str, char *buf, int size);

////////// do_functions.c //////////
int do_kfork();
int do_kforkcustom();
int do_exit();
int do_wait();
int do_switch();
int do_umode();

////////// pipe.c //////////
int show_pipe(PIPE *ptr);
int pfd();
int read_pipe(int ifd, char *buf, int n);
int write_pipe(int ifd, char *buf, int n);
OFT *get_free_fd();
PIPE *get_free_pipe();
int kpipe(int pd[2]);
int close_pipe(int fd);

////////// pv.c //////////
int P(struct semaphore *s);
int V(struct semaphore *s);

////////// serial.c //////////
int bputc(int port, int c);
int bgetc(int port);
int enable_irq(u8 irq_nr);
int sinit();
int s0handler();
int s1handler();
int shandler(int port);
int do_errors();
int do_modem();
enable_tx(struct stty *t);
disable_tx(struct stty *t);
int secho(struct stty *tty, int c);
int do_rx(struct stty *tty);
int sgetc(struct stty *tty);
int sgetline(char *line);
int do_tx(struct stty *tty);
int sputc(struct stty *tty, int c);
int sputline(char *line);
int usgets(char *y);
int oline();
int iline();

////////// helpers.c //////////


//////////////////////////////////////// COMMAND TABLE ////////////////////////////////////////
COMMANDTABLE commands[] = {
	{ 's', "Switch", do_switch, "Switch to the next ready process." },
	{ 'f', "Fork", do_kfork, "Forks a new process from the free processes." },
	{ 'g', "Fork Custom", do_kforkcustom, "Forks a new process from the free processes with a custom image." },
	{ 'q', "Kill", do_exit, "Makes the running PROC die." },
	{ 'p', "PrintProcs", kpd, "Print pid, ppid, and status of all processes." },
	{ 'w', "Wait", do_wait, "Running proc to sleep on an event value." },
	{ 'u', "UMode", do_umode, "Goes into umode." },
	{ '?', "Help", help, "Brings up this help menu!" },
	{0, 0, 0, 0}
};

#endif
