/* CS4760 Project 4: Process Scheduling
 * Author: Daniel Eggers
 * Instructor: Mark Hauschild
 * Date: November 5, 2020 
 */

#include <iostream>
#include <stdio.h>      //for perror
//#include <getopt.h>     //for cmdl parsing
//#include <stdlib.h>     //for atoi
//#include <cstdio>       //for printf
//#include <string.h>     //for cpp strings
#include <unistd.h>     //for fork & exec
//#include <sys/wait.h>   //for wait
#include <sys/msg.h>    //for message queue
//#include <errno.h>      //for ENOMSG, etc
#include <sys/shm.h>    //for shared memory
#include <cstdlib>      //for exit() function
//#include <cstring>
#include "pcbstruct.h"
#define PERM (0666)
#define IPC_ERR (-1)
using namespace std;


int main() {
  pid_t mypid = getpid(); 
  cout << "user: hello world, my PID is " << mypid << endl;

  //shared memory var
  key_t shmkey, shmkey2;      //PCT, clock
  int shmid, shmid2;          //PCT, clock
  pcb_t *pctable;             //array of structs, process table containing each PCB (shared)
  myclock_t *clocksim;        //hold 2 values: seconds, nanoseconds (shared)
  shmkey = 99841;             //PCT
  shmkey2 = 99842;            //clock
  int msqid;                  //id returned by msgget
  key_t msqkey = 99843;       //message queue key
  mymsg_t mymsg;              //holds message type and message data
  const size_t MSG_SIZE = 80; //msgrcv msgsz

  //check for existing shared memory (does not create)
  shmid = shmget(shmkey, SHM_SIZE, 0);
  if (shmid == IPC_ERR) { perror("user shmget PCT"); exit(1); }
  shmid2 = shmget(shmkey2, sizeof(int)*2, 0);
  if (shmid2 == IPC_ERR) { perror("user shmget clock"); exit(1); }
   
  //attach to existing shared memory 
  pctable = (pcb_t *)shmat(shmid, NULL, 0);
  if (pctable == (void *)IPC_ERR) { perror("user shmat PCT"); exit(1); }
  clocksim = (myclock_t *)shmat(shmid2, NULL, 0);
  if (clocksim == (void *)IPC_ERR) { perror("user shmat clock"); exit(1); }
   
  //find message queue
  msqid = msgget(msqkey, PERM);
  if (msqid == IPC_ERR) { perror("user msgget"); }
  
  //wait on first message from oss
  int msgrcvTimeout = 3;  //timeout (seconds) to abort if no message is received
  bool loopAgain = true;  //while loop control
  while(msgrcvTimeout > 0 && loopAgain) {
    if (msgrcv(msqid, &mymsg, MSG_SIZE, mypid, IPC_NOWAIT) == IPC_ERR) { 
      if (errno != ENOMSG) { 
        perror("user msgrcv"); exit(1);
      }
      else {
        sleep(1);
        --msgrcvTimeout;
      }
    }
    else {
      cout << "user " << mypid << " received message: " << mymsg.mpcb << endl;
      loopAgain = false;
    }
  }
  if (msgrcvTimeout <= 0 && loopAgain) { cout << "user " << mypid << " msgrcv timeout." << endl; }

  //cout << "user: read from PCB, mypid is " << pctable[0].local_simulated_pid << endl;
  //cout << "user: read from clock (sec): " << clocksim->seconds << endl;
  //cout << "user: read from clock (ns):  " << clocksim->nanoseconds << endl;

  //detach from existing shared memory
  if (shmdt((void *)pctable) == IPC_ERR) { perror("user shmdt PCT"); }
  if (shmdt((void *)clocksim) == IPC_ERR) { perror("user shmdt clock"); }

  return 0;
}
