/*
** StudioSettings.cpp
**
** Copyright (C) 2013-2019 Thomas Geijtenbeek and contributors. All rights reserved.
**
** This file is part of SCONE. For more information, see http://scone.software.
*/

#include "StudioSettings.h"

#include "scone/core/system_tools.h"
#include "scone/core/version.h"
#include "scone/core/Log.h"
#include "xo/serialization/serialize.h"
#include "xo/filesystem/filesystem.h"
#include "xo/serialization/prop_node_serializer_zml.h"
#include "studio_config.h"
#include "studio_tools.h"
#include "studio_settings_schema.h"

namespace scone
{
	StudioSettings::StudioSettings() :
		xo::settings(
			xo::parse_zml( studio_settings_schema ),
			GetSettingsFolder() / "studio-settings.zml",
			GetSconeVersion()
		)
	{
		if ( data_version() < version( 0, 17, 0 ) )
		{
			log::warning( "Restoring studio settings to default" );
			reset(); // ignore settings from version < 0.17.0
		}

		// set defaults
		if ( get<path>( "gait_analysis.template" ).empty() )
			set<path>( "gait_analysis.template", GetSconeStudioFolder().make_preferred() / "resources/gaitanalysis/default.zml" );

		if ( get<path>( "video.path_to_ffmpeg" ).empty() )
			set<path>( "video.path_to_ffmpeg", xo::get_application_dir() / SCONE_FFMPEG_EXECUTABLE );
	}

	xo::settings& GetStudioSettings()
	{
		static StudioSettings settings;
		return settings;
	}
}
