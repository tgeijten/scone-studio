#include "ModelVis.h" 

#include "StudioSettings.h"
#include "vis/scene.h"
#include "xo/filesystem/filesystem.h"
#include "scone/core/Log.h"
#include "scone/model/Muscle.h"
#include "scone/model/Ligament.h"
#include "scone/model/Spring.h"
#include "scone/model/Joint.h"
#include "xo/geometry/path_alg.h"
#include "scone/core/math.h"
#include "xo/geometry/quat.h"
#include "scone/core/Settings.h"

namespace scone
{
	using namespace xo::angle_literals;

	ModelVis::ModelVis( const Model& model, vis::scene& s, const ViewOptions& settings ) :
		view_flags( settings ),
		ground_tile_size_( GetStudioSetting<float>( "viewer.tile_size" ) ),
		ground_follows_com_( GetStudioSetting<bool>( "viewer.tiles_follow_body" ) ),
		root_node_( &s ),
		specular_( GetStudioSetting<float>( "viewer.specular" ) ),
		shininess_( GetStudioSetting<float>( "viewer.shininess" ) ),
		ambient_( GetStudioSetting<float>( "viewer.ambient" ) ),
		combine_contact_forces_( GetStudioSetting<bool>( "viewer.combine_contact_forces" ) ),
		forces_cast_shadows_( GetStudioSetting<bool>( "viewer.forces_cast_shadows" ) ),
		joint_forces_are_for_parents_( GetStudioSetting<bool>( "viewer.joint_forces_are_for_parents" ) ),
		joint_arrow_length_( GetStudioSetting<float>( "viewer.joint_arrow_length" ) ),
		force_arrow_length_( GetStudioSetting<float>( "viewer.force_arrow_length" ) ),
		arrow_shape_( GetStudioSetting<float>( "viewer.arrow_shape" ) ),
		fixed_muscle_width_( GetStudioSetting<float>( "viewer.muscle_width" ) ),
		spring_width_( GetStudioSetting<float>( "viewer.spring_width" ) ),
		bone_mat( { GetStudioSetting<xo::color>( "viewer.bone" ), specular_, shininess_, ambient_ } ),
		joint_mat( { GetStudioSetting<xo::color>( "viewer.joint" ), specular_, shininess_, ambient_ } ),
		com_mat( { GetStudioSetting<xo::color>( "viewer.com" ), specular_, shininess_, ambient_ } ),
		muscle_mat( { GetStudioSetting<xo::color>( "viewer.muscle_0" ), specular_, shininess_, ambient_ } ),
		tendon_mat( { GetStudioSetting<xo::color>( "viewer.tendon" ), specular_, shininess_, ambient_ } ),
		ligament_mat( { GetStudioSetting<xo::color>( "viewer.ligament" ), specular_, shininess_, ambient_ } ),
		spring_mat( { GetStudioSetting<xo::color>( "viewer.spring" ), specular_, shininess_, ambient_ } ),
		force_mat( { GetStudioSetting<xo::color>( "viewer.force" ), specular_, shininess_, ambient_, GetStudioSetting<float>( "viewer.force_alpha" ) } ),
		joint_force_mat( { GetStudioSetting<xo::color>( "viewer.joint_force" ), specular_, shininess_, ambient_, GetStudioSetting<float>( "viewer.force_alpha" ) } ),
		moment_mat( { GetStudioSetting<xo::color>( "viewer.moment" ), specular_, shininess_, ambient_ } ),
		contact_mat( { GetStudioSetting<xo::color>( "viewer.contact" ), specular_, shininess_, ambient_, GetStudioSetting<float>( "viewer.contact_alpha" ) } ),
		auxiliary_mat( { GetStudioSetting<xo::color>( "viewer.auxiliary" ), specular_, shininess_, ambient_, GetStudioSetting<float>( "viewer.auxiliary_alpha" ) } ),
		static_mat( { GetStudioSetting<xo::color>( "viewer.static" ), 0.0f, 0.0f, ambient_ } ),
		object_mat( { GetStudioSetting<xo::color>( "viewer.object" ), 0.0f, 0.0f, ambient_ } ),
		muscle_gradient( {
			{ -1.0f, GetStudioSetting<xo::color>( "viewer.muscle_min100" ) },
			{ 0.0f, GetStudioSetting<xo::color>( "viewer.muscle_0" ) },
			{ 0.5f, GetStudioSetting<xo::color>( "viewer.muscle_50" ) },
			{ 1.0f, GetStudioSetting<xo::color>( "viewer.muscle_100" ) } } ),
			color_materials_( [&]( const xo::color& c ) { return vis::material( { c, specular_, shininess_, ambient_, c.a } ); } )
	{
		// ground plane
		if ( auto* gp = model.GetGroundPlane() )
		{
			auto& plane = std::get<xo::plane>( gp->GetShape() );
			auto col1 = GetStudioSetting<xo::color>( "viewer.tile1" );
			auto col2 = GetStudioSetting<xo::color>( "viewer.tile2" );
			auto tile_count_x = GetStudioSetting<int>( "viewer.tile_count_x" );
			auto tile_count_z = GetStudioSetting<int>( "viewer.tile_count_z" );
			ground_ = vis::plane( root_node_, tile_count_x, tile_count_z, ground_tile_size_, col1, col2 );
			auto normal_rot = xo::quat_from_directions( xo::vec3f::unit_y(), plane.normal_ );
			//ground_plane = scene_.add< vis::plane >( xo::vec3f( 64, 0, 0 ), xo::vec3f( 0, 0, -64 ), GetFolder( SCONE_UI_RESOURCE_FOLDER ) / "stile160.png", 64, 64 );
			ground_tf_.p = vis::vec3f( gp->GetPos() );
			ground_tf_.q = normal_rot * xo::quatf( gp->GetOri() );
			ground_.pos_ori( ground_tf_.p, ground_tf_.q );
		}

		auto model_folder = model.GetModelFile().parent_path();
		auto geom_model_dirs = { model_folder, model_folder / "geometry", model_folder / "Geometry" };
		auto geometry_extra = GetSconeSetting<string>( "folders.geometry_extra" );
		std::vector<path> geom_search_dirs;
		geom_search_dirs.emplace_back( GetFolder( SconeFolder::Geometry ) );
		auto geometry_extra_vec = xo::split_str( geometry_extra, ";," );
		std::copy( geometry_extra_vec.begin(), geometry_extra_vec.end(), std::back_inserter( geom_search_dirs ) );
		string new_geom_dir;
		for ( auto& body : model.GetBodies() )
		{
			auto& body_node = bodies.emplace_back( vis::node( &root_node_ ) );
			body_node.set_name( body->GetName().c_str() );
			body_axes.push_back( vis::axes( body_node, vis::axes_info{ vis::vec3f::diagonal( 0.1 ) } ) );
			body_axes.back().set_cast_shadows( body->GetMass() > 0 );

			if ( body->GetMass() > 0 )
			{
				body_com.push_back( vis::mesh( body_node, vis::shape_info{ xo::sphere( 0.02f ), xo::color::green(), xo::vec3f::zero(), 0.75f } ) );
				body_com.back().set_material( com_mat );
				body_com.back().pos( xo::vec3f( body->GetLocalComPos() ) );
			}

			for ( auto& dg : body->GetDisplayGeometries() )
			{
				if ( !dg.filename_.empty() )
				{
					// MESH FILE
					dg.filename_.make_preferred();
					try {
						auto geom_file = xo::try_find_file( dg.filename_, geom_model_dirs );
						if ( geom_file ) {
							// remember for future geometry_extra (used for future results playback)
							if ( new_geom_dir.empty() && !xo::try_find_file( dg.filename_, geom_search_dirs ) )
								new_geom_dir = xo::left_str( geom_file->str(), -(int)dg.filename_.str().size() );
						}
						else geom_file = xo::try_find_file( dg.filename_, geom_search_dirs );

						if ( geom_file ) {
							log::trace( "Loading geometry for body ", body->GetName(), ": ", *geom_file );
							vis::mesh_options mo;
							mo.mirror_on_load = dg.options_.get<DisplayGeometryOptions::mirror>();
							const vis::material& mat = dg.color_.is_null() ? bone_mat : color_materials_( dg.color_ );
							body_meshes.push_back( MakeMesh( body_node, *geom_file, mat, dg.pos_, dg.ori_, dg.scale_, mo ) );
							body_meshes.back().set_name( body->GetName().c_str() );
						}
						else log::warning( "Could not find ", dg.filename_ );
					}
					catch ( std::exception& e ) {
						log::warning( "Could not load ", dg.filename_, ": ", e.what() );
					}
				}
				else {
					// SHAPE
					if ( dg.options_.get<DisplayGeometryOptions::auxiliary>() ) {
						const vis::material& mat = auxiliary_mat;
						auxiliary_geoms.emplace_back(
							MakeMesh( body_node, dg.shape_, xo::color::cyan(), mat, dg.pos_, dg.ori_, dg.scale_ ) );
						auxiliary_geoms.back().set_name( "!" );
					}
					else {
						const vis::material& mat = dg.color_.is_null() ? object_mat : color_materials_( dg.color_ );
						body_meshes.emplace_back(
							MakeMesh( body_node, dg.shape_, xo::color::cyan(), mat, dg.pos_, dg.ori_, dg.scale_ ) );
						bool clickable = dg.color_.is_null() || dg.color_.a == 1;
						body_meshes.back().set_name( clickable ? body->GetName().c_str() : "!" );
					}
				}
			}
		}

		// update search dirs in settings
		if ( !new_geom_dir.empty() && !xo::contains( geometry_extra_vec, new_geom_dir ) ) {
			log::debug( "Adding ", new_geom_dir, " to geometry search paths" );
			xo::append_str( geometry_extra, new_geom_dir, ";" );
			GetSconeSettings().set( "folders.geometry_extra", geometry_extra );
			GetSconeSettings().save();
		}

		for ( auto& cg : model.GetContactGeometries() )
		{
			auto idx = xo::find_index_if( model.GetBodies(), [&]( const auto& b ) { return &cg->GetBody() == b; } );
			auto& parent_node = idx != NoIndex ? bodies[idx] : root_node_;
			bool has_display_geom = !cg->GetBody().GetDisplayGeometries().empty();
			bool is_static = cg->GetBody().GetMass() == 0 && !has_display_geom;
			bool is_object_geom = cg->GetPos().is_null() && !has_display_geom;
			vis::mesh geom_mesh;
			if ( cg->HasFileName() )
			{
				auto model_folder = model.GetModelFile().parent_path();
				auto geom_file = FindFile( model_folder / cg->GetFileName() );
				geom_mesh = MakeMesh( parent_node, geom_file, is_static ? static_mat : contact_mat, cg->GetPos(), cg->GetOri() );
			}
			else if ( !std::holds_alternative<xo::plane>( cg->GetShape() ) )
			{
				auto& mat = is_static ? static_mat : ( is_object_geom ? object_mat : contact_mat );
				geom_mesh = MakeMesh( parent_node, cg->GetShape(), xo::color::cyan(), mat, cg->GetPos(), cg->GetOri() );
			}

			// add the mesh to the right container
			if ( geom_mesh ) {
				geom_mesh.set_name( cg->GetName().c_str() );
				geom_mesh.set_cast_shadows( GetStudioSetting<bool>( "viewer.contact_geometries_cast_shadows" ) );
				auto& geom_cont = is_static ? static_contact_geoms : is_object_geom ? object_contact_geoms : contact_geoms;
				geom_cont.push_back( std::move( geom_mesh ) );
			}
		}

		const auto auto_muscle_width = GetStudioSetting<bool>( "viewer.auto_muscle_width" );
		const auto auto_muscle_width_factor = GetStudioSetting<float>( "viewer.auto_muscle_width_factor" );
		const auto relative_tendon_width = GetStudioSetting<float>( "viewer.relative_tendon_width" );
		const auto muscle_position = GetStudioSetting<float>( "viewer.muscle_position" );

		for ( auto& muscle : model.GetMuscles() )
		{
			MuscleVis mv;
			mv.muscle_radius = auto_muscle_width_factor * float( std::sqrt( muscle->GetPCSA() / xo::constantsd::pi() ) );
			mv.tendon_radius = relative_tendon_width * mv.muscle_radius;

			// add path
			mv.ten1 = vis::trail( root_node_, vis::trail_info{ mv.tendon_radius, xo::color::yellow(), 0.3f } );
			mv.ten2 = vis::trail( root_node_, vis::trail_info{ mv.tendon_radius, xo::color::yellow(), 0.3f } );
			mv.ten1.set_material( tendon_mat );
			mv.ten2.set_material( tendon_mat );
			mv.ten1.set_name( muscle->GetName().c_str() );
			mv.ten2.set_name( muscle->GetName().c_str() );
			mv.ce = vis::trail( root_node_, vis::trail_info{ mv.muscle_radius, xo::color::red(), 0.5f } );
			mv.mat = muscle_mat.clone();
			mv.mat.emissive( vis::color() );
			mv.ce.set_material( mv.mat );
			mv.ce.set_name( muscle->GetName().c_str() );
			mv.ce_pos = muscle_position;
			muscles.push_back( std::move( mv ) );
		}

		for ( auto& ligament : model.GetLigaments() )
		{
			auto lv = vis::trail( root_node_, vis::trail_info{ fixed_muscle_width_, xo::color::yellow(), 0.3f } );
			lv.set_material( ligament_mat );
			ligaments.push_back( std::move( lv ) );
		}

		for ( auto* spr : model.GetSprings() )
		{
			auto sv = vis::trail( root_node_, vis::trail_info{ spring_width_, xo::color::yellow(), 0.3f } );
			sv.set_material( spring_mat );
			springs.push_back( std::move( sv ) );
		}

		const auto joint_radius = GetStudioSetting<float>( "viewer.joint_radius" );
		for ( auto& j : model.GetJoints() )
		{
			joints.push_back( vis::mesh( root_node_, vis::shape_info{ xo::sphere( joint_radius ), xo::color::red(), xo::vec3f::zero(), 0.75f } ) );
			joints.back().set_material( joint_mat );
			joints.back().set_name( j->GetName().c_str() );

			joint_forces.emplace_back( root_node_, vis::arrow_info{ 0.01, 0.02, xo::color::yellow(), 0.3f } );
			joint_forces.back().set_material( joint_force_mat );
			joint_forces.back().show( view_flags( ViewOption::JointReactionForces ) );
			joint_forces.back().set_cast_shadows( forces_cast_shadows_ );
		}

		heading_ = vis::arrow( root_node_, vis::arrow_info{ 0.01f, 0.02f, xo::color::green() } );
		heading_.set_material( com_mat );

		ApplyViewOptions( view_flags );
	}

