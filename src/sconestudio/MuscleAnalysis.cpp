#include "MuscleAnalysis.h"
#include "StudioSettings.h"
#include "scone/model/Dof.h"
#include "scone/model/Model.h"
#include "scone/model/Muscle.h"
#include "scone/core/Angle.h"
#include "qt_convert.h"
#include <QHBoxLayout>
#include <QSizePolicy>

namespace scone
{
	MuscleAnalysis::MuscleAnalysis( QWidget* parent ) :
		storage(),
		storageModel( &storage )
	{
		QHBoxLayout* baseLayout = new QHBoxLayout( this );
		baseLayout->setMargin( 0 );

		view = new QDataAnalysisView( storageModel, this );
		view->setObjectName( "Muscle Analysis" );
		view->setAutoFitVerticalAxis( scone::GetStudioSettings().get<bool>( "analysis.auto_fit_vertical_axis" ) );
		view->setLineWidth( scone::GetStudioSettings().get<float>( "analysis.line_width" ) );
		connect( view, &QDataAnalysisView::timeChanged, this,
			[this]( TimeInSeconds t ) { emit dofValueChanged( dofName, DegToRad( t ) ); view->setTime( t, true ); } );

		// add combobox and button
		dofSelect = new QComboBox( view );
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
		dofSelect->setPlaceholderText( "Select Coordinate" );
#endif
		connect( dofSelect, &QComboBox::currentTextChanged, this, &MuscleAnalysis::dofChanged );

		dofReload = new QPushButton( view );
		dofReload->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Minimum );
		dofReload->setIcon( style()->standardIcon( QStyle::SP_BrowserReload ) );
		connect( dofReload, &QPushButton::clicked, this, [this]() { dofChanged( dofName ); } );

		auto* comboLayout = new QHBoxLayout();
		comboLayout->addWidget( dofSelect );
		comboLayout->addWidget( dofReload );
		view->itemGroupWidget()->layout_->insertItem( 0, comboLayout );

		baseLayout->addWidget( view );
	}

	void MuscleAnalysis::init( Model& model )
	{
		clear();
		for ( auto* d : model.GetDofs() )
			if ( d->GetJoint() )
				dofSelect->addItem( to_qt( d->GetName() ) );
		dofSelect->blockSignals( true );
		dofSelect->setCurrentText( dofName );
		dofSelect->blockSignals( false );
	}

	void MuscleAnalysis::clear()
	{
		storage.Clear();
		view->reloadData();
		dofSelect->blockSignals( true );
		dofSelect->clear();
		dofSelect->blockSignals( false );
		activeDof = nullptr;
	}

	void MuscleAnalysis::setDof( Model& model, const QString& dof_name )
	{
		activeDof = FindByName( model.GetDofs(), dof_name.toStdString() );
		dofName = dof_name;
		storage.Clear();

		State original_state = model.GetState();
		auto rr = activeDof->GetRange();
		BoundsDeg r = BoundsRad( rr.min, rr.max );
		Real max_steps = 180.0;
		Degree step = std::max( Degree( 1 ), r.length() / max_steps );
		for ( auto v = r.lower; v <= r.upper; v += step ) {
			activeDof->SetPos( v.rad_value() );
			model.UpdateStateFromDofs();
			auto& f = storage.AddFrame( v.deg_value() );
			for ( auto mus : model.GetMuscles() )
				if ( mus->HasMomentArm( *activeDof ) )
					StoreMuscleData( f, *mus, *activeDof );
		}

		storageModel.setStorage( &storage );
		view->reloadData();
		view->setRange( r.lower.deg_value(), r.upper.deg_value() );

		model.SetState( original_state, 0.0 );
		model.UpdateStateFromDofs();
	}

	void MuscleAnalysis::setEnableEditing( bool enable )
	{
		dofSelect->setDisabled( !enable );
		dofReload->setDisabled( !enable );
	}

	void MuscleAnalysis::StoreMuscleData( Storage<Real>::Frame& frame, const Muscle& mus, const Dof& dof ) const
	{
		const auto& name = mus.GetName();
		bool normalized_only = true;

		if ( !normalized_only ) {
			frame[name + ".fiber_length"] = mus.GetFiberLength();
			frame[name + ".tendon_length"] = mus.GetTendonLength();
			//frame[name + ".mtu_length"] = mus.GetLength();
			frame[name + ".mtu_force"] = mus.GetForce();
		}

		// tendon / mtu properties
		frame[name + ".moment_arm"] = mus.GetMomentArm( dof );
		frame[name + ".fiber_length_norm"] = mus.GetNormalizedFiberLength();
		frame[name + ".tendon_length_norm"] = mus.GetNormalizedTendonLength() - 1;
		//frame[name + ".mtu_length_norm"] = mus.GetLength() / ( mus.GetNormalizedFiberLength() + mus.GetTendonSlackLength() );
		frame[name + ".mtu_force_norm"] = mus.GetNormalizedForce();

		// fiber properties
		frame[name + ".activation"] = mus.GetActivation();
		frame[name + ".cos_pennation_angle"] = mus.GetCosPennationAngle();
		frame[name + ".force_length_multiplier"] = mus.GetActiveForceLengthMultipler();
	}
}
