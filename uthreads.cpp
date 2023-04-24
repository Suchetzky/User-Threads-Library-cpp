#include "uthreads.h"
#include <iostream>
#include <list>
#include <algorithm>
#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
using namespace std;



#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address (address_t addr)
{
  address_t ret;
  asm volatile("xor    %%fs:0x30,%0\n"
               "rol    $0x11,%0\n"
      : "=g" (ret)
      : "0" (addr));
  return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5


/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}


#endif

#define EXTRA 2
#define SLEEP_INIT -1
#define EXIT_WITH_FAILURE 1
#define TIME_FACTOR 1000000
#define SUCCESS 0
#define FAILURE -1


// Messages
// System error msg
#define SYS_ERR_SIGPROCMASK "system error: sigprocmask failed \n"
#define SYS_ERR_SETITIMER "system error: setitimer failed \n"
#define SYS_ERR_SIGACTION "system error: sigaction failed \n"

// Thread library msg
#define THREAD_LIB_ERR_ID "thread library error: id is not valid.\n"
#define THREAD_LIB_ERR_QUANTUM "thread library error: quantum most be positive.\n"
#define THREAD_LIB_ERR_MAX_THREADS "thread library error: max thread error.\n"
#define THREAD_LIB_ERR_BLOCK_MAIN "thread library error: Try to block main thread.\n"
#define THREAD_LIB_ERR_SLEEP_MAIN "thread library error: Try to sleep main thread.\n"


// Library structs and variables

enum state
{
    READY, RUNNING, BLOCKED
};

typedef struct
{
    int id;
    thread_entry_point entryPoint;
    state _state;
    char stack[STACK_SIZE];
    sigjmp_buf env;
    int numRunningQuantums;
    int sleepTime = SLEEP_INIT;

} threadStruct;

list<threadStruct *> readyThreadsList;
threadStruct *runningThread;
threadStruct *usedIds[MAX_THREAD_NUM + EXTRA];

int _cur_quantum_usecs;
int _cur_quantum_secs;
int _count_quantums;
struct sigaction sa = {0};
struct itimerval timer;
sigset_t set;
sigset_t testSave;


int uthread_spawn (thread_entry_point entry_point);
void switch_running ();
void move_next_ready_to_running();
int uthread_resume (int tid);
int release_all();

void block_unblock(int signal)
{
    // signal is SIG_BLOCK or SIG_UNBLOCK
    if (sigprocmask(signal,&set, nullptr) == -1)
    {
        std::cerr << SYS_ERR_SIGPROCMASK;
        release_all ();
        ::exit (EXIT_WITH_FAILURE);
    }
}

int check_tid (int tid)
{
  if (tid < 0 || tid > MAX_THREAD_NUM - 1 || usedIds[tid] == nullptr)
    {
      std::cerr << THREAD_LIB_ERR_ID;
      return FAILURE;
    }
  return SUCCESS;
}

void lower_1_from_sleep()
{
  for (auto thread : usedIds)
    {
      if (thread != nullptr)
        {
            thread->sleepTime--;
            if (thread->sleepTime == 0) {
                if (thread->_state == READY) {
                    readyThreadsList.push_back(thread);
                }
            }
        }
      }
}

void timer_handler (int sig)
{
  switch_running ();
}

int reset_timer ()
{
  // reset the current timer to the interval.
  timer.it_value = timer.it_interval;
  if (setitimer (ITIMER_VIRTUAL, &timer, nullptr))
    {
      std::cerr << SYS_ERR_SETITIMER;
      release_all ();
      ::exit (EXIT_WITH_FAILURE);
    }
}

int start_timer ()
{
  // moved timer and sa to be global variables
  // Install timer_handler as the signal handler for SIGVTALRM.
  sa.sa_handler = &timer_handler;
  if (sigaction (SIGVTALRM, &sa, nullptr) < 0)
    {
      std::cerr << SYS_ERR_SIGACTION;
      release_all ();
      ::exit (EXIT_WITH_FAILURE);
    }

  // Configure the timer to expire after 1 sec... */
  timer.it_value.tv_sec = _cur_quantum_secs;  // first time interval
  timer.it_value.tv_usec = _cur_quantum_usecs;  // first time interval

  // configure the timer to expire every 3 sec after that.
  timer.it_interval.tv_sec = _cur_quantum_secs; // following time intervals
  timer.it_interval.tv_usec = _cur_quantum_usecs;  // following time intervals

  // Start a virtual timer. It counts down whenever this process is executing.
  if (setitimer (ITIMER_VIRTUAL, &timer, nullptr))
    {
      std::cerr << SYS_ERR_SETITIMER;
      release_all ();
      ::exit (EXIT_WITH_FAILURE);
    }
}

