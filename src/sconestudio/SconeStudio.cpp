/*
** SconeStudio.cpp
**
** Copyright (C) 2013-2019 Thomas Geijtenbeek and contributors. All rights reserved.
**
** This file is part of SCONE. For more information, see http://scone.software.
*/

#include "SconeStudio.h"
#include "StudioSettings.h"
#include "scone/core/system_tools.h"
#include "scone/core/Log.h"
#include "scone/core/Profiler.h"
#include "scone/core/Settings.h"
#include "studio_config.h"

#include <osgDB/ReadFile>
#include "simvis/color.h"
#include "simvis/osg_tools.h"
#include "simvis/plane.h"

#include <QtWidgets/QFileSystemModel.h>
#include <QtWidgets/QMessageBox.h>
#include <QtWidgets/QFileDialog>
#include <QTextStream>
#include <QTabWidget>
#include "qcustomplot.h"
#include "qevent.h"
#include "qt_tools.h"
#include "ui_SconeSettings.h"

#include "xo/filesystem/filesystem.h"
#include "xo/system/system_tools.h"
#include "xo/utility/types.h"
#include "xo/system/platform.h"

using namespace scone;
using namespace std;

SconeStudio::SconeStudio( QWidget *parent, Qt::WindowFlags flags ) :
QCompositeMainWindow( parent, flags ),
slomo_factor( 1 ),
com_delta( Vec3( 0, 1, 0 ) ),
close_all( false ),
evaluation_time_step( 1.0 / 8 ),
captureProcess( nullptr ),
scene_( true )
{
	xo::log::debug( "Constructing UI elements" );
	ui.setupUi( this );

	createFileMenu( make_qt( GetFolder( SCONE_SCENARIO_FOLDER ) ), "Scone Scenario (*.scone)" );

	auto viewMenu = menuBar()->addMenu( "&View" );
	viewActions[ StudioModel::ShowForces ] = addMenuAction( viewMenu, "Show External &Forces", this, &SconeStudio::updateViewSettings );
	viewActions[ StudioModel::ShowMuscles ] = addMenuAction( viewMenu, "Show &Muscles", this, &SconeStudio::updateViewSettings );
	viewActions[ StudioModel::ShowTendons ] = addMenuAction( viewMenu, "Show &Tendons", this, &SconeStudio::updateViewSettings );
	viewActions[ StudioModel::ShowGeometry ] = addMenuAction( viewMenu, "Show &Bone Geometry", this, &SconeStudio::updateViewSettings );
	viewActions[ StudioModel::ShowContactGeom ] = addMenuAction( viewMenu, "Show &Contact Geometry", this, &SconeStudio::updateViewSettings );
	viewActions[ StudioModel::ShowAxes ] = addMenuAction( viewMenu, "Show Body &Axes", this, &SconeStudio::updateViewSettings );
	viewActions[ StudioModel::ShowGroundPlane ] = addMenuAction( viewMenu, "Show &Ground Plane", this, &SconeStudio::updateViewSettings );
	for ( auto& va : viewActions )
	{
		va.second->setCheckable( true );
		va.second->setChecked( va.first != StudioModel::ShowAxes );
	}

	auto scenarioMenu = menuBar()->addMenu( "&Scenario" );
	addMenuAction( scenarioMenu, "&Optimize Scenario", this, &SconeStudio::optimizeScenario, QKeySequence( "Ctrl+F5" ) );
	addMenuAction( scenarioMenu, "Run &Multiple Optimizations", this, &SconeStudio::optimizeScenarioMultiple, QKeySequence( "Ctrl+Shift+F5" ), true );
	addMenuAction( scenarioMenu, "&Abort Optimizations", this, &SconeStudio::abortOptimizations, QKeySequence(), true );
	addMenuAction( scenarioMenu, "&Run Scenario", this, &SconeStudio::runScenario, QKeySequence( "Ctrl+T" ) );

	auto toolsMenu = menuBar()->addMenu( "&Tools" );
	addMenuAction( toolsMenu, "Generate &Video...", this, &SconeStudio::createVideo );
	addMenuAction( toolsMenu, "Save &Image...", this, &SconeStudio::captureImage, QKeySequence( "Ctrl+I" ), true );
	addMenuAction( toolsMenu, "&Preferences...", this, &SconeStudio::showSettingsDialog );

	auto* actionMenu = menuBar()->addMenu( "&Playback" );
	addMenuAction( actionMenu, "&Play", ui.playControl, &QPlayControl::togglePlay, Qt::Key_F5 );
	addMenuAction( actionMenu, "&Stop / Reset", ui.playControl, &QPlayControl::stopReset, Qt::Key_F8 );
	addMenuAction( actionMenu, "Toggle Play", ui.playControl, &QPlayControl::togglePlay, Qt::Key_Space );
	addMenuAction( actionMenu, "Toggle Loop", ui.playControl, &QPlayControl::toggleLoop, QKeySequence( "Ctrl+L" ) );
	addMenuAction( actionMenu, "Play F&aster", ui.playControl, &QPlayControl::faster, QKeySequence( "Ctrl+Up" ) );
	addMenuAction( actionMenu, "Play S&lower", ui.playControl, &QPlayControl::slower, QKeySequence( "Ctrl+Down" ), true );
	addMenuAction( actionMenu, "Step &Back", ui.playControl, &QPlayControl::stepBack, QKeySequence( "Ctrl+Left" ) );
	addMenuAction( actionMenu, "Step &Forward", ui.playControl, &QPlayControl::stepForward, QKeySequence( "Ctrl+Right" ) );
	addMenuAction( actionMenu, "Page &Back", ui.playControl, &QPlayControl::pageBack, QKeySequence( "Ctrl+PgUp" ) );
	addMenuAction( actionMenu, "Page &Forward", ui.playControl, &QPlayControl::pageForward, QKeySequence( "Ctrl+PgDown" ) );
	addMenuAction( actionMenu, "Goto &Begin", ui.playControl, &QPlayControl::reset, QKeySequence( "Ctrl+Home" ) );
	addMenuAction( actionMenu, "Go to &End", ui.playControl, &QPlayControl::end, QKeySequence( "Ctrl+End" ), true );
	//addMenuAction( actionMenu, "&Evaluate Current Scenario", this, &SconeStudio::runScenario, QKeySequence( "Ctrl+T" ) );

	createWindowMenu();
	createHelpMenu();

	ui.stackedWidget->setCurrentIndex( 0 );
	ui.playControl->setDigits( 6, 3 );

	analysisView = new QDataAnalysisView( &storageModel, this );
	analysisView->setObjectName( "Analysis" );
	analysisView->setMinSeriesInterval( 0 );
	analysisView->setLineWidth( scone::GetStudioSettings().get< float >( "analysis.line_width" ) );

	setDockNestingEnabled( true );
	setCorner( Qt::TopLeftCorner, Qt::LeftDockWidgetArea );
	setCorner( Qt::BottomLeftCorner, Qt::LeftDockWidgetArea );
	setCorner( Qt::TopRightCorner, Qt::RightDockWidgetArea );
	setCorner( Qt::BottomRightCorner, Qt::RightDockWidgetArea );
	//setTabPosition( Qt::AllDockWidgetAreas, QTabWidget::North );

	addDockWidget( Qt::LeftDockWidgetArea, ui.resultsDock );
	registerDockWidget( ui.resultsDock, "Optimization &Results" );
	addDockWidget( Qt::BottomDockWidgetArea, ui.messagesDock );
	registerDockWidget( ui.messagesDock, "&Messages" );
	auto* adw = createDockWidget( "&Analysis", analysisView, Qt::BottomDockWidgetArea );
	tabifyDockWidget( ui.messagesDock, adw );

	// init scene
	ground_plane = vis::plane( scene_, 128, 128, 0.5f, scone::GetStudioSetting< vis::color >( "viewer.tile1" ), scone::GetStudioSetting< vis::color >( "viewer.tile2" ) );
	ui.osgViewer->setClearColor( to_osg( scone::GetStudioSetting< vis::color >( "viewer.background" ) ) );
	//ground_plane = scene_.add< vis::plane >( xo::vec3f( 64, 0, 0 ), xo::vec3f( 0, 0, -64 ), GetFolder( SCONE_UI_RESOURCE_FOLDER ) / "stile160.png", 64, 64 );
	//ui.osgViewer->setClearColor( to_osg( vis::make_from_hex( 0xa0a0a0 ) ) );
}

