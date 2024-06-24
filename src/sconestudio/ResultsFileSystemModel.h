/*
** ResultsFileSystemModel.h
**
** Copyright (C) Thomas Geijtenbeek and contributors. All rights reserved.
**
** This file is part of SCONE. For more information, see http://scone.software.
*/

#pragma once

#include <QFileSystemModel>
#include <QDateTime>
#include "xo/container/flat_map.h"

class ResultsFileSystemModel : public QFileSystemModel
{
	Q_OBJECT

public:
	ResultsFileSystemModel( QObject* parent );
	virtual ~ResultsFileSystemModel();

	enum Column { GenCol = 0, ScoreCol = 1, ColCount = 2 };
	virtual int columnCount( const QModelIndex& parent = QModelIndex() ) const override;

	struct Status {
		enum class Type { Invalid, Par, Pt };
		int gen = 0;
		double best = 0.0;
		Type type = Type::Invalid;
		QDateTime modified;
	};

private:
	Status getStatus( QFileInfo& fi ) const;
	virtual QVariant headerData( int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const override;
	virtual QVariant data( const QModelIndex& index, int role = Qt::DisplayRole ) const override;
	virtual Qt::ItemFlags flags( const QModelIndex& index ) const override;

	mutable xo::flat_map< std::string, Status > m_StatusCache;
};
