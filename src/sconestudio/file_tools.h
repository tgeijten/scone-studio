#pragma once

#include <utility>
#include <QString>
#include <QFileInfo>

namespace scone
{
	std::pair<int, double> extractGenBestFromParFile( const QFileInfo& parFile );
	QFileInfo findBestPar( const QDir& dir );

	void tryInstallTutorials();
}
