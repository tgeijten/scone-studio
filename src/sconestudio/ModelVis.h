#pragma once

#include "scone/model/Model.h"
#include "vis/trail.h"
#include "vis/material.h"
#include "vis/plane.h"
#include "vis/axes.h"
#include "vis/arrow.h"
#include "xo/utility/color_gradient.h"
#include "ViewSettings.h"

namespace scone
{
	class ModelVis
	{
	public:
		ModelVis( const Model& model, vis::scene& s, const ViewSettings& view_settings );
		~ModelVis();

		void Update( const Model& model );

		void ApplyViewSettings( const ViewSettings& f );
		const ViewSettings& GetViewSettings() const { return view_flags; }

	private:
		struct MuscleVis
		{
			vis::trail ten1;
			vis::trail ten2;
			vis::trail ce;
			vis::material mat;
			float ce_pos = 0.5f;
		};

		void UpdateForceVis( index_t force_idx, Vec3 cop, Vec3 force );
		void UpdateMuscleVis( const class Muscle& mus, MuscleVis& vis );

		vis::mesh MakeMesh( vis::node& parent,
			const xo::shape& sh, const xo::color& col, const vis::material& mat,
			const Vec3& pos, const Quat& ori, const Vec3& scale = Vec3::diagonal( 1.0 ) );
		vis::mesh MakeMesh( vis::node& parent,
			const xo::path& file, const vis::material& mat,
			const Vec3& pos, const Quat& ori, const Vec3& scale = Vec3::diagonal( 1.0 ) );

		// view settings
		ViewSettings view_flags;
		vis::plane ground_;
		vis::node root_node_;
		vis::arrow heading_;
		float specular_;
		float shininess_;
		float ambient_;
		vis::material bone_mat;
		vis::material joint_mat;
		vis::material com_mat;
		vis::material muscle_mat;
		vis::material tendon_mat;
		vis::material arrow_mat;
		vis::material contact_mat;
		xo::color_gradient muscle_gradient;
		std::vector< vis::mesh > body_meshes;
		std::vector< vis::mesh > joints;
		std::vector< MuscleVis > muscles;
		std::vector< vis::arrow > forces;
		std::vector< vis::axes > body_axes;
		std::vector< vis::mesh > body_com;
		std::vector< vis::node > bodies;
		std::vector< vis::mesh > contact_geoms;
	};
}
