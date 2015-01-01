#include "MainWindow.h"

using namespace boost;
using namespace std;
using namespace opengl;
using namespace state;
using namespace collision;
using namespace action_authoring;
using namespace affordance;


#define REQUEST_IK_SOLUTION_CHANNEL "REQUEST_IK_SOLUTION_AT_TIME_FOR_ACTION_SEQUENCE"
#define REQUEST_MOTION_PLAN_CHANNEL "REQUEST_MOTION_PLAN_FOR_ACTION_SEQUENCE"

#define RESPONSE_IK_SOLUTION_CHANNEL "RESPONSE_IK_SOLUTION_AT_TIME_FOR_ACTION_SEQUENCE"
#define RESPONSE_MOTION_PLAN_CHANNEL "RESPONSE_MOTION_PLAN_FOR_ACTION_SEQUENCE"

//#define IK_RESPONSES_MESSAGE_CHANNEL "REQUEST_IK_SOLUTION_AT_TIME_FOR_ACTION_SEQUENCE"
//#define IK_TRAJECTORY_MESSAGE_CHANNEL "ACTION_AUTHORING_IK_ROBOT_PLAN"

#define ROBOT_URDF_MODEL_PATH "/mit_gazebo_models/mit_robot_drake/model_minimal_contact.urdf"

#define ROBOT_NAME "atlas" //used for LCM messages that go to planning

/*
 * Filler method to populate affordance and constraint lists until we get proper
 * data sources set up.
 */
void MainWindow::handleAffordancesChanged()
{
    std::cout << std::endl << "HANDLE AFFORDANCES CHANGED ENTERED " << std::endl;

    //------------------------REDRAW
    //clear the scene.  todo : is there a better way to handle
    //memory management thru boost pointers?  opengl_scene doesn't use boost
    if (_worldState.glObjects.size() != _worldState.collisionObjs.size())
    {
        throw InvalidStateException("glObjects and collisionObjs should have the same size");
    }

    _widget_opengl.opengl_scene().clear_objects();

    for (uint i = 0; i < _worldState.glObjects.size(); i++)
    {
        delete _worldState.glObjects[i];
        delete _worldState.collisionObjs[i];
    }

    _worldState.glObjects.clear();
    _worldState.collisionObjs.clear();
    // TODO: just have it sync with worldState collisionObjs
    _widget_opengl.clear_collision_objects();

    for (uint i = 0; i < _worldState.affordances.size(); i++)
    {
        AffConstPtr next = _worldState.affordances[i];

        if (!Collision_Object_Affordance::isSupported(next))
        {
            cout << "\n Collision_Object_Affordance doesn't support " << next->getName() << endl;
            continue;
        }

        if (!OpenGL_Affordance::isSupported(next))
        {
            cout << "\n OpenGL_Affordance doesn't support " << next->getName() << endl;
            continue;
        }

        //opengl
        OpenGL_Affordance *asGlAff = new OpenGL_Affordance(next);
        _widget_opengl.opengl_scene().add_object(*asGlAff);
        _worldState.glObjects.push_back(asGlAff);

        //collisions:  Create CollisionObject_Affordances, add to scene, and add to _worldState.glObjects
        Collision_Object *collision_object_affordance = new Collision_Object_Affordance(next);
        _widget_opengl.add_collision_object(collision_object_affordance);
        _worldState.collisionObjs.push_back(collision_object_affordance);
    }

    //----------handle constraint macros
    unordered_map<string, AffConstPtr> nameToAffMap;

    for (vector<AffConstPtr>::iterator iter = _worldState.affordances.begin();
            iter != _worldState.affordances.end();
            ++iter)
    {
        nameToAffMap[(*iter)->getName()] = *iter;
    }

    GUIManipulators::createManipulators(_worldState, _widget_opengl);

    updateFlyingManipulators();

    _widget_opengl.opengl_scene().add_object(point_contact_axis);
    //_worldState.glObjects.push_back(&point_contact_axis);
    _widget_opengl.opengl_scene().add_object(point_contact_axis2);
    //_worldState.glObjects.push_back(&point_contact_axis2);

    //----------add robot and vehicle
    _widget_opengl.opengl_scene().add_object(_worldState.colorRobot); //add robot

    //  connect(&_widget_opengl, SIGNAL(raycastPointIntersectCallback(Eigen::Vector3f)),
    //	  this, SLOT(mainRaycastCallback(Eigen::Vector3f)));

    // TODO resolve path
//    _worldState.colorVehicle = new opengl::OpenGL_Object_DAE("vehicle",
//    "/home/drc/drc/software/models/mit_gazebo_models/" "mit_golf_cart/meshes/new_golf_cart.dae");
//    _widget_opengl.opengl_scene().add_object(*_worldState.colorVehicle); //add vehicle

    _widget_opengl.update();
    //_widget_opengl.add_object_with_collision(_collision_object_gfe);

}

MainWindow::MainWindow(const shared_ptr<lcm::LCM> &theLcm, QWidget *parent)
    : _widget_opengl(),
      _constraint_container(new QWidget()),
      _constraint_vbox(new QVBoxLayout()),
      _worldState(theLcm, ROBOT_URDF_MODEL_PATH)