	ModelVis::~ModelVis()
	{}

	void ModelVis::Update( const Model& model )
	{
		index_t force_count = 0;
		index_t moment_count = 0;

		// update bodies
		auto& model_bodies = model.GetBodies();
		for ( index_t i = 0; i < model_bodies.size(); ++i ) {
			auto& b = model_bodies[i];
			bodies[i].pos_ori( vis::vec3f( b->GetOriginPos() ), vis::quatf( b->GetOrientation() ) );

			// contact forces
			if ( auto f = b->GetContactForce(); view_flags( ViewOption::ExternalForces ) && combine_contact_forces_ && !f.is_null() ) {
				auto [vec, r] = GetArrowVec( f, force_arrow_length_, force_scale_ );
				UpdateForceVis( force_count++, b->GetContactPoint(), vec, r );
			}

			// external forces
			if ( auto f = b->GetExternalForce(); !f.is_null() ) {
				auto [vec, r] = GetArrowVec( f, force_arrow_length_, force_scale_ );
				UpdateForceVis( force_count++, b->GetPosOfPointOnBody( b->GetExternalForcePoint() ), vec, r );
			}

			// external moments
			if ( auto m = b->GetExternalMoment(); !m.is_null() )
				UpdateMomentVis( moment_count++, b->GetComPos(), m );
		}

		// update muscle paths
		auto& model_muscles = model.GetMuscles();
		for ( index_t i = 0; i < model_muscles.size(); ++i )
			UpdateMuscleVis( *model_muscles[i], muscles[i] );

		// update ligament paths
		auto& model_ligaments = model.GetLigaments();
		for ( index_t i = 0; i < model_ligaments.size(); ++i ) {
			auto p = model_ligaments[i]->GetLigamentPath();
			ligaments[i].set_points( p.begin(), p.end() );
		}

		// update spring paths
		auto& model_springs = model.GetSprings();
		for ( index_t i = 0; i < model_springs.size(); ++i ) {
			const auto& spr = *model_springs[i];
			springs[i].show( spr.IsActive() );
			if ( spr.IsActive() ) {
				auto p = std::array{ spr.GetParentPos(), spr.GetChildPos() };
				springs[i].set_points( p.begin(), p.end() );
			}
		}

		// update joints
		auto& model_joints = model.GetJoints();
		auto sign = joint_forces_are_for_parents_ ? -1.0 : 1.0;
		for ( index_t i = 0; i < model_joints.size(); ++i ) {
			auto pos = model_joints[i]->GetPos();
			joints[i].pos( vis::vec3f( pos ) );
			if ( view_flags( ViewOption::JointReactionForces ) ) {
				auto [vec, r] = GetArrowVec( sign * model_joints[i]->GetReactionForce(), joint_arrow_length_, joint_force_scale_ );
				joint_forces[i].pos_ofs( vis::vec3f( pos ), vis::vec3f( vec ), r );
			}
			if ( view_flags( ViewOption::Joints ) ) {
				auto [vec, r] = GetArrowVec( sign * model_joints[i]->GetLimitTorque(), joint_arrow_length_, moment_scale_ );
				UpdateMomentVis( moment_count++, pos - 0.5 * vec, vec, r );
			}
		}

		// update individual contact forces
		if ( view_flags( ViewOption::ExternalForces ) && !combine_contact_forces_ ) {
			auto fvec = model.GetContactForceValues();
			for ( auto& cf : fvec ) {
				auto [vec, r] = GetArrowVec( cf.force, force_arrow_length_, force_scale_ );
				UpdateForceVis( force_count++, cf.point, vec, r );
			}
		}

		// update com / heading
		if ( view_flags( ViewOption::ModelComHeading ) ) {
			if ( model.HasRootBody() ) {
				auto pos = xo::vec3f( model.GetComPos() );
				auto dir = xo::quatf( model.GetRootBody().GetOrientation() ) * xo::vec3f( 0.5f, 0, 0 );
				heading_.pos_ofs( pos, dir );
			}
		}

		if ( ground_follows_com_ && ground_ ) {
			auto d = ground_tile_size_ * 2;
			auto pos = xo::vec3f( model.GetProjectedOntoGround( model.GetComPos() ) );
			auto x_pos = std::floor( xo::dot_product( ground_tf_ * xo::vec3f::unit_x(), pos ) / d + 0.5f ) * d;
			auto z_pos = std::floor( xo::dot_product( ground_tf_ * xo::vec3f::unit_z(), pos ) / d + 0.5f ) * d;
			auto new_pos = ground_tf_.q * xo::vec3f( x_pos, ground_tf_.p.y, z_pos );
			ground_.pos( new_pos );
		}

		// finalize update: remove remaining arrows
		while ( force_count < forces.size() )
			forces.pop_back();
		while ( moment_count < moments.size() )
			moments.pop_back();
	}

