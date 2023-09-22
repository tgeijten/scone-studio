/*
** SconeStorageDataModel.cpp
**
** Copyright (C) Thomas Geijtenbeek and contributors. All rights reserved.
**
** This file is part of SCONE. For more information, see http://scone.software.
*/

#include "SconeStorageDataModel.h"
#include "xo/numerical/math.h"
#include "scone/core/Log.h"
#include "gui_profiler.h"

SconeStorageDataModel::SconeStorageDataModel( const scone::Storage<>* s ) :
	storage( s ),
	equidistant_delta_time( false ),
	index_cache{ -1, 0 }
{}

void SconeStorageDataModel::setStorage( const scone::Storage<>* s )
{
	GUI_PROFILE_FUNCTION;

	index_cache = { -1, 0 };
	storage = s;

	// set equidistant_delta_time by checking all deltas
	equidistant_delta_time = true;
	if ( storage && storage->GetFrameCount() >= 3 )
	{
		auto tprev = s->GetFrame( 1 ).GetTime();
		auto delta = tprev - s->GetFrame( 0 ).GetTime();

		for ( xo::index_t i = 2; i < s->GetFrameCount(); ++i )
		{
			auto t = s->GetFrame( i ).GetTime();
			auto dt = t - tprev;

			if ( !xo::equal( delta, dt, 0.001 * delta ) )
			{
				equidistant_delta_time = false;
				xo::log::trace( "Storage delta time is not equidistant, ", xo::stringf( "%.16f != %.16f", delta, dt ) );
				break;
			}
			tprev = t;
		}
	}
}

int SconeStorageDataModel::channelCount() const
{
	return storage ? storage->GetChannelCount() : 0;
}

int SconeStorageDataModel::frameCount() const
{
	return storage ? storage->GetFrameCount() : 0;
}

QString SconeStorageDataModel::label( int idx ) const
{
	return storage ? QString( storage->GetLabels()[idx].c_str() ) : QString();
}

double SconeStorageDataModel::value( int idx, double time ) const
{
	return storage ? storage->GetFrame( timeIndex( time ) )[idx] : 0;
}

double SconeStorageDataModel::value( int channel, int frame ) const
{
	return storage ? storage->GetFrame( frame )[channel] : 0;
}

std::vector< std::pair< float, float > > SconeStorageDataModel::getSeries( int idx, double min_interval ) const
{
	std::vector< std::pair< float, float > > series;
	if ( storage )
	{
		series.reserve( storage->GetFrameCount() ); // this may be a little much, but ensures no reallocation
		double last_time = timeStart() - 2 * min_interval;
		for ( size_t i = 0; i < storage->GetFrameCount(); ++i )
		{
			auto& f = storage->GetFrame( i );
			if ( f.GetTime() - last_time >= min_interval )
			{
				series.emplace_back( static_cast<float>( f.GetTime() ), static_cast<float>( f[idx] ) );
				last_time = f.GetTime();
			}
		}
	}
	return series;
}

double SconeStorageDataModel::timeFinish() const
{
	return storage && !storage->IsEmpty() ? storage->Back().GetTime() : 0.0;
}

double SconeStorageDataModel::timeStart() const
{
	return storage && !storage->IsEmpty() ? storage->Front().GetTime() : 0.0;
}

int SconeStorageDataModel::timeIndex( double time ) const
{
	if ( !storage || storage->IsEmpty() )
		return -1;

	if ( index_cache.first == time )
		return index_cache.second;

	int idx = -1;
	if ( equidistant_delta_time )
	{
		auto front_time = storage->Front().GetTime(), back_time = storage->Back().GetTime();
		double reltime = ( time - front_time ) / ( back_time - front_time );
		idx = xo::clamped( int( reltime * ( storage->GetFrameCount() - 1 ) + 0.5 ), 0, int( storage->GetFrameCount() - 1 ) );
	}
	else
	{
		// real binary search to find closest index
		idx = static_cast<int>( storage->GetClosestFrameIndex( time ) );
		scone::log::trace( "located time ", time, " at index=", idx, " time=", storage->GetFrame( idx ).GetTime() );
	}

	index_cache = { time, idx };
	return idx;
}

double SconeStorageDataModel::timeValue( int idx ) const
{
	return storage && idx != xo::no_index ? storage->GetFrame( idx ).GetTime() : 0.0;
}
