/* CS4760 Project 4: Process Scheduling
 * Author: Daniel Eggers
 * Instructor: Mark Hauschild
 * Date: November 5 , 2020 
 */

#include <iostream>
//#include <getopt.h>   //for cmdl parsing
//#include <stdlib.h>   //for atoi
//#include <cstdio>     //for printf
#include <string>       //for cpp strings
#include <string.h>     //for strcpy
#include <unistd.h>     //for fork & exec
#include <sys/wait.h>   //for wait
#include <sys/msg.h>    //for message queue
//#include <errno.h>    //for ENOMSG, etc
#include <sys/shm.h>    //for shared memory
#include <cstdlib>      //for exit() function
//#include <cstring>
#include <fstream>      //for logging
#include <bitset>       //for bitvector
#include "pcbstruct.h"

#define PERM (0666)     //permissions for shmflg
#define IPC_ERR (-1)    //return value for failed shm functions
using namespace std;

/* GLOBALS */
static int logcount;                    //counter for number of lines written to log
static bitset<MAX_PCBS> pctracker;      //process control tracker (bitvector)
static const int SECTONS = 1000000000;  //conversion: no. of nanoseconds in a second
const char *LOGFILE = "log.txt";        //specify name of logfile
volatile sig_atomic_t gSTOP = 0;        //signal handling: to declare program should stop running
volatile sig_atomic_t gSHM = 0;         //signal handling: flag that shared memory exists (pct)
volatile sig_atomic_t gSHM2 = 0;        //signal handling: flag that shared memory exists (clock)
volatile sig_atomic_t gMSQ = 0;         //signal handling: flag that message queue exists

/* PROTOTYPES */
void writeLog(const string &message, bool append); 
int fexe();
void terminateSelf(int _shmid, int shmid2, int _msqid);
void interruptHandler(int sig);
//void cleanSHM();
//void cleanMSQ();
//void cleanAll();
myclock_t getRandomInterval(const int max_sec, const long max_ns); 
myclock_t addTimeToClock(myclock_t timeval, myclock_t current);

