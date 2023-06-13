#include "MusclePlot.h"
#include "StudioSettings.h"
#include "scone/model/Dof.h"
#include "scone/model/Model.h"
#include "scone/model/Muscle.h"
#include "qt_convert.h"

namespace scone
{
	MusclePlot::MusclePlot( QWidget* parent ) :
		storage(),
		storageModel( &storage )
	{
		QHBoxLayout* l = new QHBoxLayout( this );
		l->setMargin( 0 );

		view = new QDataAnalysisView( storageModel, this );
		view->setObjectName( "Model Analysis" );
		view->setAutoFitVerticalAxis( scone::GetStudioSettings().get<bool>( "analysis.auto_fit_vertical_axis" ) );
		view->setLineWidth( scone::GetStudioSettings().get<float>( "analysis.line_width" ) );

		dofSelect = new QComboBox( view );
		dofSelect->setPlaceholderText( "Select Coordinate" );
		view->itemGroupWidget()->layout_->insertWidget( 0, dofSelect );
		connect( dofSelect, &QComboBox::currentTextChanged, this, &MusclePlot::dofChanged );

		l->addWidget( view );
	}

	void MusclePlot::init( Model& model )
	{
		clear();
		for ( auto* d : model.GetDofs() )
			if ( d->IsRotational() )
				dofSelect->addItem( to_qt( d->GetName() ) );
	}

	void MusclePlot::clear()
	{
		storage.Clear();
		view->reloadData();
		dofSelect->blockSignals( true );
		dofSelect->clear();
		dofSelect->blockSignals( false );
	}

	void MusclePlot::setDof( Model& model, const QString& dof_name )
	{
		Dof* dof = FindByName( model.GetDofs(), dof_name.toStdString() );
		storage.Clear();

		State original_state = model.GetState();
		model.SetNullState();

		auto r = dof->GetRange();
		auto step = r.GetLength() / 100.0;
		for ( auto v = r.min; v <= r.max; v += step ) {
			dof->SetPos( v );
			model.UpdateStateFromDofs();
			auto& f = storage.AddFrame( v );
			for ( auto mus : model.GetMuscles() )
				if ( mus->HasMomentArm( *dof ) )
					StoreMuscleData( f, *mus, *dof );
		}

		storageModel.setStorage( &storage );
		view->reloadData();
		view->setRange( r.min, r.max );

		model.SetState( original_state, 0.0 );
		model.UpdateStateFromDofs();
	}

	void MusclePlot::StoreMuscleData( Storage<Real>::Frame& frame, const Muscle& mus, const Dof& dof ) const
	{
		const auto& name = mus.GetName();

		// tendon / mtu properties
		frame[name + ".moment_arm"] = mus.GetMomentArm( dof );
		frame[name + ".fiber_length"] = mus.GetFiberLength();
		frame[name + ".fiber_length_norm"] = mus.GetNormalizedFiberLength();
		frame[name + ".tendon_length"] = mus.GetTendonLength();
		frame[name + ".tendon_length_norm"] = mus.GetNormalizedTendonLength() - 1;
		//frame[name + ".mtu_length"] = mus.GetLength();
		//frame[name + ".mtu_length_norm"] = mus.GetLength() / ( mus.GetNormalizedFiberLength() + mus.GetTendonSlackLength() );
		frame[name + ".mtu_force"] = mus.GetForce();
		frame[name + ".mtu_force_norm"] = mus.GetNormalizedForce();

		// fiber properties
		frame[name + ".cos_pennation_angle"] = mus.GetCosPennationAngle();
		frame[name + ".force_length_multiplier"] = mus.GetActiveForceLengthMultipler();
	}
}
