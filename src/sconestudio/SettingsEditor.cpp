/*
** SettingsEditor.cpp
**
** Copyright (C) Thomas Geijtenbeek and contributors. All rights reserved.
**
** This file is part of SCONE. For more information, see http://scone.software.
*/

#include "SettingsEditor.h"

#include <QDialog>
#include <QMessageBox>
#include <QClipboard>
#include "qt_convert.h"
#include "QPropNodeItemModel.h"

#include "ui_SconeSettings.h"
#include "scone/core/PropNode.h"
#include "scone/core/system_tools.h"
#include "scone/core/Settings.h"
#include "scone/core/Log.h"
#include "xo/filesystem/path.h"
#include "xo/system/type_class.h"
#include "xo/container/flat_map.h"
#include "StudioSettings.h"
#include "QSettingsItemModel.h"
#include "scone/sconelib_config.h"
#include "ui_LicenseDialog.h"

namespace scone
{
	int ShowPreferencesDialog( QWidget* parent )
	{
		QDialog dialog_window( parent );
		Ui::Settings ui;
		ui.setupUi( &dialog_window );

		// init SCONE settings
		auto& scone_settings = GetSconeSettings();

		// folders
		auto ga_template_path = to_qt( GetStudioSetting<xo::path>( "gait_analysis.template" ).make_preferred() );
		ui.scenariosFolder->init( QFileEdit::Directory, "", to_qt( GetFolder( SconeFolder::Scenarios ).make_preferred() ) );
		ui.resultsFolder->init( QFileEdit::Directory, "", to_qt( GetFolder( SconeFolder::Results ).make_preferred() ) );
		ui.geometryFolder->init( QFileEdit::Directory, "", to_qt( GetFolder( SconeFolder::Geometry ).make_preferred() ) );
		ui.gaitAnalysisFolder->init( QFileEdit::OpenFile, "Gait Analysis Templates (*.zml)", ga_template_path );

		// data checkboxes
		xo::flat_map< string, QListWidgetItem* > data_checkboxes;
		for ( auto& item : scone_settings.schema().get_child( "data" ) )
		{
			if ( item.second.get<string>( "type" ) != "bool" )
				continue;
			if ( !GetExperimentalFeaturesEnabled() && item.second.get<bool>( "experimental", 0 ) )
				continue;

			auto* checkbox = new QListWidgetItem( item.second.get<string>( "label" ).c_str() );
			checkbox->setCheckState( scone_settings.get< bool >( "data." + item.first ) ? Qt::Checked : Qt::Unchecked );
			ui.dataList->addItem( checkbox );
			data_checkboxes[item.first] = checkbox;
		}

		// advanced settings
		auto* advancedModel = new QSettingsItemModel( scone_settings );
		ui.advancedTree->setModel( advancedModel );
		ui.advancedTree->expandAll();

		// init STUDIO settings
		auto& studio_settings = GetStudioSettings();
		auto* studioModel = new QSettingsItemModel( studio_settings );
		ui.studioTree->setModel( studioModel );
		ui.studioTree->expandAll();

#if SCONE_HYFYDY_ENABLED
		bool hfd_enabled = scone_settings.get<bool>( "hyfydy.enabled" );
		auto hfd_license = scone_settings.get<String>( "hyfydy.license" );
		ui.hfdEnabled->setCheckState( hfd_enabled ? Qt::Checked : Qt::Unchecked );
		ui.hfdLicenseKey->setPlainText( to_qt( hfd_license ) );
		QObject::connect( ui.hfdRequest, &QPushButton::clicked, []() { ShowRequestLicenseDialog(); } );
		QObject::connect( ui.hfdLicenseKey, &QPlainTextEdit::textChanged,
			[&]() { ui.hfdEnabled->setChecked( !ui.hfdLicenseKey->document()->isEmpty() ); } );
#else
		ui.tabWidget->removeTab( ui.tabWidget->indexOf( ui.hfdTab ) );
#endif

		for ( int i = 0; i < 3; i++ )
		{
			ui.advancedTree->resizeColumnToContents( i );
			ui.studioTree->resizeColumnToContents( i );
		}

		int ret = dialog_window.exec();
		if ( ret == QDialog::Accepted )
		{
			// update settings
			if ( ui.scenariosFolder->text().toStdString() != GetFolder( SconeFolder::Scenarios ) )
				scone_settings.set( "folders.scenarios", ui.scenariosFolder->text().toStdString() );
			if ( ui.resultsFolder->text().toStdString() != GetFolder( SconeFolder::Results ) )
				scone_settings.set( "folders.results", ui.resultsFolder->text().toStdString() );
			if ( ui.geometryFolder->text().toStdString() != GetFolder( SconeFolder::Geometry ) )
				scone_settings.set( "folders.geometry", ui.geometryFolder->text().toStdString() );
			if ( ui.gaitAnalysisFolder->text() != ga_template_path ) {
				log::debug( "Updating gait_analysis.template from ", ga_template_path.toStdString(), " to ", ui.gaitAnalysisFolder->text().toStdString() );
				studio_settings.set( "gait_analysis.template", ui.gaitAnalysisFolder->text().toStdString() );
			}

			// copy checkboxes
			for ( auto& item : data_checkboxes )
				scone_settings.set< bool >( "data." + item.first, item.second->checkState() == Qt::Checked );

#if SCONE_HYFYDY_ENABLED
			auto hfd_new_license = ui.hfdLicenseKey->toPlainText().toStdString();
			auto hfd_new_enabled = ui.hfdEnabled->isChecked();
			if ( hfd_new_license != hfd_license || hfd_enabled != hfd_new_enabled )
			{
				scone_settings.set( "hyfydy.license", hfd_new_license );
				scone_settings.set( "hyfydy.enabled", hfd_new_enabled );
				if ( hfd_new_enabled )
					if ( ShowLicenseDialog( parent ) == QDialog::Accepted )
						sconehfd::RegisterSconeHfd( hfd_new_license );
			}
#endif
			scone_settings.save();
			studio_settings.save();
		}
		else
		{
			// cancel was pressed, reload old settings
			scone_settings.load();
			studio_settings.load();
		}

		return ret;
	}