{
    _theLcm = theLcm;
	_theLcm->subscribe(RESPONSE_IK_SOLUTION_CHANNEL,
                       &MainWindow::updateRobotState, this);
	_newConstraintCounter = 0;
    //point_contact_axis = new OpenGL_Object_Coordinate_Axis();
    //point_contact_axis2 = new OpenGL_Object_Coordinate_Axis();

    // setup the OpenGL scene
    _worldState.state_gfe.from_urdf(ROBOT_URDF_MODEL_PATH);
    _worldState.colorRobot.set(_worldState.state_gfe);

    _widget_opengl.setMinimumHeight(100);
    _widget_opengl.setMinimumWidth(500);
    _widget_opengl.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    connect(&_widget_opengl, SIGNAL(raycastCallback(std::string, Eigen::Vector3f)),
            this, SLOT(selectedOpenGLObjectChanged(std::string, Eigen::Vector3f)));

    //setup timer to look for affordance changes
    _affordanceUpdateTimer = new QTimer;
    connect(_affordanceUpdateTimer, SIGNAL(timeout()),
    this, SLOT(affordanceUpdateCheck()));
    _affordanceUpdateTimer->start(1000); //1Hz

    _scrubberTimer = new QTimer;
    connect(_scrubberTimer, SIGNAL(timeout()), this, SLOT(nextKeyFrame()));

    this->setWindowTitle("Action Authoring Interface");

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setMargin(0);

    QSplitter *splitter = new QSplitter();
    QWidget *leftside = new QWidget();
    QVBoxLayout *vbox = new QVBoxLayout();

    QToolBar* toptoolbar = new QToolBar("top toolbar");
    QLabel *fileslistlabel = new QLabel("saved action sequences : ");
    toptoolbar->addWidget(fileslistlabel);
    _filesList = new QComboBox();
    updateFilesListComboBox();
    toptoolbar->addWidget(_filesList);
    QPushButton *loadfromcombo = new QPushButton("Load");
    toptoolbar->addWidget(loadfromcombo);
    toptoolbar->addSeparator();
    QPushButton *loaddiff = new QPushButton("Load Action...");
    loaddiff->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogOpenButton));
    toptoolbar->addWidget(loaddiff);
    vbox->addWidget(toptoolbar);

    QToolBar *toolbar = new QToolBar("main toolbar");
    toolbar->addWidget(new QLabel("Action: "));
    _actionName = new QLineEdit("Ingress");
    toolbar->addWidget(_actionName);
    QLabel *actionTypeLabel = new QLabel(" acts on: ");
    toolbar->addWidget(actionTypeLabel);
    QComboBox *actionType = new QComboBox();
    actionType->insertItem(0, "Vehicle");
    actionType->insertItem(0, "Door");
    actionType->insertItem(0, "Ladder");
    actionType->insertItem(0, "Table");
    toolbar->addWidget(actionType);
    toolbar->addSeparator();
    QPushButton *savebutton = new QPushButton("Save Action");
    savebutton->setIcon(QApplication::style()->standardIcon(QStyle::SP_DialogSaveButton));
    toolbar->addWidget(savebutton);
    QPushButton *planbutton = new QPushButton("Publish For Planning");
    planbutton->setIcon(QApplication::style()->standardIcon(QStyle::SP_DriveNetIcon));
    toolbar->addWidget(planbutton);

    vbox->addWidget(toolbar);

    //    _constraint_vbox->setSizeConstraint(QLayout::SetMinAndMaxSize);
    //    _constraint_vbox->setMargin(0);
    _constraint_vbox->setAlignment(Qt::AlignTop);
    _constraint_container->setLayout(_constraint_vbox);
    QScrollArea *area = new QScrollArea();
    //    area->setBackgroundRole(QPalette::Dark);
    area->setWidgetResizable(true);
    area->setWidget(_constraint_container);
    area->setMinimumSize(QSize(725, 600));
    area->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    area->setAlignment(Qt::AlignTop);
    //    vbox->addWidget(_constraint_container);
    vbox->addWidget(area);
    vbox->setStretchFactor(area, 1000);

    QToolBar *constraint_toolbar = new QToolBar("main toolbar");
    QPushButton *deletebutton = new QPushButton("delete");
    deletebutton->setIcon(QApplication::style()->standardIcon(QStyle::SP_TrashIcon));
    _moveUpButton = new QPushButton("move up");
    //_moveUpButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_ArrowUp));
    _moveDownButton = new QPushButton("move down");
    //_moveDownButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_ArrowDown));
    QPushButton *addconstraintbutton = new QPushButton("+ add constraint");
    //    addconstraintbutton->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileDialogNewFolder));
    
    //***********
    // TODO : Remove demoware
    //*********
    QPushButton *testIKPublishButton = new QPushButton("Publish for IK");
    //end demoware
    
    constraint_toolbar->addWidget(deletebutton);
    constraint_toolbar->addSeparator();
    constraint_toolbar->addWidget(_moveUpButton);
    constraint_toolbar->addWidget(_moveDownButton);
    constraint_toolbar->addSeparator();
    constraint_toolbar->addWidget(addconstraintbutton);
    constraint_toolbar->addSeparator();
    
    //demoware
    constraint_toolbar->addWidget(testIKPublishButton);
    //end demoware

    vbox->addStretch(1);
    vbox->addWidget(constraint_toolbar);

    QGroupBox *mediaControls = new QGroupBox();
    QHBoxLayout *mediaControlsLayout = new QHBoxLayout();
    _fbwd = new QPushButton();
    _bwd = new QPushButton();
    _play = new QPushButton();
    _fwd = new QPushButton();
    _ffwd = new QPushButton();

    // see http://www.qtcentre.org/wiki/index.php?title=Embedded_resources
    QPixmap pixmap1(":/trolltech/styles/commonstyle/images/media-skip-backward-32.png");
    _fbwd->setIcon(QIcon(pixmap1));
    _fbwd->setIconSize(pixmap1.rect().size());

    QPixmap pixmap2(":/trolltech/styles/commonstyle/images/media-seek-backward-32.png");
    _bwd->setIcon(QIcon(pixmap2));
    _bwd->setIconSize(pixmap2.rect().size());

    QPixmap pixmap3(":/trolltech/styles/commonstyle/images/media-play-32.png");
    _play->setIcon(QIcon(pixmap3));
    _play->setIconSize(pixmap3.rect().size());
    _isPlaying = true;

    QPixmap pixmap4(":/trolltech/styles/commonstyle/images/media-seek-forward-32.png");
    _fwd->setIcon(QIcon(pixmap4));
    _fwd->setIconSize(pixmap4.rect().size());

    QPixmap pixmap5(":/trolltech/styles/commonstyle/images/media-skip-forward-32.png");
    _ffwd->setIcon(QIcon(pixmap5));
    _ffwd->setIconSize(pixmap5.rect().size());

    mediaControlsLayout->addSpacerItem(new QSpacerItem(100, 0));
    mediaControlsLayout->addWidget(_fbwd);
    mediaControlsLayout->addWidget(_bwd);
    mediaControlsLayout->addWidget(_play);
    mediaControlsLayout->addWidget(_fwd);
    mediaControlsLayout->addWidget(_ffwd);
    mediaControlsLayout->addSpacerItem(new QSpacerItem(100, 0));
    mediaControls->setLayout(mediaControlsLayout);
    _play->resize(_play->width() * 2, _play->height());

    _segmentedButton = new QtSegmentControl();
    _segmentedButton->setCount(2);
    _segmentedButton->setSegmentText(0, tr("Authoring"));
    _segmentedButton->setSegmentText(1, tr("Live"));
    _segmentedButton->setSelectionBehavior(QtSegmentControl::SelectOne);

    QWidget *_liveWidgets = new QWidget(this);
    //    QHBoxLayout* metaActionLayout = new QHBoxLayout();
    //    QPushButton* addAffordanceButton = new QPushButton("Copy Selected Affordance for Authoring");
    //    metaActionLayout->addWidget(addAffordanceButton);
    //    QPushButton* bindButton = new QPushButton("Bind To...");
    //    metaActionLayout->addWidget(bindButton);
    //    _liveWidgets->setLayout(metaActionLayout);

    QWidget *_authoringWidgets = new QWidget(this);
    //    QHBoxLayout* authoringWidgetsLayout = new QHBoxLayout();
    //    QPushButton* something = new QPushButton("authoring view button");
    //    metaActionLayout->addWidget(something);
    //    _authoringWidgets->setLayout(authoringWidgetsLayout);

    QGroupBox *widgetWrapper = new QGroupBox();
    widgetWrapper->setStyleSheet("QGroupBox { border: 1px solid gray; border-radius: 0px; padding: 0px; margin: 0px; background-color: black; }");
    QVBoxLayout *widgetWrapperLayout = new QVBoxLayout();
    widgetWrapperLayout->setSpacing(0);
    widgetWrapperLayout->setMargin(0);
    widgetWrapperLayout->addWidget(&_widget_opengl);
    widgetWrapperLayout->addWidget(mediaControls);
    widgetWrapper->setLayout(widgetWrapperLayout);

    QVBoxLayout *rightsidelayout = new QVBoxLayout();
    QWidget *rightside = new QWidget();
    _scrubber = new DefaultValueSlider(Qt::Horizontal, this);
    _scrubber->setRange(0, 1000);

    _authoringState._selected_pcr_gui = QtPointContactRelationPtr(new QtPointContactRelation());
    _authoringState._selected_pcr_gui->setPointContactRelation(getCurrentPCR());
    connect(_authoringState._selected_pcr_gui.get(), SIGNAL(activatedSignal()),
            this, SLOT(updatePointContactRelation()));
    rightsidelayout->addWidget(_authoringState._selected_pcr_gui->getPanel());

    //    rightsidelayout->addWidget(_jointSlider);
    rightsidelayout->addWidget(_segmentedButton);
    rightsidelayout->addWidget(_liveWidgets);
    rightsidelayout->addWidget(_authoringWidgets);
    rightsidelayout->addWidget(widgetWrapper);
    rightsidelayout->addWidget(_scrubber);
    rightside->setLayout(rightsidelayout);

    splitter->addWidget(leftside);
    splitter->addWidget(rightside);
    splitter->setStretchFactor(1, 1000);

    layout->addWidget(splitter);
    vbox->addStretch(1);
    leftside->setLayout(vbox);

    // fix the borders on the group boxes
    this->setStyleSheet("QGroupBox { border: 1px solid gray; border-radius: 3px; padding: 5px; } "
                        "QGroupBox::title { background-color: transparent; "
                        "subcontrol-position: top left; /* position at the top left*/ "
                        "padding:2 5px; } ");

    this->setLayout(layout);

    // wire up the buttons
    connect(planbutton, SIGNAL(released()), this, SLOT(requestMotionPlan()));
    //connect(_segmentedButton, SIGNAL(segmentSelected(int)), this, SLOT(changeMode()));

    connect(_play, SIGNAL(released()), this, SLOT(mediaPlay()));
    connect(_ffwd, SIGNAL(released()), this, SLOT(mediaFastForward()));
    connect(_fbwd, SIGNAL(released()), this, SLOT(mediaFastBackward()));
    connect(_fwd, SIGNAL(released()), this, SLOT(mediaForward()));
    connect(_bwd, SIGNAL(released()), this, SLOT(mediaBackward()));

    connect(deletebutton, SIGNAL(released()), this, SLOT(handleDeleteConstraint()));
    connect(_moveUpButton, SIGNAL(released()), this, SLOT(handleMoveUp()));
    connect(_moveDownButton, SIGNAL(released()), this, SLOT(handleMoveDown()));
    connect(addconstraintbutton, SIGNAL(released()), this, SLOT(handleAddConstraint()));

    connect(savebutton, SIGNAL(released()), this, SLOT(handleSaveAction()));
    connect(loaddiff, SIGNAL(released()), this, SLOT(handleLoadAction()));
    connect(loadfromcombo, SIGNAL(released()), this, SLOT(handleLoadActionCombo()));

    connect(_scrubber, SIGNAL(valueChanged(int)), this, SLOT(handleScrubberChange()));

    //TODO remove demoware
    connect(testIKPublishButton, SIGNAL(released()), this, SLOT(requestIKSolution()));
    //end demoware

    //sets the enable/disable for media buttons
    handleScrubberChange();
}

