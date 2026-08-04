#include "GaitPlot.h"

#include "qcustomplot/qcustomplot.h"

#include "StudioSettings.h"
#include "scone/core/math.h"
#include "xo/container/flat_map.h"
#include "xo/container/prop_node_tools.h"
#include "xo/numerical/bounds.h"
#include "xo/numerical/interpolation.h"
#include "xo/numerical/math.h"
#include "xo/utility/frange.h"
#include "scone/core/Log.h"
#include "xo/container/container_tools.h"
#include "xo/utility/irange.h"
#include "xo/container/container_algorithms.h"

namespace scone
{
	GaitPlot::GaitPlot( const PropNode& pn, QWidget* parent ) :
		QWidget( parent ),
		data_( pn ),
		plot_( nullptr ),
		plot_title_( nullptr ),
		norm_top_(),
		norm_bot_(),
		match_percentage_(),
		base_graph_count_(),
		base_item_count_()
	{
		auto l = new QHBoxLayout( this );
		l->setContentsMargins( 0, 0, 0, 0 );

		plot_ = new QCustomPlot( this );
		l->addWidget( plot_ );

		// title
		plot_->plotLayout()->insertRow( 0 ); // inserts an empty row above the default axis rect
		plot_title_ = new QCPPlotTitle( plot_, data_.title_.c_str() );
		plot_title_->setFont( QFont( plot_->font().family(), GetStudioSetting<int>( "gait_analysis.title_font_size" ), QFont::Bold ) );
		plot_->plotLayout()->addElement( 0, 0, plot_title_ );

		// norm data
		if ( data_.HasNormData() ) {
			norm_top_ = plot_->addGraph();
			norm_bot_ = plot_->addGraph();
			norm_top_->setPen( Qt::NoPen );
			norm_bot_->setPen( Qt::NoPen );
			norm_top_->setBrush( QColor( 0, 0, 0, 30.0 ) );
			norm_top_->setChannelFillGraph( norm_bot_ );

			setNormDataGraph();
		}

		// event bounds
		if ( GetStudioSetting<int>( "gait_analysis.show_swing_start" ) != 0 && data_.norm_event_ ) {
			auto* bar = new QCPItemRect( plot_ );
			bar->topLeft->setCoords( data_.norm_event_->lower, data_.y_min_ );
			bar->bottomRight->setCoords( data_.norm_event_->upper, data_.y_max_ );
			bar->setPen( Qt::NoPen );
			bar->setBrush( QColor( 0, 0, 0, 30.0 ) );
			plot_->addItem( bar );
		}

		// margins
		plot_->plotLayout()->setMargins( QMargins( 2, 2, 2, 2 ) );
		plot_->axisRect()->setMinimumMargins( QMargins( 1, 1, 1, 1 ) );
		plot_->yAxis->setLabelPadding( 2 );
		plot_->yAxis->setTickLabelPadding( 2 );
		plot_->xAxis->setLabelPadding( 2 );
		plot_->xAxis->setTickLabelPadding( 2 );

		// fonts
		auto labelFont = plot_->xAxis->labelFont();
		labelFont.setPointSize( GetStudioSetting<int>( "gait_analysis.axis_font_size" ) );
		plot_->xAxis->setLabelFont( labelFont );
		plot_->yAxis->setLabelFont( labelFont );

		// labels
		plot_->xAxis->setLabel( data_.x_label_.c_str() );
		plot_->yAxis->setLabel( data_.y_label_.c_str() );

		// set axes ranges, so we see all data:
		plot_->xAxis->setRange( 0, 100 );
		plot_->xAxis->setAutoTickStep( true );
		plot_->xAxis->setAutoSubTicks( false );
		plot_->xAxis->setAutoTickCount( 4 );
		plot_->yAxis->setRange( data_.y_min_, data_.y_max_ );
		plot_->yAxis->setAutoTickStep( true );
		plot_->yAxis->setAutoSubTicks( false );
		plot_->yAxis->setAutoTickCount( 4 );

		base_graph_count_ = plot_->graphCount();
		base_item_count_ = plot_->itemCount();

		plot_->replot();
	}

	GaitPlot::~GaitPlot()
	{
		delete plot_; // deleted automatically by parent destructor, but we do it here anyway
	}

