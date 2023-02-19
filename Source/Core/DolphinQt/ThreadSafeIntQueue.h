#ifndef THREAD_SAFE_QUEUE
#define THREAD_SAFE_QUEUE
#include <atomic>
#include <mutex>

class ThreadSafeIntQueue
{
public:
  // Using a Michael & Scott queue to make the queue threadsafe

  class QueueObject
  {
  public:
    QueueObject()
    {
      value = -1;
      next_pointer = nullptr;
    }

    QueueObject(int new_value, QueueObject* new_next_pointer)
    {
      value = new_value;
      next_pointer = new_next_pointer;
    }

    int value;
    QueueObject* next_pointer;
  };

  ThreadSafeIntQueue()
  {
    head_lock = new std::mutex();
    tail_lock = new std::mutex();
    head_pointer = tail_pointer = new QueueObject();
  }

  ~ThreadSafeIntQueue()
  {
    QueueObject* travel = head_pointer;

    while (travel != NULL)
    {
      head_pointer = head_pointer->next_pointer;
      delete travel;
      travel = head_pointer;
    }

    head_pointer = tail_pointer = nullptr;
    delete head_lock;
    delete tail_lock;
  }

  void push(int new_val)
  {
    QueueObject* temp = new QueueObject(new_val, nullptr);
    tail_lock->lock();
    tail_pointer->next_pointer = temp;
    tail_pointer = temp;
    tail_lock->unlock();
  }

  int pop()
  {
    QueueObject* oldHead;
    int returnVal;
    head_lock->lock();
    oldHead = head_pointer;
    QueueObject* newHead = head_pointer->next_pointer;

    if (newHead == nullptr)
    {
      head_lock->unlock();
      return -1;
    }

    returnVal = newHead->value;
    head_pointer = newHead;
    head_lock->unlock();
    delete oldHead;
    return returnVal;
  }

  std::mutex* head_lock;
  std::mutex* tail_lock;
  QueueObject* head_pointer;
  QueueObject* tail_pointer;
};
#endif