MainWindow::~MainWindow()
{

}

void
MainWindow::
handleLoadActionCombo() 
{
    handleLoadAction(_filesList->currentText().toStdString());
}

void 
MainWindow::
handleLoadAction() 
{
    QString fileName = QFileDialog::getOpenFileName(this,
                       tr("Open Action"), "", tr("Action XML Files (*.xml)"));

    if (fileName.toStdString() == "")
    {
        return;
    }
    handleLoadAction(fileName.toStdString());
}

void
MainWindow::
handleLoadAction(std::string fileName)
{
    std::vector<ConstraintMacroPtr> revivedConstraintMacros;
    std::vector<AffConstPtr> revivedAffordances;

#ifdef DATABASE
    DatabaseManager::retrieve(fileName, revivedAffordances, revivedConstraintMacros);

    // todo: Chris, store the action name 
    //std::string action_name;
    //_actionName->setText(); //QString::fromStdString(action_name));

    printf("done retrieving.\n");
    _worldState.affordances.clear();


    printf("done clearing world state affordances.\n");

    for (int i = 0; i < (int)revivedAffordances.size(); i++)
    {
        _worldState.affordances.push_back(revivedAffordances[i]);
    }

    _authoringState._all_gui_constraints.clear();
    printf("done clearing authoring state gui constraints.\n");

    for (int i = 0; i < (int)revivedConstraintMacros.size(); i++)
    {
        printf("making Qt4ConstraintMacroPtr %i\n", i);
        _authoringState._all_gui_constraints.push_back((Qt4ConstraintMacroPtr)new Qt4ConstraintMacro(revivedConstraintMacros[i], i));
    }

    printf("Now calling rebuild gui from state.");
    rebuildGUIFromState(_authoringState, _worldState);

#endif
}

