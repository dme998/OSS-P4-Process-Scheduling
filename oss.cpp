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
int logcount;                //counter for number of lines written to log
bitset<MAX_PCBS> pctracker;  //process control tracker (bitvector)

/* PROTOTYPES */
void writeLog(const string &message, bool append); 
int fexe();
void cleanSHM();
void cleanMSQ();
void cleanAll();

int main() {
  
  writeLog("oss: initialized", false); //init logfile (clear it out)

  //shared memory var
  key_t shmkey;
  int shmid;
  pcb_t *pctable;  //array of structs, process table containing each PCB
  
  //allocate shared memory
  shmkey = 9984; //hardcoded key because ftok always returns 0 for some reason

  shmid = shmget(shmkey, SHM_SIZE, PERM | IPC_CREAT | IPC_EXCL);
  if (shmid == IPC_ERR) { perror("shmget"); exit(1); }
  else writeLog("oss: allocated shared memory", true); 
  pctable = (pcb_t *)shmat(shmid, NULL, 0);
  if (pctable == (void *)IPC_ERR) { perror("shmat"); exit(1); }
  else writeLog("oss: attached to shared memory", true);
  
  //lets just put something in the pctable
  pid_t mypid = fexe();
  writeLog( ("oss: fork and exec process with PID " + to_string(mypid)), true );
  pctable[0].local_simulated_pid = mypid;
  pctracker.set(0);

  //detach shared memory
  if (shmdt((void *)pctable) == IPC_ERR) { perror("shmdt"); }
  writeLog("oss: detached from shared memory", true);
  
  //free shared memory, wait for children
  pid_t wpid;
  int status = 0;
  while( (wpid = wait(&status)) > 0 );
  cout << "oss: no more children." << endl;

  if (shmctl(shmid, IPC_RMID, NULL) == IPC_ERR) { perror("perror: shmctl"); }
  writeLog("oss: freed shared memory", true);
  

  //terminate
  writeLog("oss: terminating", true);
  return 0;
}




/* FUNCTIONS */
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
  pid_t mypid = fork();
  if (mypid == -1) {
    perror("fork failed");
  }
  else if (mypid >= 0) {
    //fork successful
    //TODO increment pr_count
  }
  if (mypid == 0) {
    //I am child process
    char *args[2] = { (char*)("./user_proc"), NULL};
    
    if ( execv(args[0], args) == -1) {
      perror("execv failed");
      exit(1);
    }
  }
  return mypid;
}


