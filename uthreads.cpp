//
// Created by talsu on 12/04/2023.
//
#include "uthreads.h"
#include <iostream>
#include <list>
using namespace std;

typedef struct
{
    int _ID;
    thread_entry_point _entry_point;

} threadStruct;

int uthread_init (int quantum_usecs)
{
  if (quantum_usecs <= 0)
    {
      std::cerr << "thread library error: quantum is non-positive.\n";
    }
  int cur_quantum_usecs = quantum_usecs;
  int lowest_ID_available = 1;
  std::list<threadStruct> READY_threads_list;

}

int uthread_spawn (thread_entry_point entry_point)
{

}

int uthread_terminate (int tid)
{

}

int uthread_block (int tid)
{

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