void
MainWindow::
handleSaveAction()
{
    QString fileName = QFileDialog::getSaveFileName(this,
                       tr("Save Action"), _actionName->text() + ".xml", tr("Action XML Files (*.xml)"));

    if (fileName.toStdString() == "")
    {
        return;
    }

#ifdef DATABASE
    vector<ConstraintMacroPtr> all_constraints;

    for (int i = 0; i < (int)_authoringState._all_gui_constraints.size(); i++)
    {
        all_constraints.push_back(_authoringState._all_gui_constraints[i]->getConstraintMacro());
    }

    // todo: hack
    for (uint i = 0; i < _authoringState._all_gui_constraints.size(); i++)
    {
        cout << "gui constraint i " << all_constraints[i]->getAtomicConstraint()->getAffordance().get() << endl;

        for (uint j = 0; j < _worldState.affordances.size(); j++)
        {
            cout << "......vs affordance j " << _worldState.affordances[j].get() << endl;

            if (_worldState.affordances[j] == all_constraints[i]->getAtomicConstraint()->getAffordance())
            {
                //	 if (_worldState.affordances[j]->getGUIDAsString() == all_constraints[i]->getAtomicConstraint()->getAffordance()->getGUIDAsString()) {
                cout << "matched i, j " << i << ", " << j << endl;
                all_constraints[i]->getAtomicConstraint()->setAffordance(
                                       _worldState.affordances[j]->getGlobalUniqueId());
            }
        }
    }

    /* for (uint j = 0; j < _worldState.affordances.size(); j++) {
         cout << "......vs affordance j " << j << " : " << (*_worldState.affordances[j] << endl;
         //if (_worldState.affordances[j] == all_constraints[i]->getAtomicConstraint()->getAffordance()) {
     }
    */

    DatabaseManager::store(fileName.toStdString(), _worldState.affordances, all_constraints);

    // make sure the new file shows up in our list
    updateFilesListComboBox();

#endif //DATABASE
}


int
MainWindow::
getSelectedGUIConstraintIndex()
{
    if (_authoringState._selected_gui_constraint == NULL)
    {
        return -1;
    }

    std::vector<Qt4ConstraintMacroPtr> &constraints = _authoringState._all_gui_constraints;
    std::vector<Qt4ConstraintMacroPtr>::iterator it = std::find(
                constraints.begin(), constraints.end(),
                _authoringState._selected_gui_constraint);

    if (it != constraints.end())
    {
        int i = it - constraints.begin();
        return i;
    }
    else
    {
        return -1;
    }
}

void
MainWindow::
handleDeleteConstraint()
{
    int i = getSelectedGUIConstraintIndex();

    if (i >= 0)
    {
        _authoringState._all_gui_constraints.erase(_authoringState._all_gui_constraints.begin() + i);
        rebuildGUIFromState(_authoringState, _worldState);
    }
}