int main() {
  signal(SIGINT, interruptHandler);
  srand(time(NULL));
  writeLog("oss: initialized", false); //init logfile (clear it out)

  //shared memory var
  key_t shmkey, shmkey2;        //keys used in shmget
  int shmid, shmid2;            //ids returned by shmget for pct and clock, respectively
  pcb_t *pctable;               //array of structs, process table containing each PCB 
  myclock_t *clocksim;          //holds two values: sec and nanosec (shared)
  shmkey = 99841;               //for process control table
  shmkey2 = 99842;              //for shared clock
  int msqid;                    //id returned by msgget
  key_t msqkey = 99843;         //message queue key
  mymsg_t mymsg;                //holds message type and message data
  
  if(gSTOP == 1) {exit(1);}

  //allocate shared memory - PCT
  shmid = shmget(shmkey, SHM_SIZE, PERM | IPC_CREAT | IPC_EXCL);
  if (shmid == IPC_ERR) { perror("shmget PCT"); exit(1); }
  else { gSHM = 1; writeLog("oss: allocated shared memory for process control table", true); }
  pctable = (pcb_t *)shmat(shmid, NULL, 0);
  if (pctable == (void *)IPC_ERR) { perror("shmat PCT"); terminateSelf(shmid, -1, -1); }
  else { writeLog("oss: attached to shared memory for process control table", true); }

  if(gSTOP == 1) {terminateSelf(shmid, -1, -1);}

  //allocate shared memory - clock
  shmid2 = shmget(shmkey2, sizeof(myclock_t), PERM | IPC_CREAT | IPC_EXCL);
  if (shmid2 == IPC_ERR) { perror("shmget clock"); terminateSelf(shmid, -1, -1); }
  else { gSHM2 = 1; writeLog("oss: allocated shared memory for simulated clock", true); }
  clocksim = (myclock_t *)shmat(shmid2, NULL, 0);
  if (clocksim == (void *)IPC_ERR) { perror("shmat clock"); terminateSelf(shmid, shmid2, -1); }
  else { writeLog("oss: attached to shared memory for simulated clock", true); }
  
  if(gSTOP == 1) { terminateSelf(shmid, shmid2, -1); }

  //setup the message queue
  msqid = msgget(msqkey, PERM | IPC_CREAT | IPC_EXCL);
  if (msqid == IPC_ERR) { perror("msgget"); terminateSelf(shmid, shmid2, -1); }
  else { gMSQ = 1; writeLog("oss: created message queue", true); }

  if(gSTOP == 1) {terminateSelf(shmid, shmid2, msqid);}

  /*************************************************************************/

  
  //processing loop (scheduling, etc.)
  const int maxIntervalSC = 2;                  //max interval to generate new proc
  const int maxIntervalNS = SECTONS / 2;        //max interval to generate new proc
  const int maxProcessesToGenerate = 10;        //should be 100 according to Project outline
  int totalProcessesGenerated = 0;              //running total of fork-exec'd processes 
  myclock_t interval;                           //interval between processes (random) 
    interval.seconds = 0;
    interval.nanoseconds = 0;
  myclock_t localClocksim;                      //clocksim local to program, not yet in shm
    localClocksim.seconds = 0;
    localClocksim.nanoseconds = 0;
  int pcbnum = 0;                               //pctable[] index
  writeLog("oss: initialized process loop variables", true);
  
  if (totalProcessesGenerated > maxProcessesToGenerate) {
    writeLog("oss: error: running process total is initialized higher than max",true);
    terminateSelf(shmid, shmid2, msqid);
  }
  
  //launching the first processes...
  interval = getRandomInterval(maxIntervalSC, maxIntervalNS);
  localClocksim = addTimeToClock(interval, localClocksim);
  
  if(gSTOP == 1) {terminateSelf(shmid, shmid2, msqid);}

  //lets just put something in the pctable to test
  pid_t childpid = fexe();
  writeLog("oss: Generated process with PID " + to_string(childpid) + " at time " 
           + to_string(clocksim->seconds) + ":" + to_string(clocksim->nanoseconds), true);
  pctable[pcbnum].local_simulated_pid = childpid;
  pctracker.set(pcbnum);
  
  mymsg.mtype = childpid; 
  mymsg.mpcb = pcbnum;
  if ( msgsnd(msqid, &mymsg, sizeof(mymsg.mpcb), IPC_NOWAIT) == IPC_ERR ) {
    perror("msgsnd");
    terminateSelf(shmid, shmid2, msqid);
  }
  writeLog("oss: sent exclusive test message to first user process", true);
  
  while(totalProcessesGenerated < maxProcessesToGenerate) {
    
    if(gSTOP == 1) {terminateSelf(shmid, shmid2, msqid);}
    
    //generate processes at random intervals
    interval = getRandomInterval(maxIntervalSC, maxIntervalNS);
        
    if (pctracker.count() < MAX_PCBS) {
      if(gSTOP == 1) {terminateSelf(shmid, shmid2, msqid);}
      
      int i = 0;
      while(!pctracker.test(i)){ i++; }  //find first available PCB using pctracker bitvector
      pcbnum = i;
      pctracker.set(i);
      pid_t childpid = fexe();
      totalProcessesGenerated++;
      localClocksim = addTimeToClock(interval, localClocksim);
      clocksim->seconds = localClocksim.seconds;
      clocksim->nanoseconds = localClocksim.nanoseconds;
      writeLog("oss: Generated process with PID " + to_string(childpid) + " at time " 
                + to_string(clocksim->seconds) + ":" + to_string(clocksim->nanoseconds), true);
      pctable[pcbnum].local_simulated_pid = childpid; 
      mymsg.mtype = childpid;
      mymsg.mpcb = pcbnum;
      if ( msgsnd(msqid, &mymsg, sizeof(mymsg.mpcb), IPC_NOWAIT) == IPC_ERR ) {
        perror("msgsnd");
        terminateSelf(shmid, shmid2, msqid);
      }
      writeLog("oss: sent exclusive message to user" + to_string(childpid), true);
    }
    
  }
  
  if(gSTOP == 1) {terminateSelf(shmid, shmid2, msqid);}

  /*************************************************************************/

  //detach shared memory
  if (shmdt((void *)pctable) == IPC_ERR) { perror("shmdt PCT"); }
  else writeLog("oss: detached from shared memory for process control table", true);
  if (shmdt((void *)clocksim) == IPC_ERR) { perror("shmdt clock"); }
  else writeLog("oss: detached from shared memory for simulated clock", true);
  
  //free shared memory, wait for children
  pid_t wpid;
  int status = 0;
  while( (wpid = wait(&status)) > 0 );
  cout << "oss: no more children." << endl;

  if (shmctl(shmid, IPC_RMID, NULL) == IPC_ERR) { perror("shmctl PCT"); }
  else writeLog("oss: freed shared memory for process control table", true);
  if (shmctl(shmid2, IPC_RMID, NULL) == IPC_ERR) { perror("shmctl clock"); }
  else writeLog("oss: freed shared memory for simulated clock", true);

  //remove message queue
  if ( msgctl(msqid, IPC_RMID, NULL) == IPC_ERR ) { perror("msgctl RMID"); }
  else writeLog("oss: removed message queue", true);

  //terminate
  writeLog("oss: terminating", true);
  return 0;
}



