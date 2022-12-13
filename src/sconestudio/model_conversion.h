#pragma once

class QWidget;

namespace scone
{
	class StudioModel;
	void ShowModelConversionDialog( QWidget* parent );
	void ShowConvertScenarioDialog( QWidget* parent, const StudioModel& scenario );
}
