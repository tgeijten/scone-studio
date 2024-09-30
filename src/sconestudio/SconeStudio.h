/*
** SconeStudio.h
**
** Copyright (C) Thomas Geijtenbeek and contributors. All rights reserved.
**
** This file is part of SCONE. For more information, see http://scone.software.
*/

#ifndef SCONESTUDIO_H
#define SCONESTUDIO_H

#include <QtCore/QtGlobal>
#include <QtCore/QTimer>
#include <QtCore/QProcess>
#include <QtWidgets/QMainWindow>
#include "QCodeEditor.h"
#include "QCompositeMainWindow.h"
#include "QDataAnalysisView.h"
#include "QGroup.h"
#include "QValueSlider.h"
#include "QDockWidget"
#include "QPropNodeItemModel.h"
#include <QFileSystemWatcher>
#include <QProcess>

#include "ui_SconeStudio.h"

#include "scone/core/PropNode.h"
#include "scone/core/Statistic.h"

#include "ModelVis.h"
#include "GaitAnalysis.h"
#include "ProgressDockWidget.h"
#include "ResultsFileSystemModel.h"
#include "SconeStorageDataModel.h"
#include "SettingsEditor.h"
#include "StudioModel.h"

#include "vis/plane.h"
#include "vis/vis_api.h"
#include "vis/scene.h"

#include "xo/container/flat_map.h"
#include "xo/numerical/delta.h"
#include "xo/system/log_sink.h"
#include "xo/time/timer.h"
#include "GaitAnalysis.h"
#include "ParTableModel.h"
#include "DofEditor.h"
#include "ViewOptions.h"
#include "UserInputEditor.h"
#include "MuscleAnalysis.h"

using scone::TimeInSeconds;
enum class EvaluationMode { offline, real_time };

class SconeStudio : public QCompositeMainWindow
{
	Q_OBJECT

public:
	SconeStudio( QWidget* parent = 0, Qt::WindowFlags flags = 0 );
	~SconeStudio();

	bool init();
	virtual void openFile( const QString& filename ) override;
	virtual bool tryExit() override;

public slots:
	void windowShown();
	void activateBrowserItem( QModelIndex idx );
	void selectBrowserItem( const QModelIndex& idx, const QModelIndex& idxold );
	void resultsSelectionChanged( const QItemSelection& newitem, const QItemSelection& olditem ) {}
	void start();
	void stop();
	void refreshAnalysis();
	void dofEditorValueChanged();
	void userInputValueChanged();
	void muscleAnalysisValueChanged( const QString& dof, double value );

	virtual void fileOpenTriggered() override;
	virtual void fileReloadTriggered();
	virtual void fileSaveTriggered() override;
	virtual void fileSaveAsTriggered() override;
	virtual void fileCloseTriggered() override;
	void handleFileChanged( const QString& filename );

	void helpSearch();
	void helpForum();
	void evaluateActiveScenario();
	void writeEvaluationResults();
	void optimizeScenario();
	void optimizeScenarioMultiple();
	bool abortOptimizations();
	void updateBackgroundTimer();
	void handleAutoReload();
	void checkActiveProcesses();
	void updateOptimizations();
	void createVideo();
	void captureImage();
	void modelAnalysis();
	void updateGaitAnalysis();
	void tabCloseRequested( int idx );
	void applyViewOptions();
	void showSettingsDialog();
	void setPlaybackTime( TimeInSeconds t ) { setTime( t, true ); }
	void updateTabTitles();
	void findDialog() { if ( auto* e = getActiveCodeEditor() ) e->findDialog(); }
	void findNext() { if ( auto* e = getActiveCodeEditor() ) e->findNext(); }
	void findPrevious() { if ( auto* e = getActiveCodeEditor() ) e->findNext( true ); }
	void toggleComments() { if ( auto* e = getActiveCodeEditor() ) e->toggleComments(); }
	void resetWindowLayout();
	void fixViewerWindowSize();
	void viewerTooltip();
	void viewerSelect();
	void viewerPress();
	void viewerDrag();
	void viewerRelease();
	void exportCoordinates();
	void resetCoordinates();
	void convertScenario();

