#include "studio_tools.h"

#include "scone/core/system_tools.h"

namespace scone
{
	static xo::stopwatch g_StudioStopwatch;

	xo::path GetSconeStudioFolder()
	{
		return scone::GetInstallFolder().parent_path();
	}

	xo::stopwatch& GetStudioStopwatch()
	{
		return g_StudioStopwatch;
	}
}