void
MainWindow::
handleAddConstraint()
{
    if (_worldState.affordances.size() == 0 || _worldState.manipulators.size() == 0)
    {
        QMessageBox msgBox;
        msgBox.setText("Need at least one affordance and manipulator to add a constraint");
        msgBox.exec();
        return;
    }

    AffConstPtr left = _worldState.affordances[0];
    ManipulatorStateConstPtr manip = _worldState.manipulators[0];

    PointContactRelationPtr relstate(new PointContactRelation());
    AtomicConstraintPtr new_atomic(new ManipulationConstraint(left->getGlobalUniqueId(),
                                                              manip->getGlobalUniqueId(), 
                                                              &_worldState, //passed in as AffordanceManipMap interface
                                                              relstate));
    stringstream ss;
    ss << _newConstraintCounter;
    _newConstraintCounter += 1;
    ConstraintMacroPtr new_constraint(new ConstraintMacro("UntitedConstraint" + ss.str(), new_atomic));

    // compute the number
    int index = _authoringState._all_gui_constraints.size();
    _authoringState._all_gui_constraints.push_back((Qt4ConstraintMacroPtr)new Qt4ConstraintMacro(new_constraint, index));
    rebuildGUIFromState(_authoringState, _worldState);
    setSelectedAction(_authoringState._all_gui_constraints.back().get());
}

void
MainWindow::
moveQt4Constraint(bool up)
{
    int i = getSelectedGUIConstraintIndex();
    std::vector<Qt4ConstraintMacroPtr> &constraints = _authoringState._all_gui_constraints;

    if ((up && i > 0) || ((! up) && (i < (int)constraints.size() - 1)))
    {
        int j = i + (up ? -1 : 1);
        std::cout << "swapping " << i << " with " << j << std::endl;
        //std::swap(constraints[i], constraints[i + (up ? i - 1 : i + 1)]);
        //rebuildGUIFromState(_authoringState, _worldState);
    }
}

void
MainWindow::
handleMoveUp()
{
    moveQt4Constraint(true);
}

void
MainWindow::
handleMoveDown()
{
    moveQt4Constraint(false);
}

void
MainWindow::
updateScrubber()
{
    _MaxEndTime = 0.0;
    _scrubber->clearTicks();

    for (std::vector<int>::size_type i = 0; i != _authoringState._all_gui_constraints.size(); i++)
    {
      double end_time = _authoringState._all_gui_constraints[i]->getConstraintMacro()->getTimeUpperBound();
      if (end_time > _MaxEndTime)
	{
	  _MaxEndTime = end_time;
	}
    }
    
    for (std::vector<int>::size_type i = 0; i != _authoringState._all_gui_constraints.size(); i++)
    {
      float start_time = _authoringState._all_gui_constraints[i]->getConstraintMacro()->getTimeLowerBound();
      float end_time = _authoringState._all_gui_constraints[i]->getConstraintMacro()->getTimeUpperBound();

      _scrubber->addMajorTick(start_time / _MaxEndTime);
      _scrubber->addMajorTick(end_time / _MaxEndTime);
    }
    
    float selected_start_time = _authoringState._selected_gui_constraint->getConstraintMacro()->getTimeLowerBound() / _MaxEndTime;
    float selected_end_time = _authoringState._selected_gui_constraint->getConstraintMacro()->getTimeUpperBound() / _MaxEndTime;
    
    _scrubber->setSelectedRange(selected_start_time, selected_end_time);
    _scrubber->update();
    handleScrubberChange();
}

void
MainWindow::
handleScrubberChange()
{
  for (std::vector<int>::size_type i = 0; i != _authoringState._all_gui_constraints.size(); i++)
    {
      double start_time = _authoringState._all_gui_constraints[i]->getConstraintMacro()->getTimeLowerBound() / _MaxEndTime;
      double end_time = _authoringState._all_gui_constraints[i]->getConstraintMacro()->getTimeUpperBound() / _MaxEndTime;
      float scrubber_time = float(_scrubber->value()) / float((_scrubber->maximum() - _scrubber->minimum()));
      bool active = scrubber_time >= start_time && scrubber_time < end_time;
      _authoringState._all_gui_constraints[i]->getPanel()->setConstraintActiveStatus(active);
    }

  _fwd->setEnabled(_scrubber->hasNextTick());
  _bwd->setEnabled(_scrubber->hasPreviousTick());
  _ffwd->setEnabled(_scrubber->hasNextMajorTick());
  _fbwd->setEnabled(_scrubber->hasPreviousMajorTick());
}


/*
 * This function is connected to the activated() signal of all of the
 * Qt4ConstraintMacros. It is called whenever a piece of constraint state
 * is modified in the GUI.
 */
void
MainWindow::
setSelectedAction(Qt4ConstraintMacro *activator)
{
    if (activator == NULL) 
    {
        return;
    }
    //int selected_index = -1;
    //bool noChange = false;

    for (std::vector<int>::size_type i = 0; i != _authoringState._all_gui_constraints.size(); i++)
    {
        if (activator == _authoringState._all_gui_constraints[i].get())
        {
            _authoringState._all_gui_constraints[i]->setSelected(true);

            if (_authoringState._selected_gui_constraint != _authoringState._all_gui_constraints[i])
            {
                _authoringState._selected_gui_constraint = _authoringState._all_gui_constraints[i];
            }
        }
        else
        {
            _authoringState._all_gui_constraints[i]->setSelected(false);
        } 
    }

    // update the scrubber tick lines
    updateScrubber();

    // highlight the affordance
    selectedOpenGLObjectChanged(_authoringState._selected_gui_constraint->getConstraintMacro()
                                ->getAtomicConstraint()->getAffordance()->getGUIDAsString());

    // highlight the manipulator
    _worldState.colorRobot.setSelectedLink(_authoringState._selected_gui_constraint->getConstraintMacro()
                                           ->getAtomicConstraint()->getManipulator()->getName());

    // update any flying manipulators
    updateFlyingManipulators();

    updatePointVisualizer();

    //update the inequality constraints display
    setP2PFromCurrConstraint();
}

/*
 * Get the toggle panels from the Qt4ConstraintMacro objects and populate the gui
 */
