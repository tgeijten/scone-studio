#pragma once

class QWidget;

namespace scone
{
	class StudioModel;
	void ShowModelConversionDialog( QWidget* parent );
	QString ShowConvertScenarioDialog( QWidget* parent, StudioModel& scenario );
}
