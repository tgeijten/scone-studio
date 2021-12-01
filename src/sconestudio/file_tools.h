#pragma once

#include <utility>
#include <QString>
#include <QFileInfo>

namespace scone
{
	std::pair<int, double> extractGenBestFromParFile( const QFileInfo& parFile );
	QFileInfo findBestPar( const QDir& dir );

	bool moveToTrash( const QString& path );
	bool moveFilesToTrash( const QStringList& files );

	struct CheckInstallationResult {
		int checked;
		int missing;
		int older;
		int newer;
		bool good() const { return checked > 0 && missing == 0 && older == 0 && newer == 0; }
	};
	CheckInstallationResult checkInstallation( const QString& source, const QString& target );
	void installTutorials();
}