	xo::error_message GaitPlot::update( const Storage<>& sto, const std::vector<GaitCycle>& cycles )
	{
		if ( hasData() ) {
			log::warning( "Clearing existing data in plot ", data_.title_ ); // this shouldn't happen and may cause leaks
			while ( plot_->graphCount() > base_graph_count_ )
				plot_->removeGraph( plot_->graphCount() - 1 );
			while ( plot_->itemCount() > base_item_count_ )
				plot_->removeItem( plot_->itemCount() - 1 );
		}

		// find channels, report error if not found
		const auto& labels = sto.GetLabels();
		auto channels_r = xo::find_indices_if( labels, [&]( auto& l ) { return data_.right_channel_( l ); } );
		auto channels_l = xo::find_indices_if( labels, [&]( auto& l ) { return data_.left_channel_( l ); } );
		if ( channels_r.empty() && channels_l.empty() )
			return "Could not find " + data_.left_channel_.str() + " / " + data_.right_channel_.str() + "; please verify Tools->Preferences->Data";

		// get settings (read here so they can be updated)
		bool plot_cycles = GetStudioSetting<bool>( "gait_analysis.plot_individual_cycles" );
		int show_swing_start = GetStudioSetting<int>( "gait_analysis.show_swing_start" );
		Real lookahead = sto.GetAverageFrameDuration() * GetStudioSetting<Real>( "gait_analysis.contact_timing_offset" );

		// plot cycles and gather range and avg data
		xo::boundsd range( data_.y_min_, data_.y_max_ );
		xo::flat_map< double, double > avg_data;
		auto event_line_extents = range.length() / 8;

		for ( const auto& cycle : cycles ) {
			bool right = cycle.side_ == Side::Right;
			const auto& channels = right ? channels_r : channels_l;
			if ( !channels.empty() ) {
				auto* graph = plot_cycles ? plot_->addGraph() : nullptr;
				if ( graph ) graph->setPen( QPen( right ? Qt::red : Qt::blue, 1 ) );
				double factor = data_.mirror_left_ && !right ? -data_.channel_multiply_ : data_.channel_multiply_;
				for ( Real perc : xo::frange<Real>( 0.0, 100.0, 0.5 ) ) {
					auto f = sto.ComputeInterpolatedFrame( cycle.begin_ + perc * cycle.duration() / 100.0 - lookahead );
					Real value = data_.channel_offset_ + factor * xo::average( channels, 0.0, [&]( Real v, index_t i ) { return v + f.value( i ); } );
					if ( graph ) graph->addData( perc, value );
					avg_data[perc] += value / cycles.size();
					range.extend( value );
				}

				// plot swing start point
				if ( show_swing_start == 1 && plot_cycles ) {
					auto t = 100.0 * cycle.stance_duration() / cycle.duration();
					auto f = sto.ComputeInterpolatedFrame( cycle.begin_ + t * cycle.duration() / 100.0 - lookahead );
					Real value = data_.channel_offset_ + factor * xo::average( channels, 0.0, [&]( Real v, index_t i ) { return v + f.value( i ); } );
					auto* line = new QCPItemLine( plot_ );
					line->setPen( QPen( right ? Qt::red : Qt::blue, 1 ) );
					plot_->addItem( line );
					line->start->setCoords( t, value - event_line_extents );
					line->end->setCoords( t, value + event_line_extents );
				}
			}
			else log::warning( "Gait Analysis could not find: ", right ? data_.right_channel_ : data_.left_channel_ ); // only shown when *either* left / right is missing
		}

		// plot swing start_lines
		if ( show_swing_start == 2 && plot_cycles ) {
			for ( const auto& cycle : cycles ) {
				bool right = cycle.side_ == Side::Right;
				auto t = 100.0 * cycle.stance_duration() / cycle.duration();
				auto* line = new QCPItemLine( plot_ );
				QPen pen( right ? Qt::red : Qt::blue, 1 );
				pen.setStyle( Qt::DotLine );
				line->setPen( pen );
				plot_->addItem( line );
				line->start->setCoords( t, range.lower );
				line->end->setCoords( t, range.upper );
			}
		}

		// add average data plot
		if ( !avg_data.empty() ) {
			auto* avg_graph = plot_->addGraph();
			avg_graph->setPen( QPen( Qt::black, 1.5 ) );
			for ( auto& e : avg_data )
				avg_graph->addData( e.first, e.second );
		}

		// compute average error in STD
		if ( !avg_data.empty() && !data_.norm_data_.empty() ) {
			double error = 0.0;
			for ( const auto& r : data_.norm_data_ ) {
				double x = 100.0 * xo::index_of( r, data_.norm_data_ ) / ( data_.norm_data_.size() - 1 );
				error += xo::abs( r.get_excess( xo::lerp_map( avg_data, x ) ) ) / xo::max( 0.01, r.length() );
			}
			error /= data_.norm_data_.size();
			match_percentage_ = 100.0 * xo::clamped( 1.0 - error, 0.0, 1.0 );
			if ( plot_title_ && GetStudioSetting<bool>( "gait_analysis.show_fit" ) )
				plot_title_->setText( data_.title_.c_str() + QString::asprintf( " (%.1f%%)", match_percentage_ ) );
		}
		plot_->yAxis->setRange( range.lower, range.upper );
		plot_->replot();

		return {};
	}

	bool GaitPlot::hasData() const
	{
		return plot_ && ( plot_->graphCount() > base_graph_count_ || plot_->itemCount() > base_item_count_ );
	}

	void GaitPlot::setNormDataGraph()
	{
		norm_top_->clearData();
		norm_bot_->clearData();
		for ( index_t i = 0; i < data_.norm_data_.size(); ++i ) {
			const auto& r = data_.norm_data_[i];
			auto yt = data_.norm_data_multiply_ * r.upper;
			auto yb = data_.norm_data_multiply_ * r.lower;
			double x = 100.0 * i / ( data_.norm_data_.size() - 1 );
			norm_top_->addData( x, yt );
			norm_bot_->addData( x, yb );
		}
	}
}