	std::tuple<scone::Vec3, Real> ModelVis::GetArrowVec( Vec3 vec, Real length, Real scale, Real shape )
	{
		if ( shape < 0 )
			shape = arrow_shape_;

		auto v = scale * xo::normalize( vec );
		if ( xo::equal( v, Real( 0 ) ) )
			return { Vec3::zero(), 0.0 };

		auto r = std::pow( v, shape / 2 );
		auto l = length * v / ( r * r );

		return { l * vec, r };
	}

	void ModelVis::UpdateForceVis( index_t force_idx, Vec3 cop, Vec3 force, float rad_scale )
	{
		while ( forces.size() <= force_idx )
		{
			forces.emplace_back( root_node_, vis::arrow_info{ 0.01, 0.02, xo::color::yellow(), 0.3f } );
			forces.back().set_material( force_mat );
			forces.back().show( view_flags( ViewOption::ExternalForces ) );
			forces.back().set_cast_shadows( forces_cast_shadows_ );
		}
		forces[force_idx].pos_ofs( vis::vec3f( cop ), vis::vec3f( force ), rad_scale );
	}

	void ModelVis::UpdateMomentVis( index_t moment_idx, Vec3 pos, Vec3 moment, float rad_scale )
	{
		while ( moments.size() <= moment_idx )
		{
			moments.emplace_back( root_node_, vis::arrow_info{ 0.01, 0.02, xo::color::red(), 0.3f } );
			moments.back().set_material( moment_mat );
			moments.back().show( view_flags( ViewOption::Joints ) );
			moments.back().set_cast_shadows( forces_cast_shadows_ );
		}
		moments[moment_idx].pos_ofs( vis::vec3f( pos ), vis::vec3f( moment ), rad_scale );
	}

