#ifndef THREAD_SAFE_QUEUE
#define THREAD_SAFE_QUEUE
#include <atomic>
#include <mutex>

template <typename T>
class QueueObject
{
public:
  QueueObject()
  {
    value = 0;
    next_pointer = nullptr;
  }

  QueueObject(T new_value, QueueObject* new_next_pointer)
  {
    value = new_value;
    next_pointer = new_next_pointer;
  }

  T value;
  QueueObject<T>* next_pointer;
};

template <typename T>
class ThreadSafeQueue
{
public:
  // Using a Michael & Scott queue to make the queue threadsafe

  ThreadSafeQueue()
  {
    head_lock = new std::mutex();
    tail_lock = new std::mutex();
    head_pointer = tail_pointer = new QueueObject<T>();
  }

  ~ThreadSafeQueue()
  {
    QueueObject<T>* travel = head_pointer;

    while (travel != nullptr)
    {
      head_pointer = head_pointer->next_pointer;
      delete travel;
      travel = head_pointer;
    }

    head_pointer = tail_pointer = nullptr;
    delete head_lock;
    delete tail_lock;
  }

  void push(T new_val)
  {
    QueueObject<T>* temp = new QueueObject<T>(new_val, nullptr);
    tail_lock->lock();
    tail_pointer->next_pointer = temp;
    tail_pointer = temp;
    tail_lock->unlock();
  }

  T pop()
  {
    QueueObject<T>* oldHead;
    T returnVal;
    head_lock->lock();
    oldHead = head_pointer;
    QueueObject<T>* newHead = head_pointer->next_pointer;

    if (newHead == nullptr)
    {
      head_lock->unlock();
      return 0;
    }

    returnVal = newHead->value;
    head_pointer = newHead;
    head_lock->unlock();
    delete oldHead;
    return returnVal;
  }

  bool IsEmpty() { return head_pointer->next_pointer == nullptr; }

  std::mutex* head_lock;
  std::mutex* tail_lock;
  QueueObject<T>* head_pointer;
  QueueObject<T>* tail_pointer;
};
#endif
