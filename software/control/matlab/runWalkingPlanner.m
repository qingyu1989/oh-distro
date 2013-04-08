function runWalkingPlanner(lcm_plan, goal_x, goal_y, goal_yaw)
if nargin < 4; goal_yaw = 0.0; end
if nargin < 3; goal_y = 0.0; end
if nargin < 2; goal_x = 2.0; end
if nargin < 1; lcm_plan = true; end

addpath(fullfile(pwd,'frames'));
addpath(fullfile(getDrakePath,'examples','ZMP'));

options.floating = true;
options.dt = 0.001;
r = Atlas('../../models/mit_gazebo_models/mit_robot_drake/model_foot_contact.urdf', options);

while true 

  d = load('data/atlas_fp.mat');
  xstar = d.xstar;
  r = r.setInitialState(xstar);
  state_frame = getStateFrame(r);
  state_frame.subscribe('EST_ROBOT_STATE');

  nq = getNumDOF(r);
  x0 = xstar;
  qstar = xstar(1:nq);

  pose = [goal_x;goal_y;0;0;0;goal_yaw];
  navgoal = [pose; 20];

  if ~lcm_plan
    footsteps = planFootsteps(r, x0, navgoal, struct('plotting', true, 'interactive', false));
  else
    footstep_plan_listener = FootstepPlanListener('COMMITTED_FOOTSTEP_PLAN');
    msg ='Walking Planner: Listening for plans'; disp(msg); send_status(3,0,0,msg);
    waiting = true;
    foottraj = [];

    while waiting
      footsteps = footstep_plan_listener.getNextMessage(0);
      if (~isempty(footsteps))
        msg ='Walking Planner: plan received'; disp(msg); send_status(3,0,0,msg);
        waiting = false;
      end
      [x,~] = getNextMessage(state_frame,10);
      if (~isempty(x))
        %%% TEMP HACK FOR QUAL 1 %%%
        x(3) = x(3)-1.0;
        %%% TEMP HACK FOR QUAL 1 %%%
        x0=x;
      end
    end
  end
  [xtraj, qtraj, htraj, supptraj, comtraj, lfoottraj,rfoottraj, V, ts] = walkingPlanFromSteps(r, x0, qstar, footsteps);
  
  % publish robot plan
  msg ='Walking Planner: Publishing robot plan...'; disp(msg); send_status(3,0,0,msg);
  %%%% TMP HACK FOR QUAL 1 %%%%%
  xtraj(3,:) = xtraj(3,:)+ 1;
  %%%% TMP HACK FOR QUAL 1 %%%%%
  joint_names = r.getStateFrame.coordinates(1:getNumDOF(r));
  joint_names = regexprep(joint_names, 'pelvis', 'base', 'preservecase'); % change 'pelvis' to 'base'
  plan_pub = RobotPlanPublisher('atlas',joint_names,true,'CANDIDATE_ROBOT_PLAN');
  plan_pub.publish(ts,xtraj);

  if 0 % do proper TV linear system approach
    disp('Computing ZMP controller...');
    limp = LinearInvertedPendulum(htraj);
    [~,V] = ZMPtracker(limp,zmptraj); 
  end

  hddot = fnder(htraj,2);

  tt = 0:0.02:ts(end);
  compoints = ones(3,length(tt));
  for i=1:length(tt)
    compoints(1:2,i) = comtraj.eval(tt(i));
  end
  plot_lcm_points(compoints',[zeros(length(tt),1), ones(length(tt),1), zeros(length(tt),1)],555,'Desired COM',1,true);
  

  msg ='Walking Planner: Waiting for confirmation...'; disp(msg); send_status(3,0,0,msg);
  plan_listener = RobotPlanListener('atlas',joint_names,true,'COMMITTED_ROBOT_PLAN');
  reject_listener = RobotPlanListener('atlas',joint_names,true,'REJECTED_ROBOT_PLAN');
  waiting = true;
  execute = true;
  while waiting
    rplan = plan_listener.getNextMessage(100);
    if (~isempty(rplan))
      % for now don't do anything with it, just use it as a flag
      msg ='Walking Planner: Confirmed. Executing...'; disp(msg); send_status(3,0,0,msg);
      waiting = false;
    end
    rplan = reject_listener.getNextMessage(100);
    if (~isempty(rplan))
      % for now don't do anything with it, just use it as a flag
      disp('Plan rejected.');
      waiting = false;
      execute = false;
    else 
      plan_pub.publish(ts,xtraj);
      pause(0.5);
    end
  end

  if execute
    walking_pub = WalkingPlanPublisher('COMMITTED_WALKING_PLAN');
    walking_pub.publish(0,struct('Straj',V.S,'htraj',htraj,'hddtraj', ...
      hddot,'supptraj',supptraj,'comtraj',comtraj,...
      'lfoottraj',lfoottraj,'rfoottraj',rfoottraj));
  end
end

end
