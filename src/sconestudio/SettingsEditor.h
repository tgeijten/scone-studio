/*
** SettingsEditor.h
**
** Copyright (C) Thomas Geijtenbeek and contributors. All rights reserved.
**
** This file is part of SCONE. For more information, see http://scone.software.
*/

#pragma once

#include <QWidget>

namespace scone
{
	int ShowPreferencesDialog( QWidget* parent );
	int ShowLicenseDialog( QWidget* parent );
	void ShowRequestLicenseDialog();
}