void
MainWindow::
rebuildGUIFromState(AuthoringState &state, WorldStateView &worldState)
{
    // delete all of the current constraint's toggle panel widgets
    //    std::cout << "deleting rendered children " << std::endl;
    //    qDeleteAll(_constraint_container->findChildren<QWidget*>());

    for (std::vector<int>::size_type i = 0; i != state._all_gui_constraints.size(); i++)
    {
        state._all_gui_constraints[i]->setModelObjects(worldState.affordances, worldState.manipulators);

        if (! state._all_gui_constraints[i]->isInitialized())   // have we already added this tp?
        {
            TogglePanel *tp = state._all_gui_constraints[i]->getPanel();
            // TODO: the connect() call here appears to be quadratic in the
            // number of previously connected signals
            connect(state._all_gui_constraints[i].get(),
                    SIGNAL(activatedSignal(Qt4ConstraintMacro *)),
                    this, SLOT(setSelectedAction(Qt4ConstraintMacro *)));
            //		    Qt::UniqueConnection);
            //	    std::cout << "adding constraint #" << i << std::endl;
            _constraint_vbox->addWidget(tp);
        }
    }

    if (state._all_gui_constraints.size() == 0)
    {
        setSelectedAction(NULL);
    } 
    else 
    {
        setSelectedAction(state._all_gui_constraints[0].get()); // Qt ugliness requires .get
    }
}

/*
 * world state affordance updating
 */
void
MainWindow::
affordanceUpdateCheck()
{
    int origSize = _worldState.affordances.size();

    //---------grab latest from server
    vector<AffConstPtr> affsFromServer;    
    _worldState.affServerWrapper.getAllAffordances(affsFromServer);

    //split up the car into pieces if it's their
    vector<AffConstPtr> affsWithSplitCar;
    for(uint i = 0; i < affsFromServer.size(); i++)
      {
        if (affsFromServer[i]->getOTDFType() != AffordanceState::CAR)
          {
            affsWithSplitCar.push_back(affsFromServer[i]);
            continue;
          }

        //spilt up the car
        //todo
      }


    //udpate our internal list
    _worldState.affordances = affsWithSplitCar;

    if ((int)_worldState.affordances.size() == origSize) //todo : use a better check than size (like "==" on each affordance if the sizes are equal )
    {
        return;
    }

    cout << "\n\n\n size of _worldState.affordances changed \n\n" << endl;
    handleAffordancesChanged();
}

void
MainWindow::
selectedOpenGLObjectChanged(const std::string &modelGUID)
{
    selectedOpenGLObjectChanged(modelGUID, Eigen::Vector3f(0, 0, 0));
}


/*
 * Select the OpenGL object corresponding to this affordance by highlighting it
 * in the GUI. Connected to the raycastCallback signal from the OpenGL widget pane.
 */
void
MainWindow::
selectedOpenGLObjectChanged(const std::string &modelGUID, Eigen::Vector3f hitPoint)
{
    cout << " hit " << modelGUID << " at point " << hitPoint.x() << ", " << hitPoint.y() << ", " << hitPoint.z() << endl;

    if (_authoringState._selected_gui_constraint == NULL)
    {
        return;
    }

    // set the selected manipulator or affordance in the currently selected constraint pane
    // begin by getting the ModelState object from the selected openGL object
    // TODO: slow; use map objectsToModels
    bool wasAffordance = false;

    for (int i = 0; i < (int)_worldState.affordances.size(); i++)
    {
        if (_worldState.affordances[i]->getGUIDAsString() == modelGUID)
        {
            //std::cout << " setting affordance" << std::endl;
            _authoringState._selected_affordance_guid = modelGUID;
            _authoringState._selected_gui_constraint->getConstraintMacro()->getAtomicConstraint()->setAffordance(
                                                          _worldState.affordances[i]->getGlobalUniqueId());

            _authoringState._selected_gui_constraint->updateElementsFromState();
            wasAffordance = true;
            break;
        }
    }

    if (! wasAffordance)
    {
        for (int i = 0; i < (int)_worldState.manipulators.size(); i++)
        {
            if (_worldState.manipulators[i]->getGUIDAsString() == modelGUID)
            {
                //std::cout << " setting manipulator" << std::endl;
                _authoringState._selected_manipulator_guid = modelGUID;
                _authoringState._selected_gui_constraint->getConstraintMacro()->getAtomicConstraint()
                  ->setManipulator(_worldState.manipulators[i]->getGlobalUniqueId());
                _authoringState._selected_gui_constraint->updateElementsFromState();
                break;
            }
        }
    }

    // TODO: refractor out into separate method
    // highlight the object in the GUI
    for (uint i = 0; i < _worldState.glObjects.size(); i++)
    {
        if (_worldState.glObjects[i]->id() == _authoringState._selected_manipulator_guid ||
            _worldState.glObjects[i]->id() == _authoringState._selected_affordance_guid)
        {
            _worldState.glObjects[i]->setHighlighted(true);
        }
        else
        {
            _worldState.glObjects[i]->setHighlighted(false);
        }
    }

    _widget_opengl.update();

    if (_authoringState._selected_gui_constraint != NULL && ! (hitPoint.x() == 0 && hitPoint.y() == 0 && hitPoint.z() == 0))
    {
        // set the contact point for the relation
        RelationStatePtr rel = _authoringState._selected_gui_constraint->getConstraintMacro()->getAtomicConstraint()->getRelationState();

        if (rel->getRelationType() == RelationState::POINT_CONTACT)
        {
            PointContactRelationPtr pc = boost::static_pointer_cast<PointContactRelation>(rel);
            if (wasAffordance)
            {
                pc->setPoint2(hitPoint);
            }
            else
            {
                pc->setPoint1(hitPoint);
            }
            updatePointVisualizer();
            _authoringState._selected_pcr_gui->setPointContactRelation(pc);
        }
    }
    return;
}

