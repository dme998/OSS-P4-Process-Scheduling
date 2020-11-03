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
//#include <unistd.h>     //for fork & exec
//#include <sys/wait.h>   //for wait
#include <sys/msg.h>    //for message queue
//#include <errno.h>      //for ENOMSG, etc
#include <sys/shm.h>    //for shared memory
#include <cstdlib>      //for exit() function
//#include <cstring>
#include "pcbstruct.h"

#define IPC_ERR (-1)
using namespace std;


int main() {
  
  cout << "user: hello world" << endl;

  //shared memory var
  key_t shmkey; 
  int shmid;
  pcb_t *pctable;  //array of structs, process table containing each PCB (shared)

  //check for existing shared memory (does not create)
  shmkey = 9984;
  shmid = shmget(shmkey, SHM_SIZE, 0);
  if (shmid == IPC_ERR) { perror("user shmget"); exit(1); }
   
  //attach to existing shared memory 
  pctable = (pcb_t *)shmat(shmid, NULL, 0);
  if (pctable == (void *)IPC_ERR) { perror("user shmat"); exit(1); }
 
  cout << "user: read from PCB, mypid is " << pctable[0].local_simulated_pid << endl;

  //detach from existing shared memory
  if (shmdt((void *)pctable) == IPC_ERR) { perror("user shmdt"); }

  return 0;
}
