#pragma once

#include "xo/container/flag_set.h"

namespace scone
{
	// IMPORTANT: Always add new settings at the end, to preserve load/save settings
	enum class ViewOption {
		ShowForces,
		ShowMuscles,
		ShowTendons,
		ShowBodyGeom,
		ShowJoints,
		ShowBodyAxes,
		ShowContactGeom,
		ShowGroundPlane,
		EnableShadows,
		ShowBodyCom,
		ShowModelComHeading,
		StaticCamera
	};

	using ViewOptions = xo::flag_set<ViewOption>;

	inline ViewOptions MakeDefaultViewOptions() {
		return ViewOptions( { ViewOption::ShowForces, ViewOption::ShowMuscles, ViewOption::ShowTendons, ViewOption::ShowBodyGeom, ViewOption::EnableShadows, ViewOption::ShowModelComHeading } );
	}
}