bool SconeStudio::init()
{
	// init file model and browser widget
	auto results_folder = scone::GetFolder( SCONE_RESULTS_FOLDER );
	xo::create_directories( results_folder );

	resultsModel = new ResultsFileSystemModel( nullptr );
	ui.resultsBrowser->setModel( resultsModel );
	ui.resultsBrowser->setNumColumns( 1 );
	ui.resultsBrowser->setRoot( make_qt( results_folder ), "*.par;*.sto" );
	ui.resultsBrowser->header()->setFrameStyle( QFrame::NoFrame | QFrame::Plain );

	connect( ui.resultsBrowser->selectionModel(),
		SIGNAL( currentChanged( const QModelIndex&, const QModelIndex& ) ),
		this, SLOT( selectBrowserItem( const QModelIndex&, const QModelIndex& ) ) );

	ui.osgViewer->setScene( &scene_.osg_group() );
	ui.osgViewer->setHud( GetFolder( SCONE_UI_RESOURCE_FOLDER ) / "scone_hud.png" );
	//ui.tabWidget->tabBar()->tabButton( 0, QTabBar::RightSide )->resize( 0, 0 );

	ui.playControl->setRange( 0, 100 );
	connect( ui.playControl, &QPlayControl::playTriggered, this, &SconeStudio::start );
	connect( ui.playControl, &QPlayControl::stopTriggered, this, &SconeStudio::stop );
	connect( ui.playControl, &QPlayControl::timeChanged, this, &SconeStudio::setPlaybackTime );
	connect( ui.playControl, &QPlayControl::sliderReleased, this, &SconeStudio::refreshAnalysis );
	connect( analysisView, &QDataAnalysisView::timeChanged, ui.playControl, &QPlayControl::setTime );

	// start timer for viewer
	connect( &backgroundUpdateTimer, SIGNAL( timeout() ), this, SLOT( updateBackgroundTimer() ) );
	backgroundUpdateTimer.start( 500 );

	// add outputText to global sinks (only *after* the ui has been initialized)
	xo::log::add_sink( ui.outputText );
	ui.outputText->set_log_level( XO_IS_DEBUG ? xo::log::trace_level : xo::log::debug_level );

	restoreSettings( "SCONE", "SconeStudio" );
	ui.messagesDock->raise();

	return true;
}

