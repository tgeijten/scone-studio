/*
** SconeStudio.cpp
**
** Copyright (C) 2013-2019 Thomas Geijtenbeek and contributors. All rights reserved.
**
** This file is part of SCONE. For more information, see http://scone.software.
*/

#include "SconeStudio.h"

#include "StudioSettings.h"
#include "studio_config.h"
#include "ui_SconeSettings.h"
#include "GaitAnalysis.h"
#include "OptimizerTaskExternal.h"
#include "OptimizerTaskThreaded.h"

#include "xo/container/container_tools.h"
#include "xo/container/prop_node_tools.h"
#include "xo/filesystem/filesystem.h"
#include "xo/system/system_tools.h"
#include "xo/utility/color.h"

#include "scone/core/Benchmark.h"
#include "scone/core/Factories.h"
#include "scone/core/Log.h"
#include "scone/core/Settings.h"
#include "scone/core/StorageIo.h"
#include "scone/core/profiler_config.h"
#include "scone/core/storage_tools.h"
#include "scone/core/system_tools.h"
#include "scone/core/version.h"
#include "scone/model/muscle_tools.h"
#include "scone/optimization/Optimizer.h"
#include "scone/optimization/opt_tools.h"
#include "scone/model/Dof.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QTabWidget>
#include <QTextStream>
#include <osgDB/ReadFile>

#include "qcustomplot.h"
#include "qt_convert.h"

#include "vis-osg/osg_object_manager.h"
#include "vis-osg/osg_tools.h"
#include "vis/plane.h"
#include "help_tools.h"
#include "xo/thread/thread_priority.h"
#include "file_tools.h"
#include "model_conversion.h"
#include "scone/sconelib_config.h"
#include "studio_tools.h"

using namespace scone;
using namespace xo::time_literals;

