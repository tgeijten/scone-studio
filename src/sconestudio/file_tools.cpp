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
	inline std::filesystem::path to_fs( const xo::path& p ) { return std::filesystem::path( p.str() ); }

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

	QByteArray fileHash( QFile& file ) {
		QCryptographicHash hash( QCryptographicHash::Md5 );
		file.open( QIODevice::ReadOnly );
		hash.addData( file.readAll() );
		file.close();
		return hash.result();
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
				auto srcHash = fileHash( srcFile );
				auto trgHash = fileHash( trgFile );
				if ( srcHash != trgHash ) {
					log::debug( trgFile.fileName().toStdString(), " is different from the installed version" );
					result.different.append( trgFile.fileName() );
				}
			}
			else {
				result.missing++;
				log::debug( trgFile.fileName().toStdString(), " is missing" );
			}
		}
		return result;
	}

	bool okToUpdateFiles( const QStringList& l )
	{
		if ( !l.empty() )
		{
			QString msg = QString( "The following SCONE scenario files are not up-to-date:\n\n" ) + l.join( '\n' );
			msg += QString( "\n\nDo you wish to update these files (recommended)?" );
			return QMessageBox::question( nullptr, "Update Files", msg, QMessageBox::Yes, QMessageBox::No ) == QMessageBox::Yes;
		}
		else return true;
	}

	void installTutorials()
	{
		auto srcPath = scone::GetFolder( scone::SCONE_ROOT_FOLDER ) / "scenarios";
		auto trgPath = scone::GetFolder( scone::SCONE_SCENARIO_FOLDER );
		auto tutsrc = srcPath / "Tutorials";
		auto tuttrg = trgPath / "Tutorials";
		auto exsrc = srcPath / "Examples";
		auto extrg = trgPath / "Examples";
		bool keepOldTutorials = false;

		// check tutorials
		if ( fs::exists( to_fs( trgPath ) / "Tutorials/Tutorial 1 - Introduction.scone" ) )
		{
			// backup 1.X tutorials
			auto trgtutbackup = xo::find_unique_directory( tuttrg + "Backup" );
			QString msg = QString( "A previous version of the SCONE Tutorials was detected and will be moved to:\n\n" ) + trgtutbackup.c_str();
			if ( QMessageBox::question( nullptr, "Backup Tutorials", msg, QMessageBox::Ok, QMessageBox::Cancel ) == QMessageBox::Ok )
				fs::rename( to_fs( tuttrg ), to_fs( trgtutbackup ) );
			else keepOldTutorials = true;
		}

		// check updates
		auto tutCheck = checkInstallation( to_qt( tutsrc ), to_qt( tuttrg ) );
		auto exCheck = checkInstallation( to_qt( exsrc ), to_qt( extrg ) );
		if ( !tutCheck.good() || !exCheck.good() )
		{
			auto different = keepOldTutorials ? exCheck.different : tutCheck.different + exCheck.different;
			if ( !different.empty() && !okToUpdateFiles( different ) )
				return; // user want to keep existing versions

			log::info( "Updating Scenarios" );
			auto options = fs::copy_options::overwrite_existing | fs::copy_options::recursive;
			if ( !keepOldTutorials )
				fs::copy( to_fs( tutsrc ), to_fs( tuttrg ), options );
			fs::copy( to_fs( exsrc ), to_fs( extrg ), options );
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
