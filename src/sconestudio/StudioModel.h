/*
** StudioModel.h
**
** Copyright (C) 2013-2019 Thomas Geijtenbeek and contributors. All rights reserved.
**
** This file is part of SCONE. For more information, see http://scone.software.
*/

#pragma once

#include "scone/core/system_tools.h"
#include "scone/model/Model.h"
#include "scone/optimization/SimulationObjective.h"
#include "scone/optimization/Objective.h"
#include "scone/model/State.h"
#include "scone/core/types.h"
#include "scone/optimization/ModelObjective.h"
#include "scone/optimization/Optimizer.h"

#include "ModelVis.h"
#include "qt_convert.h"

#include <future>

namespace scone
{
	class StudioModel
	{
	public:
		StudioModel( vis::scene& s, const path& filename, const ViewOptions& vs );
		virtual ~StudioModel();

		void UpdateVis( TimeInSeconds t );
		void EvaluateTo( TimeInSeconds t );

		void AbortEvaluation();

		const Storage<>& GetData() { return storage_; }
		bool HasModel() const { return bool( model_ ) && IsValid(); }
		bool HasData() const { return !storage_.IsEmpty() && !state_data_index.empty(); }

		Model& GetModel() { SCONE_ASSERT( model_ ); return *model_; }
		const Model& GetModel() const { SCONE_ASSERT( model_ ); return *model_; }
		const Objective& GetObjective() const { return optimizer_ ? optimizer_->GetObjective() : null_objective_; }
		ModelObjective* TryGetModelObjective() const { return model_objective_; }

		bool IsEvaluating() const { return status_ == Status::Evaluating; }
		bool IsEvaluatingStart() const { return status_ == Status::Evaluating && GetTime() == 0.0; }
		bool IsReady() const { return status_ == Status::Ready; }
		bool IsValid() const { return status_ != Status::Error; }

		TimeInSeconds GetTime() const { return model_ ? model_->GetTime() : 0.0; }
		TimeInSeconds GetMaxTime() const;

		void ApplyViewOptions( const ViewOptions& f );
		const ViewOptions& GetViewOptions() const;
		Vec3 GetFollowPoint() const;
		void ResetModelVis( vis::scene& s, const ViewOptions& f );

		const path& GetFileName() const { return filename_; }
		const path& GetScenarioPath() const { return scenario_filename_; }
		QString GetScenarioFileName() const { return to_qt( scenario_filename_ ); }
		const PropNode& GetScenarioProps() const { return scenario_pn_; }

		enum class Status { Initializing, Evaluating, Ready, Aborted, Error };
		Status GetStatus() const { return status_; }

		const PropNode& GetResult();

		using WriteResultsInfo = std::pair<std::vector<path>, xo::time>;
		WriteResultsInfo WriteResults();
		void CheckWriteResults();
		void SetWriteResultsAfterEvaluation( bool b ) { write_results_after_evaluation_ = b; }

	private:
		void FinalizeEvaluation();
		void InvokeError( const String& message );

		// visualizer
		u_ptr<ModelVis> vis_;

		// model / scenario data
		Storage<> storage_;
		OptimizerUP optimizer_;
		ModelObjective* model_objective_;
		Objective null_objective_;
		ModelUP model_;
		Body* follow_body_;
		path filename_;
		path scenario_filename_;
		PropNode scenario_pn_;
		PropNode result_pn_;

		Status status_;

		std::future<WriteResultsInfo> write_results_;
		void WaitForWriteResults();
		bool write_results_after_evaluation_;

		// model state
		std::vector< size_t > state_data_index;
		scone::State model_state;
		void InitStateDataIndices();
	};
}
