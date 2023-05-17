#include "DofEditor.h"

#include <QBoxLayout>
#include <QScrollArea>
#include <QStyle>
#include <QtGlobal>

#include "scone/model/Dof.h"
#include "qt_convert.h"
#include "xo/system/log.h"
#include "gui_profiler.h"

namespace scone
{
	inline double degToRadIf( double v, bool conv ) { return conv ? xo::deg_to_rad( v ) : v; }
	inline double radToDegIf( double v, bool conv ) { return conv ? xo::rad_to_deg( v ) : v; }

	DofWidgets::DofWidgets( const Dof& dof, DofEditorGroup* deg, int row ) :
		useDegrees( dof.IsRotational() ),
		stepSize_( 0.01 )
	{
		GUI_PROFILE_FUNCTION;

		label_ = new QLabel( to_qt( dof.GetName() ) );
		deg->grid()->addWidget( label_, row, 0 );

		const auto r = dof.GetRange();
		auto pos = radToDegIf( dof.GetPos(), useDegrees );
		auto min = radToDegIf( r.min, useDegrees );
		auto max = radToDegIf( r.max, useDegrees );

		spin_ = new QDoubleSpinBox();
		spin_->setSingleStep( useDegrees ? 1.0 : 0.01 );
		spin_->setDecimals( xo::round_cast<int>( log10( 1 / stepSize_ ) ) );
		spin_->setRange( min, max );
		spin_->setAlignment( Qt::AlignRight );
		QObject::connect( spin_, QOverload<double>::of( &QDoubleSpinBox::valueChanged ), deg,
			[=]( double d ) { slider_->setValue( to_int( d ) ); emit deg->valueChanged(); } );
		deg->grid()->addWidget( spin_, row, 1 );

		min_ = new QLabel( QString::asprintf( "%g", min ) );
		min_->setDisabled( true );
		min_->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
		deg->grid()->addWidget( min_, row, 2 );

		slider_ = new QSlider( Qt::Horizontal );
		slider_->setRange( to_int( min ), to_int( max ) );
		slider_->setSingleStep( useDegrees ? to_int( 1 ) : to_int( 0.01 ) );
		slider_->setPageStep( useDegrees ? to_int( 10 ) : to_int( 0.1 ) );
		slider_->setTickInterval( useDegrees ? to_int( 10 ) : to_int( 1 ) );
		slider_->setTickPosition( QSlider::NoTicks );
		QObject::connect( slider_, &QSlider::actionTriggered, deg,
			[=]() { spin_->setValue( to_double( slider_->sliderPosition() ) ); } );
		deg->grid()->addWidget( slider_, row, 3 );

		max_ = new QLabel( QString::asprintf( "%g", max ) );
		max_->setDisabled( true );
		max_->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
		deg->grid()->addWidget( max_, row, 4 );

		velocity_ = new QDoubleSpinBox();
		velocity_->setSingleStep( useDegrees ? 10.0 : 0.1 );
		velocity_->setDecimals( useDegrees ? 0 : xo::round_cast<int>( log10( 1 / stepSize_ ) ) );
		double max_vel = useDegrees ? 9999.0 : 99.0;
		velocity_->setRange( -max_vel, max_vel );
		velocity_->setAlignment( Qt::AlignRight );
		QObject::connect( velocity_, QOverload<double>::of( &QDoubleSpinBox::valueChanged ), deg,
			[=]() { emit deg->valueChanged(); } );
		deg->grid()->addWidget( velocity_, row, 5 );

		setWidgetValues( dof );
	}

	void DofWidgets::setWidgetValues( const Dof& dof )
	{
		spin_->setValue( radToDegIf( dof.GetPos(), useDegrees ) );
		velocity_->setValue( radToDegIf( dof.GetVel(), useDegrees ) );
	}

	void DofWidgets::updateDof( Dof& dof ) const
	{
		dof.SetPos( degToRadIf( spin_->value(), useDegrees ) );
		dof.SetVel( degToRadIf( velocity_->value(), useDegrees ) );
	}

	DofEditorGroup::DofEditorGroup( QWidget* parent ) :
		QWidget( parent ),
		dofGrid( nullptr )
	{
		GUI_PROFILE_FUNCTION;

		QVBoxLayout* wl = new QVBoxLayout( this );
		wl->setMargin( 0 );
		wl->setSpacing( 0 );

		QWidget* group = new QWidget( this );
		QVBoxLayout* vl = new QVBoxLayout( group );
		vl->setMargin( 8 );
		vl->setSpacing( 16 );

		dofGrid = new QWidget( group );
		gridLayout = new QGridLayout();
		gridLayout->setMargin( 0 );
		dofGrid->setLayout( gridLayout );
		vl->addWidget( dofGrid );

		exportButton = new QPushButton( "Export Coordinates...", this );
		exportButton->setIcon( style()->standardIcon( QStyle::SP_DirOpenIcon ) );
		connect( exportButton, &QPushButton::pressed, this, &DofEditorGroup::exportCoordinates );
		vl->addWidget( exportButton );
		exportButton->hide();

		vl->insertStretch( -1 );

		auto scrollWidget = new QScrollArea( this );
		scrollWidget->setWidgetResizable( true ); // very important
		scrollWidget->setWidget( group );
		wl->addWidget( scrollWidget );
	}

	void DofEditorGroup::init( const Model& model )
	{
		GUI_PROFILE_FUNCTION;

		dofEditors.clear();
		qDeleteAll( dofGrid->findChildren<QWidget*>( "", Qt::FindDirectChildrenOnly ) );

		createLabel( "<b>Name</b>", 0, 0, Qt::AlignLeft );
		createLabel( "<b>Value</b>", 0, 1 );
		createLabel( "<b>Velocity</b>", 0, 5 );

		blockSignals( true );
		for ( int idx = 0; idx < model.GetDofs().size(); ++idx )
			dofEditors.push_back( new DofWidgets( *model.GetDofs()[ idx ], this, idx + 1 ) );
		blockSignals( false );

		exportButton->show();
	}

	void DofEditorGroup::setSlidersFromDofs( const Model& model )
	{
		GUI_PROFILE_FUNCTION;

		blockSignals( true );
		const auto& dofs = model.GetDofs();
		SCONE_ASSERT( dofs.size() == dofEditors.size() );
		for ( index_t i = 0; i < dofs.size(); ++i )
			dofEditors[ i ]->setWidgetValues( *dofs[ i ] );
		blockSignals( false );
	}

	void DofEditorGroup::setDofsFromSliders( Model& model )
	{
		GUI_PROFILE_FUNCTION;

		const auto& dofs = model.GetDofs();
		SCONE_ASSERT( dofs.size() == dofEditors.size() );
		for ( index_t i = 0; i < dofs.size(); ++i )
			dofEditors[ i ]->updateDof( *dofs[ i ] );
	}

	void DofEditorGroup::setEnableEditing( bool enable )
	{
		dofGrid->setDisabled( !enable );
	}

	void DofEditorGroup::createLabel( const char* str, int row, int col, Qt::Alignment align )
	{
		auto label = new QLabel( str );
		label->setAlignment( align );
		label->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Fixed );
		gridLayout->addWidget( label, row, col );
	}
}
