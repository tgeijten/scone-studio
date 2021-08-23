#include "ModelVis.h" 

#include "StudioSettings.h"
#include "vis/scene.h"
#include "xo/filesystem/filesystem.h"
#include "scone/core/Log.h"
#include "scone/model/Muscle.h"
#include "scone/model/Joint.h"
#include "xo/geometry/path_alg.h"
#include "scone/core/math.h"
#include "xo/geometry/quat.h"

namespace scone
{
	using namespace xo::angle_literals;

	ModelVis::ModelVis( const Model& model, vis::scene& s ) :
		root_node_( &s ),
		specular_( GetStudioSetting < float >( "viewer.specular" ) ),
		shininess_( GetStudioSetting< float >( "viewer.shininess" ) ),
		ambient_( GetStudioSetting< float >( "viewer.ambient" ) ),
		bone_mat( { GetStudioSetting< xo::color >( "viewer.bone" ), specular_, shininess_, ambient_ } ),
		joint_mat( { GetStudioSetting< xo::color >( "viewer.joint" ), specular_, shininess_, ambient_ } ),
		com_mat( { GetStudioSetting< xo::color >( "viewer.com" ), specular_, shininess_, ambient_ } ),
		muscle_mat( { GetStudioSetting< xo::color >( "viewer.muscle_0" ), specular_, shininess_, ambient_ } ),
		tendon_mat( { GetStudioSetting< xo::color >( "viewer.tendon" ), specular_, shininess_, ambient_ } ),
		arrow_mat( { GetStudioSetting< xo::color >( "viewer.force" ), specular_, shininess_, ambient_ } ),
		contact_mat( { GetStudioSetting< xo::color >( "viewer.contact" ), specular_, shininess_, ambient_, GetStudioSetting<float>( "viewer.contact_alpha" ) } ),
		muscle_gradient( {
			{ 0.0f, GetStudioSetting< xo::color >( "viewer.muscle_0" ) },
			{ 0.5f, GetStudioSetting< xo::color >( "viewer.muscle_50" ) },
			{ 1.0f, GetStudioSetting< xo::color >( "viewer.muscle_100" ) } } )
	{
		// #todo: don't reset this every time, keep view_flags outside ModelVis
		view_flags.set( { ShowForces, ShowMuscles, ShowTendons, ShowBodyGeom, EnableShadows, ShowModelComHeading } );

		// ground plane
		if ( auto* gp = model.GetGroundPlane() )
		{
			auto& plane = std::get<xo::plane>( gp->GetShape() );
			auto col1 = GetStudioSetting<xo::color>( "viewer.tile1" );
			auto col2 = GetStudioSetting<xo::color>( "viewer.tile2" );
			auto tile_size = GetStudioSetting<float>( "viewer.tile_size" );
			auto tile_count_x = GetStudioSetting<int>( "viewer.tile_count_x" );
			auto tile_count_z = GetStudioSetting<int>( "viewer.tile_count_z" );
			ground_ = vis::plane( root_node_, tile_count_x, tile_count_z, tile_size, col1, col2 );
			auto normal_rot = xo::quat_from_directions( xo::vec3f::unit_y(), plane.normal_ );
			//ground_plane = scene_.add< vis::plane >( xo::vec3f( 64, 0, 0 ), xo::vec3f( 0, 0, -64 ), GetFolder( SCONE_UI_RESOURCE_FOLDER ) / "stile160.png", 64, 64 );
			ground_.pos_ori( vis::vec3f( gp->GetPos() ), normal_rot * xo::quatf( gp->GetOri() ) );
		}

		for ( auto& body : model.GetBodies() )
		{
			bodies.push_back( vis::node( &root_node_ ) );
			body_axes.push_back( vis::axes( bodies.back(), vis::axes_info{ vis::vec3f::diagonal( 0.1 ) } ) );
			if ( body->GetMass() > 0 )
			{
				body_com.push_back( vis::mesh( bodies.back(), vis::shape_info{ xo::sphere( 0.02f ), xo::color::green(), xo::vec3f::zero(), 0.75f } ) );
				body_com.back().set_material( com_mat );
				body_com.back().pos( xo::vec3f( body->GetLocalComPos() ) );
			}

			auto geoms = body->GetDisplayGeometries();
			for ( const auto& geom : geoms )
			{
				if ( !geom.filename_.empty() )
				{
					try
					{
						auto model_folder = model.GetModelFile().parent_path();
						auto file_options = {
							geom.filename_,
							path( "geometry" ) / geom.filename_,
							model_folder / geom.filename_,
							model_folder / "geometry" / geom.filename_,
							GetFolder( SCONE_GEOMETRY_FOLDER ) / geom.filename_
						};
						auto geom_file = xo::try_find_file( file_options );
						if ( geom_file )
						{
							log::trace( "Loading geometry for body ", body->GetName(), ": ", *geom_file );
							body_meshes.push_back( MakeMesh( bodies.back(), *geom_file, bone_mat, geom.pos_, geom.ori_, geom.scale_ ) );
						}
						else log::warning( "Could not find ", geom.filename_ );
					}
					catch ( std::exception& e )
					{
						log::warning( "Could not load ", geom.filename_, ": ", e.what() );
					}
				}
				else {
					// shape
					body_meshes.push_back( MakeMesh(
						bodies.back(), geom.shape_, xo::color::cyan(), bone_mat, geom.pos_, geom.ori_, geom.scale_ ) );
				}
			}
		}

		for ( auto& cg : model.GetContactGeometries() )
		{
			auto idx = xo::find_index_if( model.GetBodies(), [&]( const auto& b ) { return &cg->GetBody() == b.get(); } );
			auto& parent_node = idx != NoIndex ? bodies[ idx ] : root_node_;
			if ( cg->HasFileName() )
			{
				auto model_folder = model.GetModelFile().parent_path();
				auto geom_file = FindFile( model_folder / cg->GetFileName() );
				contact_geoms.push_back( MakeMesh( parent_node, geom_file, contact_mat, cg->GetPos(), cg->GetOri() ) );
			}
			else if ( !std::holds_alternative<xo::plane>( cg->GetShape() ) )
			{
				// #todo: add support for other shapes (i.e. planes)
				bool use_bone_mat = cg->GetPos().is_null() && parent_node.size() <= 3;
				contact_geoms.push_back( MakeMesh(
					parent_node, cg->GetShape(), xo::color::cyan(), use_bone_mat ? bone_mat : contact_mat,
					cg->GetPos(), cg->GetOri() ) );
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
			mv.ce = vis::trail( root_node_, vis::trail_info{ muscle_radius, xo::color::red(), 0.5f } );
			mv.mat = muscle_mat.clone();
			mv.ce.set_material( mv.mat );
			mv.ce_pos = muscle_position;
			muscles.push_back( std::move( mv ) );
		}

		const auto joint_radius = GetStudioSetting<float>( "viewer.joint_radius" );
		for ( auto& j : model.GetJoints() )
		{
			joints.push_back( vis::mesh( root_node_, vis::shape_info{ xo::sphere( joint_radius ), xo::color::red(), xo::vec3f::zero(), 0.75f } ) );
			joints.back().set_material( joint_mat );
		}

		heading_ = vis::arrow( root_node_, vis::arrow_info{ 0.01f, 0.02f, xo::color::green() } );
		heading_.set_material( com_mat );

		ApplyViewSettings( view_flags );
	}

	ModelVis::~ModelVis()
	{}

	void ModelVis::Update( const Model& model )
	{
		index_t force_count = 0;

		// update bodies
		auto& model_bodies = model.GetBodies();
		for ( index_t i = 0; i < model_bodies.size(); ++i )
		{
			auto& b = model_bodies[ i ];
			bodies[ i ].pos_ori( vis::vec3f( b->GetOriginPos() ), vis::quatf( b->GetOrientation() ) );

			// external forces
			if ( auto f = b->GetExternalForce(); !f.is_null() )
				UpdateForceVis( force_count++, b->GetPosOfPointOnBody( b->GetExternalForcePoint() ), f );

			// external moments
			if ( auto m = b->GetExternalMoment(); !m.is_null() )
				UpdateForceVis( force_count++, b->GetComPos(), m );
		}

		// update muscle paths
		auto& model_muscles = model.GetMuscles();
		for ( index_t i = 0; i < model_muscles.size(); ++i )
			UpdateMuscleVis( *model_muscles[ i ], muscles[ i ] );

		// update joints
		auto& model_joints = model.GetJoints();
		for ( index_t i = 0; i < model_joints.size(); ++i )
		{
			auto pos = model_joints[ i ]->GetPos();
			joints[ i ].pos( vis::vec3f( pos ) );
			if ( view_flags.get< ShowJoints >() )
				UpdateForceVis( force_count++, pos, -model_joints[ i ]->GetReactionForce() );
		}

		// update forces
		if ( view_flags.get< ShowForces >() )
		{
			auto fvec = model.GetContactForceValues();
			for ( auto& cf : fvec )
				UpdateForceVis( force_count++, cf.point, cf.force );
		}
		while ( force_count < forces.size() )
			forces.pop_back();

		// update com / heading
		if ( view_flags.get<ShowModelComHeading>() )
		{
			if ( auto* root = model.GetRootBody() )
			{
				auto pos = xo::vec3f( model.GetComPos() );
				auto dir = xo::quatf( root->GetOrientation() ) * xo::vec3f( 0.5f, 0, 0 );
				heading_.pos_ofs( pos, dir );
			}
		}
	}

	void ModelVis::UpdateForceVis( index_t force_idx, Vec3 cop, Vec3 force )
	{
		while ( forces.size() <= force_idx )
		{
			forces.emplace_back( root_node_, vis::arrow_info{ 0.01f, 0.02f, xo::color::yellow(), 0.3f } );
			forces.back().set_material( arrow_mat );
			forces.back().show( view_flags.get< ShowForces >() );
		}
		forces[ force_idx ].pos( vis::vec3f( cop ), vis::vec3f( cop + 0.001 * force ) );
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
			log::warning( mus.GetName(), " muscle tendon length: ", tlen, "; clamping to zero" );
			tlen = 0.0;
		}
		auto a = mus.GetActivation();
		auto p = mus.GetMusclePath();

		xo::color c = muscle_gradient( float( a ) );
		vis.mat.diffuse( c );
		vis.mat.emissive( vis::color() );
		vis.mat.ambient( c );

		if ( view_flags.get<ShowTendons>() )
		{
			auto i1 = insert_path_point( p, tlen );
			auto i2 = insert_path_point( p, tlen + mlen );
			SCONE_ASSERT( i1 <= i2 );
			vis.ten1.set_points( p.begin(), p.begin() + i1 + 1 );
			vis.ce.set_points( p.begin() + i1, p.begin() + i2 + 1 );
			vis.ten2.set_points( p.begin() + i2, p.end() );
		} else vis.ce.set_points( p.begin(), p.end() );
	}

