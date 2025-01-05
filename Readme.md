## Author
Eli Davidson
## Contact
elid@uci.edu

# Description

The assignment code will create 4 worker threads. Each thread has its own task
execution time, period (deadline), and so on. You need to give 1 argument. 
Based on the argument value, your program uses a different scheduling function 
(0: FIFO scheduling, 1: Earliest deadline first (EDF) scheduling, and 2: Rate-Monotonic (RM) scheduling). 

For example, if 0 is the input argument, then your
program should use FIFO scheduling.

Your job is to implement two thread scheduling functions in main.cpp, fifo_schedule, and edf_schedule. 

RM scheduling is NOT mandatory. 

You must read all of the starter codes and understand the behavior of the program before starting the assignment.

○ You will need to implement the FIFO scheduling function first. Then check
whether the FIFO scheduling satisfies all the deadlines or not.


○ After that, you need to implement your own scheduling function based on EDF
scheduling. Your EDF-based scheduling function MUST satisfy all the deadlines.


○ In order to satisfy all the deadlines, you will need to use preemption, this is
in the starter code and can be accomplished as follows
supported // preempt the current running thread

preempt = 1;
pthread_cond_wait(&preempt_task, &mutex);
_
// thread is preempted here
○ If you complete the RM scheduling function, you will get extra points.

● You will need to understand pthread conditional variables to complete this
assignment. 

○ You will need to use pthread_cond_wait and pthread_cond_signal. 

The ready_queue vector stores all ready threads by their id. The running_thread variable keeps track of what thread is currently working, a value of
-1 means no thread is currently working.
● Your scheduling function should schedule threads from the ready_queue vector by
signaling t o the thread using the corresponding pthread conditional variable resume[x]
where x is the thread id.



# Score 
100% + 20 bonus for correct implementation for RM scheduling.