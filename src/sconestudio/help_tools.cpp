#include "help_tools.h"
#include <QFile>
#include <QMessageBox>
#include <QDesktopServices>
#include "xo/container/flat_map.h"
#include "xo/filesystem/filesystem.h"
#include "scone/core/system_tools.h"
#include "studio_tools.h"
#include "scone/core/version.h"
#include "qt_convert.h"

namespace scone
{
	QUrl GetHelpUrl( const QString& keyword )
	{
		static xo::flat_map<QString, QString> data;
		if ( data.empty() )
		{
			auto vec = xo::split_str( xo::load_string( GetSconeStudioFolder() / "resources/help/keywords.txt" ), "\n" );
			for ( const auto& k : vec )
			{
				QString doku = QString( k.c_str() ).remove( ".txt" );
				QString keyword = QString( doku ).remove( '_' );
				data[ keyword ] = doku;
			}
		}

		if ( !keyword.isEmpty() )
		{
			auto k = keyword.toLower();
			if ( auto it = data.find( k ); it != data.end() )
				return QUrl( "https://scone.software/doku.php?id=ref:" + it->second );
			else return QUrl( "https://scone.software/doku.php?do=search&q=" + keyword );
		}
		else return QUrl( "https://scone.software/doku.php?id=start" );
	}

	void showAbout( QWidget* parent )
	{
		QString title = "<b>" + to_qt( "SCONE version " + xo::to_str( GetSconeVersion() ) ) + "</b><br><br>";
		QString author = "Copyright (C) 2013 - 2021 Thomas Geijtenbeek and contributors. All rights reserved.<br><br>";
		QString scone_license =
			"<b>SCONE</b> is licensed under the <a href='https://www.apache.org/licenses/LICENSE-2.0'>Apache License, Version 2.0</a>. "
			"It uses the following external libraries:<ul>"
			"<li>OpenSim 3.3 (Apache 2.0)"
			"<li>Simbody (Apache 2.0)"
			"<li>Lua (MIT)"
			"<li>Sol 3 (MIT)"
			"<li>TCLAP (MIT)"
			"<li>xo (Apache 2.0)"
			"<li>spot (Apache 2.0)"
			"</ul><br>";
		QString studio_license =
			"<b>SCONE Studio</b> is licensed under the <a href='https://www.gnu.org/licenses/gpl-3.0.en.html'>GNU Public License 3.0</a>. "
			"It uses the following external libraries:<ul>"
			"<li>Qt (GPL / LGPL)"
			"<li>QCustomPlot (GPL v3)"
			"<li>OpenSceneGraph (OSGPL)"
			"<li>TCLAP (MIT)"
			"<li>xo (Apache 2.0)"
			"<li>spot (Apache 2.0)"
			"<li>vis (Apache 2.0)"
			"<li>qtfx (Apache 2.0)"
			"</ul>";
		QMessageBox::information( parent, "About SCONE", title + author + scone_license + studio_license );
	}
}
