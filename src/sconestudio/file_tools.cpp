#include "file_tools.h"

#include "QDirIterator"
#include "scone/core/system_tools.h"
#include "xo/system/log.h"

namespace scone
{
	std::pair<int, double> scone::extractGenBestFromParFile( const QFileInfo& parFile )
	{
		auto s = parFile.completeBaseName().split( "_" );
		if ( s.size() > 2 )
			return { s[ 0 ].toInt(), s[ 2 ].toDouble() };
		else return { -1, 0.0 };
	}

	QFileInfo findBestPar( const QDir& dir )
	{
		QFileInfo bestFile;
		int bestGen = -1;
		for ( QDirIterator it( dir ); it.hasNext(); it.next() )
		{
			auto fi = it.fileInfo();
			if ( fi.isFile() && fi.suffix() == "par" )
			{
				auto [gen, best] = extractGenBestFromParFile( fi );
				if ( gen > bestGen ) {
					bestFile = fi;
					bestGen = gen;
				}
			}
		}
		return bestFile;
	}

	void tryInstallTutorials()
	{
		auto sourceDir = scone::GetFolder( scone::SCONE_ROOT_FOLDER );
		auto targetDir = scone::GetFolder( scone::SCONE_SCENARIO_FOLDER );
		xo::log::info( "source=", sourceDir, " target=", targetDir );
	}
}