SconeStudio::~SconeStudio()
{}

void SconeStudio::runSimulation( const QString& filename )
{
	SCONE_PROFILE_RESET;
	if ( createModel( filename.toStdString() ) )
	{
		updateViewSettings();
		storageModel.setStorage( &model->GetData() );
		analysisView->reset();
		if ( model->IsEvaluating() )
			evaluate();
		ui.playControl->setRange( 0, model->GetMaxTime() );
	}
	SCONE_PROFILE_REPORT;
}

void SconeStudio::activateBrowserItem( QModelIndex idx )
{
	currentParFile = ui.resultsBrowser->fileSystemModel()->fileInfo( idx ).absoluteFilePath();
	ui.playControl->reset();
	runSimulation( currentParFile );
	if ( model )
		ui.playControl->play();
}

void SconeStudio::selectBrowserItem( const QModelIndex& idx, const QModelIndex& idxold )
{
	auto item = ui.resultsBrowser->fileSystemModel()->fileInfo( idx );
	string dirname = item.isDir() ? item.filePath().toStdString() : item.dir().path().toStdString();
}

void SconeStudio::start()
{
	ui.osgViewer->stopTimer();
}

void SconeStudio::stop()
{
	ui.osgViewer->startTimer();
	refreshAnalysis();
}

void SconeStudio::refreshAnalysis()
{
	analysisView->refresh( current_time );
}

