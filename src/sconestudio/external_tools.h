#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>

namespace scone
{
	void runExternalProcess( const QString& title, const QString& file, QStringList& args, QWidget* parent = nullptr );
	void evaluateDeprlCheckpoint( const QString& file, QWidget* parent = nullptr );
}
