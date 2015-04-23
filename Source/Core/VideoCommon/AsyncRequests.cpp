#include "VideoCommon/AsyncRequests.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/RenderBase.h"

AsyncRequests AsyncRequests::s_singleton;

AsyncRequests::AsyncRequests()
: m_enable(false), m_passthrough(true)
{
}

void AsyncRequests::PullEventsInternal()
{
	std::unique_lock<std::mutex> lock(m_mutex);
	m_empty.store(true);

	while (!m_queue.empty())
	{
		const Event& e = m_queue.front();

		lock.unlock();
		HandleEvent(e);
		lock.lock();

		m_queue.pop();
	}

	if (m_wake_me_up_again)
	{
		m_wake_me_up_again = false;
		m_cond.notify_all();
	}
}

void AsyncRequests::PushEvent(const AsyncRequests::Event& event, bool blocking)
{
	std::unique_lock<std::mutex> lock(m_mutex);

	if (m_passthrough)
	{
		HandleEvent(event);
		return;
	}

	m_empty.store(false);
	m_wake_me_up_again |= blocking;

	if (!m_enable)
		return;

	m_queue.push(event);

	RunGpu();
	if (blocking)
	{
		m_cond.wait(lock, [this]{return m_queue.empty();});
	}
}

void AsyncRequests::SetEnable(bool enable)
{
	std::unique_lock<std::mutex> lock(m_mutex);
	m_enable = enable;

	if (!enable)
	{
		// flush the queue on disabling
		while (!m_queue.empty())
			m_queue.pop();
		if (m_wake_me_up_again)
			m_cond.notify_all();
	}
}

void AsyncRequests::HandleEvent(const AsyncRequests::Event& e)
{
	EFBRectangle rc;
	switch (e.type)
	{
		case Event::EFB_POKE_COLOR:
			g_renderer->AccessEFB(POKE_COLOR, e.efb_poke.x, e.efb_poke.y, e.efb_poke.data);
			break;

		case Event::EFB_POKE_Z:
			g_renderer->AccessEFB(POKE_Z, e.efb_poke.x, e.efb_poke.y, e.efb_poke.data);
			break;

		case Event::EFB_PEEK_COLOR:
			*e.efb_peek.data = g_renderer->AccessEFB(PEEK_COLOR, e.efb_peek.x, e.efb_peek.y, 0);
			break;

		case Event::EFB_PEEK_Z:
			*e.efb_peek.data = g_renderer->AccessEFB(PEEK_Z, e.efb_peek.x, e.efb_peek.y, 0);
			break;

		case Event::SWAP_EVENT:
			Renderer::Swap(e.swap_event.xfbAddr, e.swap_event.fbWidth, e.swap_event.fbStride, e.swap_event.fbHeight, rc);
			break;

		case Event::BBOX_READ:
			*e.bbox.data = g_renderer->BBoxRead(e.bbox.index);
			break;

		case Event::PERF_QUERY:
			g_perf_query->FlushResults();
			break;

	}
}

void AsyncRequests::SetPassthrough(bool enable)
{
	std::unique_lock<std::mutex> lock(m_mutex);
	m_passthrough = enable;
}