void
MainWindow::
mediaFastForward()
{
    _scrubber->advanceToNextMajorTick();
}

void
MainWindow::
mediaFastBackward()
{
    _scrubber->returnToPreviousMajorTick();
}

void
MainWindow::
mediaForward()
{
    _scrubber->advanceToNextTick();
}

void
MainWindow::
mediaBackward()
{
    _scrubber->returnToPreviousTick();
}


void
MainWindow::
mediaPlay()
{
    if (_isPlaying)
    {
        QPixmap pixmap3(":/trolltech/styles/commonstyle/images/media-pause-32.png");
        _play->setIcon(QIcon(pixmap3));
        _play->setIconSize(pixmap3.rect().size());
        _scrubberTimer->start(50); // 20 HZ
    }
    else
    {
        QPixmap pixmap3(":/trolltech/styles/commonstyle/images/media-play-32.png");
        _play->setIcon(QIcon(pixmap3));
        _play->setIconSize(pixmap3.rect().size());
        _scrubberTimer->stop();
    }

    _isPlaying = ! _isPlaying;
}

void
MainWindow::
nextKeyFrame()
{
    if (_scrubber->value() == _scrubber->maximum())
    {
        _scrubberTimer->stop();
    }
    else
    {
        _scrubber->setValue(_scrubber->value() + 1);
    }

    double fraction = (double)_scrubber->value() / (double)_scrubber->maximum();
    double acc = 0.0;

    // round fraction to the nearest constraint and select it
    for (std::vector<int>::size_type i = 0; i != _authoringState._all_gui_constraints.size(); i++)
    {
        acc += _authoringState._all_gui_constraints[i]->getConstraintMacro()->getTimeUpperBound();

        if ((acc / _MaxEndTime) > fraction)
        {
	  //setSelectedAction(_authoringState._all_gui_constraints[i].get()); // todo ugly; have to because of qt
            break;
        }
    }
}

void
MainWindow::
getContactGoals(std::vector<drc::contact_goal_t> *contact_goals)
{
    std::vector<Qt4ConstraintMacroPtr> all_gui_constraints = _authoringState._all_gui_constraints;

    for (int i = 0; i < (int) all_gui_constraints.size(); i++)
    {
        std::vector<drc::contact_goal_t> constraint_contact_goals = all_gui_constraints[i]->getConstraintMacro()->toLCM();

        for (int j = 0; j < (int) constraint_contact_goals.size(); j++)
        {
            contact_goals->push_back(constraint_contact_goals[j]);
        }
    }
}

void
MainWindow::
requestMotionPlan()
{
    std::vector<drc::contact_goal_t> contact_goals;
    MainWindow::getContactGoals(&contact_goals);

    drc::action_sequence_t actionSequence;

    // TODO: get the proper time
    actionSequence.utime = 0;
    actionSequence.robot_name = ROBOT_NAME;
    _worldState.state_gfe.to_lcm(&(actionSequence.q0));
    actionSequence.q0.robot_name = ROBOT_NAME;

    actionSequence.num_contact_goals = (int) contact_goals.size();
    actionSequence.contact_goals = contact_goals;
    actionSequence.ik_time = 0;
    _theLcm->publish(REQUEST_MOTION_PLAN_CHANNEL, &actionSequence);
}

void
MainWindow::
requestIKSolution()
{
    std::vector<drc::contact_goal_t> contact_goals;
    MainWindow::getContactGoals(&contact_goals);

    drc::action_sequence_t actionSequence;

    // TODO: get the proper time
    actionSequence.utime = 0;
    actionSequence.robot_name = ROBOT_NAME;
    _worldState.state_gfe.to_lcm(&(actionSequence.q0));
    actionSequence.q0.robot_name = ROBOT_NAME;

    actionSequence.num_contact_goals = (int) contact_goals.size();
    actionSequence.contact_goals = contact_goals;
    actionSequence.ik_time = (float(_scrubber->value()) / float(_scrubber->maximum()) * _MaxEndTime);
    _theLcm->publish(REQUEST_IK_SOLUTION_CHANNEL, &actionSequence);
}

void
MainWindow::
changeMode()
{
    cout << "hi htere" << endl;

    if (_segmentedButton->isSegmentSelected(0))   // authoring view
    {
        _authoringWidgets->show();
        //_liveWidgets->hide();
    }
    else if (_segmentedButton->isSegmentSelected(1))   // live view
    {
        //_authoringWidgets->hide();
        //_liveWidgets->show();
    }

    cout << "done there" << endl;
}


