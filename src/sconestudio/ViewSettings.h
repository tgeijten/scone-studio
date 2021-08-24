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

	using ViewSettings = xo::flag_set<ViewOption>;

	inline ViewSettings MakeDefaultViewSettings() {
		return ViewSettings( { ViewOption::ShowForces, ViewOption::ShowMuscles, ViewOption::ShowTendons, ViewOption::ShowBodyGeom, ViewOption::EnableShadows, ViewOption::ShowModelComHeading } );
	}
}
