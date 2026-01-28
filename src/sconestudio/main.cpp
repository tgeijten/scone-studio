/*
** main.cpp
**
** Copyright (C) Thomas Geijtenbeek and contributors. All rights reserved.
**
** This file is part of SCONE. For more information, see http://scone.software.
*/

#include "SconeStudio.h"
#include <QtCore/QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#	include <QApplication>
#	include <QMessageBox>
#else
#	include <QtGui/QApplication>
#endif

#include "scone/core/system_tools.h"
#include "scone/core/string_tools.h"
#include "scone/core/Log.h"

#include "xo/system/log_sink.h"
#include "xo/system/system_tools.h"
#include "xo/filesystem/filesystem.h"
#include "xo/serialization/prop_node_serializer_zml.h"
#include "scone/core/version.h"

#include "scone/sconelib_config.h"
#include "scone/core/Exception.h"
#include "studio_tools.h"
#include "StudioSettings.h"
#include "QSafeApplication.h"
#include "scone/core/profiler_config.h"
#include <clocale>

int main( int argc, char* argv[] )
{
	// Qt setup, required before creating QApplication
	QCoreApplication::setAttribute( Qt::AA_UseDesktopOpenGL );
	QApplication::setStyle( "fusion" );
	QSafeApplication app( argc, argv );
	scone::TimeSection( "InitApplication" );

	// QApplication changes C locale on Linux, set it back to "C"
	std::setlocale( LC_ALL, "C" );

	// Fix inactive selection color being nearly invisible
	app.setStyleSheet("QTreeView::item:selected:!active { background-color: lightgray }");

	try
	{
		// init console logging
#if defined( _DEBUG )
		xo::log::console_sink console_log_sink( xo::log::level::trace );
#elif !defined( _MSC_VER )
		xo::log::console_sink console_log_sink( xo::log::level::info );
#endif
		xo::path log_file = scone::GetSettingsFolder() / "log" / xo::path( xo::get_date_time_str( "%Y%m%d_%H%M%S" ) + ".log" );
		xo::log::file_sink file_sink( log_file, xo::log::level::debug, xo::log::sink_mode::current_thread );
		SCONE_THROW_IF( !file_sink.file_stream().good(), "Could not create file " + log_file.str() );
		xo::log::debug( "Created log file: ", log_file );
		scone::TimeSection( "InitLog" );

		// init plash screen
		QPixmap splash_pm( to_qt( scone::GetSconeStudioFolder() / "resources/ui/scone_splash.png" ) );
		QSplashScreen splash( splash_pm );
		splash.show();
		app.processEvents();
		scone::TimeSection( "ShowSplash" );

		// init main window
		SconeStudio w;
		w.show();
		w.init();
		scone::TimeSection( "ShowSconeStudio" );

		// init scone file format and libraries
		scone::Initialize();
		scone::TimeSection( "InitScone" );

#if SCONE_HYFYDY_ENABLED
		// check if license agreement has been updated
		if ( sconehfd::GetHfdStatus() == sconehfd::SconeHfdStatus::LicenseNotAccepted )
			scone::ShowLicenseDialog( &w );
#endif

		// close splash screen
		splash.close(); // DO NOT USE QSplashScreen::finish() because it's slow
		
		// open file if argument was passed
		if ( argc >= 2 )
			w.openFile( argv[1] );

		return app.exec();
	}
	catch ( std::exception& e )
	{
		QMessageBox::critical( nullptr, "Exception", e.what() );
	}
	catch ( ... )
	{
		QMessageBox::critical( nullptr, "Exception", "Unknown Exception" );
	}
}

#if defined( _WIN32 ) && !defined( DEBUG )
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, char* lpCmdLine, int nShowCmd )
{
	return main( __argc, __argv );
}
#endif
