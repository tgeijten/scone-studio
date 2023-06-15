#pragma once

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QString>
#include "scone/core/Storage.h"
#include "SconeStorageDataModel.h"
#include "QDataAnalysisView.h"

namespace scone
{
	class MuscleAnalysis : public QWidget
	{
		Q_OBJECT

	public:
		MuscleAnalysis( QWidget* parent );
		virtual ~MuscleAnalysis() = default;

		void init( Model& model );
		void clear();
		void setDof( Model& model, const QString& dof );
		void setEnableEditing( bool enable );

	signals:
		void dofChanged( const QString& );
		void dofValueChanged( const QString&, double );

	private:
		void StoreMuscleData( Storage<Real>::Frame& frame, const Muscle& mus, const Dof& dof ) const;

		scone::Storage<> storage;
		SconeStorageDataModel storageModel;
		QComboBox* dofSelect;
		QPushButton* dofReload;
		QDataAnalysisView* view = nullptr;
		Dof* activeDof = nullptr;
		QString dofName;
	};
}
