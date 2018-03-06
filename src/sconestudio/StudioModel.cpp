#include "StudioModel.h"

#include "scone/optimization/opt_tools.h"
#include "scone/core/StorageIo.h"
#include "scone/core/Profiler.h"
#include "scone/core/Factories.h"
#include "scone/core/system_tools.h"
#include "scone/core/math.h"
#include "scone/controllers/cs_tools.h"
#include "scone/model/Muscle.h"

#include "xo/time/timer.h"
#include "xo/filesystem/filesystem.h"

namespace scone
{
	StudioModel::StudioModel( vis::scene& s, const path& file, bool force_evaluation ) :
	bone_mat( vis::color( 1, 0.98, 0.95 ), 1, 15, 0, 0.5f ),
	muscle_mat( vis::make_blue(), 0.5f, 15, 0, 0.5f ),
	arrow_mat( vis::make_yellow(), 1.0, 15, 0, 0.5f ),
	is_evaluating( false )
	{
		view_flags.set( ShowForces ).set( ShowMuscles ).set( ShowGeometry ).set( EnableShadows );
		store_data_downsample_stride = 10;

		// create the objective form par file or config file
		model_objective = CreateModelObjective( file );
		ParamInstance par( model_objective->info() );
		if ( file.extension() == "par" )
			par.import_values( file );
		model = model_objective->CreateModelFromParInstance( par );

		// accept filename and clear data
		filename = file;

		// see if we can load a matching .sto file
		auto sto_file = xo::path( file.str() ).replace_extension( "sto" );
		if ( !force_evaluation && xo::exists( sto_file ) && filename.extension() == "par" )
		{
			xo::timer t;
			log::info( "Reading ", sto_file.string() );
			ReadStorageSto( data, sto_file.string() );
			InitStateDataIndices();
			log::trace( "File read in ", t.seconds(), " seconds" );
		}
		else
		{
			// start evaluation
			is_evaluating = true;
			model->SetStoreData( true );
			if ( GetSconeSettings().get< bool >( "data.muscle" ) )
				model->GetStoreDataFlags().set( { StoreDataTypes::MuscleExcitation, StoreDataTypes::MuscleFiberProperties } );
			if ( GetSconeSettings().get< bool >( "data.body" ) )
				model->GetStoreDataFlags().set( { StoreDataTypes::BodyOriginPosition, StoreDataTypes::BodyOrientation } );
			if ( GetSconeSettings().get< bool >( "data.sensor" ) )
				model->GetStoreDataFlags().set( { StoreDataTypes::SensorData } );
			if ( GetSconeSettings().get< bool >( "data.controller" ) )
				model->GetStoreDataFlags().set( { StoreDataTypes::ControllerData } );

			model->SetSimulationEndTime( model_objective->GetDuration() );
			log::info( "Evaluating ", filename );
			EvaluateTo( 0 ); // evaluate one step so we can init vis
		}

		InitVis( s );
		UpdateVis( 0 );
	}

	StudioModel::~StudioModel()
	{
		if ( is_evaluating )
			log::warning( "Closing model while thread is still running" );
	}

	void StudioModel::InitVis( vis::scene& scone_scene )
	{
		scone_scene.attach( root );

		xo::timer t;
		for ( auto& body : model->GetBodies() )
		{
			body_meshes.push_back( std::vector< vis::mesh >() );
			auto geom_files = body->GetDisplayGeomFileNames();

			for ( auto geom_file : geom_files )
			{
				//log::trace( "Loading geometry for body ", body->GetName(), ": ", geom_file );
				try
				{
					if ( !xo::file_exists( geom_file ) )
						geom_file = scone::GetFolder( scone::SCONE_GEOMETRY_FOLDER ) / geom_file;
					body_meshes.back().push_back( root.add_mesh( geom_file ) );
					body_meshes.back().back().set_material( bone_mat );
					body_axes.push_back( vis::axes( root, vis::vec3f( 0.1, 0.1, 0.1 ), 0.5f ) );
				}
				catch ( std::exception& e )
				{
					log::warning( "Could not load ", geom_file, ": ", e.what() );
				}
			}
		}
		log::debug( "Meshes loaded in ", t.seconds(), " seconds" );

		for ( auto& muscle : model->GetMuscles() )
		{
			// add path
			auto p = muscle->GetMusclePath();
			auto vispath = vis::trail( root, p.size(), 0.005f, vis::make_red(), 0.3f );
			auto vismat = muscle_mat.clone();
			vispath.set_material( vismat );
			muscles.push_back( std::make_pair( vispath, vismat ) );
		}

		ApplyViewSettings( view_flags );
	}

	void StudioModel::InitStateDataIndices()
	{
		// setup state_data_index (lazy init)
		SCONE_ASSERT( state_data_index.empty() );
		model_state = model->GetState();
		state_data_index.resize( model_state.GetSize() );
		for ( size_t state_idx = 0; state_idx < state_data_index.size(); state_idx++ )
		{
			auto data_idx = data.GetChannelIndex( model_state.GetName( state_idx ) );
			SCONE_ASSERT_MSG( data_idx != NoIndex, "Could not find state channel " + model_state.GetName( state_idx ) );
			state_data_index[ state_idx ] = data_idx;
		}
	}