	void deleteSelectedFileOrFolder();
	void copyToScenarioFolder();
	void evaluateSelectedFiles();
	void sortResultsByDate();
	void sortResultsByName();
	void onResultBrowserCustomContextMenu( const QPoint& );

public:
	bool close_all;
	bool isRecording() { return !captureFilename.isEmpty(); }
	bool isEvalutating() { return scenario_ && scenario_->IsEvaluating(); }
	bool hasModel() const { return scenario_ && scenario_->HasModel(); }

private:
	QStringList getSelectedFiles();
	QCodeEditor* getActiveCodeEditor();
	QCodeEditor* getActiveScenario();

	void restoreCustomSettings( QSettings& settings ) override;
	void saveCustomSettings( QSettings& settings ) override;
	scone::ViewOptions getViewOptionsFromMenu() const;
	void initViewerSettings();

	void performanceTest( bool write_stats );
	void saveUserInputs( bool show_dialog );

	void evaluate();
	void evaluateOffline();
	void evaluateRealTime();
	void startRealTimeEvaluation();
	void setTime( TimeInSeconds t, bool update_vis );

	std::vector< QCodeEditor* > changedDocuments();
	bool requestSaveChanges( const std::vector<QCodeEditor*>& modified_docs );
	bool requestSaveChanges( QCodeEditor* s );
	int getTabIndex( QCodeEditor* s );
	void addProgressDock( ProgressDockWidget* pdw );

	// ui
	Ui::SconeStudioClass ui;

	// model
	std::unique_ptr< scone::StudioModel > scenario_;
	QString currentFilename;
	void clearScenario();
	bool createScenario( const QString& any_file );
	bool createAndVerifyActiveScenario( bool always_create, bool must_have_parameters = false );
	void updateEvaluationReport();
	void updateModelDataWidgets();

	// simulation
	TimeInSeconds current_time;
	TimeInSeconds evaluation_time_step;
	TimeInSeconds max_real_time_evaluation_step_;
	bool real_time_evaluation_enabled_;

	// scenario
	std::vector< ProgressDockWidget* > optimizations;
	ResultsFileSystemModel* resultsModel;
	std::vector< QCodeEditor* > codeEditors;
	QFileSystemWatcher fileWatcher;
	QStringList reloadFiles;

	// viewer
	xo::flat_map< scone::ViewOption, QAction* > viewActions;
	vis::scene scene_;
	QTimer backgroundUpdateTimer;
	double slomo_factor;
	xo::delta< scone::Vec3 > com_delta;
	std::pair<std::string, scone::Vec3> prev_click;
	double drag_distance_;
	void setOrbitVelocity( float v );

	// video capture
	QString captureFilename;
	QProcess* captureProcess = nullptr;
	QDir captureImageDir;
	void finalizeCapture();

	// analysis
	SconeStorageDataModel analysisStorageModel;
	QDataAnalysisView* analysisView = nullptr;
	QDockWidget* analysisDock = nullptr;

	// gait analysis
	scone::GaitAnalysis* gaitAnalysis = nullptr;
	QDockWidget* gaitAnalysisDock = nullptr;

	// parameters
	QTableView* parView = nullptr;
	ParTableModel* parModel = nullptr;
	QDockWidget* parViewDock = nullptr;

	// evaluation report
	QTreeView* reportView = nullptr;
	QPropNodeItemModel* reportModel = nullptr;
	QDockWidget* reportDock = nullptr;

	// dof editor
	scone::DofEditorGroup* dofEditor = nullptr;
	QDockWidget* dofDock = nullptr;

	// model inspector
	QSplitter* inspectorSplitter = nullptr;
	QTreeView* inspectorView = nullptr;
	QTreeView* inspectorDetails = nullptr;
	QPropNodeItemModel* inspectorModel = nullptr;
	QDockWidget* inspectorDock = nullptr;

#if SCONE_EXPERIMENTAL_FEATURES_ENABLED
	// UserInput editor
	scone::UserInputEditor* userInputEditor = nullptr;
#endif

	// Muscle Analysis
	scone::MuscleAnalysis* muscleAnalysis = nullptr;
	QDockWidget* muscleAnalysisDock = nullptr;

	// Optimization History
	scone::Storage<> optimizationHistoryStorage;
	SconeStorageDataModel optimizationHistoryStorageModel;
	QDataAnalysisView* optimizationHistoryView = nullptr;
	QDockWidget* optimizationHistoryDock = nullptr;

	// Background processing
	std::vector<std::unique_ptr<QProcess>> activeProcesses, queuedProcesses;
};

#endif // SCONESTUDIO_H
