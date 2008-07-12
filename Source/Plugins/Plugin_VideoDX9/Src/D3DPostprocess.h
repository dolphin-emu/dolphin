#pragma	once

namespace Postprocess
{

	void Initialize();
	void Cleanup();

	void BeginFrame();
	void FinalizeFrame();

	int GetWidth();
	int GetHeight();

	const char **GetPostprocessingNames();
}