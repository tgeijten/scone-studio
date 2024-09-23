#include "external_tools.h"

#include "scone/core/Exception.h"
#include "scone/core/Log.h"

#include <QProcess>
#include <QProgressDialog>
#include <QApplication>
#include <QDir>
#include "StudioSettings.h"

namespace scone
{
	std::unique_ptr<QProcess> makeProcess( const QString& file, QStringList& args, QWidget* parent )
	{
		auto process = std::make_unique<QProcess>( parent );
		process->setProcessChannelMode( QProcess::MergedChannels );
		process->setProgram( file );
		process->setArguments( args );
		return process;
	}

	void startProcess( QProcess& p ) {
		p.start();
		if ( !p.waitForStarted() )
			SCONE_ERROR( "Could not start process:\n\n" + p.program().toStdString() + p.arguments().join( ' ' ).toStdString() );
	}

	std::unique_ptr<QProcess> startProcess( const QString& file, QStringList& args, QWidget* parent ) {
		auto p = makeProcess( file, args, parent );
		startProcess( *p );
		return p;
	}

	bool waitForProcess( QProcess& p, const QString& title, QWidget* parent, log::level l ) {
		QProgressDialog dlg( title, "Abort", 0, 1000, parent );
		dlg.setWindowModality( Qt::WindowModal );
		dlg.show();
		QApplication::processEvents();

		int progress = 0;
		QByteArray data;
		while ( !dlg.wasCanceled() && p.state() == QProcess::Running ) {
			if ( p.waitForReadyRead( 3000 ) ) {
				data = p.readAll();
				xo::log::message( l, data.toStdString() );
			}
			progress = progress + 0.1 * ( 1000 - progress );
			dlg.setValue( progress );
		}
		p.close();

		return dlg.wasCanceled();
	}

	bool runExternalProcess( const QString& title, const QString& file, QStringList& args, QWidget* parent, log::level l )
	{
		auto process = startProcess( file, args, parent );
		return waitForProcess( *process, title, parent, l );
	}

	QProcessPtr makeCheckpointProcess( const QString& file, QWidget* parent  ) {
		auto f = QDir::toNativeSeparators( file );
		auto episodes = QString::number( scone::GetStudioSetting<int>( "sconegym.num_episodes" ) );
		auto python = QString( scone::GetStudioSetting<String>( "sconegym.python" ).c_str() );
		QStringList args{ "-m", "deprl.play", "--checkpoint_file", f, "--num_episodes", episodes };
		return makeProcess( python, args, parent );
	}

	bool evaluateCheckpointSync( const QString& file, QWidget* parent )
	{
		auto p = makeCheckpointProcess( file, parent );
		xo::log::info( "Evaluating ", file.toStdString() );
		startProcess( *p );
		return waitForProcess( *p,  "Evaluating " + file, parent, log::level::info );
	}

	QProcessPtr evaluateCheckpointAsync( const QString& file, QWidget* parent )
	{
		auto p = makeCheckpointProcess( file, parent );
		startProcess( *p );
		return p;
	}
}

