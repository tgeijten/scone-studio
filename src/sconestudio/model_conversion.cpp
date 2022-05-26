#include "model_conversion.h"

#include "ui_ModelTool.h"
#include "scone/core/Log.h"
#include "scone/core/system_tools.h"
#include "qt_convert.h"
#include "xo/filesystem/path.h"
#include "scone/sconelib_config.h"

#include <QProcess>
#include <QMessageBox>
#include <QPushButton>

namespace scone
{
	QString g_inputFileName;

	void ShowModelConversionDialog( QWidget* parent )
	{
		QDialog dlg( parent );
		Ui::ModelTool ui;
		ui.setupUi( &dlg );
		ui.inputFile->init( QFileEdit::OpenFile, "OpenSim 3 Models (*.osim)", "", to_qt( GetFolder( SCONE_SCENARIO_FOLDER ) ) );
		ui.inputFile->setText( g_inputFileName );
		auto updateInputFile = [&]() {
			const auto inputFile = xo::path( ui.inputFile->text().toStdString() );
			ui.buttonBox->button( QDialogButtonBox::Ok )->setEnabled( !inputFile.empty() );
			if ( !inputFile.empty() )
			{
				auto hfdFilename = xo::path( inputFile ).replace_extension( "hfd" );
				auto osim4Filename = xo::path( inputFile ).replace_stem( inputFile.stem() + "_osim4" );
				ui.outputFileHfd->setText( to_qt( hfdFilename ) );
				ui.outputFileOsim4->setText( to_qt( osim4Filename ) );
			}
		};

		updateInputFile();
		QObject::connect( ui.inputFile, &QFileEdit::textChanged, updateInputFile );

		ui.convertOsim4->setEnabled( SCONE_OPENSIM_4_ENABLED );
		ui.convertOsim4->setChecked( false );
		ui.convertHfd->setEnabled( SCONE_HYFYDY_ENABLED );
		ui.convertHfd->setChecked( SCONE_HYFYDY_ENABLED );

		ui.jointStiffness->setValue( 1e6 );
		ui.limitStiffness->setValue( 500 );
		ui.bodyMassThreshold->setValue( 1.0 );

		if ( QDialog::Accepted == dlg.exec() )
		{
			g_inputFileName = ui.inputFile->text();
			const xo::path inputFile = xo::path( ui.inputFile->text().toStdString() );

#if SCONE_HYFYDY_ENABLED
			if ( ui.convertHfd->isChecked() )
			{
				const xo::path outputFile = xo::path( ui.outputFileHfd->text().toStdString() );

				QString program = to_qt( GetApplicationFolder() / "hfdmodeltool" );
				QStringList args;
				args << to_qt( inputFile ) << "-o" << to_qt( outputFile );
				args << "--remote";
				args << "--joint-stiffness" << QString::number( ui.jointStiffness->value() );
				args << "--limit-stiffness" << QString::number( ui.limitStiffness->value() );
				args << "--body-mass-threshold" << QString::number( ui.bodyMassThreshold->value() );

				auto joinedArgs = args.join( ' ' );
				log::info( "Converting model: ", joinedArgs.toStdString() );
				auto proc = new QProcess( parent );
				proc->start( program, args );
				if ( !proc->waitForFinished() )
				{
					QString msg = "Timeout waiting for " + program;
					log::error( msg.toStdString() );
					QMessageBox::critical( parent, "Error", msg );
					return;
				}

				// check return code and output
				QString output = proc->readAllStandardOutput();
				if ( proc->exitCode() == 0 )
				{
					QString title = "Successfully converted " + to_qt( inputFile.filename() );
					log::info( title.toStdString() );
					log::info( output.toStdString() );
					QMessageBox::information( parent, title, output );
				}
				else
				{
					QString title = "Error converting " + to_qt( inputFile.filename() );
					if ( output.isEmpty() )
						output = "Could not convert model with these settings:\n\n" + joinedArgs;
					log::error( title.toStdString() );
					log::error( output.toStdString() );
					QMessageBox::critical( parent, title, output );
				}
			}
#endif

#if SCONE_OPENSIM_4_ENABLED
			if ( ui.convertOsim4->isChecked() )
			{
				// create os4 model (test)
				const xo::path outputFile = xo::path( ui.outputFileOsim4->text().toStdString() );
				ConvertModelOpenSim4( inputFile, outputFile );
			}
#endif
		}
	}
}
