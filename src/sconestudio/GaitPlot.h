#pragma once

#include <QWidget>
#include <QGridLayout>

#include "scone/core/Storage.h"
#include "scone/core/PropNode.h"
#include "scone/core/types.h"
#include "scone/core/GaitCycle.h"
#include "xo/container/flat_map.h"
#include "xo/numerical/bounds.h"
#include "xo/utility/result.h"
#include "xo/string/pattern_matcher.h"
#include "scone/core/GaitPlotData.h"

class QCustomPlot;
class QCPPlotTitle;
class QCPGraph;

namespace scone
{
	class GaitPlot : public QWidget
	{
	public:
		GaitPlot( const PropNode& pn, QWidget* parent = nullptr );
		virtual ~GaitPlot();

		xo::error_message update( const Storage<>& sto, const std::vector<GaitCycle>& cycles );
		double matchPercentage() const { return match_percentage_; }
		bool hasNormData() const { return data_.HasNormData(); }
		bool hasData() const;

		GaitPlotData data_;

	private:
		void setNormDataGraph();

		QCustomPlot* plot_;
		QCPPlotTitle* plot_title_;
		QCPGraph* norm_top_;
		QCPGraph* norm_bot_;
		double match_percentage_;
		int base_graph_count_;
		int base_item_count_;
	};
}
