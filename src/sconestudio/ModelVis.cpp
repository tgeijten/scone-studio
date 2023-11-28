#include "ModelVis.h" 

#include "StudioSettings.h"
#include "vis/scene.h"
#include "xo/filesystem/filesystem.h"
#include "scone/core/Log.h"
#include "scone/model/Muscle.h"
#include "scone/model/Ligament.h"
#include "scone/model/Joint.h"
#include "xo/geometry/path_alg.h"
#include "scone/core/math.h"
#include "xo/geometry/quat.h"

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
		bone_mat( { GetStudioSetting<xo::color>( "viewer.bone" ), specular_, shininess_, ambient_ } ),
		joint_mat( { GetStudioSetting<xo::color>( "viewer.joint" ), specular_, shininess_, ambient_ } ),
		com_mat( { GetStudioSetting<xo::color>( "viewer.com" ), specular_, shininess_, ambient_ } ),
		muscle_mat( { GetStudioSetting<xo::color>( "viewer.muscle_0" ), specular_, shininess_, ambient_ } ),
		tendon_mat( { GetStudioSetting<xo::color>( "viewer.tendon" ), specular_, shininess_, ambient_ } ),
		ligament_mat( { GetStudioSetting<xo::color>( "viewer.ligament" ), specular_, shininess_, ambient_ } ),
		arrow_mat( { GetStudioSetting<xo::color>( "viewer.force" ), specular_, shininess_, ambient_, GetStudioSetting<float>( "viewer.force_alpha" ) } ),
		moment_mat( { GetStudioSetting<xo::color>( "viewer.moment" ), specular_, shininess_, ambient_ } ),
		contact_mat( { GetStudioSetting<xo::color>( "viewer.contact" ), specular_, shininess_, ambient_, GetStudioSetting<float>( "viewer.contact_alpha" ) } ),
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

		for ( auto& body : model.GetBodies() )
		{
			bodies.push_back( vis::node( &root_node_ ) );
			bodies.back().set_name( body->GetName().c_str() );
			body_axes.push_back( vis::axes( bodies.back(), vis::axes_info{ vis::vec3f::diagonal( 0.1 ) } ) );
			body_axes.back().set_cast_shadows( body->GetMass() > 0 );

			if ( body->GetMass() > 0 )
			{
				body_com.push_back( vis::mesh( bodies.back(), vis::shape_info{ xo::sphere( 0.02f ), xo::color::green(), xo::vec3f::zero(), 0.75f } ) );
				body_com.back().set_material( com_mat );
				body_com.back().pos( xo::vec3f( body->GetLocalComPos() ) );
			}

			auto geoms = body->GetDisplayGeometries();
			for ( auto& geom : geoms )
			{
				if ( !geom.filename_.empty() )
				{
					geom.filename_.make_preferred();
					try {
						auto model_folder = model.GetModelFile().parent_path();
						auto file_options = {
							geom.filename_,
							path( "geometry" ) / geom.filename_,
							model_folder / geom.filename_,
							model_folder / "geometry" / geom.filename_,
							GetFolder( SconeFolder::Geometry ) / geom.filename_
						};
						auto geom_file = xo::try_find_file( file_options );
						if ( geom_file )
						{
							log::trace( "Loading geometry for body ", body->GetName(), ": ", *geom_file );
							body_meshes.push_back( MakeMesh( bodies.back(), *geom_file, bone_mat, geom.pos_, geom.ori_, geom.scale_ ) );
							body_meshes.back().set_name( body->GetName().c_str() );
						}
						else log::warning( "Could not find ", geom.filename_ );
					}
					catch ( std::exception& e ) {
						log::warning( "Could not load ", geom.filename_, ": ", e.what() );
					}
				}
				else {
					// shape
					const vis::material& mat = geom.color_.is_null() ? object_mat : color_materials_( geom.color_ );
					body_meshes.push_back( MakeMesh(
						bodies.back(), geom.shape_, xo::color::cyan(), mat, geom.pos_, geom.ori_, geom.scale_ ) );
					bool clickable = geom.color_.is_null() || geom.color_.a == 1;
					body_meshes.back().set_name( clickable ? body->GetName().c_str() : "!" );
				}
			}
		}

		for ( auto& cg : model.GetContactGeometries() )
		{
			auto idx = xo::find_index_if( model.GetBodies(), [&]( const auto& b ) { return &cg->GetBody() == b; } );
			auto& parent_node = idx != NoIndex ? bodies[idx] : root_node_;
			//bool is_static = idx == 0 || idx == NoIndex;
			bool is_static = cg->GetBody().GetMass() == 0;
			bool use_bone_mat = cg->GetPos().is_null() && cg->GetBody().GetDisplayGeometries().empty();
			vis::mesh geom_mesh;
			if ( cg->HasFileName() )
			{
				auto model_folder = model.GetModelFile().parent_path();
				auto geom_file = FindFile( model_folder / cg->GetFileName() );
				geom_mesh = MakeMesh( parent_node, geom_file, is_static ? static_mat : contact_mat, cg->GetPos(), cg->GetOri() );
			}
			else if ( !std::holds_alternative<xo::plane>( cg->GetShape() ) )
			{
				auto& mat = is_static ? static_mat : ( use_bone_mat ? object_mat : contact_mat );
				geom_mesh = MakeMesh( parent_node, cg->GetShape(), xo::color::cyan(), mat, cg->GetPos(), cg->GetOri() );
			}

			// add the mesh to the right container
			if ( geom_mesh ) {
				geom_mesh.set_name( cg->GetName().c_str() );
				geom_mesh.set_cast_shadows( GetStudioSetting<bool>( "viewer.contact_geometries_cast_shadows" ) );
				auto& geom_container = is_static || use_bone_mat ? individual_contact_geoms : contact_geoms;
				geom_container.push_back( std::move( geom_mesh ) );
			}
		}

		const auto auto_muscle_width = GetStudioSetting<bool>( "viewer.auto_muscle_width" );
		const auto auto_muscle_width_factor = GetStudioSetting<float>( "viewer.auto_muscle_width_factor" );
		const auto muscle_width = GetStudioSetting<float>( "viewer.muscle_width" );
		const auto relative_tendon_width = GetStudioSetting<float>( "viewer.relative_tendon_width" );
		const auto muscle_position = GetStudioSetting<float>( "viewer.muscle_position" );

		for ( auto& muscle : model.GetMuscles() )
		{
			float muscle_radius = auto_muscle_width ?
				auto_muscle_width_factor * float( sqrt( muscle->GetPCSA() / xo::constantsd::pi() ) ) :
				muscle_width;

			float tendon_radius = relative_tendon_width * muscle_radius;

			// add path
			MuscleVis mv;
			mv.ten1 = vis::trail( root_node_, vis::trail_info{ tendon_radius, xo::color::yellow(), 0.3f } );
			mv.ten2 = vis::trail( root_node_, vis::trail_info{ tendon_radius, xo::color::yellow(), 0.3f } );
			mv.ten1.set_material( tendon_mat );
			mv.ten2.set_material( tendon_mat );
			mv.ten1.set_name( muscle->GetName().c_str() );
			mv.ten2.set_name( muscle->GetName().c_str() );
			mv.ce = vis::trail( root_node_, vis::trail_info{ muscle_radius, xo::color::red(), 0.5f } );
			mv.mat = muscle_mat.clone();
			mv.mat.emissive( vis::color() );
			mv.ce.set_material( mv.mat );
			mv.ce.set_name( muscle->GetName().c_str() );
			mv.ce_pos = muscle_position;
			muscles.push_back( std::move( mv ) );
		}

		for ( auto& ligament : model.GetLigaments() )
		{
			auto lv = vis::trail( root_node_, vis::trail_info{ muscle_width, xo::color::yellow(), 0.3f } );
			lv.set_material( ligament_mat );
			ligaments.push_back( std::move( lv ) );
		}

		const auto joint_radius = GetStudioSetting<float>( "viewer.joint_radius" );
		for ( auto& j : model.GetJoints() )
		{
			joints.push_back( vis::mesh( root_node_, vis::shape_info{ xo::sphere( joint_radius ), xo::color::red(), xo::vec3f::zero(), 0.75f } ) );
			joints.back().set_material( joint_mat );
			joints.back().set_name( j->GetName().c_str() );
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
		for ( index_t i = 0; i < model_bodies.size(); ++i )
		{
			auto& b = model_bodies[i];
			bodies[i].pos_ori( vis::vec3f( b->GetOriginPos() ), vis::quatf( b->GetOrientation() ) );

			// contact forces
			if ( auto f = b->GetContactForce(); view_flags( ViewOption::ExternalForces ) && combine_contact_forces_ && !f.is_null() )
				UpdateForceVis( force_count++, b->GetContactPoint(), f );

			// external forces
			if ( auto f = b->GetExternalForce(); !f.is_null() )
				UpdateForceVis( force_count++, b->GetPosOfPointOnBody( b->GetExternalForcePoint() ), f, 0.004f, 0.5f );

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

		// update joints
		auto& model_joints = model.GetJoints();
		for ( index_t i = 0; i < model_joints.size(); ++i )
		{
			auto pos = model_joints[i]->GetPos();
			joints[i].pos( vis::vec3f( pos ) );
			if ( view_flags( ViewOption::Joints ) )
			{
				UpdateForceVis( force_count++, pos, -model_joints[i]->GetReactionForce() );
				UpdateMomentVis( moment_count++, pos, -model_joints[i]->GetLimitTorque() );
			}
		}

		// update contact forces
		if ( view_flags( ViewOption::ExternalForces ) && !combine_contact_forces_ )
		{
			auto fvec = model.GetContactForceValues();
			for ( auto& cf : fvec )
				UpdateForceVis( force_count++, cf.point, cf.force );
		}

		// update com / heading
		if ( view_flags( ViewOption::ModelComHeading ) )
		{
			if ( model.HasRootBody() )
			{
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

	void ModelVis::UpdateForceVis( index_t force_idx, Vec3 cop, Vec3 force, float len_scale, float rad_scale )
	{
		while ( forces.size() <= force_idx )
		{
			forces.emplace_back( root_node_, vis::arrow_info{ 0.01, 0.02, xo::color::yellow(), 0.3f } );
			forces.back().set_material( arrow_mat );
			forces.back().show( view_flags( ViewOption::ExternalForces ) );
			forces.back().set_cast_shadows( forces_cast_shadows_ );
		}
		forces[force_idx].pos_ofs( vis::vec3f( cop ), len_scale * vis::vec3f( force ), rad_scale );
	}

	void ModelVis::UpdateMomentVis( index_t moment_idx, Vec3 pos, Vec3 moment, float len_scale, float rad_scale )
	{
		while ( moments.size() <= moment_idx )
		{
			moments.emplace_back( root_node_, vis::arrow_info{ 0.01, 0.02, xo::color::blue(), 0.3f } );
			moments.back().set_material( moment_mat );
			moments.back().show( view_flags( ViewOption::ExternalForces ) );
			moments.back().set_cast_shadows( forces_cast_shadows_ );
		}
		moments[moment_idx].pos_ofs( vis::vec3f( pos ), len_scale * vis::vec3f( moment ), rad_scale );
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

		xo::color c = muscle_gradient( float( a ) );
		vis.mat.diffuse( c );
		vis.mat.ambient( c );

		auto p = mus.GetMusclePath();
		if ( view_flags( ViewOption::Tendons ) )
		{
			auto i1 = insert_path_point( p, tlen );
			auto i2 = insert_path_point( p, tlen + mlen );
			SCONE_ASSERT( i1 <= i2 );
			vis.ten1.set_points( p.begin(), p.begin() + i1 + 1 );
			vis.ce.set_points( p.begin() + i1, p.begin() + i2 + 1 );
			vis.ten2.set_points( p.begin() + i2, p.end() );
		}
		else vis.ce.set_points( p.begin(), p.end() );
	}

	vis::mesh ModelVis::MakeMesh( vis::node& parent, const xo::shape& sh, const xo::color& col, const vis::material& mat, const Vec3& pos, const Quat& ori, const Vec3& scale )
	{
		auto msh = vis::mesh( parent, vis::shape_info{ sh, col, xo::vec3f::zero(), 0.75f } );
		msh.set_material( mat );
		auto y_to_z = std::holds_alternative<xo::capsule>( sh ) || std::holds_alternative<xo::cylinder>( sh ) || std::holds_alternative<xo::cone>( sh );
		auto fixed_ori = y_to_z ? vis::quatf( ori ) * xo::quat_from_x_angle( vis::degreef( 90 ) ) : vis::quatf( ori );
		msh.pos_ori( vis::vec3f( pos ), fixed_ori );
		msh.scale_enable_normalize( vis::vec3f( scale ) );
		return msh;
	}

	vis::mesh ModelVis::MakeMesh( vis::node& parent, const xo::path& file, const vis::material& mat, const Vec3& pos, const Quat& ori, const Vec3& scale )
	{
		auto msh = vis::mesh( parent, file );
		msh.set_material( mat );
		auto fix_obj_ori = file.extension_no_dot() == "obj";
		auto fixed_ori = fix_obj_ori ? vis::quatf( ori ) * xo::quat_from_x_angle( -90_degf ) : vis::quatf( ori );
		msh.pos_ori( vis::vec3f( pos ), fixed_ori );
		auto fixed_scale = fix_obj_ori ? vis::vec3f( scale.x, scale.z, scale.y ) : vis::vec3f( scale );
		msh.scale_enable_normalize( fixed_scale );
		return msh;
	}

	void ModelVis::ApplyViewOptions( const ViewOptions& f )
	{
		view_flags = f;
		for ( auto& f : forces )
			f.show( view_flags( ViewOption::ExternalForces ) );
		for ( auto& m : moments )
			m.show( view_flags( ViewOption::ExternalForces ) );

		for ( auto& m : muscles )
		{
			m.ce.show( view_flags( ViewOption::Muscles ) );
			m.ten1.show( view_flags( ViewOption::Muscles ) && view_flags( ViewOption::Tendons ) );
			m.ten2.show( view_flags( ViewOption::Muscles ) && view_flags( ViewOption::Tendons ) );
		}

		for ( auto& l : ligaments )
			l.show( view_flags( ViewOption::BodyGeom ) );

		for ( auto& e : joints )
			e.show( view_flags( ViewOption::Joints ) );

		for ( auto& e : body_meshes )
			e.show( view_flags( ViewOption::BodyGeom ) );

		for ( auto& e : body_axes )
			e.show( view_flags( ViewOption::BodyAxes ) );

		for ( auto& e : body_com )
			e.show( view_flags( ViewOption::BodyCom ) );

		for ( auto& e : contact_geoms )
			e.show( view_flags( ViewOption::ContactGeom ) );

		if ( ground_.node_id() )
			ground_.show( view_flags( ViewOption::GroundPlane ) );

		heading_.show( view_flags( ViewOption::ModelComHeading ) );
	}
}