/****************************************************/
/*                   FUNCTIONS                      */
/****************************************************/
void interruptHandler(int sig) { 
  write(STDOUT_FILENO, " oss: signal received\n", 22);
  gSTOP = 1; 
}

void writeLog(const string &message, bool append) {
  if (logcount >= 1000) {
   write(STDOUT_FILENO, "error: cannot write to log: file is too big.\n", 45);
   return;
  }
  std::ofstream f;
  if (append) {
    f.open(LOGFILE, std::fstream::out | std::ios_base::app); 
    logcount++;
  }
  else {
    f.open(LOGFILE, std::fstream::out); 
    logcount = 1;
  }
  f << message << std::endl;
  f.close();
}


int fexe() {  
  pid_t childpid = fork();
  if (childpid == -1) {
    perror("fork failed");
  }
  else if (childpid >= 0) {
    //fork successful
    //TODO
  }
  if (childpid == 0) {
    //I am child process
    char *args[2] = { (char*)("./user_proc"), NULL};
    
    if ( execv(args[0], args) == -1) {
      perror("execv failed");
      gSTOP = 1;
    }
  }
  return childpid;
}


myclock_t getRandomInterval(const int max_sec, const long max_ns) { 
  myclock_t newInterval;
  
  if (max_sec > 0) newInterval.seconds = rand() % (max_sec + 1);
  else newInterval.seconds = 0;
  
  if (max_ns > 0) newInterval.nanoseconds = rand() % (max_ns + 1);
  else newInterval.nanoseconds = 0;
  
  //convert nanoseconds to seconds
  while(newInterval.nanoseconds >= SECTONS) {
    newInterval.nanoseconds -= SECTONS;
    newInterval.seconds++;
  }
  
  //sanity check
  if(newInterval.nanoseconds < 0 || newInterval.seconds < 0) {
    writeLog("oss: error: getRandomInterval() is returning negative", true);
    interruptHandler(SIGINT);
  }
  return newInterval;
}


myclock_t addTimeToClock(myclock_t timeval, myclock_t current) {
  int tempSEC = timeval.seconds + current.seconds;
  long tempNS = timeval.nanoseconds + current.nanoseconds;
  myclock_t updatedClock;

  //convert nanoseconds to seconds
  while(tempNS >= SECTONS) {
    tempNS -= SECTONS;
    tempSEC++;
  }

  //sanity check
  if(tempSEC < 0 || tempNS < 0) {
    writeLog("oss: error: addTimeToClock() is getting a negative result", true);
    interruptHandler(SIGINT);
  }
  else {
    updatedClock.seconds = tempSEC;
    updatedClock.nanoseconds = tempNS;
  }
  return updatedClock;
}


void terminateSelf(int _shmid, int _shmid2, int _msqid) {
  if (gSHM == 1) {
    //free shm pctable
    if (shmctl(_shmid, IPC_RMID, NULL) == IPC_ERR) { perror("shmctl PCT"); }
    else writeLog("oss: freed shared memory for process control table", true);
  }

  if (gSHM2 == 1) {
    //free shm clock
    if (shmctl(_shmid2, IPC_RMID, NULL) == IPC_ERR) { perror("shmctl clock"); }
    else writeLog("oss: freed shared memory for simulated clock", true);
  }

  if (gMSQ == 1) {
    //free message queue
    if ( msgctl(_msqid, IPC_RMID, NULL) == IPC_ERR ) { perror("msgctl RMID"); }
    else writeLog("oss: removed message queue", true);
  }


  cout << "early termination.  Check shared memory and user processes." << endl;
  exit(1);
}
