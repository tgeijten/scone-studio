#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>
#include "scone/core/Log.h"

namespace scone
{
	void runExternalProcess( const QString& title, const QString& file, QStringList& args, QWidget* parent = nullptr, log::level l = log::level::info );
	void evaluateDeprlCheckpoint( const QString& file, QWidget* parent = nullptr );
}