SconeStudio::SconeStudio( QWidget* parent, Qt::WindowFlags flags ) :
	QCompositeMainWindow( parent, flags ),
	close_all( false ),
	current_time(),
	evaluation_time_step( 1.0 / 8 ),
	scene_( true, GetStudioSetting< float >( "viewer.ambient_intensity" ) ),
	slomo_factor( 1 ),
	com_delta( Vec3( 0, 0, 0 ) ),
	captureProcess( nullptr )
{
	xo::log::debug( "Constructing UI elements" );
	ui.setupUi( this );

	// Analysis
	analysisView = new QDataAnalysisView( &analysisStorageModel, this );
	analysisView->setObjectName( "Analysis" );
	analysisView->setMinSeriesInterval( 0 );
	analysisView->setLineWidth( scone::GetStudioSettings().get< float >( "analysis.line_width" ) );

	// File menu
	auto fileMenu = menuBar()->addMenu( ( "&File" ) );
	fileMenu->addAction( "&Open...", this, &SconeStudio::fileOpenTriggered, QKeySequence( "Ctrl+O" ) );
	recentFilesMenu = addMenuAction( fileMenu, "Open &Recent", this, &QCompositeMainWindow::fileOpenTriggered, QKeySequence() );
	fileMenu->addAction( "Re&load", this, &SconeStudio::fileReloadTriggered, QKeySequence( "Ctrl+R" ) );
	fileMenu->addSeparator();
	fileMenu->addAction( "&Save", this, &SconeStudio::fileSaveTriggered, QKeySequence( "Ctrl+S" ) );
	fileMenu->addAction( "Save &As...", this, &SconeStudio::fileSaveAsTriggered, QKeySequence( "Ctrl+Shift+S" ) );
	fileMenu->addAction( "&Close", this, &SconeStudio::fileCloseTriggered, QKeySequence( "Ctrl+W" ) );
	fileMenu->addSeparator();
	fileMenu->addAction( "E&xit", this, &SconeStudio::fileExitTriggered, QKeySequence( "Alt+X" ) );

	// Edit menu
	auto editMenu = menuBar()->addMenu( "&Edit" );
	editMenu->addAction( "&Find...", this, &SconeStudio::findDialog, QKeySequence( "Ctrl+F" ) );
	editMenu->addAction( "Find &Next", this, &SconeStudio::findNext, Qt::Key_F3 );
	editMenu->addAction( "Find &Previous", this, &SconeStudio::findPrevious, QKeySequence( "Shift+F3" ) );
	editMenu->addSeparator();
	editMenu->addAction( "&Toggle Comments", this, &SconeStudio::toggleComments, QKeySequence( "Ctrl+/" ) );
	editMenu->addAction( "&Duplicate Selection", this, [&]() { if ( auto* e = getActiveCodeEditor() ) e->duplicateText(); }, QKeySequence( "Ctrl+U" ) );

	// View menu
	auto viewMenu = menuBar()->addMenu( "&View" );
	viewActions[ ViewOption::ShowForces ] = viewMenu->addAction( "Show External &Forces", this, &SconeStudio::applyViewOptions );
	viewActions[ ViewOption::ShowMuscles ] = viewMenu->addAction( "Show &Muscles", this, &SconeStudio::applyViewOptions );
	viewActions[ ViewOption::ShowTendons ] = viewMenu->addAction( "Show &Tendons", this, &SconeStudio::applyViewOptions );
	viewActions[ ViewOption::ShowBodyGeom ] = viewMenu->addAction( "Show &Body Geometry", this, &SconeStudio::applyViewOptions );
	viewActions[ ViewOption::ShowBodyAxes ] = viewMenu->addAction( "Show Body &Axes", this, &SconeStudio::applyViewOptions );
	viewActions[ ViewOption::ShowBodyCom ] = viewMenu->addAction( "Show Body Cente&r of Mass", this, &SconeStudio::applyViewOptions );
	viewActions[ ViewOption::ShowJoints ] = viewMenu->addAction( "Show &Joints", this, &SconeStudio::applyViewOptions );
	viewActions[ ViewOption::ShowContactGeom ] = viewMenu->addAction( "Show &Contact Geometry", this, &SconeStudio::applyViewOptions );
	viewActions[ ViewOption::ShowGroundPlane ] = viewMenu->addAction( "Show &Ground Plane", this, &SconeStudio::applyViewOptions );
	viewActions[ ViewOption::ShowModelComHeading ] = viewMenu->addAction( "Show Model COM and &Heading", this, &SconeStudio::applyViewOptions );
	viewMenu->addSeparator();
	viewActions[ ViewOption::StaticCamera ] = viewMenu->addAction( "&Static Camera", this, &SconeStudio::applyViewOptions );
	auto uncheckedViewOptions = { ViewOption::ShowBodyAxes, ViewOption::ShowJoints, ViewOption::ShowBodyCom, ViewOption::ShowModelComHeading, ViewOption::StaticCamera };
	for ( auto& va : viewActions )
	{
		va.second->setCheckable( true );
		va.second->setChecked( xo::find( uncheckedViewOptions, va.first ) == uncheckedViewOptions.end() );
	}

	// Scenario menu
	auto scenarioMenu = menuBar()->addMenu( "&Scenario" );
	scenarioMenu->addAction( "&Evaluate Scenario", this, &SconeStudio::evaluateActiveScenario, QKeySequence( "Ctrl+E" ) );
	scenarioMenu->addSeparator();
	scenarioMenu->addAction( "&Optimize Scenario", this, &SconeStudio::optimizeScenario, QKeySequence( "Ctrl+F5" ) );
	scenarioMenu->addAction( "Run &Multiple Optimizations", this, &SconeStudio::optimizeScenarioMultiple, QKeySequence( "Ctrl+Shift+F5" ) );
	scenarioMenu->addSeparator();
	scenarioMenu->addAction( "&Abort Optimizations", this, &SconeStudio::abortOptimizations, QKeySequence() );
	scenarioMenu->addSeparator();
	scenarioMenu->addAction( "&Performance Test", this, &SconeStudio::performanceTestNormal, QKeySequence( "Ctrl+P" ) );
	scenarioMenu->addAction( "Performance Test (Write Stats)", this, &SconeStudio::performanceTestWriteStats, QKeySequence( "Ctrl+Shift+P" ) );

	// Tools menu
	auto toolsMenu = menuBar()->addMenu( "&Tools" );
	toolsMenu->addAction( "Generate &Video...", this, &SconeStudio::createVideo );
	toolsMenu->addAction( "Save &Image...", this, &SconeStudio::captureImage, QKeySequence( "Ctrl+I" ) );
	toolsMenu->addSeparator();
	toolsMenu->addAction( "&Model Analysis", this, &SconeStudio::modelAnalysis );
	toolsMenu->addAction( "M&uscle Analysis", this, &SconeStudio::muscleAnalysis );
	toolsMenu->addAction( "&Gait Analysis", this, &SconeStudio::updateGaitAnalysis, QKeySequence( "Ctrl+G" ) );
	toolsMenu->addAction( "Fil&ter Analysis", this, &SconeStudio::activateAnalysisFilter, QKeySequence( "Ctrl+L" ) );
	toolsMenu->addAction( "&Keep Current Analysis Graphs", analysisView, &QDataAnalysisView::holdSeries, QKeySequence( "Ctrl+Shift+K" ) );
	toolsMenu->addSeparator();
	toolsMenu->addAction( "&Export Coordinates...", this, &SconeStudio::exportCoordinates );
	toolsMenu->addSeparator();
#if SCONE_HYFYDY_ENABLED
	toolsMenu->addAction( "&Convert Model...", [=]() { ShowModelConversionDialog( this ); } );
	toolsMenu->addSeparator();
#endif

	toolsMenu->addAction( "&Preferences...", this, &SconeStudio::showSettingsDialog, QKeySequence( "Ctrl+," ) );

	// Action menu
	auto* actionMenu = menuBar()->addMenu( "&Playback" );
	actionMenu->addAction( "&Play or Evaluate", ui.playControl, &QPlayControl::togglePlay, Qt::Key_F5 );
	actionMenu->addAction( "&Stop / Reset", ui.playControl, &QPlayControl::stopReset, Qt::Key_F8 );
	actionMenu->addAction( "Toggle Play", ui.playControl, &QPlayControl::togglePlay, QKeySequence( "Ctrl+Space" ) );
	actionMenu->addAction( "Toggle Loop", ui.playControl, &QPlayControl::toggleLoop, QKeySequence( "Ctrl+Shift+L" ) );
	actionMenu->addAction( "Play F&aster", ui.playControl, &QPlayControl::faster, QKeySequence( "Ctrl+Up" ) );
	actionMenu->addAction( "Play S&lower", ui.playControl, &QPlayControl::slower, QKeySequence( "Ctrl+Down" ) );
	actionMenu->addSeparator();
	actionMenu->addAction( "Step &Back", ui.playControl, &QPlayControl::stepBack, QKeySequence( "Ctrl+Left" ) );
	actionMenu->addAction( "Step &Forward", ui.playControl, &QPlayControl::stepForward, QKeySequence( "Ctrl+Right" ) );
	actionMenu->addAction( "Page &Back", ui.playControl, &QPlayControl::pageBack, QKeySequence( "Ctrl+PgUp" ) );
	actionMenu->addAction( "Page &Forward", ui.playControl, &QPlayControl::pageForward, QKeySequence( "Ctrl+PgDown" ) );
	actionMenu->addAction( "Goto &Begin", ui.playControl, &QPlayControl::reset, QKeySequence( "Ctrl+Home" ) );
	actionMenu->addAction( "Go to &End", ui.playControl, &QPlayControl::end, QKeySequence( "Ctrl+End" ) );

	// Window menu
	auto windowMenu = createWindowMenu();

	// Help menu
	auto helpMenu = menuBar()->addMenu( ( "&Help" ) );
	helpMenu->addAction( "View &Help...", this, &SconeStudio::helpSearch, QKeySequence( "F1" ) );
	helpMenu->addAction( "Online &Documentation...", this, []() { QDesktopServices::openUrl( GetWebsiteUrl() ); } );
	helpMenu->addAction( "Check for &Updates...", this, []() { QDesktopServices::openUrl( GetDownloadUrl() ); } );
	helpMenu->addAction( "User &Forum...", this, []() { QDesktopServices::openUrl( GetForumUrl() ); } );
	helpMenu->addAction( "Repair &Tutorials...", this, []() { scone::installTutorials(); } );
	helpMenu->addSeparator();
	helpMenu->addAction( "&About...", this, &SconeStudio::helpAbout );

	// Results Browser
	auto results_folder = scone::GetFolder( SCONE_RESULTS_FOLDER );
	xo::create_directories( results_folder );
	resultsModel = new ResultsFileSystemModel( nullptr );
	ui.resultsBrowser->setModel( resultsModel );
	ui.resultsBrowser->setNumColumns( 1 );
	ui.resultsBrowser->setRoot( to_qt( results_folder ), "*.par;*.sto" );
	ui.resultsBrowser->header()->setFrameStyle( QFrame::NoFrame | QFrame::Plain );
	ui.resultsBrowser->setSelectionMode( QAbstractItemView::ExtendedSelection );
	ui.resultsBrowser->setSelectionBehavior( QAbstractItemView::SelectRows );
	ui.resultsBrowser->setContextMenuPolicy( Qt::CustomContextMenu );
	connect( ui.resultsBrowser, SIGNAL( customContextMenuRequested( const QPoint& ) ), this, SLOT( onResultBrowserCustomContextMenu( const QPoint& ) ) );
	connect( ui.resultsBrowser->selectionModel(),
		SIGNAL( currentChanged( const QModelIndex&, const QModelIndex& ) ),
		this, SLOT( selectBrowserItem( const QModelIndex&, const QModelIndex& ) ) );

	// Play Control
	ui.stackedWidget->setCurrentIndex( 0 );
	ui.playControl->setDigits( 6, 3 );
	ui.playControl->setRange( 0, 100 );
	connect( ui.playControl, &QPlayControl::playTriggered, this, &SconeStudio::start );
	connect( ui.playControl, &QPlayControl::stopTriggered, this, &SconeStudio::stop );
	connect( ui.playControl, &QPlayControl::timeChanged, this, &SconeStudio::setPlaybackTime );
	connect( ui.playControl, &QPlayControl::sliderReleased, this, &SconeStudio::refreshAnalysis );
	connect( analysisView, &QDataAnalysisView::timeChanged, ui.playControl, &QPlayControl::setTimeStop );

	// docking
	setDockNestingEnabled( true );
	setCorner( Qt::TopLeftCorner, Qt::LeftDockWidgetArea );
	setCorner( Qt::BottomLeftCorner, Qt::LeftDockWidgetArea );
	setCorner( Qt::TopRightCorner, Qt::RightDockWidgetArea );
	setCorner( Qt::BottomRightCorner, Qt::BottomDockWidgetArea );

	connect( ui.viewerDock, &QDockWidget::topLevelChanged, this, &SconeStudio::fixViewerWindowSize );

	addDockWidget( Qt::LeftDockWidgetArea, ui.resultsDock );
	registerDockWidget( ui.resultsDock, "&Optimization Results" );

	addDockWidget( Qt::BottomDockWidgetArea, ui.messagesDock );
	registerDockWidget( ui.messagesDock, "&Messages" );

	analysisDock = createDockWidget( "&Analysis", analysisView, Qt::BottomDockWidgetArea );
	tabifyDockWidget( ui.messagesDock, analysisDock );

	// gait analysis
	gaitAnalysis = new GaitAnalysis( this );
	gaitAnalysisDock = createDockWidget( "&Gait Analysis", gaitAnalysis, Qt::BottomDockWidgetArea );
	tabifyDockWidget( ui.messagesDock, gaitAnalysisDock );
	gaitAnalysisDock->hide();

	// parameter view
	parView = new QTableView( this );
	parModel = new ParTableModel();
	parView->setModel( parModel );
	parView->setEditTriggers( QAbstractItemView::NoEditTriggers );
	for ( int i = 0; i < parView->horizontalHeader()->count(); ++i )
		parView->horizontalHeader()->setSectionResizeMode( i, i == 0 ? QHeaderView::Stretch : QHeaderView::ResizeToContents );
	parView->verticalHeader()->setSectionResizeMode( QHeaderView::Fixed );
	parView->verticalHeader()->setDefaultSectionSize( 24 );
	parViewDock = createDockWidget( "Optimization &Parameters", parView, Qt::BottomDockWidgetArea );
	tabifyDockWidget( ui.messagesDock, parViewDock );
	parViewDock->hide();

	// evaluation report
	reportModel = new QPropNodeItemModel();
	reportModel->setDefaultIcon( style()->standardIcon( QStyle::SP_FileIcon ) );
	reportView = new QTreeView( this );
	reportView->setIndentation( 16 );
	reportView->setModel( reportModel );
	reportView->setEditTriggers( QAbstractItemView::NoEditTriggers );
	reportView->header()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
	reportDock = createDockWidget( "Evaluation &Report", reportView, Qt::BottomDockWidgetArea );
	reportDock->hide();

#if SCONE_EXPERIMENTAL_FEATURES_ENABLED
	// evaluation report
	inspectorModel = new QPropNodeItemModel();
	inspectorModel->setMaxPreviewChildren( 3 );
	//inspectorModel->setDefaultIcon( style()->standardIcon( QStyle::SP_FileIcon ) );
	inspectorView = new QTreeView( this );
	inspectorView->setIndentation( 16 );
	inspectorView->setModel( inspectorModel );
	inspectorView->setEditTriggers( QAbstractItemView::NoEditTriggers );
	inspectorView->header()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
	inspectorDock = createDockWidget( "Model &Inspector", inspectorView, Qt::BottomDockWidgetArea );
	inspectorDock->hide();
#endif // SCONE_EXPERIMENTAL_FEATURES_ENABLED

	// dof editor
	dofEditor = new DofEditorGroup( this );
	connect( dofEditor, &DofEditorGroup::valueChanged, this, &SconeStudio::dofEditorValueChanged );
	connect( dofEditor, &DofEditorGroup::exportCoordinates, this, &SconeStudio::exportCoordinates );
	dofDock = createDockWidget( "&Coordinates", dofEditor, Qt::BottomDockWidgetArea );
	tabifyDockWidget( ui.messagesDock, dofDock );
	dofDock->hide();

	// finalize windows menu
	windowMenu->addSeparator();
	windowMenu->addAction( "Reset Window Layout", this, &SconeStudio::resetWindowLayout );

	// init viewer / scene
	ui.osgViewer->setClearColor( vis::to_osg( scone::GetStudioSetting< xo::color >( "viewer.background" ) ) );
	ui.osgViewer->setScene( &vis::osg_group( scene_.node_id() ) );
	ui.osgViewer->createHud( GetSconeStudioFolder() / "resources/ui/scone_hud.png" );

	// reset layout if requested
	createSettings( "SCONE", "SconeStudio" );
	if ( GetStudioSetting<bool>( "ui.reset_layout" ) )
	{
		GetStudioSettings().set( "ui.reset_layout", false );
		GetStudioSettings().save();
	}
	else restoreSettings();
}