	void ModelVis::UpdateMuscleVis( const class Muscle& mus, MuscleVis& vis )
	{
		auto mlen = mus.GetFiberLength();
		if ( mlen <= 0.0 ) {
			log::warning( mus.GetName(), " muscle fiber length: ", mlen, "; clamping to zero" );
			mlen = 0.0;
		}
		auto tlen = mus.GetTendonLength() * vis.ce_pos;
		if ( tlen <= 0.0 ) {
			log::trace( mus.GetName(), " muscle tendon length: ", tlen, "; clamping to zero" );
			tlen = 0.0;
		}

		Real a;
		if ( view_flags( ViewOption::MuscleActivation ) )
			a = mus.GetActivation();
		else if ( view_flags( ViewOption::MuscleForce ) )
			a = mus.GetNormalizedForce();
		else if ( view_flags( ViewOption::MuscleFiberLength ) )
			a = 1.5 * ( mus.GetNormalizedFiberLength() - 1 );
		else a = 0.0;

		float mw = 1.0f, tw = 1.0f;
		if ( view_flags( ViewOption::MuscleRadiusFixed ) )
			mw = tw = fixed_muscle_width_ / vis.muscle_radius;
		else if ( view_flags( ViewOption::MuscleRadiusPcsa ) )
			mw = tw = 1.0f;
		else if ( view_flags( ViewOption::MuscleRadiusPcsaDynamic ) )
			mw = float( std::sqrt( 1.0 / mus.GetNormalizedFiberLength() ) );

		xo::color c = muscle_gradient( float( a ) );
		vis.mat.diffuse( c );
		vis.mat.ambient( c );

		auto p = mus.GetMusclePath();
		if ( view_flags( ViewOption::Tendons ) )
		{
			auto i1 = insert_path_point( p, tlen );
			auto i2 = insert_path_point( p, tlen + mlen );
			SCONE_ASSERT( i1 <= i2 );
			vis.ten1.set_points( p.begin(), p.begin() + i1 + 1, tw );
			vis.ce.set_points( p.begin() + i1, p.begin() + i2 + 1, mw );
			vis.ten2.set_points( p.begin() + i2, p.end(), tw );
		}
		else vis.ce.set_points( p.begin(), p.end(), mw );
	}

