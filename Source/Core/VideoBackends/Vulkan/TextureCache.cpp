#include "VideoBackends/Vulkan/TextureCache.h"

namespace Vulkan {

TextureCache::TextureCache(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr, StateTracker* state_tracker)
	: m_object_cache(object_cache)
	, m_command_buffer_mgr(command_buffer_mgr)
	, m_state_tracker(state_tracker)
{

}

TextureCache::~TextureCache()
{

}

}		// namespace Vulkan
