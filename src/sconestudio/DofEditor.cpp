#include "DofEditor.h"

#include <QBoxLayout>
#include <QScrollArea>
#include <QStyle>

#include "scone/model/Dof.h"
#include "qt_convert.h"
#include "xo/system/log.h"

namespace scone
{
	inline double degToRadIf( double v, bool conv ) { return conv ? xo::deg_to_rad( v ) : v; }
	inline double radToDegIf( double v, bool conv ) { return conv ? xo::rad_to_deg( v ) : v; }

	DofEditor::DofEditor( const Dof& dof ) : 
		useDegrees( dof.IsRotational() ),
		stepSize_( 0.01 )
	{
		QHBoxLayout* l = new QHBoxLayout;
		setLayout( l );
		l->setMargin( 0 );

		label_ = new QLabel( to_qt( dof.GetName() ) );

		const auto r = dof.GetRange();
		auto pos = radToDegIf( dof.GetPos(), useDegrees );
		auto min = radToDegIf( r.min, useDegrees );
		auto max = radToDegIf( r.max, useDegrees );

		spin_ = new QDoubleSpinBox( this );
		spin_->setSingleStep( useDegrees ? 1.0 : 0.01 );
		spin_->setDecimals( xo::round_cast<int>( log10( 1 / stepSize_ ) ) );
		spin_->setRange( min, max );
		spin_->setAlignment( Qt::AlignRight );
		connect( spin_, SIGNAL( valueChanged( double ) ), this, SLOT( spinValueChanged( double ) ) );
		l->addWidget( spin_ );

		min_ = new QLabel( this );
		min_->setDisabled( true );
		min_->setText( QString::asprintf( "%g", min ) );
		min_->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
		l->addWidget( min_ );

		slider_ = new QSlider( Qt::Horizontal, this );
		slider_->setRange( to_int( min ), to_int( max ) );
		slider_->setSingleStep( useDegrees ? to_int( 1 ) : to_int( 0.01 ) );
		slider_->setPageStep( useDegrees ? to_int( 10 ) : to_int( 0.1 ) );
		slider_->setTickInterval( useDegrees ? to_int( 10 ) : to_int( 1 ) );
		slider_->setTickPosition( QSlider::NoTicks );
		connect( slider_, &QSlider::actionTriggered, this, [this]() { spin_->setValue( to_double( slider_->sliderPosition() ) ); } );
		l->addWidget( slider_ );

		max_ = new QLabel( this );
		max_->setDisabled( true );
		max_->setText( QString::asprintf( "%g", max ) );
		max_->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
		l->addWidget( max_ );

		velocity_ = new QDoubleSpinBox( this );
		velocity_->setSingleStep( useDegrees ? 10.0 : 0.1 );
		velocity_->setDecimals( xo::round_cast<int>( log10( 1 / stepSize_ ) ) );
		velocity_->setRange( -999.99, 999.99 );
		velocity_->setAlignment( Qt::AlignRight );
		connect( velocity_, SIGNAL( valueChanged( double ) ), this, SIGNAL( valueChanged() ) );
		l->addWidget( velocity_ );

		setValue( dof );
	}

	void DofEditor::spinValueChanged( double d )
	{
		blockSignals( true );
		slider_->setValue( to_int( d ) );
		blockSignals( false );
		emit valueChanged();
	}

	void DofEditor::setValue( const Dof& dof )
	{
		blockSignals( true );
		auto pos = radToDegIf( dof.GetPos(), useDegrees );
		spin_->setValue( pos );
		slider_->setValue( to_int( pos ) );
		velocity_->setValue( radToDegIf( dof.GetVel(), useDegrees ) );
		blockSignals( false );
	}

	void DofEditor::updateDofFromWidget( Dof& dof )
	{
		dof.SetPos( degToRadIf( spin_->value(), useDegrees ) );
		dof.SetVel( degToRadIf( velocity_->value(), useDegrees ) );
	}

	void DofEditor::addToGrid( QGridLayout* l, int row )
	{
		l->addWidget( label_, row, 0 );
		l->addWidget( spin_, row, 1 );
		l->addWidget( min_, row, 2 );
		l->addWidget( slider_, row, 3 );
		l->addWidget( max_, row, 4 );
		l->addWidget( velocity_, row, 5 );
	}

	DofEditorGroup::DofEditorGroup( QWidget* parent ) :
		QWidget( parent ),
		dofGrid( nullptr )
	{
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
		exportButton->setIcon( style()->standardIcon( QStyle::SP_DirOpenIcon) );
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
		dofEditors.clear();
		qDeleteAll( dofGrid->findChildren<QWidget*>( "", Qt::FindDirectChildrenOnly ) );

		createHeader( "<b>Name</b>", 0, Qt::AlignLeft );
		createHeader( "<b>Value</b>", 1 );
		createHeader( "<b>Velocity</b>", 5 );

		for ( auto& dof : model.GetDofs() )
		{
			auto* edit = new DofEditor( *dof );
			edit->addToGrid( gridLayout, dofEditors.size() + 1 );
			dofEditors.push_back( edit );
			connect( edit, &DofEditor::valueChanged, this, &DofEditorGroup::valueChanged );
		}

		exportButton->show();
	}

	void DofEditorGroup::setSlidersFromDofs( const Model& model )
	{
		const auto& dofs = model.GetDofs();
		SCONE_ASSERT( dofs.size() == dofEditors.size() );
		for ( index_t i = 0; i < dofs.size(); ++i )
			dofEditors[ i ]->setValue( *dofs[ i ] );
	}

	void DofEditorGroup::setDofsFromSliders( Model& model )
	{
		const auto& dofs = model.GetDofs();
		SCONE_ASSERT( dofs.size() == dofEditors.size() );
		for ( index_t i = 0; i < dofs.size(); ++i )
			dofEditors[ i ]->updateDofFromWidget( *dofs[ i ] );
	}

	void DofEditorGroup::setEnableEditing( bool enable )
	{
		dofGrid->setDisabled( !enable );
	}

	void DofEditorGroup::createHeader( const char* str, int col, Qt::Alignment align )
	{
		auto label = new QLabel( str );
		label->setAlignment( align );
		label->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Fixed );
		gridLayout->addWidget( label, 0, col );
	}
}