	void ModelVis::UpdateShadowCast()
	{
		auto squared_dist = xo::squared( GetStudioSetting<float>( "viewer.max_shadow_dist" ) );
		for ( index_t i = 1; i < bodies.size(); ++i ) // skip ground body; #todo: skip all static bodies?
			bodies[i].set_cast_shadows( xo::squared_distance( bodies[i].pos(), focus_point_ ) < squared_dist );
		for ( auto& b : static_contact_geoms )
			b.set_cast_shadows( xo::squared_distance( b.pos(), focus_point_ ) < squared_dist );
	}

	vis::mesh ModelVis::MakeMesh(
		vis::node& parent, const xo::shape& sh, const xo::color& col, const vis::material& mat,
		const Vec3& pos, const Quat& ori, const Vec3& scale )
	{
		auto msh = vis::mesh( parent, vis::shape_info{ sh, col, xo::vec3f::zero(), 0.75f } );
		msh.set_material( mat );
		auto y_to_z = std::holds_alternative<xo::capsule>( sh ) || std::holds_alternative<xo::cylinder>( sh ) || std::holds_alternative<xo::cone>( sh );
		auto fixed_ori = y_to_z ? vis::quatf( ori ) * xo::quat_from_x_angle( vis::degreef( 90 ) ) : vis::quatf( ori );
		msh.pos_ori( vis::vec3f( pos ), fixed_ori );
		msh.scale_enable_normalize( vis::vec3f( scale ) );
		return msh;
	}