bool SconeStudio::init()
{
	// add outputText to global sinks (only *after* the ui has been initialized)
	xo::log::add_sink( ui.outputText );
	ui.outputText->set_sink_mode( xo::log::sink_mode::current_thread );
	ui.outputText->set_log_level( XO_IS_DEBUG_BUILD ? xo::log::level::trace : xo::log::level::debug );

	// start timer for viewer
	connect( &backgroundUpdateTimer, SIGNAL( timeout() ), this, SLOT( updateBackgroundTimer() ) );
	backgroundUpdateTimer.start( 500 );

	// see if this is a new version of SCONE
	auto version = xo::to_str( scone::GetSconeVersion() );
	scone::log::info( "SCONE version ", version );
	if ( GetStudioSetting<std::string>( "ui.last_version_run" ) != version )
	{
		GetStudioSettings().set( "ui.last_version_run", version );
		GetStudioSettings().save();
		if ( GetStudioSetting<bool>( "ui.install_tutorials" ) )
			scone::installTutorials();
	}

	ui.messagesDock->raise();

	return true;
}

SconeStudio::~SconeStudio()
{}

void SconeStudio::restoreCustomSettings( QSettings& settings )
{
	if ( settings.contains( "viewSettings" ) )
	{
		ViewOptions f( settings.value( "viewSettings" ).toULongLong() );
		for ( auto& va : viewActions )
			va.second->setChecked( f.get( va.first ) );
	}
}

