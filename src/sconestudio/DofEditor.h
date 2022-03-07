#pragma once

#include <QWidget>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QSlider>
#include <QGridLayout>
#include <QPushButton>

#include "scone/model/Model.h"

namespace scone
{
	struct DofWidgets
	{
		DofWidgets( const Dof& dof, class DofEditorGroup* deg, int row );

		void setWidgetValues( const Dof& dof );
		void updateDof( Dof& dof ) const;

		int to_int( double d ) { return xo::round_cast<int>( d / stepSize_ ); }
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
		QGridLayout* grid() { return gridLayout; }

	signals:
		void valueChanged();
		void exportCoordinates();

	private:
		void createLabel( const char* str, int row, int col, Qt::Alignment align = Qt::AlignCenter );
		QWidget* dofGrid;
		QPushButton* exportButton;
		QGridLayout* gridLayout;
		std::vector<DofWidgets*> dofEditors;
	};
}