	vis::mesh ModelVis::MakeMesh( vis::node& parent, const xo::shape& sh, const xo::color& col, const vis::material& mat, const Vec3& pos, const Quat& ori, const Vec3& scale )
	{
		auto msh = vis::mesh( parent, vis::shape_info{ sh, col, xo::vec3f::zero(), 0.75f } );
		msh.set_material( mat );
		auto y_to_z = std::holds_alternative<xo::capsule>( sh ) || std::holds_alternative<xo::cylinder>( sh ) || std::holds_alternative<xo::cone>( sh );
		auto fixed_ori = y_to_z ? vis::quatf( ori ) * xo::quat_from_x_angle( vis::degreef( 90 ) ) : vis::quatf( ori );
		msh.pos_ori( vis::vec3f( pos ), fixed_ori );
		return msh;
	}

	vis::mesh ModelVis::MakeMesh( vis::node& parent, const xo::path& file, const vis::material& mat, const Vec3& pos, const Quat& ori, const Vec3& scale )
	{
		auto msh = vis::mesh( parent, file );
		msh.set_material( mat  );
		auto fix_obj_ori = file.extension_no_dot() == "obj";
		auto fixed_ori = fix_obj_ori ? xo::quat_from_x_angle( -90_degf ) * vis::quatf( ori ) : vis::quatf( ori );
		msh.pos_ori( vis::vec3f( pos ), fixed_ori );
		msh.scale( vis::vec3f( scale ) );
		return msh;
	}

	void ModelVis::ApplyViewSettings( const ViewSettings& f )
	{
		view_flags = f;
		for ( auto& f : forces )
			f.show( view_flags.get<ShowForces>() );

		for ( auto& m : muscles )
		{
			m.ce.show( view_flags.get<ShowMuscles>() );
			m.ten1.show( view_flags.get<ShowMuscles>() && view_flags.get<ShowTendons>() );
			m.ten2.show( view_flags.get<ShowMuscles>() && view_flags.get<ShowTendons>() );
		}

		for ( auto& e : joints )
			e.show( view_flags.get<ShowJoints>() );

		for ( auto& e : body_meshes )
			e.show( view_flags.get<ShowBodyGeom>() );

		for ( auto& e : body_axes )
			e.show( view_flags.get<ShowBodyAxes>() );

		for ( auto& e : body_com )
			e.show( view_flags.get<ShowBodyCom>() );

		for ( auto& e : contact_geoms )
			e.show( view_flags.get<ShowContactGeom>() );

		if ( ground_.node_id() )
			ground_.show( view_flags.get<ShowGroundPlane>() );

		heading_.show( view_flags.get<ShowModelComHeading>() );
	}
}
