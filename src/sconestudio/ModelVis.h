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
		struct MuscleVis {
			vis::trail ten1;
			vis::trail ten2;
			vis::trail ce;
			vis::material mat;
			float ce_pos = 0.5f;
			float muscle_radius = 0.0f;
			float tendon_radius = 0.0f;
		};

		struct BodyVis {
			vis::node node;
			vis::axes axes;
			vis::mesh com;
			bool is_static = false;
		};

		std::tuple<Vec3, Real> GetArrowVec( Vec3 vec, Real length, Real scale, Real shape = -1.0 );
		void UpdateForceVis( index_t force_idx, Vec3 cop, Vec3 force, float rad_scale = 1.0f );
		void UpdateMomentVis( index_t moment_idx, Vec3 pos, Vec3 moment, float rad_scale = 1.0f );
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
		float joint_arrow_length_;
		float force_arrow_length_;
		float force_scale_ = 0.002f;
		float joint_force_scale_ = 0.002f;
		float moment_scale_ = 0.03f;
		Real arrow_shape_;
		vis::vec3f focus_point_;
		float fixed_muscle_width_;
		float spring_width_;
		float body_axes_length_;

		xo::color bone_color;
		vis::material bone_mat;
		xo::color joint_color;
		vis::material joint_mat;
		vis::material com_mat;
		vis::material muscle_mat;
		vis::material tendon_mat;
		vis::material ligament_mat;
		vis::material spring_mat;
		vis::material force_mat;
		vis::material joint_force_mat;
		vis::material moment_mat;
		vis::material contact_mat;
		vis::material auxiliary_mat;
		vis::material auxiliary_opaque_mat;
		vis::material static_mat;
		vis::material object_mat;
		xo::color_gradient muscle_gradient;

		std::vector< BodyVis > bodies;
		std::vector< vis::mesh > body_meshes;
		std::vector< vis::node > joints;
		std::vector< vis::mesh > joint_meshes;
		std::vector< vis::axes > joint_axes;
		std::vector< MuscleVis > muscles;
		std::vector< vis::trail > ligaments;
		std::vector< vis::trail > springs;
		std::vector< vis::arrow > forces;
		std::vector< vis::arrow > joint_forces;
		std::vector< vis::arrow > moments;
		std::vector< vis::mesh > contact_geoms;
		std::vector< vis::mesh > auxiliary_geoms;
		std::vector< vis::mesh > object_contact_geoms;
		std::vector< vis::mesh > static_contact_geoms;
		xo::memoize< vis::material( xo::color ) > color_materials_;
	};
}