	int ShowLicenseDialog( QWidget* parent )
	{
#if SCONE_HYFYDY_ENABLED
		auto license_key = GetSconeSetting<String>( "hyfydy.license" );
		if ( auto la_result = sconehfd::GetHfdLicenseAgreement( license_key.c_str() ) )
		{
			auto& agreement = la_result.value();
			QDialog lic_dlg( parent );
			Ui::LicenseDialog ui;
			ui.setupUi( &lic_dlg );
			ui.textBrowser->setText( to_qt( agreement.licenseAgreement ) );
			auto agreement_str = to_qt( agreement.licenseType + " AGREEMENT" );
			lic_dlg.setWindowTitle( agreement_str );
			ui.checkBox->setText( "I accept the " + agreement_str );
			auto* okButton = ui.buttonBox->button( QDialogButtonBox::Ok );
			okButton->setDisabled( true );
			QWidget::connect( ui.checkBox, &QCheckBox::stateChanged,
				[&]( int i ) { okButton->setDisabled( i != Qt::Checked ); } );
			const auto result = lic_dlg.exec();
			if ( result == QDialog::Accepted && ui.checkBox->isChecked() )
			{
				GetSconeSettings().set( "hyfydy.license_agreement_accepted_version", agreement.licenseVersion );
			}
			else
			{
				GetSconeSettings().set( "hyfydy.enabled", false );
				GetSconeSettings().set( "hyfydy.license_agreement_accepted_version", 0 );
			}
			GetSconeSettings().save();

			return result;
		}
		else
		{
			GetSconeSettings().set( "hyfydy.enabled", false );
			GetSconeSettings().set( "hyfydy.license_agreement_accepted_version", 0 );
			GetSconeSettings().save();
			log::error( "License error: ", la_result.error().message() );
			QMessageBox::critical( nullptr, "Hyfydy License Error", to_qt( la_result.error().message() ) );
		}
#endif

		// Hyfydy is either disabled or the agreement is invalid
		return QDialog::Rejected;
	}

	void ShowRequestLicenseDialog()
	{
#if SCONE_HYFYDY_ENABLED
		auto hid = to_qt( sconehfd::GetHardwareId() );
		QApplication::clipboard()->setText( hid );
		auto message = "This is your Hardware ID (copied to clipboard):<br><br><b>" + hid + "</b><br><br>";
		message += "Please navigate to <a href='https://hyfydy.com/trial'>hyfydy.com/trial</a> to complete your trial request.";
		QMessageBox msgBox;
		msgBox.setWindowTitle( "Hyfydy License Request" );
		msgBox.setTextFormat( Qt::RichText );   //this is what makes the links clickable
		msgBox.setText( message );
		msgBox.setIcon( QMessageBox::Information );
		msgBox.exec();
#endif
	}
}
