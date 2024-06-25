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
		process->start( file, args );
		QString command = file + ' ' + args.join( ' ' );
		if ( !process->waitForStarted() )
			SCONE_ERROR( "Could not start process:\n\n" + command.toStdString() );
		return process;
	}

	bool runExternalProcess( const QString& title, const QString& file, QStringList& args, QWidget* parent, log::level l )
	{
		QProgressDialog dlg( title, "Abort", 0, 1000, parent );
		dlg.setWindowModality( Qt::WindowModal );
		dlg.show();
		QApplication::processEvents();

		auto process = makeProcess( file, args, parent );

		int progress = 0;
		QByteArray data;
		while ( !dlg.wasCanceled() && process->state() == QProcess::Running ) {
			if ( process->waitForReadyRead( 3000 ) ) {
				data = process->readAll();
				xo::log::message( l, data.toStdString() );
			}
			progress = progress + 0.1 * ( 1000 - progress );
			dlg.setValue( progress );
		}
		process->close();

		return dlg.wasCanceled();
	}

	bool evaluateCheckpointSync( const QString& file, QWidget* parent )
	{
		auto f = QDir::toNativeSeparators( file );
		auto episodes = QString::number( scone::GetStudioSetting<int>( "sconegym.num_episodes" ) );
		auto python = QString( scone::GetStudioSetting<String>( "sconegym.python" ).c_str() );
		xo::log::info( "Evaluating ", f.toStdString() );
		QStringList args{ "-m", "deprl.play", "--checkpoint_file", f, "--num_episodes", episodes };
		return runExternalProcess( "Evaluating " + f, python, args, parent, log::level::info );
	}

	QProcessPtr evaluateCheckpointAsync( const QString& file, QWidget* parent )
	{
		auto f = QDir::toNativeSeparators( file );
		auto episodes = QString::number( scone::GetStudioSetting<int>( "sconegym.num_episodes" ) );
		auto python = QString( scone::GetStudioSetting<String>( "sconegym.python" ).c_str() );
		xo::log::info( "Evaluating ", f.toStdString() );
		QStringList args{ "-m", "deprl.play", "--checkpoint_file", f, "--num_episodes", episodes };
		return makeProcess( python, args, parent );
	}
}

