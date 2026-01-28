#include "UserInputEditor.h"

#include <QBoxLayout>
#include <QScrollArea>
#include <QStyle>
#include <QtGlobal>

#include "qt_convert.h"
#include "xo/system/log.h"
#include <cmath>

namespace scone
{
	inline double degToRadIf( double v, bool conv ) { return conv ? xo::deg_to_rad( v ) : v; }
	inline double radToDegIf( double v, bool conv ) { return conv ? xo::rad_to_deg( v ) : v; }

	UserInputItem::UserInputItem( UserInput& input, class UserInputEditor* editor, int row ) :
		input_( input ),
		range_( input.GetRange() ),
		animatePeriod( 10 ),
		animateOffset( 0 )
	{
		auto decimals = std::min( 2, static_cast<int>( std::ceil( std::log10( 50.0 / range_.GetLength() ) ) ) );
		stepSize = 1.0 / std::pow( 10, decimals );

		label_ = new QLabel( to_qt( input_.GetLabel() ) );
		editor->grid()->addWidget( label_, row, 0 );

		spin_ = new QDoubleSpinBox();
		spin_->setSingleStep( stepSize );
		spin_->setDecimals( decimals );
		spin_->setRange( input_.GetRange().min, input_.GetRange().max );
		spin_->setAlignment( Qt::AlignRight );
		QObject::connect( spin_, QOverload<double>::of( &QDoubleSpinBox::valueChanged ), editor,
			[=]( double d ) { input_.SetValue( d ); slider_->setValue( to_int( d ) ); emit editor->valueChanged(); } );
		editor->grid()->addWidget( spin_, row, 1 );

		min_ = new QLabel( QString::asprintf( "%g", input_.GetRange().min ) );
		min_->setDisabled( true );
		min_->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
		editor->grid()->addWidget( min_, row, 2 );

		slider_ = new QSlider( Qt::Horizontal );
		slider_->setRange( to_int( input_.GetRange().min ), to_int( input_.GetRange().max ) );
		slider_->setSingleStep( to_int( stepSize ) );
		slider_->setPageStep( to_int( 0.1 ) );
		slider_->setTickInterval( to_int( 1 ) );
		slider_->setTickPosition( QSlider::NoTicks );
		QObject::connect( slider_, &QSlider::actionTriggered, editor,
			[=]() { spin_->setValue( to_double( slider_->sliderPosition() ) ); } );
		editor->grid()->addWidget( slider_, row, 3 );

		max_ = new QLabel( QString::asprintf( "%g", input_.GetRange().max ) );
		max_->setDisabled( true );
		max_->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
		editor->grid()->addWidget( max_, row, 4 );

		animate_ = new QCheckBox();
		animate_->setCheckState( Qt::Unchecked );
		editor->grid()->addWidget( animate_, row, 5 );

		updateWidgetValues();
	}

	void UserInputItem::updateWidgetValues()
	{
		spin_->setValue( input_.GetValue() );
	}

	void UserInputItem::animateValue( double t )
	{
		if ( animate_->isChecked() )
		{
			auto tw = xo::triangle_wave( animateOffset + ( t / animatePeriod ) );
			auto r = input_.GetRange();
			spin_->setValue( tw * r.GetLength() + r.min );
		}
	}

	UserInputEditor::UserInputEditor( QWidget* parent ) :
		QWidget( parent ),
		dofGrid( nullptr )
	{
		QVBoxLayout* wl = new QVBoxLayout( this );
		wl->setContentsMargins( 0, 0, 0, 0 );
		wl->setSpacing( 0 );

		QWidget* group = new QWidget( this );
		QVBoxLayout* vl = new QVBoxLayout( group );
		vl->setContentsMargins( 8, 8, 8, 8 );
		vl->setSpacing( 8 );

		dofGrid = new QWidget( group );
		gridLayout = new QGridLayout();
		gridLayout->setContentsMargins( 0, 0, 0, 0 );
		dofGrid->setLayout( gridLayout );
		vl->addWidget( dofGrid );

		buttonGroup = new QWidget( this );
		QHBoxLayout* buttonLayout = new QHBoxLayout( buttonGroup );
		buttonLayout->setContentsMargins( 0, 0, 0, 0 );
		buttonLayout->setSpacing( 4 );
		auto animateButton = new QPushButton( "Toggle Animation", this );
		connect( animateButton, &QPushButton::pressed, this, &UserInputEditor::toggleAnimation );
		buttonLayout->addWidget( animateButton );
		auto saveButton = new QPushButton( "Save UserInputs to ", this );
		saveButton->setIcon( style()->standardIcon( QStyle::SP_DirOpenIcon ) );
		connect( saveButton, &QPushButton::pressed, this, &UserInputEditor::savePressed );
		buttonLayout->addWidget( saveButton );
		buttonGroup->hide();
		vl->addWidget( buttonGroup );

		vl->insertStretch( -1 );

		auto scrollWidget = new QScrollArea( this );
		scrollWidget->setWidgetResizable( true ); // very important
		scrollWidget->setWidget( group );
		wl->addWidget( scrollWidget );

		connect( &qtimer_, &QTimer::timeout, this, &UserInputEditor::timeout );
	}

	void UserInputEditor::init( const Model& model )
	{
		items.clear();
		qDeleteAll( dofGrid->findChildren<QWidget*>( "", Qt::FindDirectChildrenOnly ) );

		blockSignals( true );
		String groupName;
		int row = 0;
		for ( auto& input : model.GetUserInputs() )
		{
			if ( input->GetGroupName() != groupName )
				createLabel( groupName = input->GetGroupName(), row++, Qt::AlignLeft );
			items.push_back( new UserInputItem( *input, this, row++ ) );
		}
		blockSignals( false );

		buttonGroup->show();
	}

	void UserInputEditor::setEnableEditing( bool enable )
	{
		dofGrid->setDisabled( !enable );
	}

	void UserInputEditor::timeout()
	{
		blockSignals( true );
		auto t = timer_().secondsd();
		for ( auto item : items )
			item->animateValue( t );
		blockSignals( false );
		emit valueChanged();
	}

	void UserInputEditor::toggleAnimation()
	{
		if ( !qtimer_.isActive() )
		{
			for ( auto item : items )
				item->animateOffset = ( item->spin_->value() - item->range_.min ) / item->range_.GetLength();
			qtimer_.start( 20 );
			timer_.restart();
		}
		else qtimer_.stop();
	}

	void UserInputEditor::createLabel( const String& str, int row, Qt::Alignment align )
	{
		auto label = new QLabel( ( "<b>" + str + "</b>" ).c_str() );
		label->setStyleSheet( "QLabel { background-color : white; color : black; }" );
		label->setAlignment( align );
		label->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Fixed );
		gridLayout->addWidget( label, row, 0, 1, 5 );
	}
}
