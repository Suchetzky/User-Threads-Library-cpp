//
// Created by talsu on 12/04/2023.
//
#include "uthreads.h"
#include <iostream>
#include <list>
#include <queue>
#include <algorithm>
#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
using namespace std;

#define TIME_FACTOR 1000000

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



// Library structs and variables

enum state
{
    READY, RUNNING, BLOCKED
};

typedef struct
{
    int ID;
    thread_entry_point _entry_point;
    state _state;
    char stack[STACK_SIZE];
    sigjmp_buf env;
    int _num_running_quantums;
    int sleep_time = 0;

} threadStruct;

list<threadStruct *> _READY_threads_list;
list<threadStruct *> _SLEEP_threads_list;
//list<threadStruct *> _BLOCKED_threads_list;
threadStruct *_RUNNING_thread;
threadStruct *_used_ids[MAX_THREAD_NUM + 2];
int _cur_quantum_usecs;
int _cur_quantum_secs;
int _count_quantums;
struct sigaction sa = {0};
struct itimerval timer;
sigset_t set;


//
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
        //TODO check errors !
        std::cerr << "block unblock problem\n";
        release_all ();
        ::exit (0);
    }
}

int check_tid (int tid)
{
  if (tid < 0 || tid > MAX_THREAD_NUM - 1 || _used_ids[tid] == nullptr)
    {
      std::cerr << "thread library error: ID doesn't exists.\n";
      return -1;
    }
  return 0;
}

void lower_1_from_sleep()
{
  for (auto thread : _SLEEP_threads_list)
    {
      if (thread->sleep_time == 0)
        {
          if (thread->_state == READY)
            {
              _READY_threads_list.push_back (thread);
            }
        }
        else
        {
          thread->sleep_time --;
        }
    }
}

void timer_handler (int sig)
{
  switch_running ();
  lower_1_from_sleep();

}

int reset_timer ()
{
  // reset the current timer to the interval. check if split ti usec
  // and sec is necessary
  timer.it_value = timer.it_interval;
  if (setitimer (ITIMER_VIRTUAL, &timer, NULL))
    {
      printf ("setitimer error.");
    }
}

int start_timer ()
{
  // moved timer and sa to be global variables

  // Install timer_handler as the signal handler for SIGVTALRM.
  sa.sa_handler = &timer_handler;
  if (sigaction (SIGVTALRM, &sa, NULL) < 0)
    {
      printf ("sigaction error.");
    }

  // Configure the timer to expire after 1 sec... */
  timer.it_value.tv_sec = _cur_quantum_secs;        // first time interval, seconds part
  timer.it_value.tv_usec = _cur_quantum_usecs;        // first time interval, microseconds part

  // configure the timer to expire every 3 sec after that.
  timer.it_interval.tv_sec = _cur_quantum_secs;    // following time intervals, seconds part
  timer.it_interval.tv_usec = _cur_quantum_usecs;    // following time intervals, microseconds part

  // Start a virtual timer. It counts down whenever this process is executing.
  if (setitimer (ITIMER_VIRTUAL, &timer, NULL))
    {
      printf ("setitimer error.");
    }


}

void switch_running ()
{
  if (sigsetjmp (_RUNNING_thread->env, 1) != 0)
  {
      return;
  }
  _READY_threads_list.push_back (_RUNNING_thread);
  _RUNNING_thread->_state = READY;
   move_next_ready_to_running();
  // Might need to reset timer every time.

}

int uthread_init (int quantum_usecs)
{
  if (quantum_usecs <= 0)
    {
      std::cerr << "thread library error: quantum is non-positive.\n";
      return -1;
    }

  _cur_quantum_usecs = quantum_usecs % TIME_FACTOR;
  _cur_quantum_secs = quantum_usecs / TIME_FACTOR;

  for (int i = 0; i < MAX_THREAD_NUM + 2; i++)
    {
      _used_ids[i] = nullptr;
    }
  _count_quantums = 1;

  _used_ids[0] = new threadStruct;
  _used_ids[0]->ID = 0;
  _used_ids[0]->_entry_point = nullptr;
  _used_ids[0]->_state = RUNNING;
  _used_ids[0]->_num_running_quantums =1;
  //sigsetjmp (_used_ids[0]->env, 1);
  //igemptyset (&_used_ids[0]->env->__saved_mask);

//    address_t sp =
//            (address_t) _used_ids[0]->stack + STACK_SIZE - sizeof (address_t);
//    address_t pc = (address_t) _used_ids[0]->_entry_point;
//    sigsetjmp (_used_ids[0]->env, 1);
//    (_used_ids[0]->env->__jmpbuf)[JB_SP] = translate_address (sp);
//    (_used_ids[0]->env->__jmpbuf)[JB_PC] = translate_address (pc);
//    sigemptyset (&_used_ids[0]->env->__saved_mask);

  _RUNNING_thread = _used_ids[0];
  start_timer ();
    //sigemptyset(&set);
    //sigaddset(&set,SIGVTALRM);
  // Need to initialize _READY_threads_list ?
  return 0;
}

