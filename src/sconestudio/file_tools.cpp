#include "file_tools.h"

#include "QDirIterator"
#include "scone/core/system_tools.h"
#include "xo/system/log.h"
#include <filesystem>
#include "QMessageBox"
#include "xo/filesystem/filesystem.h"

namespace fs = std::filesystem;

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
		auto sconeDir = scone::GetFolder( scone::SCONE_SCENARIO_FOLDER );
		auto tutDir = sconeDir / "Tutorials";
		auto instDir = scone::GetFolder( scone::SCONE_ROOT_FOLDER ) / "scenarios";
		auto fsScone = fs::path( sconeDir.str() );
		auto fsInst = fs::path( instDir.str() );
		if ( fs::exists( fsScone / "Tutorials/Tutorial 1 - Introduction.scone" ) )
		{
			auto tutBackupDir = tutDir + "Backup";
			QString msg = QString( "Existing tutorials will be copied to\n\n" ) + tutBackupDir.c_str();
			if ( QMessageBox::question( nullptr, "Rename Tutorials", msg ) != QMessageBox::Yes )
				return;
			auto sconeTutBackup = fs::path( xo::find_unique_directory( tutDir + "Backup" ).str() );
			fs::rename( fsScone / "Tutorials", sconeTutBackup );
		}

		auto options = fs::copy_options::update_existing | fs::copy_options::recursive;
		fs::copy( fsInst / "Tutorials", fsScone / "Tutorials", options );
		fs::copy( fsInst / "Examples", fsScone / "Examples", options );
	}
}
