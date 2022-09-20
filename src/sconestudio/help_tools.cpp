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
		QString author = "Copyright (C) 2013 - 2022 Thomas Geijtenbeek and contributors. All rights reserved.<br><br>";
		QString scone_license =
			"<b>SCONE</b> is licensed under the <a href='https://www.apache.org/licenses/LICENSE-2.0'>Apache License, Version 2.0</a>. "
			"It uses the following external libraries:<ul>"
			"<li><a href='https://opensim.stanford.edu/'>OpenSim 3.3</a> (Apache 2.0)"
			"<li><a href='https://opensim.stanford.edu/'>OpenSim 4.3</a> (Apache 2.0)"
			"<li><a href='https://simbody.github.io/3.5.0/index.html'>Simbody</a> (Apache 2.0)"
			"<li><a href='https://www.lua.org/'>Lua</a> (MIT)"
			"<li><a href='https://github.com/ThePhD/sol2'>Sol 3</a> (MIT)"
			"<li><a href='http://tclap.sourceforge.net'>TCLAP</a> (MIT)"
			"<li><a href='https://github.com/tgeijten/xo'>xo</a> (Apache 2.0)"
			"<li><a href='https://github.com/tgeijten/spot'>spot</a> (Apache 2.0)"
			"</ul><br>";
		QString studio_license =
			"<b>SCONE Studio</b> is licensed under the <a href='https://www.gnu.org/licenses/gpl-3.0.en.html'>GNU Public License 3.0</a>. "
			"It uses the following external libraries:<ul>"
			"<li><a href='https://qt.io'>Qt</a> (GPL / LGPL)"
			"<li><a href='https://www.qcustomplot.com/'>QCustomPlot</a> (GPL v3)"
			"<li><a href='http://www.openscenegraph.org/'>OpenSceneGraph</a> (OSGPL)"
			"<li><a href='http://tclap.sourceforge.net'>TCLAP</a> (MIT)"
			"<li><a href='https://github.com/tgeijten/xo'>xo</a> (Apache 2.0)"
			"<li><a href='https://github.com/tgeijten/spot'>spot</a> (Apache 2.0)"
			"<li><a href='https://github.com/tgeijten/vis'>vis</a> (Apache 2.0)"
			"<li><a href='https://github.com/tgeijten/qtfx'>qtfx</a> (Apache 2.0)"
			"</ul>";

		QString hyfydy_license =
			"<b>Hyfydy</b> is commercial software. Please visit <a href='https://hyfydy.com'>hyfydy.com</a> for a free trial license.";

		QMessageBox::information( parent, "About SCONE", title + author + scone_license + studio_license + hyfydy_license );
	}
}