void SconeStudio::evaluate()
{
	SCONE_ASSERT( model );

	QProgressDialog dlg( ( "Evaluating " + model->GetFileName().string() ).c_str(), "Abort", 0, 1000, this );
	dlg.setWindowModality( Qt::WindowModal );
	dlg.show();
	QApplication::processEvents();

	xo::set_thread_priority( xo::thread_priority::highest );

	const double step_size = 0.01;
	const xo::seconds_t visual_update = 0.25;
	xo::seconds_t prev_visual_time = -visual_update;
	xo::timer real_time;
	for ( double t = step_size; model->IsEvaluating(); t += step_size )
	{
		auto rt = real_time.seconds();
		if ( rt - prev_visual_time >= visual_update )
		{
			// update 3D visuals and progress bar
			setTime( t, true );
			dlg.setValue( int( 1000 * t / model->GetMaxTime() ) );
			if ( dlg.wasCanceled() ) {
				model->FinalizeEvaluation( false );
				break;
			}
			prev_visual_time = rt;
		}
		else setTime( t, false ); // 
	}

	// report duration
	auto real_dur = real_time.seconds();
	auto sim_time = model->GetTime();
	log::info( "Evaluation took ", real_dur, "s for ", sim_time, "s (", sim_time / real_dur, "x real-time)" );

	xo::set_thread_priority( xo::thread_priority::normal );

	dlg.setValue( 1000 );
	model->UpdateVis( model->GetTime() );
}

void SconeStudio::createVideo()
{
	if ( !model )
		return error( "No Scenario", "There is no scenario open" );

	captureFilename = QFileDialog::getSaveFileName( this, "Video Filename", QString(), "avi files (*.avi)" );
	if ( captureFilename.isEmpty() )
		return;

	// start recording
	QDir().mkdir( captureFilename + ".images" );
	ui.osgViewer->startCapture( captureFilename.toStdString() + ".images/image" );

	ui.osgViewer->stopTimer();
	ui.abortButton->setChecked( false );
	ui.progressBar->setValue( 0 );
	ui.progressBar->setFormat( " Creating Video (%p%)" );
	ui.stackedWidget->setCurrentIndex( 1 );

	const double step_size = ui.playControl->slowMotionFactor() / GetStudioSettings().get<double>( "video.frame_rate" );
	for ( double t = 0.0; t <= model->GetMaxTime(); t += step_size )
	{
		setTime( t, true );
		ui.progressBar->setValue( int( t / model->GetMaxTime() * 100 ) );
		QApplication::processEvents();
		if ( ui.abortButton->isChecked() )
			break;
	}

	// finalize recording
	finalizeCapture();
	ui.stackedWidget->setCurrentIndex( 0 );
	ui.osgViewer->startTimer();
}

void SconeStudio::captureImage()
{
	QString filename = QFileDialog::getSaveFileName( this, "Image Filename", QString(), "png files (*.png)" );
	if ( !filename.isEmpty() )
		ui.osgViewer->captureCurrentFrame( filename.toStdString() );
}

void SconeStudio::setTime( TimeInSeconds t, bool update_vis )
{
	if ( model )
	{
		// update current time and stop when done
		current_time = t;

		// update ui and visualization
		if ( model->IsEvaluating() )
			model->EvaluateTo( t );

		if ( update_vis )
		{
			model->UpdateVis( t );
			auto d = com_delta( model->GetSimModel().GetComPos() );
			ui.osgViewer->moveCamera( osg::Vec3( d.x, 0, d.z ) );
			ui.osgViewer->setFrameTime( current_time );

			if ( analysisView->isVisible() ) // TODO: not update so much when not playing (it's slow)
				analysisView->refresh( current_time, !ui.playControl->isPlaying() );
		}
	}
}

void SconeStudio::fileOpenTriggered()
{
	QString default_path = make_qt( GetFolder( SCONE_SCENARIO_FOLDER ) );
	if ( auto* s = getActiveScenario() )
		default_path = make_qt( path( s->fileName.toStdString() ).parent_path() );

	QString filename = QFileDialog::getOpenFileName( this, "Open Scenario", default_path, "SCONE Scenarios (*.scone *.xml *.zml)" );
	if ( !filename.isEmpty() )
		openFile( filename );
}

void SconeStudio::openFile( const QString& filename )
{
	try
	{
		QCodeEditor* edw = new QCodeEditor( this );
		edw->open( filename );
		int idx = ui.tabWidget->addTab( edw, edw->getTitle() );
		ui.tabWidget->setCurrentIndex( idx );
		connect( edw, &QCodeEditor::textChanged, this, &SconeStudio::updateTabTitles );
		scenarios.push_back( edw );
		updateRecentFilesMenu( filename );
	}
	catch ( std::exception& e )
	{
		error( "Error opening " + filename, e.what() );
	}
}

void SconeStudio::fileSaveTriggered()
{
	if ( auto* s = getActiveScenario() )
	{
		s->save();
		ui.tabWidget->setTabText( getTabIndex( s ), s->getTitle() );
	}
}

