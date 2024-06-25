#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>
#include "scone/core/Log.h"

namespace scone
{
	using QProcessPtr = std::unique_ptr<QProcess>;
	QProcessPtr makeProcess( const QString& file, QStringList& args, QWidget* parent );
	bool runExternalProcess( const QString& title, const QString& file, QStringList& args, QWidget* parent = nullptr, log::level l = log::level::info );
	bool evaluateCheckpointSync( const QString& file, QWidget* parent = nullptr );
	QProcessPtr evaluateCheckpointAsync( const QString& file, QWidget* parent = nullptr );
}
