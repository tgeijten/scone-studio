/*
** ResultsFileSystemModel.cpp
**
** Copyright (C) Thomas Geijtenbeek and contributors. All rights reserved.
**
** This file is part of SCONE. For more information, see http://scone.software.
*/

#include "ResultsFileSystemModel.h"
#include "xo/system/log.h"
#include <algorithm>
#include <QDirIterator>
#include <iostream>
#include "xo/container/prop_node_tools.h"
#include "xo/system/error_code.h"
#include "scone/core/Log.h"
#include "xo/string/string_cast.h"
#include "scone/core/system_tools.h"
#include "xo/serialization/serialize.h"
#include "qt_convert.h"
#include "xo/filesystem/filesystem.h"

#define SCONE_USE_RESULTS_CACHE 0

xo::path results_cache_file() { return scone::GetSettingsFolder() / "results_cache.zml"; }

namespace xo {
	bool from_prop_node( const prop_node& pn, ResultsFileSystemModel::Status& stat ) {
		stat.gen = pn.get<int>( "gen" );
		stat.best = pn.get<double>( "best" );
		stat.modified.fromString( to_qt( pn.get<string>( "best" ) ), Qt::ISODate );
		return true;
	}
	prop_node to_prop_node( const ResultsFileSystemModel::Status& stat ) {
		prop_node pn;
		pn["gen"] = stat.gen;
		pn["best"] = stat.best;
		pn["modified"] = stat.modified.toString( Qt::ISODate ).toStdString();
		return pn;
	}
}

ResultsFileSystemModel::ResultsFileSystemModel( QObject* parent ) : QFileSystemModel( parent )
{
#if SCONE_USE_RESULTS_CACHE
	auto f = results_cache_file();
	if ( xo::file_exists( f ) )
	{
		xo::error_code ec;
		auto status_cache = xo::load_file( f, &ec );
		if ( ec.good() )
			xo::from_prop_node( status_cache, m_StatusCache );
	}
#endif
}

ResultsFileSystemModel::~ResultsFileSystemModel()
{
#if SCONE_USE_RESULTS_CACHE
	xo::error_code ec;
	auto status_cache = xo::to_prop_node( m_StatusCache );
	xo::save_file( status_cache, results_cache_file(), &ec );
#endif
}

ResultsFileSystemModel::Status ResultsFileSystemModel::getStatus( QFileInfo& fi ) const
{
	Status stat{ -1, 0 };
	if ( fi.isFile() && fi.suffix() == "par" )
	{
		auto split = fi.completeBaseName().split( "_" );
		if ( split.size() > 2 )
		{
			bool ok = false;
			if ( auto gen = split[0].toInt( &ok ); ok )
				stat.gen = gen;
			if ( auto best = split[2].toDouble( &ok ); ok )
				stat.best = best;
		}
	}
	else if ( fi.isDir() )
	{
		fi.refresh();
		auto filename = fi.fileName().toStdString();
		auto cache_it = m_StatusCache.find( filename );
		if ( cache_it != m_StatusCache.end() && abs( cache_it->second.modified.secsTo( fi.lastModified() ) ) < 3 )
			return cache_it->second;

		//scone::log::trace( "Scanning folder ", fi.absoluteFilePath().toStdString() );
		for ( QDirIterator dir_it( fi.absoluteFilePath(), { "*.par" }, QDir::Files ); dir_it.hasNext(); )
		{
			QFileInfo fileinf = QFileInfo( dir_it.next() );
			if ( fileinf.isFile() ) {
				auto fs = getStatus( fileinf );
				if ( fs.gen > stat.gen )
					stat = fs;
			}
			else if ( fileinf.fileName() != ".." && fileinf.fileName() != "." ) {
				// do something?
			}
		}
		stat.modified = fi.lastModified();
		m_StatusCache[filename] = stat;
	}
	return stat;
}

QVariant ResultsFileSystemModel::headerData( int section, Qt::Orientation orientation, int role ) const
{
	if ( section < QFileSystemModel::columnCount() )
		return QFileSystemModel::headerData( section, orientation, role );

	switch ( section - QFileSystemModel::columnCount() )
	{
		//	case StateCol: return QVariant( "State" );
	case GenCol: return QVariant( "Gen" );
	case ScoreCol: return QVariant( "Score" );
	default: return QVariant();
	}
}

int ResultsFileSystemModel::columnCount( const QModelIndex& parent ) const
{
	return QFileSystemModel::columnCount() + ColCount;
}

QVariant ResultsFileSystemModel::data( const QModelIndex& idx, int role ) const
{
	if ( idx.column() < QFileSystemModel::columnCount() )
		return QFileSystemModel::data( idx, role );

	if ( role == Qt::DisplayRole )
	{
		auto fi = fileInfo( idx );
		auto stat = getStatus( fi );
		if ( stat.gen < 0 )
			return QVariant( QString( "" ) );

		switch ( idx.column() - QFileSystemModel::columnCount() )
		{
			//		case StateCol: return QVariant( QString( stat.state_str().c_str() ) );
		case GenCol: return QVariant( stat.gen );
		case ScoreCol: return QVariant( QString::asprintf( "%7.3f", stat.best ) );
		default: return QVariant();
		}
	}
	else if ( role == Qt::TextAlignmentRole )
	{
		return Qt::AlignRight;
	}
	else return QVariant();
}

Qt::ItemFlags ResultsFileSystemModel::flags( const QModelIndex& index ) const
{
	if ( index.column() < QFileSystemModel::columnCount() )
		return Qt::ItemIsEditable | QFileSystemModel::flags( index );

	return QFileSystemModel::flags( index );
}