void switch_running ()
{
  block_unblock(SIG_BLOCK);
  if (sigsetjmp (runningThread->env, 1) != 0)
  {
      return;
  }
  lower_1_from_sleep();
  readyThreadsList.push_back (runningThread);
  runningThread->_state = READY;
  move_next_ready_to_running();
}

int uthread_init (int quantum_usecs)
{
  if (quantum_usecs <= 0)
    {
      std::cerr << THREAD_LIB_ERR_QUANTUM;
      return FAILURE;
    }
  _cur_quantum_usecs = quantum_usecs % TIME_FACTOR;
  _cur_quantum_secs = quantum_usecs / TIME_FACTOR;
  for (int i = 0; i < MAX_THREAD_NUM + EXTRA; i++)
    {
        usedIds[i] = nullptr;
    }
  _count_quantums = 1;

  usedIds[0] = new threadStruct;
  usedIds[0]->id = 0;
  usedIds[0]->entryPoint = nullptr;
  usedIds[0]->_state = RUNNING;
  usedIds[0]->numRunningQuantums = 1;

  runningThread = usedIds[0];
  sigemptyset(&set);
  sigaddset(&set,SIGVTALRM);
  start_timer ();
  return SUCCESS;
}

int lowest_id_available ()
{
  int i = 0;
  while (usedIds[i] != nullptr)
    {
      i++;
    }
  return i;
}

int uthread_spawn (thread_entry_point entry_point)
{
    block_unblock(SIG_BLOCK);
  if (lowest_id_available () > MAX_THREAD_NUM - 1 || entry_point == nullptr)
    {
      std::cerr << THREAD_LIB_ERR_MAX_THREADS;
      block_unblock(SIG_UNBLOCK);
      return FAILURE;
    }
  auto *new_thread = new threadStruct;
  new_thread->entryPoint = entry_point;
  new_thread->id = lowest_id_available ();
    usedIds[new_thread->id] = new_thread;
  readyThreadsList.push_back (new_thread);
    usedIds[new_thread->id]->_state = READY;

  address_t sp =
      (address_t) new_thread->stack + STACK_SIZE - sizeof (address_t);
  address_t pc = (address_t) entry_point;
  sigsetjmp (new_thread->env, 1);
  (new_thread->env->__jmpbuf)[JB_SP] = translate_address (sp);
  (new_thread->env->__jmpbuf)[JB_PC] = translate_address (pc);
  sigemptyset (&new_thread->env->__saved_mask);
    block_unblock(SIG_UNBLOCK);
  return new_thread->id;
}

int release_all ()
{
  for (auto thread: usedIds)
    {
      if (thread != nullptr) {
          int id = thread->id;
          delete thread;
          usedIds[id] = nullptr;
      }
    }
  for (auto thread: readyThreadsList)
    {
      thread = nullptr;
    }
  runningThread = nullptr;
  return SUCCESS;
}

int release_thread (threadStruct *thread)
{
  switch (thread->_state)
    {

      case READY:
        readyThreadsList.remove (thread);
        break;
      case RUNNING:
        runningThread = nullptr;
        break;
    }
  usedIds[thread->id] = nullptr;
  delete (thread);
  return SUCCESS;
}

int uthread_terminate (int tid)
{
  block_unblock(SIG_BLOCK);
  if (check_tid (tid) != 0)
  {
      block_unblock(SIG_UNBLOCK);
      return FAILURE;
  }
  if (tid == 0)
    {
      release_all ();
      ::exit (SUCCESS);
    }
  int cur_running_id = runningThread->id;
  release_thread(usedIds[tid]);
  if (tid == cur_running_id)
    {
      reset_timer();
      lower_1_from_sleep();
      move_next_ready_to_running();
    }
  block_unblock(SIG_UNBLOCK);
  return SUCCESS;

}

