#include "file_tools.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QMessageBox>
#include <filesystem>

#include "scone/core/system_tools.h"
#include "scone/core/Log.h"
#include "xo/filesystem/filesystem.h"

namespace fs = std::filesystem;

namespace scone
{
	std::pair<int, double> extractGenBestFromParFile( const QFileInfo& parFile )
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

	void installTutorials()
	{
		auto sconeDir = scone::GetFolder( scone::SCONE_SCENARIO_FOLDER );
		auto tutDir = sconeDir / "Tutorials";
		auto instDir = scone::GetFolder( scone::SCONE_ROOT_FOLDER ) / "scenarios";
		auto fsScone = fs::path( sconeDir.str() );
		auto fsInst = fs::path( instDir.str() );
		if ( fs::exists( fsScone / "Tutorials/Tutorial 1 - Introduction.scone" ) )
		{
			// backup 1.X tutorials
			auto tutBackupDir = tutDir + "Backup";
			QString msg = QString( "A previous version of the SCONE Tutorials was detected and will be moved to:\n\n" ) + tutBackupDir.c_str();
			if ( QMessageBox::question( nullptr, "Rename Existing Tutorials", msg, QMessageBox::Ok, QMessageBox::Cancel ) == QMessageBox::Cancel )
				return;
			auto sconeTutBackup = fs::path( xo::find_unique_directory( tutDir + "Backup" ).str() );
			fs::rename( fsScone / "Tutorials", sconeTutBackup );
		}

		auto options = fs::copy_options::update_existing | fs::copy_options::recursive;
		log::debug( "Updating Tutorials and Examples" );
		fs::copy( fsInst / "Tutorials", fsScone / "Tutorials", options );
		fs::copy( fsInst / "Examples", fsScone / "Examples", options );
	}

	bool moveFilesToTrash( const QStringList& files )
	{
#ifdef _MSC_VER
		std::vector<char> buf;
		for ( const auto& f : files ) {
			auto fstr = f.toStdString();
			buf.insert( buf.end(), fstr.begin(), fstr.end() );
			buf.push_back( '\0' );
		}
		buf.push_back( '\0' );

		SHFILEOPSTRUCT FileOp = { 0 };
		FileOp.hwnd = NULL;
		FileOp.wFunc = FO_DELETE;
		FileOp.fFlags = FOF_ALLOWUNDO;
		FileOp.pFrom = &buf[ 0 ];
		FileOp.pTo = NULL;

		auto ret = SHFileOperation( &FileOp );
		if ( ret != 0 )
			log::warning( "Could not move files to the recycle bin, error=", ret );
		return ret == 0;
#else
		int num_deleted = 0;
		for ( const auto& f : files )
			if ( moveToTrash( f ) )
				++num_deleted;
		return num_deleted == files.size();
#endif
	}

	bool moveToTrash( const QString& path )
	{
#ifdef _MSC_VER
		auto file_str = path.toStdString();
		xo::replace_char( file_str, '/', '\\' );
		if ( file_str.back() == '\\' )
			file_str.pop_back();
		file_str.push_back( '\0' ); // strings must be double-null-terminated

		SHFILEOPSTRUCT FileOp = { 0 };
		FileOp.hwnd = NULL;
		FileOp.wFunc = FO_DELETE;
		FileOp.fFlags = FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR | FOF_ALLOWUNDO;
		FileOp.pFrom = file_str.c_str();
		FileOp.pTo = NULL;
		auto ret = SHFileOperation( &FileOp );
		if ( ret != 0 )
			log::warning( "Could not recycle ", path.toStdString(), "; error=", ret );
		return ret == 0;
#else
		return QFile::moveToTrash( path );
#endif
	}
}
