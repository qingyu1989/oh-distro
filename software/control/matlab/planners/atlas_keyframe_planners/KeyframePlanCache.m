classdef KeyframePlanCache < handle
    properties
        num_breaks
        time_2_index_scale
        v_desired
        lhand_constraint_cell
        rhand_constraint_cell
        lfoot_constraint_cell
        rfoot_constraint_cell
        pelvis_constraint_cell
        head_constraint_cell
        com_constraint_cell
        qdot_desired
        s_breaks
        qtraj
        isEndPose
        isPointWiseIK
        inTeleopMode
        qsc
        num_grasp_transitions
        grasp_transition_breaks
        grasp_transition_states
        s
    end
    
    methods
        function obj = KeyframePlanCache(r)
            obj.num_breaks = 0;% corresponds to keyframes
            obj.time_2_index_scale = 1;
            obj.v_desired = 0.1;%20cm/s
            obj.qdot_desired = 15*pi/180; %10deg/s
            obj.lhand_constraint_cell = {}; % Plan Boundary Conditions
            obj.rhand_constraint_cell = {};
            obj.lfoot_constraint_cell = {};
            obj.rfoot_constraint_cell = {};
            obj.pelvis_constraint_cell = {};
            obj.head_constraint_cell = {};
            obj.com_constraint_cell = {};
            obj.s_breaks = [];
            obj.qtraj = [];
            
            % Flag indicates KeyframeAdjustmentEngine to
            % publish an single keyframe endpose instead
            % of a keyframe plan by resolving at time T.
            obj.isPointWiseIK= false;
            obj.isEndPose =false;
            obj.inTeleopMode = false;
            obj.qsc = QuasiStaticConstraint(r);
            obj.qsc = obj.qsc.setActive(false);
            
            obj.num_grasp_transitions = 0; % corresponds to grasp transitions.
            obj.grasp_transition_breaks = [];
            obj.grasp_transition_states = [];
			      obj.s = [];
        end
        
        function clearCache(obj)
            obj.isPointWiseIK= false;
            obj.isEndPose =false;
            obj.inTeleopMode = false;        
            obj.lhand_constraint_cell = {}; % Plan Boundary Conditions
            obj.rhand_constraint_cell = {};
            obj.lfoot_constraint_cell = {};
            obj.rfoot_constraint_cell = {};
            obj.pelvis_constraint_cell = {};
            obj.head_constraint_cell = {};
            obj.com_constraint_cell = {};
            obj.s_breaks = [];
            obj.qtraj = [];
            obj.num_grasp_transitions = 0; % corresponds to grasp transitions.
            obj.grasp_transition_breaks = [];
            obj.grasp_transition_states = [];
            obj.s = [];
        end
        
        function updateRobot(obj,robot)
          for i = 1:length(obj.lhand_constraint_cell)
            obj.lhand_constraint_cell{i}.updateRobot(robot);
          end
          for i = 1:length(obj.rhand_constraint_cell)
            obj.rhand_constraint_cell{i}.updateRobot(robot);
          end
          for i = 1:length(obj.lfoot_constraint_cell)
            obj.lfoot_constraint_cell{i}.updateRobot(robot);
          end
          for i = 1:length(obj.rfoot_constraint_cell)
            obj.rfoot_constraint_cell{i}.updateRobot(robot);
          end
          for i = 1:length(obj.pelvis_constraint_cell)
            obj.pelvis_constraint_cell{i}.updateRobot(robot);
          end
          for i = 1:length(obj.head_constraint_cell)
            obj.head_constraint_cell{i}.updateRobot(robot);
          end
          obj.qsc = obj.qsc.updateRobot(robot);
        end
    end
end
