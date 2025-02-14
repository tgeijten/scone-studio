/*
** SconeStudio.cpp
**
** Copyright (C) Thomas Geijtenbeek and contributors. All rights reserved.
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
#include "xo/thread/thread_priority.h"

#include "scone/core/Benchmark.h"
#include "scone/core/Factories.h"
#include "scone/core/Log.h"
#include "scone/core/ModelConverter.h"
#include "scone/core/profiler_config.h"
#include "scone/core/Settings.h"
#include "scone/core/StorageIo.h"
#include "scone/core/storage_tools.h"
#include "scone/core/system_tools.h"
#include "scone/core/version.h"
#include "scone/model/Dof.h"
#include "scone/model/muscle_tools.h"
#include "scone/model/Muscle.h"
#include "scone/model/model_tools.h"
#include "scone/optimization/Optimizer.h"
#include "scone/optimization/opt_tools.h"
#include "scone/sconelib_config.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QTabWidget>
#include <QTextStream>
#include <osgDB/ReadFile>

#include "qcustomplot/qcustomplot.h"
#include "qt_convert.h"
#include "qtfx.h"
#include "gui_profiler.h"

#include "vis-osg/osg_object_manager.h"
#include "vis-osg/osg_tools.h"
#include "vis/plane.h"
#include "help_tools.h"
#include "file_tools.h"
#include "model_conversion.h"
#include "studio_tools.h"
#include "xo/time/interval_checker.h"
#include "external_tools.h"

using namespace scone;
using namespace xo::time_literals;

SconeStudio::SconeStudio( QWidget* parent, Qt::WindowFlags flags ) :
	QCompositeMainWindow( parent, flags ),
	close_all( false ),
	current_time(),
	evaluation_time_step( 1.0 / 8 ),
	max_real_time_evaluation_step_( 0.05 ),
	real_time_evaluation_enabled_( false ),
	scene_( true, GetStudioSetting< float >( "viewer.ambient_intensity" ) ),
	slomo_factor( 1 ),
	com_delta( Vec3( 0, 0, 0 ) ),
	drag_distance_( 0 ),
	captureProcess( nullptr )
{
	scone::TimeSection( "CreateWindow" );
	ui.setupUi( this );
	scone::TimeSection( "SetupGui" );

	//
	// Top-level menu items (must be created first because of Window menu)
	//

	auto fileMenu = menuBar()->addMenu( ( "&File" ) );
	auto editMenu = menuBar()->addMenu( "&Edit" );
	auto viewMenu = menuBar()->addMenu( "&View" );
	auto scenarioMenu = menuBar()->addMenu( "&Scenario" );
	auto toolsMenu = menuBar()->addMenu( "&Tools" );
	auto* actionMenu = menuBar()->addMenu( "&Playback" );
	auto windowMenu = createWindowMenu();
	auto helpMenu = menuBar()->addMenu( ( "&Help" ) );

	//
	// Components
	//

	// Analysis (must be done BEFORE menu, analysisView is used there)
	analysisView = new QDataAnalysisView( analysisStorageModel, this );
	analysisView->setObjectName( "Analysis" );
	analysisView->setAutoFitVerticalAxis( scone::GetStudioSettings().get<bool>( "analysis.auto_fit_vertical_axis" ) );
	analysisView->setLineWidth( scone::GetStudioSettings().get<float>( "analysis.line_width" ) );
	scone::TimeSection( "InitAnalysys" );

	// Results Browser
	auto results_folder = scone::GetFolder( SconeFolder::Results );
	xo::create_directories( results_folder );
	resultsModel = new ResultsFileSystemModel( nullptr );
	ui.resultsBrowser->setModel( resultsModel );
	ui.resultsBrowser->setNumColumns( 1 );
	ui.resultsBrowser->setRoot( to_qt( results_folder ), "*.par;*.sto;*.scone;step_*.pt" );
	ui.resultsBrowser->header()->setFrameStyle( QFrame::NoFrame | QFrame::Plain );
	ui.resultsBrowser->setSelectionMode( QAbstractItemView::ExtendedSelection );
	ui.resultsBrowser->setSelectionBehavior( QAbstractItemView::SelectRows );
	ui.resultsBrowser->setContextMenuPolicy( Qt::CustomContextMenu );
	connect( ui.resultsBrowser, SIGNAL( customContextMenuRequested( const QPoint& ) ), this, SLOT( onResultBrowserCustomContextMenu( const QPoint& ) ) );
	connect( ui.resultsBrowser->selectionModel(),
		SIGNAL( currentChanged( const QModelIndex&, const QModelIndex& ) ),
		this, SLOT( selectBrowserItem( const QModelIndex&, const QModelIndex& ) ) );
	scone::TimeSection( "InitResultsBrowser" );

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
	registerDockWidget( ui.resultsDock, "&Optimization Results", ui.resultsBrowser, QKeySequence( "Ctrl+Shift+U" ) );

	addDockWidget( Qt::BottomDockWidgetArea, ui.messagesDock );
	registerDockWidget( ui.messagesDock, "Me&ssages" );

	analysisDock = createDockWidget( "&Analysis", analysisView, Qt::BottomDockWidgetArea, analysisView->filterWidget(), QKeySequence( "Ctrl+L" ) );
	tabifyDockWidget( ui.messagesDock, analysisDock );

	scone::TimeSection( "InitDocking" );

	// evaluation report
	reportModel = new QPropNodeItemModel();
	reportModel->setDefaultIcon( style()->standardIcon( QStyle::SP_FileIcon ) );
	reportView = new QTreeView( this );
	reportView->setIndentation( 16 );
	reportView->setModel( reportModel );
	reportView->setEditTriggers( QAbstractItemView::NoEditTriggers );
	reportView->header()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
	reportDock = createDockWidget( "Evaluation &Report", reportView, Qt::BottomDockWidgetArea );
	splitDockWidget( ui.messagesDock, reportDock, Qt::Horizontal );
	reportDock->hide();
	scone::TimeSection( "InitEvaluationReport" );

	// gait analysis
	gaitAnalysis = new GaitAnalysis( this );
	gaitAnalysisDock = createDockWidget( "&Gait Analysis", gaitAnalysis, Qt::BottomDockWidgetArea );
	tabifyDockWidget( ui.messagesDock, gaitAnalysisDock );
	gaitAnalysisDock->hide();
	scone::TimeSection( "InitGaitAnalysis" );

	// parameter view
	parView = new QTableView( this );
	parModel = new ParTableModel();
	parView->setModel( parModel );
	parView->setEditTriggers( QAbstractItemView::NoEditTriggers );
	for ( int i = 0; i < parView->horizontalHeader()->count(); ++i )
		parView->horizontalHeader()->setSectionResizeMode( i, i == 0 ? QHeaderView::Stretch : QHeaderView::ResizeToContents );
	parView->verticalHeader()->setSectionResizeMode( QHeaderView::Fixed );
	parView->verticalHeader()->setDefaultSectionSize( 24 );
	parViewDock = createDockWidget( "&Parameters", parView, Qt::RightDockWidgetArea );
	splitDockWidget( ui.viewerDock, parViewDock, Qt::Horizontal );
	scone::TimeSection( "InitParameterView" );

	// dof editor
	dofEditor = new DofEditorGroup( this );
	connect( dofEditor, &DofEditorGroup::valueChanged, this, &SconeStudio::dofEditorValueChanged );
	connect( dofEditor, &DofEditorGroup::exportCoordinates, this, &SconeStudio::exportCoordinates );
	connect( dofEditor, &DofEditorGroup::resetCoordinates, this, &SconeStudio::resetCoordinates );
	dofDock = createDockWidget( "&Coordinates", dofEditor, Qt::RightDockWidgetArea );
	tabifyDockWidget( parViewDock, dofDock );
	scone::TimeSection( "InitDofEditor" );

	// model inspector
	inspectorModel = new QPropNodeItemModel();
	inspectorModel->setMaxPreviewChildren( 3 );
	inspectorView = new QTreeView( this );
	inspectorView->setIndentation( 16 );
	inspectorView->setModel( inspectorModel );
	inspectorView->setEditTriggers( QAbstractItemView::NoEditTriggers );
	inspectorView->header()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );

	inspectorDetails = new QTreeView( this );
	inspectorDetails->setModel( inspectorModel );
	inspectorDetails->setEditTriggers( QAbstractItemView::NoEditTriggers );
	inspectorDetails->header()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );

	inspectorSplitter = new QSplitter( Qt::Vertical, this );
	inspectorSplitter->addWidget( inspectorView );
	inspectorSplitter->addWidget( inspectorDetails );
	inspectorSplitter->setStretchFactor( 0, 3 );
	inspectorSplitter->setStretchFactor( 1, 1 );
	connect( inspectorView->selectionModel(), &QItemSelectionModel::currentChanged, inspectorDetails, &QTreeView::setRootIndex );

	inspectorDock = createDockWidget( "&Model Info", inspectorSplitter, Qt::RightDockWidgetArea );
	tabifyDockWidget( dofDock, inspectorDock );
	parViewDock->raise();
	scone::TimeSection( "InitModelInspector" );

#if SCONE_EXPERIMENTAL_FEATURES_ENABLED
	// model input editor
	userInputEditor = new UserInputEditor( this );
	connect( userInputEditor, &UserInputEditor::valueChanged, this, &SconeStudio::userInputValueChanged );
	connect( userInputEditor, &UserInputEditor::savePressed, [this]() { saveUserInputs( true ); } );
	auto userInputDock = createDockWidget( "Model &Inputs", userInputEditor, Qt::LeftDockWidgetArea );
	tabifyDockWidget( ui.resultsDock, userInputDock );
	userInputDock->hide();
#endif

	// optimization history
	optimizationHistoryView = new QDataAnalysisView( optimizationHistoryStorageModel, this );
	optimizationHistoryView->setObjectName( "Optimization History" );
	optimizationHistoryView->setAutoFitVerticalAxis( scone::GetStudioSettings().get<bool>( "analysis.auto_fit_vertical_axis" ) );
	optimizationHistoryView->setLineWidth( scone::GetStudioSettings().get<float>( "analysis.line_width" ) );
	optimizationHistoryDock = createDockWidget( "&Optimization History", optimizationHistoryView, Qt::BottomDockWidgetArea );
	tabifyDockWidget( ui.messagesDock, optimizationHistoryDock );
	optimizationHistoryDock->hide();

	// Muscle plot
	muscleAnalysis = new MuscleAnalysis( this );
	muscleAnalysisDock = createDockWidget( "Muscle Ana&lysis", muscleAnalysis, Qt::BottomDockWidgetArea );
	tabifyDockWidget( ui.messagesDock, muscleAnalysisDock );
	muscleAnalysisDock->hide();
	connect( muscleAnalysis, &MuscleAnalysis::dofChanged, this,
		[this]( const QString& d ) { if ( hasModel() ) muscleAnalysis->setDof( scenario_->GetModel(), d ); } );
	connect( muscleAnalysis, &MuscleAnalysis::dofValueChanged, this, &SconeStudio::muscleAnalysisValueChanged );

	//
	// Menu
	//

	// File menu
	fileMenu->addAction( "&Open...", this, &SconeStudio::fileOpenTriggered, QKeySequence( "Ctrl+O" ) );
	recentFilesMenu = addMenuAction( fileMenu, "Open &Recent", this, &QCompositeMainWindow::fileOpenTriggered, QKeySequence() );
	fileMenu->addAction( "Re&load", this, &SconeStudio::fileReloadTriggered, QKeySequence( "Ctrl+R" ) );
	fileMenu->addSeparator();
	fileMenu->addAction( "&Save", this, &SconeStudio::fileSaveTriggered, QKeySequence( "Ctrl+S" ) );
	fileMenu->addAction( "Save &As...", this, &SconeStudio::fileSaveAsTriggered, QKeySequence( "Ctrl+Shift+S" ) );
	fileMenu->addAction( "&Close", this, &SconeStudio::fileCloseTriggered, QKeySequence( "Ctrl+W" ) );
	fileMenu->addSeparator();
	fileMenu->addAction( "Save Evaluation &Data", this, &SconeStudio::writeEvaluationResults, QKeySequence( "Ctrl+Shift+E" ) );
	fileMenu->addSeparator();
	fileMenu->addAction( "&Export Model Coordinates...", this, &SconeStudio::exportCoordinates );
#if SCONE_EXPERIMENTAL_FEATURES_ENABLED
	fileMenu->addAction( "Save &Model Inputs", [this]() { saveUserInputs( false ); } );
	fileMenu->addAction( "Save &Model Inputs As...", [this]() { saveUserInputs( true ); } );
#endif
	fileMenu->addSeparator();
	fileMenu->addAction( "E&xit", this, &SconeStudio::fileExitTriggered, QKeySequence( "Alt+X" ) );

	// Edit menu
	editMenu->addAction( "&Find...", this, &SconeStudio::findDialog, QKeySequence( "Ctrl+F" ) );
	editMenu->addAction( "Find &Next", this, &SconeStudio::findNext, Qt::Key_F3 );
	editMenu->addAction( "Find &Previous", this, &SconeStudio::findPrevious, QKeySequence( "Shift+F3" ) );
	editMenu->addSeparator();
	editMenu->addAction( "P&revious Tab", [this]() { cycleTabWidget( ui.tabWidget, -1 ); }, QKeySequence( "Ctrl+PgUp" ) );
	editMenu->addAction( "Ne&xt Tab", [this]() { cycleTabWidget( ui.tabWidget, 1 ); }, QKeySequence( "Ctrl+PgDown" ) );
	editMenu->addSeparator();
	editMenu->addAction( "Toggle &Comments", this, &SconeStudio::toggleComments, QKeySequence( "Ctrl+/" ) );
	editMenu->addAction( "&Duplicate Selection", [this]() { if ( auto* e = getActiveCodeEditor() ) e->duplicateText(); }, QKeySequence( "Ctrl+U" ) );

	// View menu
	viewActions[ViewOption::ExternalForces] = viewMenu->addAction( "Show External &Forces", this, &SconeStudio::applyViewOptions, QKeySequence( "Alt+Shift+F" ) );
	viewActions[ViewOption::Muscles] = viewMenu->addAction( "Show &Muscles", this, &SconeStudio::applyViewOptions, QKeySequence( "Alt+Shift+M" ) );
	viewActions[ViewOption::Tendons] = viewMenu->addAction( "Show &Tendons", this, &SconeStudio::applyViewOptions, QKeySequence( "Alt+Shift+T" ) );
	viewActions[ViewOption::BodyGeom] = viewMenu->addAction( "Show &Body Geometry", this, &SconeStudio::applyViewOptions, QKeySequence( "Alt+Shift+B" ) );
	viewActions[ViewOption::BodyAxes] = viewMenu->addAction( "Show Body A&xes", this, &SconeStudio::applyViewOptions, QKeySequence( "Alt+Shift+A" ) );
	viewActions[ViewOption::BodyCom] = viewMenu->addAction( "Show Body Ce&nter of Mass", this, &SconeStudio::applyViewOptions, QKeySequence( "Alt+Shift+N" ) );
	viewActions[ViewOption::Joints] = viewMenu->addAction( "Show &Joints", this, &SconeStudio::applyViewOptions, QKeySequence( "Alt+Shift+J" ) );
	viewActions[ViewOption::JointReactionForces] = viewMenu->addAction( "Show Joint &Reaction Forces", this, &SconeStudio::applyViewOptions, QKeySequence( "Alt+Shift+R" ) );
	viewActions[ViewOption::ContactGeom] = viewMenu->addAction( "Show &Contact Geometry", this, &SconeStudio::applyViewOptions, QKeySequence( "Alt+Shift+C" ) );
	viewActions[ViewOption::AuxiliaryGeom] = viewMenu->addAction( "Show Au&xiliary Geometry", this, &SconeStudio::applyViewOptions, QKeySequence( "Alt+Shift+X" ) );
	viewActions[ViewOption::GroundPlane] = viewMenu->addAction( "Show &Ground Plane", this, &SconeStudio::applyViewOptions, QKeySequence( "Alt+Shift+G" ) );
	viewActions[ViewOption::ModelComHeading] = viewMenu->addAction( "Show Model COM and &Heading", this, &SconeStudio::applyViewOptions, QKeySequence( "Alt+Shift+H" ) );
	viewMenu->addSeparator();
	auto musColMenu = viewMenu->addMenu( "Muscle Co&lor" );
	auto musColGroup = new QActionGroup( this );
	musColGroup->addAction( viewActions[ViewOption::MuscleActivation] = musColMenu->addAction( "Muscle Color &Activation", this, &SconeStudio::applyViewOptions ) );
	musColGroup->addAction( viewActions[ViewOption::MuscleForce] = musColMenu->addAction( "Muscle Color F&orce", this, &SconeStudio::applyViewOptions ) );
	musColGroup->addAction( viewActions[ViewOption::MuscleFiberLength] = musColMenu->addAction( "Muscle Color Fiber &Length", this, &SconeStudio::applyViewOptions ) );
	musColGroup->setExclusive( true );
	auto musLineMenu = viewMenu->addMenu( "Muscle &Width" );
	auto musLineGroup = new QActionGroup( this );
	musLineGroup->addAction( viewActions[ViewOption::MuscleRadiusFixed] = musLineMenu->addAction( "&Fixed", this, &SconeStudio::applyViewOptions ) );
	musLineGroup->addAction( viewActions[ViewOption::MuscleRadiusPcsa] = musLineMenu->addAction( "&PCSA", this, &SconeStudio::applyViewOptions ) );
	musLineGroup->addAction( viewActions[ViewOption::MuscleRadiusPcsaDynamic] = musLineMenu->addAction( "&Dynamic PCSA", this, &SconeStudio::applyViewOptions ) );
	musLineGroup->setExclusive( true );
	viewMenu->addSeparator();
	auto orbitMenu = viewMenu->addMenu( "Camera &Orbit" );
	viewActions[ViewOption::StaticCamera] = viewMenu->addAction( "&Static Camera", this, &SconeStudio::applyViewOptions, QKeySequence( "Alt+Shift+S" ) );

	// init view options
	auto defaultOptions = MakeDefaultViewOptions();
	for ( auto& va : viewActions ) {
		va.second->setCheckable( true );
		va.second->setChecked( defaultOptions.get( va.first ) );
	}

	// init orbit menu
	auto og = new QActionGroup( this );
	og->addAction( orbitMenu->addAction( "None", [&]() { setOrbitVelocity( 0.0f ); } ) );
	orbitMenu->addSeparator();
	og->addAction( orbitMenu->addAction( "Left - Slow", [&]() { setOrbitVelocity( -0.5f ); } ) );
	og->addAction( orbitMenu->addAction( "Left - Medium", [&]() { setOrbitVelocity( -1.0f ); } ) );
	og->addAction( orbitMenu->addAction( "Left - Fast", [&]() { setOrbitVelocity( -2.0f ); } ) );
	orbitMenu->addSeparator();
	og->addAction( orbitMenu->addAction( "Right - Slow", [&]() { setOrbitVelocity( 0.5f ); } ) );
	og->addAction( orbitMenu->addAction( "Right - Medium", [&]() { setOrbitVelocity( 1.0f ); } ) );
	og->addAction( orbitMenu->addAction( "Right - Fast", [&]() { setOrbitVelocity( 2.0f ); } ) );
	og->setExclusive( true );
	for ( auto& a : og->actions() )
		a->setCheckable( true );
	og->actions().first()->setChecked( true );

	// Scenario menu
	scenarioMenu->addAction( "&Evaluate Scenario", [this]() { evaluateActiveScenario(); }, QKeySequence( "Ctrl+E" ) );
	scenarioMenu->addSeparator();
	scenarioMenu->addAction( "&Optimize Scenario", this, &SconeStudio::optimizeScenario, QKeySequence( "Ctrl+F5" ) );
	scenarioMenu->addAction( "Run &Multiple Optimizations", this, &SconeStudio::optimizeScenarioMultiple, QKeySequence( "Ctrl+Shift+F5" ) );
	scenarioMenu->addSeparator();
	scenarioMenu->addAction( "&Abort Optimizations", this, &SconeStudio::abortOptimizations, QKeySequence() );
	scenarioMenu->addSeparator();
	scenarioMenu->addAction( "&Performance Test", [this]() { performanceTest( false ); }, QKeySequence( "Ctrl+P" ) );
	scenarioMenu->addAction( "Performance Test (Write Stats)", [this]() { performanceTest( true ); }, QKeySequence( "Ctrl+Shift+P" ) );

	// Tools menu
	toolsMenu->addAction( "Generate &Video...", this, &SconeStudio::createVideo );
	toolsMenu->addAction( "Save &Image...", this, &SconeStudio::captureImage );
	toolsMenu->addSeparator();
	toolsMenu->addAction( "&Gait Analysis", this, &SconeStudio::updateGaitAnalysis, QKeySequence( "Ctrl+G" ) );
	toolsMenu->addAction( "Clear Analysis Fi&lter", [this]() { analysisView->setFilterText( "" ); analysisView->selectNone();
	analysisDock->raise(); analysisView->focusFilterEdit(); }, QKeySequence( "Ctrl+Shift+L" ) );
	toolsMenu->addAction( "&Keep Current Analysis Graphs", analysisView, &QDataAnalysisView::holdSeries, QKeySequence( "Ctrl+Shift+K" ) );
	toolsMenu->addAction( "Refresh Muscle Analysis", muscleAnalysis, &MuscleAnalysis::refresh, QKeySequence( "Ctrl+Shift+M" ) );
	toolsMenu->addSeparator();
#if SCONE_HYFYDY_ENABLED
	toolsMenu->addAction( "&Convert to Hyfydy...", this, &SconeStudio::convertScenario );
	toolsMenu->addAction( "Convert OpenSim3 &Model...", [this]() { ShowModelConversionDialog( this ); } );
	toolsMenu->addSeparator();
#endif
	toolsMenu->addAction( "&Preferences...", this, &SconeStudio::showSettingsDialog, QKeySequence( "Ctrl+," ) );

	// Action menu
	actionMenu->addAction( "&Play or Evaluate", ui.playControl, &QPlayControl::togglePlay, Qt::Key_F5 );
	actionMenu->addAction( "&Stop / Reset", ui.playControl, &QPlayControl::stopReset, Qt::Key_F8 );
	actionMenu->addAction( "Toggle Play", ui.playControl, &QPlayControl::togglePlay, QKeySequence( "Ctrl+Space" ) );
	actionMenu->addAction( "Toggle Loop", ui.playControl, &QPlayControl::toggleLoop, QKeySequence( "Ctrl+Alt+L" ) );
	actionMenu->addAction( "Play F&aster", ui.playControl, &QPlayControl::faster, QKeySequence( "Ctrl+Up" ) );
	actionMenu->addAction( "Play S&lower", ui.playControl, &QPlayControl::slower, QKeySequence( "Ctrl+Down" ) );
	actionMenu->addSeparator();
	actionMenu->addAction( "Step &Back", ui.playControl, &QPlayControl::stepBack, QKeySequence( "Alt+Left" ) );
	actionMenu->addAction( "Step &Forward", ui.playControl, &QPlayControl::stepForward, QKeySequence( "Alt+Right" ) );
	actionMenu->addAction( "Page &Back", ui.playControl, &QPlayControl::pageBack, QKeySequence( "Alt+PgUp" ) );
	actionMenu->addAction( "Page &Forward", ui.playControl, &QPlayControl::pageForward, QKeySequence( "Alt+PgDown" ) );
	actionMenu->addAction( "Goto &Begin", ui.playControl, &QPlayControl::reset, QKeySequence( "Alt+Home" ) );
	actionMenu->addAction( "Go to &End", ui.playControl, &QPlayControl::end, QKeySequence( "Alt+End" ) );

	// Help menu
	helpMenu->addAction( "View &Help...", this, &SconeStudio::helpSearch, QKeySequence( "F1" ) );
	helpMenu->addAction( "Online &Documentation...", this, []() { QDesktopServices::openUrl( GetWebsiteUrl() ); } );
	helpMenu->addAction( "Check for &Updates...", this, []() { QDesktopServices::openUrl( GetDownloadUrl() ); } );
	helpMenu->addAction( "User &Forum...", this, []() { QDesktopServices::openUrl( GetForumUrl() ); } );
	helpMenu->addAction( "Repair &Tutorials and Examples...", this, []() { updateTutorialsExamples(); } );
	helpMenu->addSeparator();
	helpMenu->addAction( "&About...", [this]() { showAbout( this ); } );
	scone::TimeSection( "InitMenu" );

	// finalize windows menu
	windowMenu->addSeparator();
	windowMenu->addAction( "Reset Window Layout", this, &SconeStudio::resetWindowLayout );

	//
	// Viewer
	//

	// init viewer / scene
	if ( GetStudioSetting<bool>( "viewer.enable_object_cache" ) )
		ui.osgViewer->enableObjectCache( true );
	ui.osgViewer->setScene( &vis::osg_group( scene_.node_id() ) );
	if ( scone::GetStudioSetting<int>( "viewer.hud_type" ) != 67 )
		ui.osgViewer->createHud( GetSconeStudioFolder() / "resources/ui/scone_hud.png" );
	connect( ui.osgViewer, &QOsgViewer::hover, this, &SconeStudio::viewerTooltip );
	connect( ui.osgViewer, &QOsgViewer::clicked, this, &SconeStudio::viewerSelect );
	connect( ui.osgViewer, &QOsgViewer::pressed, this, &SconeStudio::viewerMousePush );
	connect( ui.osgViewer, &QOsgViewer::dragged, this, &SconeStudio::viewerMouseDrag );
	connect( ui.osgViewer, &QOsgViewer::released, this, &SconeStudio::viewerMouseRelease );
	scone::TimeSection( "InitViewer" );
	initViewerSettings();

	createSettings( "SCONE", "SconeStudio" );
	if ( GetStudioSetting<bool>( "ui.reset_layout" ) )
	{
		// reset layout if requested
		GetStudioSettings().set( "ui.reset_layout", false );
		GetStudioSettings().save();
		restoreSettings( true );
	}
	else restoreSettings();
	scone::TimeSection( "RestoreSettings" );
}

bool SconeStudio::init()
{
	// add outputText to global sinks (only *after* the ui has been initialized)
	xo::log::add_sink( ui.outputText );
	ui.outputText->set_sink_mode( xo::log::sink_mode::current_thread );
	ui.outputText->set_log_level( xo::log::level( GetStudioSetting<int>( "ui.log_level" ) ) );

	// see if this is a new version of SCONE
	auto version = xo::to_str( scone::GetSconeVersion() );
	scone::log::info( "SCONE version ", version );
	bool checkTutorials = GetStudioSetting<bool>( "ui.check_tutorials_on_launch" );
	if ( GetStudioSetting<std::string>( "ui.last_version_run" ) != version )
	{
		GetStudioSettings().set( "ui.last_version_run", version );
		GetStudioSettings().save();
		checkTutorials |= GetStudioSetting<bool>( "ui.check_tutorials_new_version" );
	}

	if ( checkTutorials )
		scone::updateTutorialsExamples();

	ui.messagesDock->raise();

	connect( &backgroundUpdateTimer, SIGNAL( timeout() ), this, SLOT( updateBackgroundTimer() ) );
	backgroundUpdateTimer.start( GetStudioSetting<int>( "progress.update_interval" ) );

	QObject::connect( &fileWatcher, SIGNAL( fileChanged( const QString& ) ), this, SLOT( handleFileChanged( const QString& ) ) );

	QTimer::singleShot( 0, this, SLOT( windowShown() ) );

	return true;
}

SconeStudio::~SconeStudio()
{}

void SconeStudio::restoreCustomSettings( QSettings& settings )
{
	if ( settings.contains( "viewSettings" ) )
	{
		auto f = MakeViewOptions( settings.value( "viewSettings" ).toULongLong() );
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

void SconeStudio::windowShown()
{
	if ( scone::GetStudioSetting<bool>( "ui.show_startup_time" ) ) {
		scone::TimeSection( "FinalShown" );
		log::info( scone::GetStudioStopwatch().get_report() );
	}
}

void SconeStudio::activateBrowserItem( QModelIndex idx )
{
	auto fi = ui.resultsBrowser->fileSystemModel()->fileInfo( idx );
	if ( fi.isDir() )
		fi = scone::findBestPar( QDir( fi.absoluteFilePath() ) );
	if ( fi.exists() )
	{
		if ( fi.suffix() == "scone" ) {
			openFile( fi.absoluteFilePath() ); // open the .scone file in a text editor
		}
		else if ( fi.suffix() == "par" || fi.suffix() == "sto" ) {
			if ( createScenario( fi.absoluteFilePath() ) ) {
				if ( scenario_->IsEvaluating() )
					evaluate(); // .par file
				if ( auto t = scone::GetStudioSetting<TimeInSeconds>( "file.playback_start" ); t != 0.0 )
					ui.playControl->setTime( t );
				ui.playControl->play(); // automatic playback after evaluation
			}
		}
		else if ( fi.suffix() == "pt" ) {
			queuedProcesses.emplace_back( makeCheckpointProcess( fi.absoluteFilePath(), this ) );
			QString msg = "The following file is being evaluated in the background (see the status bar for progress):\n\n";
			information( "Evaluate .pt files", msg + fi.absoluteFilePath() );
		}
		else {
			information( "Cannot open file", "File extension is not supported:\n" + fi.absoluteFilePath() );
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
		// evaluate the simulation in real-time
		startRealTimeEvaluation();
	}
	else
	{
		// everything is up-to-date, pause idle update timer
		ui.osgViewer->startPlaybackMode();
	}
}

void SconeStudio::stop()
{
	if ( real_time_evaluation_enabled_ ) {
		real_time_evaluation_enabled_ = false;
		if ( scenario_ && scenario_->IsEvaluating() )
			scenario_->AbortEvaluation();
		if ( scenario_->HasData() )
			updateModelDataWidgets();
		scenario_->UpdateVis( scenario_->GetTime() );
		updateEvaluationReport();
	}
	ui.osgViewer->stopPlaybackMode();
	refreshAnalysis();
}

void SconeStudio::refreshAnalysis()
{
	GUI_PROFILE_FUNCTION;

	analysisView->setTime( current_time );
}

void SconeStudio::dofEditorValueChanged()
{
	if ( scenario_ && scenario_->HasModel() && scenario_->IsEvaluatingStart() )
	{
		dofEditor->setDofsFromSliders( scenario_->GetModel() );
		scenario_->GetModel().InitStateFromDofs();
		scenario_->UpdateVis( 0.0 );
		ui.osgViewer->update();
	}
}

void SconeStudio::userInputValueChanged()
{
	if ( scenario_ && scenario_->HasModel() && scenario_->IsEvaluatingStart() )
	{
		scenario_->GetModel().UpdateModelFromUserInputs();
		dofEditor->setSlidersFromDofs( scenario_->GetModel() );
		scenario_->ResetModelVis( scene_, getViewOptionsFromMenu() );
		ui.osgViewer->update();
	}
}

void SconeStudio::muscleAnalysisValueChanged( const QString& dof, double value )
{
	if ( scenario_ && scenario_->HasModel() && scenario_->IsEvaluatingStart() )
	{
		auto& model = scenario_->GetModel();
		auto dofIt = TryFindByName( model.GetDofs(), dof.toStdString() );
		if ( dofIt != model.GetDofs().end() ) {
			( *dofIt )->SetPos( value );
			dofEditor->setSlidersFromDofs( model );
			model.InitStateFromDofs();
			scenario_->UpdateVis( 0.0 );
			ui.osgViewer->update();
		}
	}
}

void SconeStudio::evaluate()
{
	GUI_PROFILE_FUNCTION;
	SCONE_ASSERT( scenario_ );

	// disable dof editor and model input editor
	dofEditor->setEnableEditing( false );
	muscleAnalysis->setEnableEditing( false );
#if SCONE_EXPERIMENTAL_FEATURES_ENABLED
	userInputEditor->setEnableEditing( false );
#endif

	// setup progress bar
	ui.progressBar->setValue( 0 );
	ui.progressBar->setFormat( QString( "Evaluating " ) + scenario_->GetFileName().filename().c_str() + " (%p%)" );
	ui.progressBar->setMaximum( 1000 );
	ui.abortButton->setChecked( false );
	ui.stackedWidget->setCurrentIndex( 1 );
	QApplication::processEvents();

	// actual evaluation
	evaluateOffline();

	// cleanup
	ui.progressBar->setValue( 1000 );
	ui.stackedWidget->setCurrentIndex( 0 );

	if ( scenario_->HasData() )
		updateModelDataWidgets();
	scenario_->UpdateVis( scenario_->GetTime() );
	updateEvaluationReport();
}

void SconeStudio::evaluateOffline()
{
	xo::scoped_thread_priority prio_raiser( xo::thread_priority::highest );

	double step_size = std::max( 0.001, scenario_->GetModel().GetSimulationStepSize() );
	auto max_time = scenario_->GetMaxTime() > 0 ? scenario_->GetMaxTime() : 60.0;

	xo::interval_checker progress_update( 250_ms );
	xo::interval_checker visualizer_update( 1000_ms );
	xo::timer real_time;
	for ( double t = step_size; scenario_->IsEvaluating(); t += step_size )
	{
		auto rt = real_time();
		if ( progress_update.check( rt ) )
		{
			// update 3D visuals and progress bar
			setTime( t, visualizer_update.check( rt ) );
			ui.progressBar->setValue( std::min( int( 1000 * t / max_time ), 1000 ) );
			QApplication::processEvents();
			if ( ui.abortButton->isChecked() )
			{
				// user pressed cancel: update data so that user can see results so far
				scenario_->AbortEvaluation();
				break;
			}
		}
		else setTime( t, false );
	}

	auto real_dur = real_time().secondsd();
	auto sim_time = scenario_->GetTime();
	log::info( "Evaluation took ", real_dur, "s for ", sim_time, "s (", sim_time / real_dur, "x real-time)" );
}

void SconeStudio::evaluateRealTime()
{
	auto max_time = scenario_->GetMaxTime() > 0 ? scenario_->GetMaxTime() : 60.0;
	auto slomo = ui.playControl->slowMotionFactor();

	xo::timer timer;
	while ( scenario_->IsEvaluating() )
	{
		auto t = slomo * timer().secondsd();
		if ( t - scenario_->GetTime() > max_real_time_evaluation_step_ ) {
			t = scenario_->GetTime() + max_real_time_evaluation_step_;
			timer.set( xo::time_from_seconds( t ) / slomo );
		}

		setTime( t, true );
		ui.progressBar->setValue( std::min( int( 1000 * t / max_time ), 1000 ) );
		QApplication::processEvents();

		if ( ui.abortButton->isChecked() )
			scenario_->AbortEvaluation();
	}
}

void SconeStudio::startRealTimeEvaluation()
{
	SCONE_ASSERT( scenario_ );

	// disable dof editor and model input editor
	dofEditor->setEnableEditing( false );
	muscleAnalysis->setEnableEditing( false );
#if SCONE_EXPERIMENTAL_FEATURES_ENABLED
	userInputEditor->setEnableEditing( false );
#endif

	real_time_evaluation_enabled_ = true;
	auto max_time = scenario_->GetMaxTime() > 0 ? scenario_->GetMaxTime() : 60.0;
	ui.playControl->setRange( 0.0, max_time );
	ui.playControl->play();
}

void SconeStudio::modelAnalysis()
{
	if ( scenario_ && scenario_->HasModel() )
		xo::log_prop_node( scenario_->GetModel().GetInfo() );
}

void SconeStudio::updateGaitAnalysis()
{
	GUI_PROFILE_FUNCTION;

	try {
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

void SconeStudio::setTime( TimeInSeconds t, bool update_vis )
{
	GUI_PROFILE_FUNCTION;

	if ( scenario_ )
	{
		// advance simulation
		if ( scenario_->IsEvaluating() ) {
			if ( real_time_evaluation_enabled_ ) {
				if ( t - scenario_->GetTime() > max_real_time_evaluation_step_ ) {
					t = scenario_->GetTime() + max_real_time_evaluation_step_;
					ui.playControl->adjustCurrentTime( t );
				}
			}
			scenario_->EvaluateTo( t );
			if ( real_time_evaluation_enabled_ && scenario_->IsFinished() )
				ui.playControl->stop(); // stop triggers real-time evaluation handling
		}

		// set time
		current_time = t;

		// update UI and visualization
		if ( update_vis && scenario_->HasModel() )
		{
			// update 3D viewer
			scenario_->UpdateVis( t );
			if ( !scenario_->GetViewOptions().get<ViewOption::StaticCamera>() ) {
				auto p = scenario_->GetFollowPoint();
				ui.osgViewer->setFocusPoint( osg::Vec3( p.x, p.y, p.z ) );
			}
			scenario_->SetVisFocusPoint( scone::Vec3( vis::from_osg( ui.osgViewer->getCameraMan().getFocusPoint() ) ) );
			ui.osgViewer->setFrameTime( current_time );

			// update UI elements
			if ( analysisView->isVisible() ) // #todo: isVisible() returns true if the tab is hidden
				analysisView->setTime( current_time, !ui.playControl->isPlaying() );
			if ( dofEditor->isVisible() )
				dofEditor->setSlidersFromDofs( scenario_->GetModel() );
		}
	}
}

void SconeStudio::fileOpenTriggered()
{
	QString default_path = to_qt( GetFolder( SconeFolder::Scenarios ) );
	if ( auto* s = getActiveScenario() )
		default_path = to_qt( path( s->fileName.toStdString() ).parent_path() );

	QString filename = QFileDialog::getOpenFileName( this, "Open Scenario", default_path,
		"Supported file formats (*.scone *.xml *.zml *.lua *.hfd *.osim *.bp);;SCONE Scenarios (*.scone *.xml *.zml);;Lua Scripts (*.lua);;Models (*.osim *.hfd *.bp)" );
	if ( !filename.isEmpty() )
		openFile( filename );
}

void SconeStudio::fileReloadTriggered()
{
	if ( auto* s = getActiveCodeEditor() ) {
		s->reload();
		createAndVerifyActiveScenario( true );
	}
}

void SconeStudio::openFile( const QString& filename )
{
	try {
		QCodeEditor* edw = new QCodeEditor( this );
		edw->open( filename );
		int idx = ui.tabWidget->addTab( edw, edw->getTitle() );
		ui.tabWidget->setCurrentIndex( idx );
		connect( edw, &QCodeEditor::modificationChanged, this, &SconeStudio::updateTabTitles );
		codeEditors.push_back( edw );
		updateRecentFilesMenu( filename );
		fileWatcher.addPath( filename );
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
		//saveUserInputs( false );
		createAndVerifyActiveScenario( true );
	}
}

void SconeStudio::fileSaveAsTriggered()
{
	try {
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
				fileWatcher.removePath( s->fileName );
				s->saveAs( filename );
				fileWatcher.addPath( filename );
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

void SconeStudio::handleFileChanged( const QString& filename )
{
	log::debug( "File changed: ", filename.toStdString() );
	if ( !reloadFiles.contains( filename ) )
		reloadFiles.append( filename );
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
	QDesktopServices::openUrl( GetForumUrl() );
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
	addDockWidget( Qt::LeftDockWidgetArea, pdw );

	int numOptimizations = static_cast<int>( optimizations.size() );
	int maxRowsBelowResults = GetStudioSetting<int>( "progress.max_rows_below_results" );
	int maxColumnsBelowResults = GetStudioSetting<int>( "progress.max_columns_below_results" );
	int maxRowsLeftofResults = GetStudioSetting<int>( "progress.max_rows_left_of_results" );
	bool moveBelowResults = numOptimizations <= maxColumnsBelowResults * maxRowsBelowResults;
	int maxRows = moveBelowResults ? maxRowsBelowResults : maxRowsLeftofResults;

	auto columns = std::max<int>( 1, ( numOptimizations + maxRows - 1 ) / maxRows );
	auto rows = ( optimizations.size() + columns - 1 ) / columns;
	log::debug( "Reorganizing windows, columns=", columns, " rows=", rows );

	// keep this so we can restore later
	auto tabDocks = tabifiedDockWidgets( ui.resultsDock );

	// first widget determines position wrt results
	if ( moveBelowResults )
		splitDockWidget( ui.resultsDock, optimizations[0], Qt::Vertical );
	else splitDockWidget( optimizations[0], ui.resultsDock, Qt::Horizontal );

	// first column
	for ( index_t r = 1; r < rows; ++r )
		splitDockWidget( optimizations[( r - 1 ) * columns], optimizations[r * columns], Qt::Vertical );

	// remaining columns
	for ( index_t c = 1; c < columns; ++c )
	{
		for ( index_t r = 0; r < rows; ++r )
		{
			index_t idx = r * columns + c;
			index_t idxPrev = idx - 1;
			if ( idx < optimizations.size() )
				splitDockWidget( optimizations[idxPrev], optimizations[idx], Qt::Horizontal );
		}
	}

	// restore tabs
	for ( auto* dw : tabDocks )
		tabifyDockWidget( ui.resultsDock, dw );
	ui.resultsDock->raise();
}

void SconeStudio::clearScenario()
{
	GUI_PROFILE_FUNCTION;

	// remove files from watcher
	if ( scenario_ )
		for ( const auto& p : scenario_->GetExternalResources().GetVec() )
			fileWatcher.removePath( to_qt( p.filename_ ) );

	ui.playControl->stop();
	scenario_.reset();
	analysisStorageModel.setStorage( nullptr );
	parModel->setObjectiveInfo( nullptr );
	gaitAnalysis->reset();
	parViewDock->setWindowTitle( "Parameters" );
	ui.playControl->setRange( 0, 0 );
	optimizationHistoryStorage.Clear();
	muscleAnalysis->clear();
}

bool SconeStudio::createScenario( const QString& any_file )
{
	GUI_PROFILE_FUNCTION;

	clearScenario();

	try {
		// create scenario and update viewer
		currentFilename = any_file;
		scenario_ = std::make_unique< StudioModel >( scene_, path_from_qt( currentFilename ), getViewOptionsFromMenu() );

		if ( scenario_->HasModel() )
		{
			// update parameter view
			parModel->setObjectiveInfo( &scenario_->GetObjective().info() );
			parViewDock->setWindowTitle( QString( "Parameters (%1)" ).arg( scenario_->GetObjective().info().size() ) );

			// setup dof editor
			dofEditor->init( scenario_->GetModel() );
			dofEditor->setEnableEditing( scenario_->IsEvaluatingStart() );

			// setup muscle plots
			muscleAnalysis->init( scenario_->GetModel() );
			muscleAnalysis->setEnableEditing( scenario_->IsEvaluatingStart() );
			if ( muscleAnalysis->isVisible() && scenario_->GetFileType() == "scone"
				&& scone::GetStudioSetting<bool>( "muscle_analysis.analyze_on_load" ) )
				muscleAnalysis->refresh(); // this may affect evaluation result, use with care

			// set data, in case the file was an sto
			if ( scenario_->HasData() )
				updateModelDataWidgets();

			// update model inspector
			inspectorModel->setData( scenario_->GetModel().GetInfo() );
			inspectorView->expandToDepth( 0 );

#if SCONE_EXPERIMENTAL_FEATURES_ENABLED
			// setup model inputs
			userInputEditor->init( scenario_->GetModel() );
			userInputEditor->setEnableEditing( scenario_->IsEvaluatingStart() );
#endif

			// reset play control -- this triggers setTime( 0 ) and updates com_delta
			ui.playControl->reset();
			if ( scenario_->IsEvaluating() && scenario_->GetModel().GetInteractionSpring() ) {
				ui.playControl->setRecordingMode( true );
				if ( !viewActions[ViewOption::StaticCamera]->isChecked() )
					viewActions[ViewOption::StaticCamera]->trigger();
			}
			else ui.playControl->setRecordingMode( false );
		}

		auto history_file = scenario_->GetFileName().parent_path() / "history.txt";
		if ( xo::file_exists( history_file ) )
		{
			try {
				scone::ReadStorageTxt( optimizationHistoryStorage, history_file );
				if ( !optimizationHistoryStorage.IsEmpty() )
				{
					optimizationHistoryStorageModel.setStorage( &optimizationHistoryStorage );
					optimizationHistoryView->reloadData();
					optimizationHistoryView->setRange( 0, optimizationHistoryStorage.Back().GetTime() );
				}
			}
			catch ( std::exception& e ) {
				log::error( e.what() );
			}
		}
	}
	catch ( FactoryNotFoundException& e ) {
		if ( e.name_ == "Model" && e.props_.has_any_key( { "ModelHyfydy", "ModelHfd", "HyfydyModel", "ModelHyfydyPrecise", "ModelHfdPrecise" } ) )
			error( "Error creating scenario",
				"This scenario uses a <b>Hyfydy model</b>, but no active license key was found.<br><br>"
				"Please check Tools -> Preferences -> Hyfydy, or visit <a href = 'https://hyfydy.com'>hyfydy.com</a> for more information." );
#if !defined( _MSC_VER )
		else if ( e.name_ == "Model" && e.props_.has_any_key( { "ModelOpenSim4" } ) )
			error( "Error creating scenario",
				"This scenario uses an <b>OpenSim 4 model</b>, which is currently not supported on Linux and macOS." );
#endif
		else error( "Error creating scenario", e.what() );
		clearScenario();
		return false;
	}
	catch ( std::exception& e ) {
		error( "Error creating scenario", e.what() );
		clearScenario();
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

QStringList SconeStudio::getSelectedFiles()
{
	QStringList fileList;
	for ( const auto& idx : ui.resultsBrowser->selectionModel()->selectedRows() )
		fileList.push_back( ui.resultsBrowser->fileSystemModel()->fileInfo( idx ).filePath() );
	return fileList;
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
	QCodeEditor* best_guess = nullptr;
	QCodeEditor* optimizer = nullptr;
	for ( auto s : codeEditors ) {
		bool is_visible = !s->visibleRegion().isEmpty();
		if ( s->filePath().extension_no_dot() == "scone" ) {
			if ( !s->document()->find( "Optimizer" ).isNull() ) {
				// document contains Optimizer; #todo: create a more robust check
				if ( is_visible )
					return s; // visible tab with optimizer
				else if ( !optimizer )
					optimizer = s; // keep for later if there are no other tabs with optimizer
			}
			else if ( best_guess == nullptr || is_visible )
				best_guess = s; // first or visible .scone file
		}
	}
	return optimizer ? optimizer : best_guess; // either active or first .scone file, or none
}

bool SconeStudio::createAndVerifyActiveScenario( bool always_create, bool must_have_parameters )
{
	GUI_PROFILE_FUNCTION;

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
			if ( LogUnusedProperties( scenario_->GetScenarioPropNode() ) )
			{
				QString message = "Invalid scenario settings in " + scenario_->GetScenarioFileName() + ":\n\n";
				message += to_qt( to_str_unaccessed( scenario_->GetScenarioPropNode() ) );
				if ( QMessageBox::warning( this, "Invalid scenario settings", message, QMessageBox::Ignore, QMessageBox::Cancel ) == QMessageBox::Cancel )
					return false; // user pressed cancel
			}
			if ( must_have_parameters && scenario_->GetObjective().dim() <= 0 ) {
				information( scenario_->GetScenarioFileName(), "This scenario does not contain any free parameters" );
				return false;
			}

			// add all model files to file watcher
			for ( const auto& p : scenario_->GetExternalResources().GetVec() )
				fileWatcher.addPath( to_qt( p.filename_ ) );
			
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

void SconeStudio::updateEvaluationReport()
{
	SCONE_ASSERT( scenario_ );
	PropNode report_pn = scenario_->GetResult();
	report_pn.append( scenario_->GetModel().GetSimulationReport() );
	reportModel->setData( std::move( report_pn ) );
	reportView->expandToDepth( 0 );
}

void SconeStudio::updateModelDataWidgets()
{
	GUI_PROFILE_FUNCTION;
	SCONE_ASSERT( scenario_ && scenario_->HasData() );

	analysisStorageModel.setStorage( &scenario_->GetData() );
	analysisView->reloadData();
	ui.playControl->setRange( 0, scenario_->GetMaxTime() );
	if ( !gaitAnalysisDock->visibleRegion().isEmpty() )
		updateGaitAnalysis(); // automatic gait analysis if visible
}

void SconeStudio::optimizeScenario()
{
	try {
		if ( createAndVerifyActiveScenario( true, true ) )
		{
			auto task = scone::createOptimizerTask( scenario_->GetScenarioFileName() );
			addProgressDock( new ProgressDockWidget( this, std::move( task ) ) );
			updateOptimizations();
		}
	}
	catch ( const std::exception& e ) {
		error( "Error optimizing " + scenario_->GetScenarioFileName(), e.what() );
	}
}

void SconeStudio::optimizeScenarioMultiple()
{
	try {
		if ( createAndVerifyActiveScenario( true, true ) )
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
	catch ( const std::exception& e ) {
		error( "Error optimizing " + scenario_->GetScenarioFileName(), e.what() );
	}
}

void SconeStudio::evaluateActiveScenario()
{
	if ( scone::GetStudioSetting<bool>( "ui.enable_profiler" ) )
		getGuiProfiler().start();

	if ( createAndVerifyActiveScenario( false ) )
	{
		if ( scenario_->IsEvaluating() )
			evaluate();
		if ( auto t = scone::GetStudioSetting<TimeInSeconds>( "file.playback_start" ); t != 0.0 )
			ui.playControl->setTime( t );
		ui.playControl->play();
	}

	if ( getGuiProfiler().enabled() )
		getGuiProfiler().log_results();
}

void SconeStudio::writeEvaluationResults()
{
	if ( scenario_ )
	{
		if ( scenario_->IsFinishedOrAborted() )
			scenario_->WriteResults();
		else if ( scenario_->IsEvaluating() ) {
			scenario_->SetWriteResultsAfterEvaluation( true );
			evaluate();
			ui.playControl->play();
		}
	}
	else if ( createAndVerifyActiveScenario( false ) )
	{
		if ( scenario_->IsEvaluating() ) {
			scenario_->SetWriteResultsAfterEvaluation( true );
			evaluate();
		}
		ui.playControl->play();
	}
	else log::warning( "No results found" );
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
				auto profiler_previously_enabled = SetProfilerEnabled( true );
				xo::timer real_time;
				auto model = mo->CreateModelFromParams( par );
				model->SetStoreData( false );
				mo->AdvanceSimulationTo( *model, model->GetSimulationEndTime() );
				auto real_dur = real_time().secondsd();
				auto sim_time = model->GetTime();
				if ( model->GetProfiler().enabled() )
					model->GetProfiler().log_results();
				log::info( "fitness = ", mo->GetResult( *model ) );
				if ( auto sim_report = model->GetSimulationReport(); !sim_report.empty() )
					log::info( sim_report.front().second );
				log::info( "Evaluation took ", real_dur, "s for ", sim_time, "s (", sim_time / real_dur, "x real-time)" );
				SetProfilerEnabled( profiler_previously_enabled );
			}
			else
			{
				auto f = scenario_->GetFileName();
				scone::BenchmarkOptions bopt;
				bopt.min_samples = 4;
				scone::BenchmarkScenario( scenario_->GetScenarioPropNode(), f, bopt );
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
	GUI_PROFILE_FUNCTION;

	if ( !ui.playControl->isPlaying() )
		updateOptimizations();
	if ( scenario_ )
		scenario_->CheckWriteResults();
	handleAutoReload();
	checkActiveProcesses();
}

void SconeStudio::handleAutoReload()
{
	bool reload = false;
	for ( auto& filename : reloadFiles ) {
		for ( auto* e : codeEditors )
			if ( e->fileName == filename )
				reload |= e->reload();
		if ( scenario_ ) {
			if ( scenario_->GetExternalResources().Contains( filename.toStdString() ) )
				reload = true;
		}
	}
	reloadFiles.clear();
	if ( reload ) {
		createAndVerifyActiveScenario( true );
	}
}

void SconeStudio::checkActiveProcesses()
{
	if ( activeProcesses.empty() && queuedProcesses.empty() )
		return;

	// handle running processes
	auto it = activeProcesses.begin();
	while ( it != activeProcesses.end() ) {
		auto& p = **it;
		if ( p.state() != QProcess::Running ) {
			p.waitForFinished();
			p.close();
			it = activeProcesses.erase( it );
		}
		else {
			if ( p.waitForReadyRead( 0 ) )
				xo::log::debug( p.readAll().toStdString() );
			it++;
		}
	}

	// start new processes
	auto max_process_count = std::thread::hardware_concurrency() / 2;
	while ( activeProcesses.size() < max_process_count && queuedProcesses.size() > 0 ) {
		activeProcesses.emplace_back( std::move( queuedProcesses.front() ) );
		queuedProcesses.erase( queuedProcesses.begin() );
		startProcess( *activeProcesses.back() );
	}

	// update status bar
	if ( activeProcesses.size() > 0 || queuedProcesses.size() > 0 ) {
		QString msg = QString().sprintf( "Number of active background processes: %d", activeProcesses.size() );
		if ( queuedProcesses.size() > 0 )
			msg += QString().sprintf( " (%d queued)", queuedProcesses.size() );
		ui.statusBar->showMessage( msg );
	}
	else ui.statusBar->showMessage( "All background processes have finished", 3000 );
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

void SconeStudio::initViewerSettings()
{
	ui.osgViewer->setClearColor( vis::to_osg( scone::GetStudioSetting< xo::color >( "viewer.background" ) ) );
	ui.osgViewer->getCameraMan().setTransitionDuration( GetStudioSetting<double>( "viewer.camera_transition_duration" ) );
	ui.osgViewer->setLightOffset( GetStudioSetting<vis::vec3f>( "viewer.camera_light_offset" ) );
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
	if ( ShowPreferencesDialog( this ) == QDialog::Accepted ) {
		gaitAnalysis->reset();
		ui.outputText->set_log_level( xo::log::level( GetStudioSetting<int>( "ui.log_level" ) ) );
		initViewerSettings();
	}
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

void SconeStudio::viewerTooltip()
{
	if ( auto* node = ui.osgViewer->getTopNamedIntersectionNode() )
		QToolTip::showText( QCursor::pos(), to_qt( node->getName() ) );
	else QToolTip::showText( QPoint(), QString() );
}

void SconeStudio::viewerSelect()
{
	if ( auto* intersection = ui.osgViewer->getTopNamedIntersection() ) {
		for ( auto it = intersection->nodePath.rbegin(); it != intersection->nodePath.rend(); it++ ) {
			const auto& name = ( *it )->getName();
			if ( name.empty() )
				continue;

			// handle body clicks
			if ( auto b = TryFindPtrByName( scenario_->GetModel().GetBodies(), name ) ) {
				const auto world_pos = vis::from_osg( intersection->getWorldIntersectPoint() );
				auto body_pos = b->GetLocalPosOfPoint( Vec3( world_pos ) );
				auto pos_str = "[ " + vec3_str( body_pos ) + " ]";
				QApplication::clipboard()->setText( to_qt( pos_str ) );
				if ( prev_click.first == name )
					pos_str += " dir = [ " + vec3_str( prev_click.second - body_pos ) + " ]";
				log::info( "body = ", name, " pos = ", pos_str );
				prev_click = { name, body_pos };
			}

			auto items = inspectorModel->match( inspectorModel->index( 0, 0 ), Qt::DisplayRole, to_qt( name ), 1, Qt::MatchRecursive );
			if ( !items.empty() ) {
				inspectorView->setCurrentIndex( items.front() );
				inspectorDock->raise();
				break;
			}
		}
	}
}

void SconeStudio::viewerMousePush()
{
	if ( scenario_ && scenario_->IsEvaluating() && scenario_->HasModel() ) {
		if ( auto* spr = scenario_->GetModel().GetInteractionSpring() ) {
			for ( auto& intersection : ui.osgViewer->getIntersections() ) {
				for ( auto it = intersection.nodePath.rbegin(); it != intersection.nodePath.rend(); it++ ) {
					if ( auto* b = TryFindPtrByName( scenario_->GetModel().GetBodies(), ( *it )->getName() ) ) {
						if ( !b->IsStatic() ) {
							const auto world_pos = Vec3( vis::from_osg( intersection.getWorldIntersectPoint() ) );
							auto body_pos = b->GetLocalPosOfPoint( world_pos );
							spr->SetParent( scenario_->GetModel().GetGroundBody(), world_pos );
							spr->SetChild( *b, body_pos );
							drag_distance_ = xo::distance( world_pos, Vec3( ui.osgViewer->getMouseRay().pos ) );
							ui.osgViewer->getCameraMan().setEnableCameraManipulation( false );
							return;
						}
					}
				}
			}
		}
	}
}

void SconeStudio::viewerMouseDrag()
{
	if ( scenario_ && scenario_->IsEvaluating() && scenario_->HasModel() ) {
		if ( auto* spr = scenario_->GetModel().GetInteractionSpring() ) {
			auto p = spr->GetParentPos();
			auto mr = ui.osgViewer->getMouseRay();
			auto p_new = Vec3( mr.pos + drag_distance_ * mr.dir );
			spr->SetParent( scenario_->GetModel().GetGroundBody(), p_new );
		}
	}
}

void SconeStudio::viewerMouseRelease()
{
	if ( scenario_ && scenario_->HasModel() )
		if ( auto* spr = scenario_->GetModel().GetInteractionSpring() )
			spr->SetChild( scenario_->GetModel().GetGroundBody(), Vec3::zero() );
	ui.osgViewer->getCameraMan().setEnableCameraManipulation( true );
}

void SconeStudio::exportCoordinates()
{
	if ( scenario_ && scenario_->HasModel() )
	{
		const auto& sif = scenario_->GetModel().state_init_file;
		auto default_file = to_qt( sif.empty() ? scenario_->GetFileName().parent_path() : sif );
		auto filename = QFileDialog::getSaveFileName( this, "State Filename", default_file, "zml files (*.zml)" );
		if ( !filename.isEmpty() )
		{
			PropNode pn;
			for ( const auto& dof : scenario_->GetModel().GetDofs() )
			{
				pn["values"].set( dof->GetName(), dof->GetPos() );
				pn["velocities"].set( dof->GetName(), dof->GetVel() );
			}
			if ( scone::GetStudioSetting<bool>( "coordinates.export_activations" ) )
				for ( const auto& mus : scenario_->GetModel().GetMuscles() )
					pn["activations"].set( mus->GetName(), mus->GetActivation() );
			xo::save_file( pn, path_from_qt( filename ) );
		}
	}
}

void SconeStudio::resetCoordinates()
{
	if ( scenario_ && scenario_->HasModel() ) {
		scenario_->GetModel().SetDefaultState();
		dofEditor->setSlidersFromDofs( scenario_->GetModel() );
		scenario_->UpdateVis( 0.0 );
		ui.osgViewer->update();
	}
}

void SconeStudio::convertScenario()
{
	if ( !( createAndVerifyActiveScenario( true ) && scenario_->HasModel() ) )
		return;
	if ( scenario_->GetModel().GetModelFile().extension_no_dot() == "hfd" ) {
		if ( !question( "Convert to Hyfydy", "This scenario already uses a Hyfydy model. Do you wish to convert it still?" ) )
			return;
	}

	auto new_scenario = ShowConvertScenarioDialog( this, *scenario_ );
	if ( !new_scenario.isEmpty() && GetStudioSetting<bool>( "ui.show_conversion_support_message" ) ) {
		openFile( new_scenario );
		QString msg = "<b></b>The scenario has been converted to:<br><br>" + new_scenario;
		msg += "<br><br>Please note that <b>some elements way not work directly as intended</b> and require further adjustment. ";
		msg += "For assistance, please contact <a href='mailto:support@goatstream.com'>support@goatstream.com</a>.";
		information( "Conversion to Hyfydy Completed", msg );
	}
}

void SconeStudio::saveUserInputs( bool show_dialog )
{
	if ( scenario_ && scenario_->HasModel() )
	{
		path filename = scenario_->GetModel().user_input_file;
		if ( show_dialog || filename.empty() )
		{
			auto default_file = to_qt( filename.empty() ? scenario_->GetFileName().parent_path() : filename );
			filename = path_from_qt( QFileDialog::getSaveFileName( this, "Filename", default_file, "zml files (*.zml)" ) );
		}

		if ( !filename.empty() )
			xo::save_file( MakePropNode( scenario_->GetModel().GetUserInputs() ), filename );
	}
}

void SconeStudio::createVideo()
{
	ui.playControl->reset(); // playback interferes with video generation

	if ( !scenario_ )
		return error( "No Scenario", "There is no scenario open" );

	if ( auto p = GetStudioSetting<path>( "video.path_to_ffmpeg" ); !xo::file_exists( p ) ) {
		QString msg = "<b></b>Could not find ffmpeg (" + to_qt( p.str() ) + ")<br><br>";
		msg += "To enable video generation, please download ffmpeg from <a href='https://ffmpeg.org'>ffmpeg.org</a> ";
		msg += "and specify its file path in Preferences -> User Interface -> Video settings.";
		return error( "Could not find ffmpeg", msg );
	}

	fixViewerWindowSize();

	QString defaultName = to_qt( xo::path( scenario_->GetFileName() ).replace_extension( "mp4" ) );

	captureFilename = QFileDialog::getSaveFileName( this, "Video Filename", defaultName, "mp4 files (*.mp4);;avi files (*.avi);;mov files (*.mov)" );
	if ( captureFilename.isEmpty() )
		return;
	if ( !captureFilename.contains( "." ) )
		captureFilename += ".mp4"; // make sure filename has an extension (needed for Linux)

	// start recording
	QDir().mkdir( captureFilename + ".images" );
	ui.osgViewer->startCapture( captureFilename.toStdString() + ".images/image" );

	ui.osgViewer->startPlaybackMode();
	ui.abortButton->setChecked( false );
	ui.progressBar->setMaximum( 100 );
	ui.progressBar->setValue( 0 );
	ui.progressBar->setFormat( "Creating Video (%p%)" );
	ui.stackedWidget->setCurrentIndex( 1 );

	const double frame_rate = GetStudioSettings().get<double>( "video.frame_rate" );
	for ( double t = 0.0; t <= scenario_->GetMaxTime(); t += ui.playControl->slowMotionFactor() / frame_rate )
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
	ui.osgViewer->stopPlaybackMode();
}

void SconeStudio::captureImage()
{
	QString filename = QFileDialog::getSaveFileName( this, "Image Filename", QString(), "png files (*.png)" );
	if ( !filename.isEmpty() )
		ui.osgViewer->captureCurrentFrame( xo::path( filename.toStdString() ).replace_extension( "" ).str() );
}

void SconeStudio::setOrbitVelocity( float v )
{
	auto s = GetStudioSetting<vis::degree>( "viewer.camera_orbit_speed" );
	ui.osgViewer->getCameraMan().setOrbitAnimation( v * s, vis::degree( 0 ), 0.0f );
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
	QStringList fileList = getSelectedFiles();
	if ( fileList.empty() )
		return information( msgTitle, tr( "No files or folders selected" ) );

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

	auto selection = ui.resultsBrowser->selectionModel()->selectedRows();
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

void SconeStudio::copyToScenarioFolder()
{
	auto fileList = getSelectedFiles();
	auto scenario = getActiveScenario();
	if ( fileList.size() >= 1 && scenario ) {
		auto src_path = path_from_qt( fileList.front() );
		auto src_dir = src_path.parent_path().stem().str();
		auto third_dot_idx = xo::find_nth_str( src_dir, ".", 3 );
		auto trg_file = path( "par" ) / src_dir.substr( 0, third_dot_idx ) + "." + src_path.filename();
		auto trg_path = scenario->filePath().parent_path() / trg_file;
		xo::create_directories( trg_path.parent_path() );
		xo::copy_file( src_path, trg_path, true );
		xo::log::info( "Copied file to ", trg_path );
		QApplication::clipboard()->setText( to_qt( trg_file ) );
		QString msgBody = "The following file has been created (filename copied to clipboard):\n\n" + to_qt( trg_file.str() );
		QMessageBox::information( nullptr, "Copy to Scenario Folder", msgBody );
	}
}

void SconeStudio::evaluateSelectedFiles()
{
	auto fileList = getSelectedFiles();
	for ( const auto& f : fileList )
		queuedProcesses.emplace_back( makeCheckpointProcess( f, this ) );

	QString msg = "The following files are being evaluated in the background (see the status bar for progress):\n\n";
	information( "Evaluate .pt files", msg + fileList.join( "\n") );
}

void SconeStudio::sortResultsByDate()
{
	ui.resultsBrowser->fileSystemModel()->sort( 3 );
}

void SconeStudio::sortResultsByName()
{
	ui.resultsBrowser->fileSystemModel()->sort( 0 );
}

void SconeStudio::onResultBrowserCustomContextMenu( const QPoint& pos )
{
	QMenu menu;
	menu.addAction( "Sort by &Name", this, &SconeStudio::sortResultsByName );
	menu.addAction( "Sort by &Date", this, &SconeStudio::sortResultsByDate );
	menu.addSeparator();

	auto sel = ui.resultsBrowser->selectionModel()->selectedRows();
	if ( sel.size() == 1 && getActiveScenario() && ui.resultsBrowser->fileSystemModel()->fileInfo( sel.front() ).suffix() == "par" ) {
		menu.addAction( "&Copy to Scenario Folder", this, &SconeStudio::copyToScenarioFolder );
		menu.addSeparator();
	}
	if ( sel.size() >= 1 && ui.resultsBrowser->fileSystemModel()->fileInfo( sel.front() ).suffix() == "pt" ) {
		menu.addAction( "&Evaluate", this, &SconeStudio::evaluateSelectedFiles );
		menu.addSeparator();
	}
	if ( sel.size() >= 1 )
		menu.addAction( "&Remove", this, &SconeStudio::deleteSelectedFileOrFolder );

	menu.exec( ui.resultsBrowser->mapToGlobal( pos ) );
}
