#include "external_tools.h"

#include "scone/core/Exception.h"
#include "scone/core/Log.h"

#include <QProcess>

namespace scone
{
	void evaluateDeprlCheckpoint( const QString& file, QWidget* parent )
	{
		QProgressDialog dlg( "Evaluating " + file, "Abort", 0, 1000, parent );
		dlg.setWindowModality( Qt::WindowModal );
		dlg.show();
		QApplication::processEvents();

		xo::log::info( "Evaluating ", file.toStdString() );
		QProcess process( parent );
		QString program{ "python" };
		QStringList args{ "-m", "deprl.play", "--checkpoint_file", file };
		process.setProcessChannelMode(QProcess::MergedChannels);
		process.start( "python", args );
		if ( !process.waitForStarted( 5000 ) ) {
			SCONE_ERROR( "Could not start process:\n\n" + ( program + " " + args.join( ' ' ) ).toStdString() );
		}

		QByteArray data;
		int i = 0;
		while ( process.waitForReadyRead() ) {
			data = process.readLine();
			xo::log::debug( data.toStdString() );
			i = i + ( 1000 - i ) / 2;
			dlg.setValue( i );
		}
		process.close();
	}
}

