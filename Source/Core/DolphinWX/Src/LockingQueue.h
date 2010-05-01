#ifndef _LOCKINGQUEUE_H_
#define _LOCKINGQUEUE_H_

#include "Thread.h"
#include <queue>

// i should make one of those single reader/ single writer queues

template <typename T>
class LockingQueue
{
public:
	size_t Size()
	{
		m_crit.Enter();
		const size_t s = m_queue.size();
		m_crit.Leave();
		return s;
	}

	void Push(const T& t)
	{
		m_crit.Enter();
		m_queue.push(t);
		m_crit.Leave();
	}

	bool Pop(T& t)
	{
		m_crit.Enter();
		if (m_queue.size())
		{
			t = m_queue.front();
			m_queue.pop();
			m_crit.Leave();
			return true;
		}
		m_crit.Leave();
		return false;
	}

private:
	std::queue<T>	m_queue;
	Common::CriticalSection		m_crit;
};

#endif
