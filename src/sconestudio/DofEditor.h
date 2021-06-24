#pragma once

#include <QWidget>
#include "QGroup.h"
#include "QValueSlider.h"
#include "scone/model/Model.h"

namespace scone
{
	class DofEditor : public QWidget
	{
		Q_OBJECT

	public:
		DofEditor( QWidget* parent );
		~DofEditor() = default;

		void init( const Model& model );
		void setSlidersFromDofs( const Model& model );
		void setDofsFromSliders( Model& model );

		bool useDegrees;

	signals:
		void valueChanged();

	private:
		std::vector<std::string> dofNames;
		QFormGroup* dofSliderGroup;
		std::vector<QValueSlider*> dofSliders;

		double dofToSliderValue( const Dof& d, double value );
		double sliderToDofValue( const Dof& d, double value );
	};
}
