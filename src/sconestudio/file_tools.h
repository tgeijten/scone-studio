#pragma once

#include <utility>
#include <QString>
#include <QFileInfo>
#include <QStringList>

namespace scone
{
	std::pair<int, double> extractGenBestFromParFile( const QFileInfo& parFile );
	QFileInfo findBestPar( const QDir& dir );

	bool moveToTrash( const QString& path );
	bool moveFilesToTrash( const QStringList& files );

	struct CheckInstallationResult {
		int checked;
		int missing;
		QStringList different;
		bool good() const { return checked > 0 && missing == 0 && different.empty(); }
	};
	bool okToUpdateFiles( const QStringList& l );
	CheckInstallationResult checkInstallation( const QString& source, const QString& target );
	void installTutorials();
}
