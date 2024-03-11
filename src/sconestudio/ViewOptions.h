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
		MuscleFiberLength,
		MuscleRadiusFixed,
		MuscleRadiusPcsa,
		MuscleRadiusPcsaDynamic
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
			ViewOption::MuscleActivation,
			ViewOption::MuscleRadiusPcsaDynamic
			} );
	}

	inline void FixViewOptions( ViewOptions& vo, std::initializer_list<ViewOption> ex_opt ) {
		// make sure only one muscle visualization option is set
		if ( vo.count( ex_opt ) != 1 ) {
			for ( auto opt : ex_opt )
				vo.set( opt, opt == *ex_opt.begin() );
		}
	}

	inline ViewOptions MakeViewOptions( xo::uint64 value ) {
		ViewOptions vo( value );
		FixViewOptions( vo, { ViewOption::MuscleActivation, ViewOption::MuscleForce, ViewOption::MuscleFiberLength } );
		FixViewOptions( vo, { ViewOption::MuscleRadiusPcsaDynamic, ViewOption::MuscleRadiusFixed, ViewOption::MuscleRadiusPcsa } );
		return vo;
	}
}
