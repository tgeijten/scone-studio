#pragma once

#include <QUrl>
#include <QString>

namespace scone
{
	QUrl GetHelpUrl( const QString& keyword );
	inline QUrl GetWebsiteUrl() { return QUrl( "https://scone.software" ); }
	inline QUrl GetDownloadUrl() { return QUrl( "https://simtk.org/frs/?group_id=1180" ); }
	inline QUrl GetForumUrl() { return QUrl( "https://simtk.org/plugins/phpBB/indexPhpbb.php?group_id=1180&pluginname=phpBB" ); }
	void showAbout( QWidget* parent );
}
