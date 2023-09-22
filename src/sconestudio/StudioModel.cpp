/*
** StudioModel.cpp
**
** Copyright (C) Thomas Geijtenbeek and contributors. All rights reserved.
**
** This file is part of SCONE. For more information, see http://scone.software.
*/

#include "StudioModel.h"

#include "scone/optimization/opt_tools.h"
#include "scone/core/StorageIo.h"
#include "scone/core/profiler_config.h"
#include "scone/core/Factories.h"
#include "scone/core/system_tools.h"
#include "scone/core/Settings.h"
#include "scone/core/math.h"
#include "scone/model/Muscle.h"

#include "xo/time/timer.h"
#include "xo/filesystem/filesystem.h"
#include "xo/utility/color.h"
#include "xo/geometry/path_alg.h"
#include "xo/geometry/quat.h"
#include "xo/shape/sphere.h"
#include "xo/serialization/serialize.h"
#include "xo/shape/shape_tools.h"

#include "StudioSettings.h"

#include <QMessageBox>
#include "qt_convert.h"

namespace scone
{
	StudioModel::StudioModel( vis::scene& s, const path& file, const ViewOptions& vs ) :
		model_objective_( nullptr ),
		null_objective_( PropNode(), file.parent_path() ),
		follow_body_( nullptr ),
		status_( Status::Initializing ),
		write_results_after_evaluation_( false )
	{
		// create the objective from par file or config file
		xo::timer load_time;
		filename_ = file;
		const auto file_type = file.extension_no_dot();
		scenario_filename_ = FindScenario( file );
		scenario_pn_ = LoadScenario( file );

		if ( auto opt_fp = TryFindFactoryProps( GetOptimizerFactory(), scenario_pn_, "Optimizer" ); opt_fp )
		{
			optimizer_ = GetOptimizerFactory().create( opt_fp.type(), opt_fp.props(), scenario_pn_, file.parent_path() );
			model_objective_ = dynamic_cast<ModelObjective*>( &optimizer_->GetObjective() );
		}
		else if ( auto mod_fp = TryFindFactoryProps( GetModelFactory(), scenario_pn_, "Model" ); mod_fp )
		{
			spot::null_objective_info par;
			model_ = CreateModel( mod_fp, par, file.parent_path() );
		}
		else log::warning( "Not a model objective, disabling visualization" );

		try
		{
			if ( model_objective_ )
			{
				// create model from par or with default parameters
				if ( file_type == "par" )
				{
					model_objective_->info().import_mean_std( file, true );
					model_ = model_objective_->CreateModelFromParFile( file );
					write_results_after_evaluation_ = GetStudioSetting<bool>( "file.auto_write_par_evaluation" );
				}
				else  // #todo: use ModelObjective::model_ instead? Needs proper parameter initialization
				{
					auto par = SearchPoint( model_objective_->info() );
					model_ = model_objective_->CreateModelFromParams( par );
					write_results_after_evaluation_ = GetStudioSetting<bool>( "file.auto_write_scone_evaluation" );
				}
			}

			if ( model_ )
			{
				if ( file_type == "sto" )
				{
					// file is a .sto, load results
					xo::timer t;
					log::debug( "Reading ", file );
					ReadStorageSto( storage_, file );
					InitStateDataIndices();
					log::trace( "Read ", file, " in ", t(), " seconds" );
					status_ = Status::Ready;
				}
				else
				{
					// file is a .par or .scone, setup for evaluation
					status_ = Status::Evaluating;
					model_->SetStoreData( true );
					EvaluateTo( 0 ); // evaluate one step so we can init vis
				}

				// set follow body
				if ( auto s = GetStudioSetting<String>( "viewer.camera_follow_body" ); !s.empty() )
					follow_body_ = FindByName( model_->GetBodies(), s );

				// create and init visualizer
				vis_ = std::make_unique<ModelVis>( *model_, s, vs );
				UpdateVis( 0 );
			}
		}
		catch ( const std::exception& e )
		{
			InvokeError( e.what() );
		}

		log::info( "Loaded ", file.filename(), "; dim=", GetObjective().dim(), "; time=", load_time() );
	}

	StudioModel::~StudioModel()
	{
		// make sure there is no pending file write
		WaitForWriteResults();
	}

	void StudioModel::InitStateDataIndices()
	{
		SCONE_ASSERT( model_ );
		SCONE_ASSERT( state_data_index.empty() );
		SCONE_ERROR_IF( storage_.IsEmpty(), "Could not find any data" );
		model_state = model_->GetState();
		state_data_index.resize( model_state.GetSize() );
		for ( size_t state_idx = 0; state_idx < state_data_index.size(); state_idx++ )
		{
			auto data_idx = ( storage_.TryGetChannelIndex( model_state.GetName( state_idx ) ) );
			SCONE_ASSERT_MSG( data_idx != NoIndex, "Could not find state channel " + model_state.GetName( state_idx ) );
			state_data_index[state_idx] = data_idx;
		}
	}

	void StudioModel::UpdateVis( TimeInSeconds time )
	{
		if ( model_ && vis_ )
		{
			SCONE_PROFILE_FUNCTION( model_->GetProfiler() );
			try
			{
				if ( !storage_.IsEmpty() && !state_data_index.empty() )
				{
					// update model state from data
					for ( index_t i = 0; i < model_state.GetSize(); ++i )
						model_state[i] = storage_.GetInterpolatedValue( time, state_data_index[i] );
					model_->SetState( model_state, time );
				}

				vis_->Update( *model_ );
			}
			catch ( std::exception& e )
			{
				InvokeError( e.what() );
			}
		}
	}

