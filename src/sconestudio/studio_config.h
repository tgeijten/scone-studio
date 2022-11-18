/*
** studio_config.h
**
** Copyright (C) 2013-2019 Thomas Geijtenbeek and contributors. All rights reserved.
**
** This file is part of SCONE. For more information, see http://scone.software.
*/

#pragma once

namespace scone
{
#ifdef _MSC_VER
#	define SCONE_SCONECMD_EXECUTABLE "sconecmd.exe"
#	define SCONE_DEFAULT_PATH_TO_FFMPEG xo::get_application_dir() / "ffmpeg.exe"
#else
#	define SCONE_SCONECMD_EXECUTABLE "sconecmd"
#	define SCONE_DEFAULT_PATH_TO_FFMPEG "/usr/bin/ffmpeg"
#endif
}
