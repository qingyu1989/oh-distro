#include <iostream>
#include <QtGui/QApplication>

#include "authoring/qt4_widget_constraint_task_space_region_editor.h"

using namespace std;
using namespace urdf;
using namespace affordance;
using namespace authoring;

int
main( int argc,
      char* argv[] ) {
  int status = 0;
  cout << "start of Qt4_Widget_Constraint_Task_Space_Region_Editor class demo program" << endl;
  QApplication app( argc, argv );
  
  Constraint_Task_Space_Region constraint_task_space_region( "constraint_task_space_region", 0.0, 0.0 );
  Model robot_model;
  vector< AffordanceState > affordance_collection;

  Qt4_Widget_Constraint_Task_Space_Region_Editor qt4_widget_constraint_task_space_region_editor( constraint_task_space_region, robot_model, affordance_collection );
  qt4_widget_constraint_task_space_region_editor.show();

  return app.exec();
}
