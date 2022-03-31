#pragma once

#include "xo/filesystem/path.h"
#include "xo/time/stopwatch.h"

namespace scone
{
	xo::path GetSconeStudioFolder();
	xo::stopwatch& GetStudioStopwatch();
	inline void TimeSection( const char* name ) { GetStudioStopwatch().split( name ); }
}
