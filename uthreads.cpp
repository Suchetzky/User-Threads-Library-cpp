//
// Created by talsu on 12/04/2023.
//
#include "uthreads.h"
#include <iostream>
#include <list>
#include <algorithm>
using namespace std;

// Library structs and variables

enum state
{
    READY, RUNNING, BLOCK
};

typedef struct
{
    int ID;
    thread_entry_point _entry_point;
    state _state;
    char stack[STACK_SIZE];

} threadStruct;

list<threadStruct *> _READY_threads_list;
list<threadStruct *> _BLOCKED_threads_list;
threadStruct *_RUNNING_thread;
threadStruct *_used_ids[105];
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

  for (int i = 0; i < 105; i++)
    {
      _used_ids[i] = nullptr;
    }

  _used_ids[0] = new threadStruct;
  _used_ids[0]->ID = 0;
  _used_ids[0]->_entry_point = nullptr;

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
  return new_thread->ID;
}

int release_all ()
{

}

int release_thread (threadStruct *thread)
{
  switch (thread->_state)
    {
      case READY:
        _READY_threads_list.remove (thread);
      case RUNNING:
      //
      case BLOCK:
      //
    }
  _used_ids[thread->ID] = nullptr;
  delete (thread);
  return 0;
}

int uthread_terminate (int tid)
{
  if (tid < 0 || tid > 104 || _used_ids[tid] == nullptr)
    {
      std::cerr << "thread library error: ID doesn't exists.\n";
      return -1;
    }
  if (tid == 0)
    {
      //REALESE ALL FUNCTION
      release_all ();
      ::exit (0);
    }
  for (auto thread: _READY_threads_list)
    {
      if (thread->ID == tid)
        {
          _READY_threads_list.remove (thread);
        }
    }
  delete (_used_ids[tid]);
  _used_ids[tid] = nullptr;

  return 0;
  // TODO: check what to do when "If a thread terminates itself or the main
  //  thread is terminated, the function does not return."
}

int uthread_block (int tid)
{
  if (_used_ids[tid] == nullptr || tid == 0)
    {
      std::cerr << "thread library error: ID doesn't exists.\n";
      return -1;
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

int main ()
{
  cout << "Hello World! Tal";
  return 0;
}