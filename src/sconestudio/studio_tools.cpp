#include "studio_tools.h"

#include "scone/core/system_tools.h"
#include "scone/core/Exception.h"

namespace scone
{
	static xo::stopwatch g_StudioStopwatch;

	xo::path GetSconeStudioFolder()
	{
		const auto& p = GetInstallFolder();
		SCONE_ERROR_IF( p.empty(), "Could not detect SCONE installation folder" );
		return p.parent_path();
	}

	xo::stopwatch& GetStudioStopwatch()
	{
		return g_StudioStopwatch;
	}
}