void
MainWindow::
updateFlyingManipulators()
{
    // for each relation, add an appropriate "flying" manipulator
    for (uint i = 0; i < _worldState.manipulators.size(); i++)
    {
        string man_link_name = _worldState.manipulators[i]->getName();
        // TODO (mfleder, ine) here is : to pass the manipulator through
        // the currently selected *relation* and render the outcome

        // find any relations that reference this link. for each relation, spawn
        // a new flying affordance.
        for (std::vector<int>::size_type i = 0; i != _authoringState._all_gui_constraints.size(); i++)
        {
            RelationStatePtr rel = _authoringState._all_gui_constraints[i]->getConstraintMacro()->getAtomicConstraint()->getRelationState();
            string constraint_link_name = _authoringState._all_gui_constraints[i]->getConstraintMacro()->getAtomicConstraint()->getManipulator()->getName();

            if (constraint_link_name == man_link_name && rel->getRelationType() == RelationState::OFFSET) 
            {
                // TODO: mfleder, ine
/*
                cout << "ITS AN OFFSET " << endl;
                //KDL::Frame shifted_frame = _worldState.manipulators[i]->getLinkFrame();
                //shifted_frame.p = KDL::Vector(shifted_frame.p + rel->getTranslation());
                //shifted_frame = KDL::Vector(shifted_frame.p.x() + 0.25, shifted_frame.p.y(), shifted_frame.p.z());
                // TODO: this is a hack, we assume that the object is an OpenGL_Object_DAE. 
                // that's not necessarily true! It could be any subclass of OpenGL_Object
                OpenGL_Object_DAE* flying_link = new OpenGL_Object_DAE(
                    *((OpenGL_Object_DAE*)_worldState.colorRobot.getOpenGLObjectForLink(man_link_name)));
                OffsetRelationPtr offset = boost::dynamic_pointer_cast<OffsetRelation>(rel);
                //flying_link->set_transform(offset->getFrame());

                _widget_opengl.opengl_scene().add_object(*flying_link);
                _worldState.glObjects.push_back(flying_link);
*/
            }
        }
    }
}

void
MainWindow::
updateRobotState(const lcm::ReceiveBuffer* rbuf, 
                 const std::string& channel,
                 const drc::robot_state_constraint_checked_t *new_robot_state)
{
    _worldState.state_gfe.from_lcm(new_robot_state->robot_state);
    _worldState.colorRobot.set(_worldState.state_gfe);

    // seems redundant but is necessary to get pose to work - don't remove!
    //_worldState.colorRobot.set(*new_robot_state);
    handleAffordancesChanged();

    for (uint i = 0; i < new_robot_state->num_constraints && i < _authoringState._all_gui_constraints.size(); i++)
    {
        TogglePanel::PlannerStatus status;
        int value  = new_robot_state->constraints_satisfied[i];
        if (value == 0)
        {
            status = TogglePanel::PLANNER_NOT_OK;
        }
        else if ( value == 1)
        {
            status = TogglePanel::PLANNER_OK;
        }
        else
        {
            status = TogglePanel::PLANNER_UNKNOWN;
        }
        _authoringState._all_gui_constraints[0]->getPanel()->setPlannerStatus(status);
    }
}

void
MainWindow::
updatePointVisualizer() 
{
    if (_authoringState._selected_gui_constraint == NULL)
        return;

    // TODO: This should probably be abstracted into a class called
    // OpenGL_PointContactRelation or something
    RelationStatePtr rel = _authoringState._selected_gui_constraint->getConstraintMacro()->getAtomicConstraint()->getRelationState();

    // update the coordinate axis visualizer
    if (rel->getRelationType() == RelationState::POINT_CONTACT)
    {
        PointContactRelationPtr prel = boost::dynamic_pointer_cast<PointContactRelation>(rel);        

        KDL::Frame trans(KDL::Rotation::Quaternion(0, 0, 0, 1.0), KDL::Vector(prel->getPoint1().x(), prel->getPoint1().y(), prel->getPoint1().z()));
        point_contact_axis.set_transform(trans);

        KDL::Frame trans2(KDL::Rotation::Quaternion(0, 0, 0, 1.0), KDL::Vector(prel->getPoint2().x(), prel->getPoint2().y(), prel->getPoint2().z()));
        point_contact_axis2.set_transform(trans2);
    }
}

void
MainWindow::
keyPressEvent(QKeyEvent *event) {
/*
    if (event->key() == Qt::Key_Right || event->key() == Qt::Key_Down)
    {
        mediaFastForward();
    }
    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right)
    {
        mediaFastBackward();
    }
*/
}

void
MainWindow::
updateFilesListComboBox() 
{
    _filesList->clear();
    // list *.xml files in directory, but them in combo box
    std::vector<std::string> names = UtilityFile::getFilenamesInDirectory(".", ".xml");
    for (uint i = 0; i < names.size(); i++) 
    {
        _filesList->insertItem(0, QString::fromStdString(names[i]));
    }
}


/**Get the current point contact relation being edited or null if no 
such PCR*/
PointContactRelationPtr MainWindow::getCurrentPCR()
{
  if (_authoringState._selected_gui_constraint == NULL)
    return PointContactRelationPtr();
  
  // get the currently selected constraint and verify its relation is of type POINT_CONTACT
  RelationStatePtr rel = _authoringState._selected_gui_constraint->getConstraintMacro()->getAtomicConstraint()->getRelationState();
  if (rel->getRelationType() != RelationState::POINT_CONTACT)
    return PointContactRelationPtr();

  return boost::dynamic_pointer_cast<PointContactRelation>(rel);
}

/**sets the inequality constraints menu (x-axis, ...) from the currently selected gui constraint*/
void MainWindow::setP2PFromCurrConstraint()
{
    if (_authoringState._selected_gui_constraint == NULL)
        return;

    // get the currently selected constraint and verify its relation is of type POINT_CONTACT
    RelationStatePtr rel = _authoringState._selected_gui_constraint->getConstraintMacro()->getAtomicConstraint()->getRelationState();

    if (rel->getRelationType() != RelationState::POINT_CONTACT) 
    {
        //set to undefined
        return;
    }

    // update the relation
    PointContactRelationPtr prel = boost::dynamic_pointer_cast<PointContactRelation>(rel);
    _authoringState._selected_pcr_gui->setPointContactRelation(prel); // automatically calls updateGUIFromState();
}

void MainWindow::updatePointContactRelation() {
    cout << "received signal!" << endl;
    _authoringState._selected_gui_constraint->
        getConstraintMacro()->getAtomicConstraint()->setRelationState(
        _authoringState._selected_pcr_gui->getPointContactRelation());

    // redraw it
    updatePointVisualizer();
}