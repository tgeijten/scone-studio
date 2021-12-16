#pragma once

#include "xo/container/flag_set.h"

namespace scone
{
	// IMPORTANT: Always add new settings at the end, to preserve load/save settings
	enum class ViewOption {
		ExternalForces,
		Muscles,
		Tendons,
		BodyGeom,
		Joints,
		BodyAxes,
		ContactGeom,
		GroundPlane,
		Shadows,
		BodyCom,
		ModelComHeading,
		StaticCamera,
		MuscleActivation,
		MuscleForce,
		MuscleFiberLength
	};

	using ViewOptions = xo::flag_set<ViewOption>;

	inline ViewOptions MakeDefaultViewOptions() {
		return ViewOptions( {
			ViewOption::ExternalForces,
			ViewOption::Muscles,
			ViewOption::Tendons,
			ViewOption::BodyGeom,
			ViewOption::ContactGeom,
			ViewOption::GroundPlane,
			ViewOption::Shadows,
			ViewOption::MuscleActivation
			} );
	}
}
