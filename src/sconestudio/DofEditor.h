#pragma once

#include <QWidget>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QSlider>
#include <QGridLayout>

#include "scone/model/Model.h"

namespace scone
{
	class DofEditor : public QWidget
	{
		Q_OBJECT

	public:
		DofEditor( const Dof& dof );
		void setValue( const Dof& dof );
		void updateDofFromWidget( Dof& dof );
		void addToGrid( QGridLayout* l, int row );

		double position() { return spin_->value(); }
		double velocity() { return velocity_->value(); }

	signals:
		void valueChanged( double d );

	private slots:
		void spinValueChanged( double d );
		void sliderAction( int i );

	private:
		int to_int( double d ) { return int( d / stepSize_ ); }
		double to_double( int i ) { return stepSize_ * i; }

		QLabel* label_;
		QDoubleSpinBox* spin_;
		QLabel* min_;
		QSlider* slider_;
		QLabel* max_;
		QDoubleSpinBox* velocity_;

		bool useDegrees;
		double stepSize_;
	};

	class DofEditorGroup : public QWidget
	{
		Q_OBJECT

	public:
		DofEditorGroup( QWidget* parent );
		~DofEditorGroup() = default;

		void init( const Model& model );
		void setSlidersFromDofs( const Model& model );
		void setDofsFromSliders( Model& model );
		void setEnableEditing( bool enable );

	signals:
		void valueChanged();

	private:
		void createHeader( const char* str, int col, Qt::Alignment align = Qt::AlignCenter );
		QWidget* dofGrid;
		QGridLayout* gridLayout;
		std::vector<DofEditor*> dofEditors;
	};
}
