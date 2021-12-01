#include "file_tools.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QMessageBox>
#include <QFile>
#include <QCryptographicHash>
#include <filesystem>
#include "scone/core/system_tools.h"
#include "scone/core/Log.h"
#include "xo/filesystem/filesystem.h"
#include "qt_convert.h"

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

	QString fileHash( QFile& file ) {
		QCryptographicHash hash( QCryptographicHash::Md5 );
		file.open( QIODevice::ReadOnly );
		hash.addData( file.readAll() );
		file.close();
		return QString( hash.result().toHex() );
	}

	CheckInstallationResult checkInstallation( const QString& sourceDir, const QString& targetDir )
	{
		auto result = CheckInstallationResult();
		QDirIterator it( sourceDir, QDir::Files, QDirIterator::Subdirectories );
		while ( it.hasNext() ) {
			QFile srcFile = it.next();
			QFile trgFile = srcFile.fileName().replace( sourceDir, targetDir );

			result.checked++;
			if ( trgFile.exists() ) {
				auto srcTime = srcFile.fileTime( QFile::FileModificationTime );
				auto trgTime = trgFile.fileTime( QFile::FileModificationTime );
				if ( srcTime != trgTime ) {
					auto srcHash = fileHash( srcFile );
					auto trgHash = fileHash( trgFile );
					if ( srcHash != trgHash ) {
						if ( srcTime > trgTime ) {
							result.older++;
							log::debug( trgFile.fileName().toStdString(), " is not up-to-date" );
						}
						else {
							result.newer++;
							log::debug( trgFile.fileName().toStdString(), " is has been modified" );
						}
					}
				}
			}
			else {
				result.missing++;
				log::debug( trgFile.fileName().toStdString(), " is missing" );
			}
		}
		return result;
	}

	void installTutorials()
	{
		auto srcPath = scone::GetFolder( scone::SCONE_ROOT_FOLDER ) / "scenarios";
		auto trgPath = scone::GetFolder( scone::SCONE_SCENARIO_FOLDER );
		auto trgTut = trgPath / "Tutorials";

		auto tres = checkInstallation( to_qt( srcPath / "Tutorials" ), to_qt( trgTut ) );
		if ( !tres.good() )
		{
			auto trgfs = fs::path( trgPath.str() );
			auto srcfs = fs::path( srcPath.str() );
			if ( fs::exists( trgfs / "Tutorials/Tutorial 1 - Introduction.scone" ) )
			{
				// backup 1.X tutorials
				auto tutBackupDir = trgTut + "Backup";
				QString msg = QString( "A previous version of the SCONE Tutorials was detected and will be moved to:\n\n" ) + tutBackupDir.c_str();
				if ( QMessageBox::question( nullptr, "Rename Existing Tutorials", msg, QMessageBox::Ok, QMessageBox::Cancel ) == QMessageBox::Cancel )
					return;
				auto sconeTutBackup = fs::path( xo::find_unique_directory( trgTut + "Backup" ).str() );
				fs::rename( trgfs / "Tutorials", sconeTutBackup );
			}
			else if ( tres.newer > 0 || tres.older > 0 )
			{
				QString msg = QString( "The following files have been modified:\n\n" );
				if ( QMessageBox::question( nullptr, "Restore Modified Files", msg, QMessageBox::Ok, QMessageBox::Cancel ) == QMessageBox::Cancel )
					return;
			}
			auto options = fs::copy_options::update_existing | fs::copy_options::recursive;
			log::debug( "Updating Tutorials and Examples" );
			fs::copy( srcfs / "Tutorials", trgfs / "Tutorials", options );
			fs::copy( srcfs / "Examples", trgfs / "Examples", options );
		}
	}

	bool moveFilesToTrash( const QStringList& files )
	{
#if defined(_MSC_VER)
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
#if defined(_MSC_VER)
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
#elif (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
		return QFile::moveToTrash( path );
#else
		log::warning( "Moving files to trash is not supported on this platform" );
		return false;
#endif
	}
}