int lowest_id_available ()
{
  int i = 0;
  while (_used_ids[i] != nullptr)
    {
      i++;
    }
  return i;
}

int uthread_spawn (thread_entry_point entry_point)
{
  if (lowest_id_available () > 99 || entry_point == nullptr)
    {
      return -1;
    }
  auto *new_thread = new threadStruct;
  new_thread->_entry_point = entry_point;
  new_thread->ID = lowest_id_available ();
  _used_ids[new_thread->ID] = new_thread;
  _READY_threads_list.push_back (new_thread);
  _used_ids[new_thread->ID]->_state = READY;

  address_t sp =
      (address_t) new_thread->stack + STACK_SIZE - sizeof (address_t);
  address_t pc = (address_t) entry_point;
  sigsetjmp (new_thread->env, 1);
  (new_thread->env->__jmpbuf)[JB_SP] = translate_address (sp);
  (new_thread->env->__jmpbuf)[JB_PC] = translate_address (pc);
  sigemptyset (&new_thread->env->__saved_mask);
  return new_thread->ID;
}

int release_all ()
{
  for (auto thread: _used_ids)
    {
      if (thread != nullptr)
        delete thread;
      thread = nullptr;
    }
  for (auto thread: _READY_threads_list)
    {
      thread = nullptr;
    }
  _RUNNING_thread = nullptr;
  return 0;
}

int release_thread (threadStruct *thread)
{
  switch (thread->_state)
    {
      case READY:
        _READY_threads_list.remove (thread);
            break;
      case RUNNING:
        _RUNNING_thread = nullptr;
            break;
    }
  _used_ids[thread->ID] = nullptr;
  delete (thread);
  return 0;
}

int uthread_terminate (int tid)
{
  check_tid (tid);
  if (tid == 0)
    {
      release_all ();
      ::exit (0);
    }

    if (tid == _RUNNING_thread->ID)
      {
        reset_timer();
        move_next_ready_to_running();
        // TODO: return value?
        //https://moodle2.cs.huji.ac.il/nu22/mod/forum/discuss.php?d=67557
      }
    release_thread(_used_ids[tid]);

  return 0;

}

void move_next_ready_to_running()
{
  _count_quantums ++;
  _RUNNING_thread = _READY_threads_list.front();
  _READY_threads_list.pop_front();
  _RUNNING_thread->_state = RUNNING;
  _RUNNING_thread->_num_running_quantums +=1;
  siglongjmp (_RUNNING_thread->env, 1);

}

/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error.
 * In addition, it is an error to try blocking the
 * main thread (tid == 0).
 * If a thread blocks itself, a scheduling decision should be made.
 * Blocking a thread in
 * BLOCKED state has no effect and is not considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_block (int tid)
{
  check_tid (tid);
  if (tid == 0)
    {
      std::cerr << "thread library error: Try to block main thread.\n";
      return -1;
    }
  if (_used_ids[tid]->_state != BLOCKED)
    {
      if (_used_ids[tid]->_state == RUNNING)
        {
          //TODO CHeck
          if (sigsetjmp (_RUNNING_thread->env, 1) != 0)
          {
              return 0;
          }
          _used_ids[tid]->_state = BLOCKED;

          // move the next ready to running
          // inside function that control time
          reset_timer();
          move_next_ready_to_running();


        }
      else if (_used_ids[tid]->_state == READY)
        {
          _used_ids[tid]->_state = BLOCKED;
          _READY_threads_list.remove (_used_ids[tid]);
        }
    }
  return 0;
}

/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered
 * as an error. If no thread with
 * ID tid exists it is considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/

int uthread_resume (int tid)
{
  check_tid (tid);

  if (_used_ids[tid]->_state == BLOCKED )
    {
      _used_ids[tid]->_state = READY;
      if (_used_ids[tid]->sleep_time == 0)
        {
          _READY_threads_list.push_back (_used_ids[tid]);
        }

    }
  return 0;
}


/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY queue.
 * If the thread which was just RUNNING should also be added to the READY queue, or if multiple threads wake up
 * at the same time, the order in which they're added to the end of the READY queue doesn't matter.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main thread (tid == 0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
*/

int uthread_sleep (int num_quantums)
{
  if (_RUNNING_thread->ID == 0 )
    {
      std::cerr << "thread library error: Try to sleep main thread.\n";
      return -1;
    }
  _RUNNING_thread->sleep_time = num_quantums;
  sigsetjmp (_RUNNING_thread->env, 1);
  _RUNNING_thread->_state = READY;
  _SLEEP_threads_list.push_back (_RUNNING_thread);

  move_next_ready_to_running();

}

int uthread_get_tid ()
{
  return _RUNNING_thread->ID;
}


int uthread_get_total_quantums ()
{
  return _count_quantums;
}

int uthread_get_quantums (int tid)
{
  if (check_tid (tid) != 0)
    {
      return -1;
    }
  return _used_ids[tid]->_num_running_quantums;
}



//
//int main ()
//{
//    uthread_init(900000);
//  return 0;
//}
