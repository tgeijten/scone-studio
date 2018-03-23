#pragma once

#include "scone/core/core.h"
#include "xo/system/settings.h"

namespace scone
{
	xo::settings GetStudioSettings();
	template< typename T > T GetStudioSetting( const String& key ) { return GetStudioSettings().get< T >( key ); }
}
