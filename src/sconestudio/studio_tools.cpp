#include "studio_tools.h"

#include "scone/core/system_tools.h"

xo::path GetSconeStudioFolder()
{
	return scone::GetInstallFolder().parent_path();
}
