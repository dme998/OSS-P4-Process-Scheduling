typedef struct ProcessControlBlock {
  int total_cpu_time_used;
  int total_time_in_system;
  int last_burst_time;
  int local_simulated_pid;
  int process_priority;
}pcb_t; 

struct myclock_t {
  int seconds = 0;
  long nanoseconds = 0;
};

struct mymsg_t {
  long mtype;
  int mpcb;
};

const int MAX_PCBS = 18;
const int SHM_SIZE = (sizeof(pcb_t) * MAX_PCBS); 