	void StudioModel::UpdateVis( TimeInSeconds time )
	{
		SCONE_PROFILE_FUNCTION;

		// initialize visualization
		std::unique_lock< std::mutex > simulation_lock( model->GetSimulationMutex(), std::defer_lock );

		if ( !is_evaluating )
		{
			// update model state from data
			SCONE_ASSERT( !state_data_index.empty() );
			for ( Index i = 0; i < model_state.GetSize(); ++i )
				model_state[ i ] = data.GetInterpolatedValue( time, state_data_index[ i ] );
			model->SetState( model_state, time );
		}

		// update com
		//com.pos( model->GetComPos() );

		// update bodies
		auto& model_bodies = model->GetBodies();
		for ( Index i = 0; i < model_bodies.size(); ++i )
		{
			vis::transformf trans( model_bodies[ i ]->GetOriginPos(), model_bodies[ i ]->GetOrientation() );

			auto bp = model_bodies[ i ]->GetOriginPos();
			for ( auto& bm : body_meshes[ i ] )
				bm.transform( trans );

			body_axes[ i ].transform( trans );
		}

		// update muscle paths
		auto &model_muscles = model->GetMuscles();
		for ( Index i = 0; i < model_muscles.size(); ++i )
		{
			auto mp = model_muscles[ i ]->GetMusclePath();
			muscles[ i ].first.set_points( mp );

			auto a = model_muscles[ i ]->GetActivation();
			muscles[ i ].second.diffuse( vis::color( a, 0, 0.5 - 0.5 * a, 1 ) );
			muscles[ i ].second.emissive( vis::color( a, 0, 0.5 - 0.5 * a, 1 ) );
		}

		// update forces
		Index force_idx = 0;
		for ( Index i = 0; i < model->GetLegCount(); ++i )
		{
			Vec3 force, moment, cop;
			model->GetLeg( i ).GetContactForceMomentCop( force, moment, cop );

			if ( force.squared_length() > REAL_WIDE_EPSILON && view_flags.get< ShowForces >() )
				UpdateForceVis( force_idx++, cop, force );
		}

		for ( auto& b : model->GetBodies() )
		{
			auto f = b->GetExternalForce();
			if ( !f.is_null() )
				UpdateForceVis( force_idx++, b->GetPosOfPointFixedOnBody( b->GetExternalForcePoint() ), f );
		}

		if ( force_idx < forces.size() )
			forces.resize( force_idx );
	}

	void StudioModel::UpdateForceVis( Index force_idx, Vec3 cop, Vec3 force )
	{
		while ( forces.size() <= force_idx )
		{
			forces.push_back( root.add_arrow( 0.01f, 0.02f, vis::make_yellow(), 0.3f ) );
			forces.back().set_material( arrow_mat );
			forces.back().show( view_flags.get< ShowForces >() );
		}
		forces[ force_idx ].pos( cop, cop + 0.001 * force );
	}

	void StudioModel::EvaluateTo( TimeInSeconds t )
	{
		SCONE_ASSERT( IsEvaluating() );
		model_objective->AdvanceModel( *model, t );
		if ( model->GetTerminationRequest() || t >= model->GetSimulationEndTime() )
			FinalizeEvaluation( true );
	}

	void StudioModel::FinalizeEvaluation( bool output_results )
	{
		// copy data and init data
		data = model->GetData().CopySlice( 0, 0, store_data_downsample_stride );
		if ( !data.IsEmpty() )
			InitStateDataIndices();

		if ( output_results )
		{
			auto fitness = model_objective->GetResult( *model );
			log::info( "fitness = ", fitness );
			PropNode results;
			results.push_back( "result", model_objective->GetReport( *model ) );
			model->WriteResult( path( filename ).replace_extension() );
			WriteStorageSto( data, path( filename ).replace_extension ( "sto" ), ( filename.parent_path().filename() / filename.stem() ).str() );

			log::info( "Results written to ", path( filename ).replace_extension( "sto" ) );
			log::info( results );
		}

		// reset this stuff
		is_evaluating = false;
	}

	void StudioModel::SetViewSetting( ViewSettings e, bool value )
	{
		view_flags.set( e, value );

		switch ( e )
		{
		case scone::StudioModel::ShowForces:
			for ( auto& f : forces ) f.show( value );
			break;
		case scone::StudioModel::ShowMuscles:
			for ( auto& m : muscles ) m.first.show( value );
			break;
		case scone::StudioModel::ShowGeometry:
			for ( auto& e : body_meshes ) for ( auto m : e ) m.show( value );
			break;
		case scone::StudioModel::ShowAxes:
			for ( auto& e : body_axes ) e.show( value );
			break;
		case scone::StudioModel::EnableShadows:
			break;
		default:
			break;
		}
	}

	void StudioModel::ApplyViewSettings( const ViewFlags& flags )
	{
		view_flags = flags;
		for ( auto& f : forces ) f.show( view_flags.get< ShowForces >() );
		for ( auto& m : muscles ) m.first.show( view_flags.get< ShowMuscles >() );
		for ( auto& e : body_meshes ) for ( auto m : e ) m.show( view_flags.get< ShowGeometry >() );
		for ( auto& e : body_axes ) e.show( view_flags.get< ShowAxes >() );
	}
}
