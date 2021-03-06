/*
** main.cpp
**
** Copyright (C) 2013-2019 Thomas Geijtenbeek and contributors. All rights reserved.
**
** This file is part of SCONE. For more information, see http://scone.software.
*/

#include "SconeStudio.h"
#include <QtCore/QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    #include <QApplication>
    #include <QMessageBox>
#else
    #include <QtGui/QApplication>
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

int main( int argc, char *argv[] )
{
	xo::timer boot_timer;

	// Qt setup, required before creating QApplication
	QCoreApplication::setAttribute( Qt::AA_UseDesktopOpenGL );
	QApplication::setStyle( "fusion" );

	// create application
	QApplication a( argc, argv );

	try
	{
		// init logging
#if !defined( _MSC_VER ) || defined( _DEBUG )
		xo::log::console_sink console_log_sink( xo::log::level::trace );
#endif
		xo::path log_file = scone::GetSettingsFolder() / "log" / xo::path( xo::get_date_time_str( "%Y%m%d_%H%M%S" ) + ".log" );
		xo::log::file_sink file_sink( log_file, xo::log::level::debug, xo::log::sink_mode::current_thread );
		SCONE_THROW_IF( !file_sink.file_stream().good(), "Could not create file " + log_file.str() );
		xo::log::debug( "Created log file: ", log_file );

		// init plash screen
		QPixmap splash_pm( to_qt( GetSconeStudioFolder() / "resources/ui/scone_splash.png" ) );
		QSplashScreen splash( splash_pm );
		splash.show();
		a.processEvents();

		// init main window
		SconeStudio w;
		w.show();
		w.init();

#if SCONE_HYFYDY_ENABLED
		// check if license agreement has been updated
		if ( scone::GetSconeSetting<bool>( "hyfydy.enabled" ) )
			if ( !scone::CheckHfdLicenseAgreement( scone::GetSconeSetting<String>( "hyfydy.license" ) ) )
				scone::ShowLicenseDialog( &w );
#endif

		// init scone file format and libraries
		scone::Initialize();

		// close splash screen
		splash.close(); // DO NOT USE QSplashScreen::finish() because it's slow

		// show startup time (optional)
		if ( scone::GetStudioSetting<bool>( "ui.show_startup_time") )
			xo::log::debug( "SCONE startup time: ", boot_timer().secondsd(), "s" );

		return a.exec();
	}
	catch ( std::exception& e )
	{
		QMessageBox::critical( 0, "Exception", e.what() );
	}
	catch ( ... )
	{
		QMessageBox::critical( 0, "Exception", "Unknown Exception" );
	}
}

#ifdef _WIN32
#ifndef DEBUG
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	return main( __argc, __argv );
}
#endif
#endif
