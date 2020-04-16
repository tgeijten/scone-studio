#include "OptimizerTaskExternal.h"

#include "qt_convert.h"
#include "scone/core/Exception.h"
#include "scone/core/Log.h"
#include "scone/core/string_tools.h"
#include "studio_config.h"
#include "xo/filesystem/filesystem.h"
#include "xo/serialization/prop_node_serializer_zml.h"
#include "xo/string/string_tools.h"

#include <sstream>

namespace scone
{
	OptimizerTaskExternal::OptimizerTaskExternal( const QString& scenario, const QStringList& options ) :
		OptimizerTask( scenario, options ),
		process_( nullptr ),
		send_process_closed_mesage_( false )
	{
		QString program = to_qt( xo::get_application_dir() / SCONE_SCONECMD_EXECUTABLE );
		QStringList args;
		args << "-o" << scenario_file_ << "-s" << "-q" << "-l" << "7" << options_;

		process_ = new QProcess();
		process_->setReadChannel( QProcess::StandardOutput );
		process_->start( program, args );
		if ( !process_->waitForStarted( 5000 ) )
			SCONE_ERROR( "Could not start scenario " + scenario_file_.toStdString() );

		scone::log::info( "Optimizing scenario ", scenario_file_.toStdString() );

		while ( !process_->waitForReadyRead( 1000 ) )
			scone::log::debug( "Waiting for process to start" );
	}

	OptimizerTaskExternal::~OptimizerTaskExternal()
	{
		if ( process_ )
		{
			if ( process_->isOpen() )
 			{
				scone::log::warning( "Deleting OptimizerTaskExternal with open process" );
				interrupt();
			}
			delete process_;
		}
	}

	bool OptimizerTaskExternal::interrupt()
	{
		if ( process_->isOpen() )
		{
			process_->close();
			send_process_closed_mesage_ = true;
			return true;
		}
		else return false;
	}

	void OptimizerTaskExternal::finish()
	{
		if ( process_->isOpen() )
			process_->close();
	}

	std::deque<PropNode> OptimizerTaskExternal::getMessages()
	{
		std::deque<PropNode> messages;
		while ( process_->canReadLine() )
		{
			xo::error_code ec;
			char buf[ 4096 ];
			string msg;
			while ( msg.empty() || !xo::str_begins_with( msg, '*' ) )
			{
				process_->readLine( buf, sizeof( buf ) - 1 );
				msg = buf;
			}
			std::stringstream str( msg.substr( 1 ) );
			xo::prop_node pn;
			xo::prop_node_serializer_zml zml( pn, &ec );
			str >> zml;
			if ( ec.good() )
				messages.push_back( std::move( pn ) );
			else xo::log::warning( "Error parsing external message: ", ec.message() );
		}

		if ( send_process_closed_mesage_ )
		{
			// send a single message to acknowledge gui
			send_process_closed_mesage_ = false;
			messages.emplace_back( PropNode( { { "finished", PropNode( "Task interrupted" ) } } ) );
		}

		return messages;
	}
}
