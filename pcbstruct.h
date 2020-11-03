typedef struct ProcessControlBlock {
  int total_cpu_time_used;
  int total_time_in_system;
  int last_burst_time;
  int local_simulated_pid;
  int process_priority;
}pcb_t; 

const int MAX_PCBS = 18; 
const int SHM_SIZE = ( sizeof(pcb_t) * MAX_PCBS ); 
const char *LOGFILE = "log.txt";

