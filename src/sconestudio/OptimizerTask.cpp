#include "OptimizerTask.h"

#include "StudioSettings.h"
#include "OptimizerTaskExternal.h"
#include "OptimizerTaskThreaded.h"

namespace scone
{
	OptimizerTask::OptimizerTask( const QString& scenario_file, const QStringList& options ) :
		scenario_file_( scenario_file ),
		options_( options ),
		has_message_( false )
	{}

	OptimizerTask::~OptimizerTask()
	{}

	u_ptr<OptimizerTask> createOptimizerTask( const QString& scenario, const QStringList& args )
	{
		if ( GetStudioSetting<bool>( "optimization.use_external_process" ) )
			return std::make_unique<OptimizerTaskExternal>( scenario, args );
		else return std::make_unique<OptimizerTaskThreaded>( scenario, args );
	}
}
