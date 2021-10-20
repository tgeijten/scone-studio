#pragma once

#include <QWidget>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QSlider>
#include <QGridLayout>
#include <QPushButton>

#include "scone/model/Model.h"
#include "scone/model/UserInput.h"

namespace scone
{
	struct UserInputItem
	{
		UserInputItem( UserInput& input, class UserInputEditor* editor, int row );

		void updateWidgetValues();
		int to_int( double d ) { return int( d / stepSize ); }
		double to_double( int i ) { return stepSize * i; }

		UserInput& input_;
		QLabel* label_;
		QDoubleSpinBox* spin_;
		QLabel* min_;
		QSlider* slider_;
		QLabel* max_;
		QDoubleSpinBox* velocity_;
		double stepSize;
	};

	class UserInputEditor : public QWidget
	{
		Q_OBJECT

	public:
		UserInputEditor( QWidget* parent );
		~UserInputEditor() = default;

		void init( const Model& model );
		void setEnableEditing( bool enable );
		QGridLayout* grid() { return gridLayout; }

	signals:
		void valueChanged();
		void exportCoordinates();

	private:
		void createLabel( const String& str, int row, Qt::Alignment align = Qt::AlignCenter );
		QWidget* dofGrid;
		QPushButton* exportButton;
		QGridLayout* gridLayout;
		std::vector<UserInputItem*> items;
	};
}