void SconeStudio::saveCustomSettings( QSettings& settings )
{
	ViewOptions f;
	for ( auto& va : viewActions )
		f.set( va.first, va.second->isChecked() );
	settings.setValue( "viewSettings", QVariant( uint( f.data() ) ) );
}

void SconeStudio::activateBrowserItem( QModelIndex idx )
{
	auto info = ui.resultsBrowser->fileSystemModel()->fileInfo( idx );
	if ( info.isDir() )
		info = scone::findBestPar( QDir( info.absoluteFilePath() ) );
	auto str = info.path().toStdString();
	if ( info.exists() )
	{
		if ( createScenario( info.absoluteFilePath() ) )
		{
			if ( scenario_->IsEvaluating() ) // .par or .sto
				evaluate();
			ui.playControl->play(); // automatic playback after evaluation
		}
	}
}

void SconeStudio::selectBrowserItem( const QModelIndex& idx, const QModelIndex& idxold )
{
	auto item = ui.resultsBrowser->fileSystemModel()->fileInfo( idx );
	string dirname = item.isDir() ? item.filePath().toStdString() : item.dir().path().toStdString();
}

void SconeStudio::start()
{
	auto s = getActiveScenario();
	if ( s && ( !scenario_ ||
		s->document()->isModified() ||
		scenario_->IsEvaluating() ||
		( s->hasFocus() && scenario_->GetFileName() != path_from_qt( s->fileName ) ) ) )
	{
		// update the simulation
		evaluateActiveScenario();
	}
	else
	{
		// everything is up-to-date, pause idle update timer
		ui.osgViewer->stopTimer();
	}
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

void SconeStudio::dofEditorValueChanged()
{
	if ( scenario_ && scenario_->HasModel() && scenario_->IsEvaluatingStart() )
	{
		dofEditor->setDofsFromSliders( scenario_->GetModel() );
		scenario_->GetModel().UpdateStateFromDofs();
		scenario_->UpdateVis( 0.0 );
		ui.osgViewer->update();
	}
}

void SconeStudio::evaluate()
{
	SCONE_ASSERT( scenario_ );

	QProgressDialog dlg( ( "Evaluating " + scenario_->GetFileName().str() ).c_str(), "Abort", 0, 1000, this );
	dlg.setWindowModality( Qt::WindowModal );
	dlg.show();
	QApplication::processEvents();

	xo::scoped_thread_priority prio_raiser( xo::thread_priority::highest );

	const double step_size = 0.01;
	const xo::time visual_update = 250_ms;
	xo::time prev_visual_time = xo::time() - visual_update;
	xo::timer real_time;
	for ( double t = step_size; scenario_->IsEvaluating(); t += step_size )
	{
		auto rt = real_time();
		if ( rt - prev_visual_time >= visual_update )
		{
			// update 3D visuals and progress bar
			setTime( t, true );
			dlg.setValue( int( 1000 * t / scenario_->GetMaxTime() ) );
			if ( dlg.wasCanceled() )
			{
				// user pressed cancel: update data so that user can see results so far
				scenario_->AbortEvaluation();
				break;
			}
			prev_visual_time = rt;
		}
		else setTime( t, false );
	}

	if ( scenario_->HasData() )
	{
		// report duration and update storage
		auto real_dur = real_time().secondsd();
		auto sim_time = scenario_->GetTime();
		log::info( "Evaluation took ", real_dur, "s for ", sim_time, "s (", sim_time / real_dur, "x real-time)" );
		updateModelDataWidgets();
	}

	dlg.setValue( 1000 );
	scenario_->UpdateVis( scenario_->GetTime() );

	reportModel->setData( scenario_->GetResult() );
	reportView->expandToDepth( 0 );
}

void SconeStudio::modelAnalysis()
{
	if ( scenario_ && scenario_->HasModel() )
		xo::log_prop_node( scenario_->GetModel().GetInfo() );
}

void SconeStudio::muscleAnalysis()
{
	if ( scenario_ && scenario_->HasModel() )
		scone::WriteMuscleInfo( scenario_->GetModel() );
}

void SconeStudio::updateGaitAnalysis()
{
	try
	{
		if ( scenario_ && !scenario_->IsEvaluating() )
		{
			gaitAnalysis->update( scenario_->GetData(), scenario_->GetFileName() );
			gaitAnalysisDock->setWindowTitle( gaitAnalysis->info() );
			gaitAnalysisDock->show();
			gaitAnalysisDock->raise();
		}
	}
	catch ( const std::exception& e ) { error( "Error", e.what() ); }
}

void SconeStudio::activateAnalysisFilter()
{
	analysisDock->raise();
	analysisView->focusFilterEdit();
}

void SconeStudio::setTime( TimeInSeconds t, bool update_vis )
{
	if ( scenario_ )
	{
		current_time = t;

		// update ui and visualization
		if ( scenario_->IsEvaluating() )
		{
			scenario_->EvaluateTo( t );
			if ( t > 0.0 )
				dofEditor->setEnableEditing( false );
		}

		if ( update_vis && scenario_->HasModel() )
		{
			scenario_->UpdateVis( t );
			if ( !scenario_->GetViewOptions().get<ViewOption::StaticCamera>() )
			{
				auto d = com_delta( scenario_->GetFollowPoint() );
				ui.osgViewer->moveCamera( osg::Vec3( d.x, d.y, d.z ) );
			}

			ui.osgViewer->setFrameTime( current_time );
			if ( analysisView->isVisible() ) // #todo: not update so much when not playing (it's slow)
				analysisView->refresh( current_time, !ui.playControl->isPlaying() );

			if ( dofEditor->isVisible() )
				dofEditor->setSlidersFromDofs( scenario_->GetModel() );
		}
	}
}

void SconeStudio::fileOpenTriggered()
{
	QString default_path = to_qt( GetFolder( SCONE_SCENARIO_FOLDER ) );
	if ( auto* s = getActiveScenario() )
		default_path = to_qt( path( s->fileName.toStdString() ).parent_path() );

	QString filename = QFileDialog::getOpenFileName( this, "Open Scenario", default_path,
		"Supported file formats (*.scone *.xml *.zml *.lua *.hfd *.osim *.bp);;SCONE Scenarios (*.scone *.xml *.zml);;Lua Scripts (*.lua);;Models (*.osim *.hfd *.bp)" );
	if ( !filename.isEmpty() )
		openFile( filename );
}

void SconeStudio::fileReloadTriggered()
{
	if ( auto* s = getActiveCodeEditor() )
	{
		s->reload();
		createAndVerifyActiveScenario( true );
	}
}

void SconeStudio::openFile( const QString& filename )
{
	try
	{
		QCodeEditor* edw = new QCodeEditor( this );
		edw->open( filename );
		int idx = ui.tabWidget->addTab( edw, edw->getTitle() );
		ui.tabWidget->setCurrentIndex( idx );
		connect( edw, &QCodeEditor::modificationChanged, this, &SconeStudio::updateTabTitles );
		codeEditors.push_back( edw );
		updateRecentFilesMenu( filename );
		createAndVerifyActiveScenario( false );
	}
	catch ( std::exception& e ) { error( "Error opening " + filename, e.what() ); }
}

void SconeStudio::fileSaveTriggered()
{
	if ( auto* s = getActiveCodeEditor() )
	{
		s->save();
		ui.tabWidget->setTabText( getTabIndex( s ), s->getTitle() );
		createAndVerifyActiveScenario( true );
	}
}

void SconeStudio::fileSaveAsTriggered()
{
	try
	{
		if ( auto* s = getActiveCodeEditor() )
		{
			// apparently, the mess below is needed to setup the (trivially) correct file filter in Qt
			QString scone_file = "SCONE scenario (*.scone *.xml)";
			QString lua_file = "Lua script (*.lua)";
			QString model_file = "Model file (*.hfd *.osim *.bp)";
			QString ext = QFileInfo( s->fileName ).suffix();
			QString filter = scone_file + ";;" + lua_file + ";;" + model_file;
			QString* default_filter = nullptr;
			if ( ext == "scone" || ext == "zml" ) default_filter = &scone_file;
			else if ( ext == "lua" ) default_filter = &lua_file;
			else if ( ext == "hfd" || ext == "osim" || ext == "bp" ) default_filter = &model_file;

			// we can finally make the actual call
			QString filename = QFileDialog::getSaveFileName( this, "Save File As", s->fileName, filter, default_filter );
			if ( !filename.isEmpty() )
			{
				s->saveAs( filename );
				ui.tabWidget->setTabText( getTabIndex( s ), s->getTitle() );
				updateRecentFilesMenu( s->fileName );
				createAndVerifyActiveScenario( true );
			}
		}
	}
	catch ( std::exception& e ) { error( "Error saving file", e.what() ); }
}

void SconeStudio::fileCloseTriggered()
{
	if ( auto idx = ui.tabWidget->currentIndex(); idx >= 0 )
		tabCloseRequested( idx );
}

void SconeStudio::helpSearch()
{
	if ( auto* s = getActiveCodeEditor() )
	{
		auto cursor = s->textCursor();
		cursor.select( QTextCursor::WordUnderCursor );
		QDesktopServices::openUrl( scone::GetHelpUrl( cursor.selectedText() ) );
	}
	else QDesktopServices::openUrl( scone::GetHelpUrl( "" ) );
}

void SconeStudio::helpForum()
{
	QDesktopServices::openUrl( QUrl( "https://simtk.org/plugins/phpBB/indexPhpbb.php?group_id=1180&pluginname=phpBB" ) );
}

void SconeStudio::helpAbout()
{
	QString title = "<b>" + to_qt( "SCONE version " + xo::to_str( GetSconeVersion() ) ) + "</b><br><br>";
	QString author = "Copyright (C) 2013 - 2021 Thomas Geijtenbeek and contributors. All rights reserved.<br><br>";
	QString scone_license =
		"<b>SCONE</b> is licensed under the <a href='https://www.apache.org/licenses/LICENSE-2.0'>Apache License, Version 2.0</a>. "
		"It uses the following external libraries:<ul>"
		"<li>OpenSim 3.3 (Apache 2.0)"
		"<li>Simbody (Apache 2.0)"
		"<li>Lua (MIT)"
		"<li>Sol 3 (MIT)"
		"<li>TCLAP (MIT)"
		"<li>xo (Apache 2.0)"
		"<li>spot (Apache 2.0)"
		"</ul><br>";
	QString studio_license =
		"<b>SCONE Studio</b> is licensed under the <a href='https://www.gnu.org/licenses/gpl-3.0.en.html'>GNU Public License 3.0</a>. "
		"It uses the following external libraries:<ul>"
		"<li>Qt (GPL / LGPL)"
		"<li>QCustomPlot (GPL v3)"
		"<li>OpenSceneGraph (OSGPL)"
		"<li>TCLAP (MIT)"
		"<li>xo (Apache 2.0)"
		"<li>spot (Apache 2.0)"
		"<li>vis (Apache 2.0)"
		"<li>qtfx (Apache 2.0)"
		"</ul>";
	information( "About SCONE", title + author + scone_license + studio_license );
}

bool SconeStudio::tryExit()
{
	if ( abortOptimizations() )
	{
		// wait until all optimizations are actually done
		while ( !optimizations.empty() )
			updateOptimizations();
		return true;
	}
	else return false;
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

bool SconeStudio::createScenario( const QString& any_file )
{
	ui.playControl->stop();
	scenario_.reset();
	analysisStorageModel.setStorage( nullptr );
	parModel->setObjectiveInfo( nullptr );
	gaitAnalysis->reset();
	parViewDock->setWindowTitle( "Optimization Parameters" );
	ui.playControl->setRange( 0, 0 );

	try
	{
		// create scenario and update viewer
		currentFilename = any_file;
		scenario_ = std::make_unique< StudioModel >( scene_, path_from_qt( currentFilename ), getViewOptionsFromMenu() );

		if ( scenario_->HasModel() )
		{
			// update parameter view
			parModel->setObjectiveInfo( &scenario_->GetObjective().info() );
			parViewDock->setWindowTitle( QString( "Optimization Parameters (%1)" ).arg( scenario_->GetObjective().info().size() ) );

			// setup dof editor
			dofEditor->init( scenario_->GetModel() );
			dofEditor->setEnableEditing( true );

			// set data, in case the file was an sto
			if ( scenario_->HasData() )
				updateModelDataWidgets();

#if SCONE_EXPERIMENTAL_FEATURES_ENABLED
			// update model inspector
			inspectorModel->setData( scenario_->GetModel().GetInfo() );
			inspectorView->expandToDepth( 0 );
#endif

			// reset play control -- this triggers setTime( 0 ) and updates com_delta
			ui.playControl->reset();
		}
	}
	catch ( FactoryNotFoundException& e )
	{
		if ( e.name_ == "Model" && e.props_.has_any_key( { "ModelHyfydy", "ModelHfd" } ) )
			error( "Error creating scenario",
				"This scenario uses a <b>Hyfydy model</b>, but no active license key was found.<br><br>"
				"Please check Tools -> Preferences -> Hyfydy, or visit <a href = 'https://hyfydy.com'>hyfydy.com</a> for more information." );
		else error( "Error creating scenario", e.what() );
		scenario_.reset();
		return false;
	}
	catch ( std::exception& e )
	{
		error( "Error creating scenario", e.what() );
		scenario_.reset();
		return false;
	}

	// always do this, also in case of error
	ui.osgViewer->repaint();

	return scenario_->IsValid();
}

std::vector<QCodeEditor*> SconeStudio::changedDocuments()
{
	std::vector< QCodeEditor* > modified_docs;
	for ( auto s : codeEditors )
		if ( s->document()->isModified() )
			modified_docs.push_back( s );
	return modified_docs;
}

bool SconeStudio::requestSaveChanges( const std::vector<QCodeEditor*>& modified_docs )
{
	if ( !modified_docs.empty() )
	{
		QString message = "The following files have been modified:\n";
		for ( auto s : modified_docs )
			message += "\n" + s->getTitle();

		if ( QMessageBox::warning( this, "Save Changes", message, QMessageBox::Save, QMessageBox::Cancel ) == QMessageBox::Save )
		{
			for ( auto s : modified_docs )
			{
				s->save();
				ui.tabWidget->setTabText( getTabIndex( s ), s->getTitle() );
			}
			return true;
		}
		else return false;
	}
	else return true;
}

bool SconeStudio::requestSaveChanges( QCodeEditor* s )
{
	if ( s && s->document()->isModified() )
	{
		QString message = "Save changes to " + s->getTitle() + "?";
		if ( QMessageBox::warning( this, "Save Changes", message, QMessageBox::Save, QMessageBox::Discard ) == QMessageBox::Save )
			s->save();
		ui.tabWidget->setTabText( getTabIndex( s ), s->getTitle() );
		return true;
	}
	else return false;
}

int SconeStudio::getTabIndex( QCodeEditor* s )
{
	for ( int idx = 0; idx < ui.tabWidget->count(); ++idx )
		if ( ui.tabWidget->widget( idx ) == (QWidget*)s )
			return idx;
	return -1;
}

QCodeEditor* SconeStudio::getActiveCodeEditor()
{
	for ( auto s : codeEditors )
		if ( !s->visibleRegion().isEmpty() )
			return s;
	return nullptr;
}

QCodeEditor* SconeStudio::getActiveScenario()
{
	QCodeEditor* first_scenario = nullptr;
	for ( auto s : codeEditors )
	{
		auto ext = path_from_qt( s->fileName ).extension_no_dot().str();
		if ( ext == "scone" )
		{
			if ( !s->visibleRegion().isEmpty() && !s->document()->find( "Optimizer" ).isNull() )
				return s; // active scone file
			else if ( first_scenario == nullptr )
				first_scenario = s; // could be single .scone file
		}
	}
	return first_scenario; // either first .scone file, or none
}

bool SconeStudio::createAndVerifyActiveScenario( bool always_create )
{
	if ( auto* s = getActiveScenario() )
	{
		auto changed_docs = changedDocuments();
		if ( !requestSaveChanges( changed_docs ) )
			return false;

		if ( scenario_
			&& scenario_->GetScenarioFileName() == s->fileName
			&& ( scenario_->IsEvaluating() && scenario_->GetTime() == 0.0 )
			&& changed_docs.empty()
			&& !always_create )
			return true; // we already have a scenario

		if ( createScenario( s->fileName ) )
		{
			s->setFocus();
			if ( LogUnusedProperties( scenario_->GetScenarioProps() ) )
			{
				QString message = "Invalid scenario settings in " + scenario_->GetScenarioFileName() + ":\n\n";
				message += to_qt( to_str_unaccessed( scenario_->GetScenarioProps() ) );
				if ( QMessageBox::warning( this, "Invalid scenario settings", message, QMessageBox::Ignore, QMessageBox::Cancel ) == QMessageBox::Cancel )
					return false; // user pressed cancel
			}
			return true; // everything loaded ok or invalid settings ignored
		}
		else return false; // failed to create scenario
	}
	else
	{
		information( "No Scenario Selected", "Please select a .scone file" );
		return false;
	}
}

void SconeStudio::updateModelDataWidgets()
{
	SCONE_ASSERT( scenario_ && scenario_->HasData() );
	analysisStorageModel.setStorage( &scenario_->GetData() );
	analysisView->reset();
	ui.playControl->setRange( 0, scenario_->GetMaxTime() );
	if ( !gaitAnalysisDock->visibleRegion().isEmpty() )
		updateGaitAnalysis(); // automatic gait analysis if visible
}

void SconeStudio::optimizeScenario()
{
	try
	{
		if ( createAndVerifyActiveScenario( true ) )
		{
			auto task = scone::createOptimizerTask( scenario_->GetScenarioFileName() );
			addProgressDock( new ProgressDockWidget( this, std::move( task ) ) );
			updateOptimizations();
		}

	}
	catch ( const std::exception& e )
	{
		error( "Error optimizing " + scenario_->GetScenarioFileName(), e.what() );
	}
}

void SconeStudio::optimizeScenarioMultiple()
{
	try
	{
		if ( createAndVerifyActiveScenario( true ) )
		{
			bool ok = true;
			int count = QInputDialog::getInt( this, "Run Multiple Optimizations", "Enter number of optimization instances: ", 3, 1, 100, 1, &ok );
			if ( ok )
			{
				for ( int i = 1; i <= count; ++i )
				{
					QStringList args( QString().sprintf( "#1.random_seed=%d", i ) );
					auto task = createOptimizerTask( scenario_->GetScenarioFileName(), args );
					addProgressDock( new ProgressDockWidget( this, std::move( task ) ) );
					QApplication::processEvents(); // needed for the ProgressDockWidgets to be evenly sized
				}
				updateOptimizations();
			}
		}
	}
	catch ( const std::exception& e )
	{
		error( "Error optimizing " + scenario_->GetScenarioFileName(), e.what() );
	}
}

void SconeStudio::evaluateActiveScenario()
{
	if ( createAndVerifyActiveScenario( false ) )
	{
		if ( scenario_->IsEvaluating() )
			evaluate();
		ui.playControl->play();
	}
}

void SconeStudio::performanceTest( bool write_stats )
{
	if ( !currentFilename.isEmpty() && createScenario( currentFilename ) )
	{
		auto filename = xo::path( currentFilename.toStdString() );
		bool is_par_file = filename.extension_no_dot() == "par";
		if ( auto* mo = scenario_->TryGetModelObjective() )
		{
			auto& inf = mo->info();
			auto par = is_par_file ? SearchPoint( inf, filename ) : SearchPoint( inf );

			if ( !write_stats )
			{
				xo::timer real_time;
				auto model = mo->CreateModelFromParams( par );
				model->SetStoreData( false );
				model->AdvanceSimulationTo( model->GetSimulationEndTime() );
				auto real_dur = real_time().secondsd();
				auto sim_time = model->GetTime();
				if ( model->GetProfiler().enabled() )
					model->GetProfiler().log_results();
				log::info( "fitness = ", mo->GetResult( *model ) );
				if ( auto sim_report = model->GetSimulationReport(); !sim_report.empty() )
					log::info( sim_report );
				log::info( "Evaluation took ", real_dur, "s for ", sim_time, "s (", sim_time / real_dur, "x real-time)" );
			}
			else
			{
				auto f = scenario_->GetFileName();
				scone::BenchmarkScenario( scenario_->GetScenarioProps(), f, f.parent_path() / "perf", 8 );
			}
		}
	}
}

bool SconeStudio::abortOptimizations()
{
	if ( optimizations.size() > 0 )
	{
		bool showWarning = false;
		QString message = QString().sprintf( "Are you sure you want to abort the following optimizations:\n\n" );
		for ( auto& o : optimizations )
		{
			showWarning |= !o->canCloseWithoutWarning();
			message += o->getIdentifier() + "\n";
		}

		if ( !showWarning || QMessageBox::warning( this, "Abort Optimizations", message, QMessageBox::Abort, QMessageBox::Cancel ) == QMessageBox::Abort )
		{
			for ( const auto& o : optimizations )
			{
				o->disableCloseWarning();
				o->close();
			}
			return true;
		}
		else return false;
	}
	else return true;
}

void SconeStudio::updateBackgroundTimer()
{
	if ( !ui.playControl->isPlaying() )
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
			QString title = "Error optimizing " + o->task_->scenario_file_;
			QString msg = o->message.c_str();
			o->close();
			QMessageBox::critical( this, title, msg );
			return; // must return here because close invalidates the iterator
		}
	}
}

