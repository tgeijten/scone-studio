#pragma once

#include "scone/model/Model.h"
#include "vis/trail.h"
#include "vis/material.h"
#include "vis/plane.h"
#include "vis/axes.h"
#include "vis/arrow.h"
#include "xo/utility/color_gradient.h"
#include "xo/utility/memoize.h"
#include "ViewOptions.h"

namespace scone
{
	class ModelVis
	{
	public:
		ModelVis( const Model& model, vis::scene& s, const ViewOptions& view_settings );
		~ModelVis();

		void Update( const Model& model );
		void SetFocusPoint( const Vec3& focus_point );

		void ApplyViewOptions( const ViewOptions& f );
		const ViewOptions& GetViewOptions() const { return view_flags; }

	private:
		struct MuscleVis
		{
			vis::trail ten1;
			vis::trail ten2;
			vis::trail ce;
			vis::material mat;
			float ce_pos = 0.5f;
			float muscle_radius = 0.0f;
			float tendon_radius = 0.0f;
		};

		void UpdateForceVis( index_t force_idx, Vec3 cop, Vec3 force, float len_scale = 0.001f, float rad_scale = 1.0f );
		void UpdateMomentVis( index_t moment_idx, Vec3 pos, Vec3 moment, float len_scale = 0.001f, float rad_scale = 1.0f );
		void UpdateMuscleVis( const class Muscle& mus, MuscleVis& vis );
		void UpdateShadowCast();

		vis::mesh MakeMesh( vis::node& parent,
			const xo::shape& sh, const xo::color& col, const vis::material& mat,
			const Vec3& pos, const Quat& ori, const Vec3& scale = Vec3::one() );
		vis::mesh MakeMesh( vis::node& parent,
			const xo::path& file, const vis::material& mat,
			const Vec3& pos, const Quat& ori, const Vec3& scale = Vec3::one(), const vis::mesh_options& mo = {} );

		// view settings
		ViewOptions view_flags;
		vis::plane ground_;
		vis::transformf ground_tf_;
		float ground_tile_size_;
		bool ground_follows_com_;
		vis::node root_node_;
		vis::arrow heading_;
		float specular_;
		float shininess_;
		float ambient_;
		const bool combine_contact_forces_;
		const bool forces_cast_shadows_;
		const bool joint_forces_are_for_parents_;
		vis::vec3f focus_point_;
		float fixed_muscle_width_;

		vis::material bone_mat;
		vis::material joint_mat;
		vis::material com_mat;
		vis::material muscle_mat;
		vis::material tendon_mat;
		vis::material ligament_mat;
		vis::material arrow_mat;
		vis::material moment_mat;
		vis::material contact_mat;
		vis::material auxiliary_mat;
		vis::material static_mat;
		vis::material object_mat;
		xo::color_gradient muscle_gradient;
		std::vector< vis::mesh > body_meshes;
		std::vector< vis::mesh > joints;
		std::vector< MuscleVis > muscles;
		std::vector< vis::trail > ligaments;
		std::vector< vis::arrow > forces;
		std::vector< vis::arrow > moments;
		std::vector< vis::axes > body_axes;
		std::vector< vis::mesh > body_com;
		std::vector< vis::node > bodies;
		std::vector< vis::mesh > contact_geoms;
		std::vector< vis::mesh > auxiliary_geoms;
		std::vector< vis::mesh > object_contact_geoms;
		std::vector< vis::mesh > static_contact_geoms;
		xo::memoize< vis::material( xo::color ) > color_materials_;
	};
}
