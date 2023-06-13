#pragma once

#include <QWidget>
#include <QComboBox>
#include "scone/core/Storage.h"
#include "SconeStorageDataModel.h"
#include "QDataAnalysisView.h"

namespace scone
{
	class MusclePlot : public QWidget
	{
		Q_OBJECT

	public:
		MusclePlot( QWidget* parent );
		virtual ~MusclePlot() = default;

		void init( Model& model );
		void clear();
		void setDof( Model& model, const QString& dof );

	signals:
		void dofChanged( const QString& );
		void dofValueChanged( Real );

	private:
		void StoreMuscleData( Storage<Real>::Frame& frame, const Muscle& mus, const Dof& dof ) const;

		scone::Storage<> storage;
		SconeStorageDataModel storageModel;
		QComboBox* dofSelect;
		QDataAnalysisView* view = nullptr;
		Dof* activeDof = nullptr;
	};
}