void SconeStudio::tabCloseRequested( int idx )
{
	auto it = xo::find( codeEditors, (QCodeEditor*)ui.tabWidget->widget( idx ) );
	SCONE_THROW_IF( it == codeEditors.end(), "Could not find scenario for tab " + to_str( idx ) );

	requestSaveChanges( *it );
	codeEditors.erase( it );
	ui.tabWidget->removeTab( idx );
}

scone::ViewOptions SconeStudio::getViewOptionsFromMenu() const
{
	ViewOptions f;
	for ( auto& va : viewActions )
		f.set( va.first, va.second->isChecked() );
	return f;
}

void SconeStudio::applyViewOptions()
{
	if ( scenario_ )
	{
		scenario_->ApplyViewOptions( getViewOptionsFromMenu() );
		ui.osgViewer->repaint();
	}
}

void SconeStudio::showSettingsDialog()
{
	if ( ShowPreferencesDialog( this ) == QDialog::Accepted )
		gaitAnalysis->reset();
}

void SconeStudio::updateTabTitles()
{
	for ( auto s : codeEditors )
		ui.tabWidget->setTabText( getTabIndex( s ), s->getTitle() );
}

void SconeStudio::resetWindowLayout()
{
	if ( question( "Reset Window Layout", "Are you sure you want to reset the window layout? This action cannot be undone." ) )
	{
		GetStudioSettings().set( "ui.reset_layout", true );
		GetStudioSettings().save();
		information( "Reset Window Layout", "Please restart SCONE for the changes to take effect" );
	}
}

