#include "DofEditor.h"

#include <QBoxLayout>
#include <QScrollArea>

#include "scone/model/Dof.h"
#include "qt_convert.h"
#include "xo/system/log.h"

namespace scone
{
	DofEditor::DofEditor( QWidget* parent ) :
		QWidget( parent ),
		useDegrees( true ),
		dofSliderGroup( nullptr )
	{
		QVBoxLayout* l = new QVBoxLayout;
		l->setMargin( 4 );
		l->setSpacing( 4 );
		setLayout( l );

		dofSliderGroup = new QFormGroup( this );
		auto scrollWidget = new QScrollArea( this );
		scrollWidget->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOn );
		scrollWidget->setWidgetResizable( true ); // very important
		scrollWidget->setWidget( dofSliderGroup );
		l->addWidget( scrollWidget );
	}

	void DofEditor::init( const Model& model )
	{
		dofSliders.clear();
		dofSliderGroup->clear();
		for ( auto& dof : model.GetDofs() )
		{
			auto* slider = new QValueSlider( this );
			slider->setRange( dofToSliderValue( *dof, dof->GetRange().min ), dofToSliderValue( *dof, dof->GetRange().max ) );
			slider->setValue( dofToSliderValue( *dof, dof->GetPos() ) );
			dofSliders.emplace_back( slider );
			dofSliderGroup->addRow( to_qt( dof->GetName() ), slider );
			connect( slider, &QValueSlider::valueChanged, this, &DofEditor::valueChanged );
		}
	}

	void DofEditor::setSlidersFromDofs( const Model& model )
	{
		const auto& dofs = model.GetDofs();
		SCONE_ASSERT( dofs.size() == dofSliders.size() );
		for ( index_t i = 0; i < dofs.size(); ++i )
			dofSliders[ i ]->setValue( dofToSliderValue( *dofs[ i ], dofs[ i ]->GetPos() ) );
	}

	void DofEditor::setDofsFromSliders( Model& model )
	{
		const auto& dofs = model.GetDofs();
		SCONE_ASSERT( dofs.size() == dofSliders.size() );
		for ( index_t i = 0; i < dofs.size(); ++i )
			dofs[ i ]->SetPos( sliderToDofValue( *dofs[ i ], dofSliders[ i ]->value() ) );
	}

	double DofEditor::dofToSliderValue( const Dof& d, double value )
	{
		return d.IsRotational() && useDegrees ? xo::rad_to_deg( value ) : value;
	}

	double DofEditor::sliderToDofValue( const Dof& d, double value )
	{
		return d.IsRotational() && useDegrees ? xo::deg_to_rad( value ) : value;
	}
}