void SconeStudio::fileSaveAsTriggered()
{
	if ( auto* s = getActiveScenario() )
	{
		QString filename = QFileDialog::getSaveFileName( this, "Save File As", s->fileName, "SCONE file (*.scone);;XML file (*.xml)" );
		if ( !filename.isEmpty() )
		{
			s->saveAs( filename );
			ui.tabWidget->setTabText( getTabIndex( s ), s->getTitle() );
			updateRecentFilesMenu( s->fileName );
		}
	}
}

bool SconeStudio::tryExit()
{
	abortOptimizations();
	return optimizations.empty();
}

void SconeStudio::addProgressDock( ProgressDockWidget* pdw )
{
	optimizations.push_back( pdw );
	if ( optimizations.size() < 4 )
	{
		// stack below results
		addDockWidget( Qt::LeftDockWidgetArea, pdw );
		for ( index_t r = 0; r < optimizations.size(); ++r )
			splitDockWidget( r == 0 ? ui.resultsDock : optimizations[ r - 1 ], optimizations[ r ], Qt::Vertical );
	}
	else
	{ 
		// organize into columns
		addDockWidget( Qt::LeftDockWidgetArea, pdw );

		auto columns = std::max<int>( 1, ( optimizations.size() + 5 ) / 6 );
		auto rows = ( optimizations.size() + columns - 1 ) / columns;
		log::debug( "Reorganizing windows, columns=", columns, " rows=", rows );

		// first column
		splitDockWidget( optimizations[ 0 ], ui.resultsDock, Qt::Horizontal );
		for ( index_t r = 1; r < rows; ++r )
			splitDockWidget( optimizations[ ( r - 1 ) * columns ], optimizations[ r * columns ], Qt::Vertical );

		// remaining columns
		for ( index_t c = 1; c < columns; ++c )
		{
			for ( index_t r = 0; r < rows; ++r )
			{
				index_t i = r * columns + c;
				if ( i < optimizations.size() )
					splitDockWidget( optimizations[ i - 1 ], optimizations[ i ], Qt::Horizontal );
			}
		}
	}
}

bool SconeStudio::createModel( const String& par_file, bool force_evaluation )
{
	try
	{
		model.reset();
		storageModel.setStorage( nullptr );
		model = std::make_unique< StudioModel >( scene_, path( par_file ), force_evaluation );
	}
	catch ( std::exception& e )
	{
		error( "Could not create model", e.what() );
		return false;
	}
	return true;
}

bool SconeStudio::requestSaveChanges( QCodeEditor* s )
{
	if ( s )
	{
		if ( s->hasTextChanged() )
		{
			QString message = "Save changes to " + s->getTitle() + "?";
			if ( QMessageBox::warning( this, "Save Changes", message, QMessageBox::Save, QMessageBox::Discard ) == QMessageBox::Save )
				s->save();
			ui.tabWidget->setTabText( getTabIndex( s ), s->getTitle() );
			return true;
		}
		else return false;
	}
	else return information( "No Scenario Selected", "No Scenario open for editing" ), false;
}

int SconeStudio::getTabIndex( QCodeEditor* s )
{
	for ( int idx = 0; idx < ui.tabWidget->count(); ++idx )
	{
		if ( ui.tabWidget->widget( idx ) == (QWidget*)s )
			return idx;
	}
	return -1;
}

void SconeStudio::optimizeScenario()
{
	if ( auto* s = getActiveScenario( true ) )
	{
		requestSaveChanges( s );
		ProgressDockWidget* pdw = new ProgressDockWidget( this, getActiveScenario()->fileName );
		addProgressDock( pdw );
		updateOptimizations();
	}
}

void SconeStudio::runScenario()
{
	ui.playControl->stop();

	if ( auto* s = getActiveScenario( false ) )
	{
		requestSaveChanges( s );
		runSimulation( s->fileName );
		if ( model )
			ui.playControl->play();
	}
}

