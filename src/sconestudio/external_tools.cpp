#include "external_tools.h"

#include "scone/core/Exception.h"
#include "scone/core/Log.h"

#include <QProcess>
#include <QProgressDialog>
#include <QApplication>

namespace scone
{
	void runExternalProcess( const QString& title, const QString& file, QStringList& args, QWidget* parent, log::level l )
	{
		QProgressDialog dlg( title, "Abort", 0, 1000, parent );
		dlg.setWindowModality( Qt::WindowModal );
		dlg.show();
		QApplication::processEvents();

		QProcess process( parent );
		process.setProcessChannelMode( QProcess::MergedChannels );
		process.start( file, args );
		QString command = file + ' ' + args.join( ' ' );
		if ( !process.waitForStarted() )
			SCONE_ERROR( "Could not start process:\n\n" + command.toStdString() );

		int progress = 0;
		QByteArray data;
		while ( !dlg.wasCanceled() && process.state() == QProcess::Running ) {
			if ( process.waitForReadyRead( 3000 ) ) {
				data = process.readAll();
				xo::log::message( l, data.toStdString() );
			}
			progress = progress + 0.1 * ( 1000 - progress );
			dlg.setValue( progress );
		}
		process.close();
	}

	void evaluateDeprlCheckpoint( const QString& file, QWidget* parent )
	{
		auto episodes = 5;
		xo::log::info( "Evaluating ", file.toStdString() );
		QStringList args{ "-m", "deprl.play", "--checkpoint_file", file, "--num_episodes", QString::number( episodes ) };
		runExternalProcess( "Evaluating " + file, "python", args, parent, log::level::info );
	}
}