void SconeStudio::fixViewerWindowSize()
{
	if ( ui.viewerDock->isFloating() )
	{
		auto borderSize = ui.viewerDock->size() - ui.osgViewer->size();
		auto videoSize = QSize( GetStudioSettings().get<int>( "video.width" ), GetStudioSettings().get<int>( "video.height" ) );
		ui.viewerDock->resize( borderSize + videoSize + QSize( 2, 2 ) );
	}
}

void SconeStudio::exportCoordinates()
{
	if ( scenario_ && scenario_->HasModel() )
	{
		PropNode pn;
		for ( const auto& dof : scenario_->GetModel().GetDofs() )
		{
			pn[ "values" ].set( dof->GetName(), dof->GetPos() );
			pn[ "velocities" ].set( dof->GetName(), dof->GetVel() );
		}
		auto filename = QFileDialog::getSaveFileName( this, "State Filename", to_qt( scenario_->GetFileName().parent_path() ), "zml files (*.zml)" );
		if ( !filename.isEmpty() )
			xo::save_file( pn, path_from_qt( filename ) );
	}
}

void SconeStudio::createVideo()
{
	if ( !scenario_ )
		return error( "No Scenario", "There is no scenario open" );

	if ( auto p = GetStudioSetting<path>( "video.path_to_ffmpeg" ); !xo::file_exists( p ) )
		return error( "Could not find ffmpeg", to_qt( "Could not find " + p.str() ) );

	fixViewerWindowSize();

	captureFilename = QFileDialog::getSaveFileName( this, "Video Filename", QString(), "mp4 files (*.mp4);;avi files (*.avi);;mov files (*.mov)" );
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
	for ( double t = 0.0; t <= scenario_->GetMaxTime(); t += step_size )
	{
		setTime( t, true );
		ui.progressBar->setValue( int( t / scenario_->GetMaxTime() * 100 ) );
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
		ui.osgViewer->captureCurrentFrame( xo::path( filename.toStdString() ).replace_extension( "" ).str() );
}

void SconeStudio::finalizeCapture()
{
	ui.osgViewer->stopCapture();

	const auto ffmpeg = GetStudioSetting<path>( "video.path_to_ffmpeg" );
	if ( !xo::file_exists( ffmpeg ) )
		return error( "Could not find ffmpeg", to_qt( "Could not find " + ffmpeg.str() ) );

	QString program = to_qt( ffmpeg );
	QStringList args;
	args << "-r" << to_qt( GetStudioSettings().get<string>( "video.frame_rate" ) )
		<< "-i" << captureFilename + ".images/image_0_%d.png"
		<< "-c:v" << "mpeg4"
		<< "-q:v" << to_qt( GetStudioSettings().get<string>( "video.quality" ) )
		<< captureFilename;

	std::cout << "starting " << program.toStdString() << endl;
	auto v = args.toVector();
	for ( auto arg : v )
		std::cout << arg.toStdString() << endl;

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

void SconeStudio::deleteSelectedFileOrFolder()
{
	auto msgTitle = tr( "Remove files or folders" );
	auto selection = ui.resultsBrowser->selectionModel()->selectedRows();
	if ( selection.empty() )
		return information( msgTitle, tr( "No files or folders selected" ) );

	QStringList fileList;
	for ( const auto& idx : selection )
		fileList.push_back( ui.resultsBrowser->fileSystemModel()->fileInfo( idx ).filePath() );

#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
	// Qt >= 15.5? Move items to thrash, display warning with default Ok
	auto msgBody = tr( "The following item(s) will be moved to the recycle bin:\n\n" ) + fileList.join( '\n' );
	if ( QMessageBox::warning( nullptr, msgTitle, msgBody, QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok ) == QMessageBox::Cancel )
		return;

	xo::timer t;
	if ( moveFilesToTrash( fileList ) )
		log::info( "Recycled ", fileList.size(), " files in ", t().secondsd(), " seconds" );
#else
	// Qt < 15.5? Remove items, display warning with default Cancel
	auto msgBody = tr( "The following item(s) will be permanently deleted:\n\n" ) + fileList.join( '\n' );
	msgBody += tr( "\nWARNING: this cannot be recovered!" );
	if ( QMessageBox::Cancel == QMessageBox::warning( this, msgTitle, msgBody, QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel ) )
		return;

	for ( const auto& idx : selection )
	{
		bool success = false;
		auto item = ui.resultsBrowser->fileSystemModel()->fileInfo( idx );
		if ( item.isDir() )
			success = QDir( item.filePath() ).removeRecursively();
		else if ( item.isFile() )
			success = ui.resultsBrowser->fileSystemModel()->remove( idx );
		if ( !success )
			warning( msgTitle, tr( "Could not remove " ) + item.filePath() );
		}
#endif
}

void SconeStudio::sortResultsByDate()
{
	ui.resultsBrowser->fileSystemModel()->sort( 3 );
}

void SconeStudio::sortResultsByName()
{
	ui.resultsBrowser->fileSystemModel()->sort( 0 );
}

void SconeStudio::onResultBrowserCustomContextMenu(const QPoint &pos)
{
	QMenu menu;
	menu.addAction( "Sort by &Name", this, &SconeStudio::sortResultsByName );
	menu.addAction( "Sort by &Date", this, &SconeStudio::sortResultsByDate );
	menu.addSeparator();
	menu.addAction( "&Remove", this, &SconeStudio::deleteSelectedFileOrFolder );
	menu.exec(ui.resultsBrowser->mapToGlobal(pos));
}