void SconeStudio::optimizeScenarioMultiple()
{
	if ( auto* s = getActiveScenario( true ) )
	{
		requestSaveChanges( s );

		bool ok = true;
		int count = QInputDialog::getInt( this, "Run Multiple Optimizations", "Enter number of optimization instances: ", 3, 1, 100, 1, &ok );
		if ( ok )
		{
			for ( int i = 1; i <= count; ++i )
			{
				QStringList args;
				args << QString().sprintf( "#1.random_seed=%d", i );
				ProgressDockWidget* pdw = new ProgressDockWidget( this, getActiveScenario()->fileName, args );
				addProgressDock( pdw );
			}
			updateOptimizations();
		}
	}
}

void SconeStudio::abortOptimizations()
{
	if ( optimizations.size() > 0 )
	{
		QString message = QString().sprintf( "Are you sure you want to abort the following optimizations:\n\n", optimizations.size() );

		for ( auto& o : optimizations )
			message += o->getIdentifier() + "\n";

		if ( QMessageBox::warning( this, "Abort Optimizations", message, QMessageBox::Abort, QMessageBox::Cancel ) == QMessageBox::Abort )
		{
			close_all = true;
			for ( auto& o : optimizations )
			{
				o->close();
			}
			optimizations.clear();
			close_all = false;
		}
	}
}

void SconeStudio::updateBackgroundTimer()
{
	updateOptimizations();
}

void SconeStudio::updateOptimizations()
{
	// clear out all closed optimizations
	for ( auto it = optimizations.begin(); it != optimizations.end(); )
	{
		ProgressDockWidget* w = *it;
		if ( w->readyForDestruction() )
		{
			delete w;
			it = optimizations.erase( it );
		}
		else ++it;
	}

	// update all optimizations
	for ( auto& o : optimizations )
	{
		if ( o->updateProgress() == ProgressDockWidget::ShowErrorResult )
		{
			QString title = "Error optimizing " + o->fileName;
			QString msg = o->message;
			o->close();
			QMessageBox::critical( this, title, msg );
			return; // must return here because close invalidates the iterator
		}
	}
}

void SconeStudio::tabCloseRequested( int idx )
{
	auto it = xo::find( scenarios, (QCodeEditor*)ui.tabWidget->widget( idx ) );
	SCONE_THROW_IF( it == scenarios.end(), "Could not find scenarion for tab " + to_str( idx ) );

	requestSaveChanges( *it );
	scenarios.erase( it );
	ui.tabWidget->removeTab( idx );
}

void SconeStudio::updateViewSettings()
{
	StudioModel::ViewFlags f;
	for ( auto& va : viewActions )
		f.set( va.first, va.second->isChecked() );

	if ( model )
		model->ApplyViewSettings( f );

	ground_plane.show( f.get< StudioModel::ShowGroundPlane >() );
}

void SconeStudio::updateTabTitles()
{
	for ( auto s : scenarios )
		ui.tabWidget->setTabText( getTabIndex( s ), s->getTitle() );
}

QCodeEditor* SconeStudio::getActiveScenario( bool show_error )
{
	for ( auto edw : scenarios )
	{
		if ( !edw->visibleRegion().isEmpty() )
			return edw;
	}

	if ( show_error )
		QMessageBox::information( this, "No Scenario Selected", "No Scenario open for editing" );

	return nullptr;
}

void SconeStudio::finalizeCapture()
{
	ui.osgViewer->stopCapture();

	QString program = make_qt( xo::get_application_folder() / SCONE_FFMPEG_EXECUTABLE );
	QStringList args;
	args << "-r" << GetStudioSettings().get<QString>( "video.frame_rate" )
		<< "-i" << captureFilename + ".images/image_0_%d.png"
		<< "-c:v" << "mpeg4"
		<< "-q:v" << GetStudioSettings().get<QString>( "video.quality" )
		<< captureFilename;

	cout << "starting " << program.toStdString() << endl;
	auto v = args.toVector();
	for ( auto arg : v ) cout << arg.toStdString() << endl;

	captureProcess = new QProcess( this );
	captureProcess->start( program, args );

	xo_error_if( !captureProcess->waitForStarted( 5000 ), "Could not start process" );
	scone::log::info( "Generating video for ", captureFilename.toStdString() );

	if ( !captureProcess->waitForFinished( 30000 ) )
		scone::log::error( "Did not finish in time" );

	scone::log::info( "Video generated" );
	QDir( captureFilename + ".images" ).removeRecursively();

	delete captureProcess;
	captureProcess = nullptr;
	captureFilename.clear();
}
