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

	QString shortenPath( const QString& fullPath, int parentCount );
	QString makeFileListString( const QStringList& files, int maxItems = 10, int parentCount = -1 );

	struct CompareFoldersResult {
		int checked;
		int missing;
		QStringList different;
		bool good() const { return checked > 0 && missing == 0 && different.empty(); }
	};
	CompareFoldersResult compareFolders( const QString& source, const QString& target );

	bool okToUpdateFiles( QStringList l );
	void updateTutorialsExamples();
}