	void StudioModel::EvaluateTo( TimeInSeconds t )
	{
		if ( model_ && IsEvaluating() )
		{
			try
			{
				if ( model_objective_ )
					model_objective_->AdvanceSimulationTo( *model_, t );
				else
					model_->AdvanceSimulationTo( t );
				auto te = model_->GetSimulationEndTime();
				auto t = model_->GetTime();
				if ( model_->HasSimulationEnded() )
					FinalizeEvaluation();
			}
			catch ( std::exception& e )
			{
				// simulation exception, abort instead of error so that data remains available
				AbortEvaluation();
				log::error( "Error evaluating ", filename_.filename(), ": ", e.what() );
				QMessageBox::critical( nullptr, "Error evaluating " + to_qt( filename_.filename() ), e.what() );
			}
		}
		else log::warning( "Unexpected call to StudioModel::EvaluateTo()" );
	}

	void StudioModel::AbortEvaluation()
	{
		try
		{
			status_ = Status::Aborted;
			storage_ = model_->GetData();
			InitStateDataIndices();
		}
		catch ( const std::exception& e )
		{
			InvokeError( e.what() );
		}
	}

	const PropNode& StudioModel::GetResult()
	{
		if ( result_pn_.empty() )
		{
			if ( model_objective_ ) {
				auto fitness = model_objective_->GetResult( *model_ ); // this calls ComputeResult which fills report
				auto& result = result_pn_.add_child( "Result", model_objective_->GetReport( *model_ ) );
			}
			else if ( model_ && model_->GetMeasure() ) {
				auto fitness = model_->GetMeasure()->GetWeightedResult( *model_ ); // this calls ComputeResult which fills report
				auto& result = result_pn_.add_child( "Result", model_->GetMeasure()->GetReport() );
				result.set_value( fitness ); // needed because result value is only set by ModelObjective
			}
			else log::warning( "Model has no Measure" );
		}
		return result_pn_;
	}

	void StudioModel::CheckWriteResults()
	{
		if ( write_results_.valid() && write_results_.wait_for( std::chrono::seconds( 0 ) ) == std::future_status::ready )
			WaitForWriteResults();
	}

	void StudioModel::FinalizeEvaluation()
	{
		if ( HasModel() )
		{
			try
			{
				// fetch data
				storage_ = model_->GetData();
				InitStateDataIndices();

				// compute and show fitness results
				if ( model_->GetMeasure() )
					log::info( GetResult() );

				// write results to file(s)
				if ( write_results_after_evaluation_ )
				{
					WaitForWriteResults();
					write_results_ = std::async( [this]() { return this->WriteResults(); } );
				}

				// we're done!
				status_ = Status::Ready;
			}
			catch ( std::exception& e )
			{
				InvokeError( e.what() );
			}
		}
		else log::warning( "Unexpected call to StudioModel::FinalizeEvaluation()" );
	}

	void StudioModel::InvokeError( const String& message )
	{
		if ( status_ != Status::Error )
		{
			status_ = Status::Error;
			log::error( "Error in ", filename_.filename(), ": ", message );
			QMessageBox::critical( nullptr, "Error in " + to_qt( filename_.filename() ), message.c_str() );
		}
		else log::error( message );
	}

	StudioModel::WriteResultsInfo StudioModel::WriteResults()
	{
		SCONE_ASSERT( model_ );
		xo::timer t;
		auto result_files = model_->WriteResults( filename_ );
		return { result_files, t() };
	}

	void StudioModel::WaitForWriteResults()
	{
		if ( write_results_.valid() ) {
			auto r = write_results_.get();
			log::debug( "Results written to ", concatenate_str( r.first, ", " ), " in ", r.second.secondsd(), "s" );
		}
	}

	TimeInSeconds StudioModel::GetMaxTime() const
	{
		if ( model_objective_ && IsEvaluating() )
			return model_objective_->GetDuration();
		else if ( !storage_.IsEmpty() )
			return storage_.Back().GetTime();
		else if ( model_ && model_->GetSimulationEndTime() < 1e12 )
			return model_->GetSimulationEndTime();
		else return 0.0;
	}

	void StudioModel::ApplyViewOptions( const ViewOptions& flags )
	{
		if ( vis_ )
		{
			vis_->ApplyViewOptions( flags );
			vis_->Update( *model_ );
		}
	}

	const ViewOptions& StudioModel::GetViewOptions() const
	{
		SCONE_ASSERT( vis_ );
		return vis_->GetViewOptions();
	}

	Vec3 StudioModel::GetFollowPoint() const
	{
		auto com = follow_body_ ? follow_body_->GetComPos() : model_->GetComPos();
		if ( auto gp = model_->GetGroundPlane() )
		{
			auto l = xo::linef( xo::vec3f( com ), xo::vec3f::neg_unit_y() );
			auto& p = std::get<xo::plane>( gp->GetShape() );
			auto t = xo::transformf( xo::vec3f( gp->GetPos() ), xo::quatf( gp->GetOri() ) );
			com.y = xo::intersection( l, p, t ).y;
		}
		else com.y = 0;
		return com;
	}

	void StudioModel::ResetModelVis( vis::scene& s, const ViewOptions& f )
	{
		vis_.reset( nullptr );
		vis_ = std::make_unique<ModelVis>( *model_, s, f );
		UpdateVis( 0 );
	}
}