	vis::mesh ModelVis::MakeMesh(
		vis::node& parent, const xo::path& file, const vis::material& mat,
		const Vec3& pos, const Quat& ori, const Vec3& scale, const vis::mesh_options& mo )
	{
		auto msh = vis::mesh( parent, file, mo );
		msh.set_material( mat );
		auto fix_obj_ori = file.extension_no_dot() == "obj";
		auto fixed_ori = fix_obj_ori ? vis::quatf( ori ) * xo::quat_from_x_angle( -90_degf ) : vis::quatf( ori );
		msh.pos_ori( vis::vec3f( pos ), fixed_ori );
		auto fixed_scale = fix_obj_ori ? vis::vec3f( scale.x, scale.z, scale.y ) : vis::vec3f( scale );
		msh.scale_enable_normalize( fixed_scale );
		return msh;
	}

	void ModelVis::SetFocusPoint( const Vec3& focus_point )
	{
		focus_point_ = vis::vec3f( focus_point );
		UpdateShadowCast();
	}

	void ModelVis::ApplyViewOptions( const ViewOptions& f )
	{
		view_flags = f;
		for ( auto& f : forces )
			f.show( view_flags( ViewOption::ExternalForces ) || view_flags( ViewOption::JointReactionForces ) );
		for ( auto& m : moments )
			m.show( view_flags( ViewOption::Joints ) );

		for ( auto& m : muscles )
		{
			m.ce.show( view_flags( ViewOption::Muscles ) );
			m.ten1.show( view_flags( ViewOption::Muscles ) && view_flags( ViewOption::Tendons ) );
			m.ten2.show( view_flags( ViewOption::Muscles ) && view_flags( ViewOption::Tendons ) );
		}

		for ( auto& l : ligaments )
			l.show( view_flags( ViewOption::Muscles ) );

		for ( auto& s : springs )
			s.show( view_flags( ViewOption::BodyGeom ) );

		for ( auto& e : joints )
			e.show( view_flags( ViewOption::Joints ) || view_flags( ViewOption::JointReactionForces ) );

		for ( auto& e : joint_forces )
			e.show( view_flags( ViewOption::JointReactionForces ) );

		for ( auto& e : body_meshes )
			e.show( view_flags( ViewOption::BodyGeom ) );

		for ( auto& e : body_axes )
			e.show( view_flags( ViewOption::BodyAxes ) );

		for ( auto& e : body_com )
			e.show( view_flags( ViewOption::BodyCom ) );

		for ( auto& e : contact_geoms )
			e.show( view_flags( ViewOption::ContactGeom ) );

		for ( auto& e : auxiliary_geoms )
			e.show( view_flags( ViewOption::AuxiliaryGeom ) );

		for ( auto& e : object_contact_geoms )
			e.show( view_flags( ViewOption::ContactGeom ) );

		if ( ground_.node_id() )
			ground_.show( view_flags( ViewOption::GroundPlane ) );

		heading_.show( view_flags( ViewOption::ModelComHeading ) );
	}
}