void move_next_ready_to_running()
{
  _count_quantums ++;
  runningThread = readyThreadsList.front();
  readyThreadsList.pop_front();
  runningThread->_state = RUNNING;
  runningThread->numRunningQuantums +=1;
  block_unblock(SIG_UNBLOCK);
  siglongjmp (runningThread->env, 1);
}

/**
 * @brief Blocks the thread with id tid. The thread may be resumed later using
 * uthread_resume.
 *
 * If no thread with id tid exists it is considered as an error.
 * In addition, it is an error to try blocking the
 * main thread (tid == 0).
 * If a thread blocks itself, a scheduling decision should be made.
 * Blocking a thread in
 * BLOCKED _state has no effect and is not considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_block (int tid)
{
    block_unblock(SIG_BLOCK);
    if(check_tid (tid) != 0)
    {
        block_unblock(SIG_UNBLOCK);
        return FAILURE;
    }
    if (tid == 0)
    {
        std::cerr << THREAD_LIB_ERR_BLOCK_MAIN;
        block_unblock(SIG_UNBLOCK);
        return FAILURE;
    }
  if (usedIds[tid]->_state != BLOCKED)
    {
      if (usedIds[tid]->_state == RUNNING)
        {
          if (sigsetjmp (runningThread->env, 1) != 0)
          {
              block_unblock(SIG_UNBLOCK);
              return SUCCESS;
          }
          usedIds[tid]->_state = BLOCKED;

          reset_timer();
          lower_1_from_sleep();
          move_next_ready_to_running();
        }
      else if (usedIds[tid]->_state == READY)
        {
          usedIds[tid]->_state = BLOCKED;
          readyThreadsList.remove (usedIds[tid]);
        }
    }
  block_unblock(SIG_UNBLOCK);
  return SUCCESS;
}

/**
 * @brief Resumes a blocked thread with id tid and moves it to the READY _state.
 *
 * Resuming a thread in a RUNNING or READY _state has no effect and is not considered
 * as an error. If no thread with
 * id tid exists it is considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/

int uthread_resume (int tid)
{
  block_unblock(SIG_BLOCK);
  if(check_tid (tid) != 0)
  {
      block_unblock(SIG_UNBLOCK);
      return FAILURE;
  }

  if (usedIds[tid]->_state == BLOCKED )
    {
      usedIds[tid]->_state = READY;
      if (usedIds[tid]->sleepTime <= 0)
        {
          readyThreadsList.push_back (usedIds[tid]);
        }
    }
  block_unblock(SIG_UNBLOCK);
  return SUCCESS;
}


/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED _state a
 * scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the
 * READY queue.
 * If the thread which was just RUNNING should also be added to the READY queue,
 * or if multiple threads wake up
 * at the same time, the order in which they're added to the end of the READY
 * queue doesn't matter.
 * The number of quantums refers to the number of times a new quantum starts,
 * regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main thread (tid == 0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
*/

int uthread_sleep (int num_quantums)
{
  block_unblock(SIG_BLOCK);
  if (runningThread->id == 0 )
    {
      std::cerr << THREAD_LIB_ERR_SLEEP_MAIN;
      block_unblock(SIG_UNBLOCK);
      return FAILURE;
    }
  lower_1_from_sleep();
  runningThread->sleepTime = num_quantums;
  if (sigsetjmp (runningThread->env, 1) != 0 )
  {
      return SUCCESS;
  }
  runningThread->_state = READY;
  reset_timer();
  move_next_ready_to_running();
  block_unblock(SIG_UNBLOCK);
  return SUCCESS;
}

int uthread_get_tid ()
{
  return runningThread->id;
}

int uthread_get_total_quantums ()
{
  return _count_quantums;
}

int uthread_get_quantums (int tid)
{
  block_unblock(SIG_BLOCK);
  if (check_tid (tid) != 0)
    {
      return FAILURE;
    }
  block_unblock(SIG_UNBLOCK);
  return usedIds[tid]->numRunningQuantums;
}

