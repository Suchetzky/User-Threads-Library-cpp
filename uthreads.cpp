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
using namespace std;


#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
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

} threadStruct;

list<threadStruct *> _READY_threads_list;
//list<threadStruct *> _BLOCKED_threads_list;
threadStruct *_RUNNING_thread;
threadStruct *_used_ids[MAX_THREAD_NUM + 2];
int _cur_quantum_usecs;

//
int uthread_spawn (thread_entry_point entry_point);

int uthread_init (int quantum_usecs)
{
  if (quantum_usecs <= 0)
    {
      std::cerr << "thread library error: quantum is non-positive.\n";
      return -1;
    }

  _cur_quantum_usecs = quantum_usecs;

  for (int i = 0; i < MAX_THREAD_NUM + 2; i++)
    {
      _used_ids[i] = nullptr;
    }

  _used_ids[0] = new threadStruct;
  _used_ids[0]->ID = 0;
  _used_ids[0]->_entry_point = nullptr;
  _used_ids[0]->_state = RUNNING;
    sigsetjmp(_used_ids[0]->env, 1);

  _RUNNING_thread = _used_ids[0];

  // Need to initialize _READY_threads_list ?

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

    address_t sp = (address_t) new_thread->stack + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t) entry_point;
    sigsetjmp(env[tid], 1);
    (env[tid]->__jmpbuf)[JB_SP] = translate_address(sp);
    (env[tid]->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&env[tid]->__saved_mask);
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
//  for (auto thread: _BLOCKED_threads_list)
//    {
//      thread = nullptr;
//    }
  _RUNNING_thread = nullptr;
  return 0;
}

int release_thread (threadStruct *thread)
{
  switch (thread->_state)
    {
      case READY:
        _READY_threads_list.remove (thread);
      case RUNNING:
        _RUNNING_thread = nullptr;
      //case BLOCKED:
        //_BLOCKED_threads_list.remove (thread);
    }
  _used_ids[thread->ID] = nullptr;
  delete (thread);
  return 0;
}

int uthread_terminate (int tid)
{
  if (tid < 0 || tid >= MAX_THREAD_NUM  || _used_ids[tid] == nullptr)
    {
      std::cerr << "thread library error: ID doesn't exists.\n";
      return -1;
    }
  if (tid == 0)
    {
      release_all ();
      ::exit (0);
    }
  release_thread (_used_ids[tid]);

  return 0;

}

int uthread_block (int tid)
{
  if (tid < 0 || tid > MAX_THREAD_NUM - 1 || _used_ids[tid] == nullptr)
    {
      std::cerr << "thread library error: ID doesn't exists.\n";
      return -1;
    }
  if (tid == 0)
    {
      std::cerr << "thread library error: Try to block main thread.\n";
      return -1;
    }
  if (_used_ids[tid]->_state != BLOCKED)
    {
      if (_used_ids[tid]->_state == RUNNING)
        {
            sigsetjmp(_used_ids[tid]->env,1);
            _used_ids[tid]->_state = BLOCKED;
            // move the next ready to running
            _RUNNING_thread = _READY_threads_list.front();
            _READY_threads_list.pop_front();
            _RUNNING_thread->_state = RUNNING;

        }
    }
  return 0;
}

int uthread_resume (int tid)
{

}

int uthread_sleep (int num_quantums)
{

}

int uthread_get_tid ()
{

}

int uthread_get_total_quantums ()
{

}

int uthread_get_quantums (int tid)
{

}

//int main ()
//{
//  cout << "Hello World! Tal";
//  return 0;
//}
