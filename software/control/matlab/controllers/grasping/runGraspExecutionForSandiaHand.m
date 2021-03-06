function runGraspExecutionForSandiaHand()
megaclear
[r_left,r_right] = createSandiaManips();

nq_l = r_left.getNumStates/2;
l_jointNames = r_left.getStateFrame.coordinates(1:nq_l);
nq_r = r_right.getNumStates/2;
r_jointNames = r_right.getStateFrame.coordinates(1:nq_r);

lcmcoder = JLCMCoder(drc.control.GraspStateCoder('sandia',l_jointNames,r_jointNames));
nx=19+nq_l+nq_r;

channel = 'COMMITTED_GRASP';
disp(channel);
grasp_state_listener=LCMCoordinateFrame('sandia',lcmcoder,'x');
setDefaultChannel(grasp_state_listener,channel);
grasp_state_listener.subscribe(channel);

floating =false;
l_jointNames = r_left.getStateFrame.coordinates(1:nq_l);
r_jointNames = r_right.getStateFrame.coordinates(1:nq_r);

Kp = 0.25*[15  10  10   15  10  10    15  10  10 15  10  10]';
Kd = [0.75 0.45 0.45 0.75 0.45 0.45   0.75 0.45 0.45 0.75 0.45 0.45]';

l_coder = JLCMCoder(drc.control.SandiaJointCommandCoder('sandia',floating,'left', l_jointNames,Kp,Kd));
l_hand_joint_cmd_publisher=LCMCoordinateFrame('sandia_left',l_coder,'q');
r_coder = JLCMCoder(drc.control.SandiaJointCommandCoder('sandia',floating,'right', r_jointNames,Kp,Kd));
r_hand_joint_cmd_publisher=LCMCoordinateFrame('sandia_right',r_coder,'q');


Kp2 = [15  0 0    15  0 0    15 0 0    15 0 0]';
Kd2 = [1.5 0 0    1.5 0 0   1.5 0 0    1.5 0 0]';
pos_control_flag = [1.0 0 0    1.0 1.0 1.0   1.0 0 0   1.0 1.0 1.0]';
%pos_control_flag = [1.0 0 0    1.0 0.0 0.0   1.0 0 0   1.0 1.0 1.0]'; % where ever there is zero we are doing mixed control
%Kd(find(pos_control_flag==0))= 0;
%Kp(find(pos_control_flag==0))= 10;
msg_timeout = 5; % ms


init = false;
while(1)
    if (init==false)
        init=true;
        out_string = 'Sandia Grasp Controller: Ready'; disp(out_string); send_status(3,0,0,out_string);
    end
    
    [x,ts] = getNextMessage(grasp_state_listener,msg_timeout);%getNextMessage(obj,timeout)
    if (~isempty(x))
        out_string = 'Grasp: Got msg'; disp(out_string); send_status(3,0,0,out_string);
        fprintf('received message at time %f\n',ts);
        %fprintf('state is %f\n',x);
        
        msg = grasp_state_listener.lcmcoder.encode(ts,x);
        if(msg.power_grasp==1.0)
         pos_control_flag = [1.0 0 0    1.0 0.0 0.0   1.0 0 0   1.0 1.0 1.0]';
         %pos_control_flag = [1.0 0 1.0    1.0 0.0 1.0   1.0 0 1.0   1.0 0.0 1.0]';
        end

        rpy = quat2rpy([x(9);x(6:8)]);
        l_hand_pose = [x(3:5);rpy(1);rpy(2);rpy(3)];
        rpy = quat2rpy([x(16);x(13:15)]); 
        r_hand_pose = [x(10:12);rpy(1);rpy(2);rpy(3)];
        
        q_l = [msg.l_joint_position];   
        q_r = [msg.r_joint_position];  
        
        K_pos=Kp;
        K_vel=Kd;
        e_l =q_l*0;
        e_r =q_r*0;
        e_l(find(pos_control_flag>0)) = 0;
        e_r(find(pos_control_flag>0)) = 0;
        if(msg.power_grasp==1.0)
          torque = 10;
          K_pos(find(pos_control_flag==0))= 0;
          K_vel(find(pos_control_flag==0))= 0;
        else
          torque =1;
        end
        if(sum(msg.l_joint_position)>0)
           e_l(find(pos_control_flag==0)) = torque;   
           %K_pos(find(pos_control_flag==0))= 0;
           %K_vel(find(pos_control_flag==0))= 0;
        elseif(sum(msg.r_joint_position)>0)
           %K_pos(find(pos_control_flag==0))= 0;
           %K_vel(find(pos_control_flag==0))= 0;
           e_r(find(pos_control_flag==0)) = torque;   %pos_control_flag = [1.0 0 0    1.0 0.0 0.0   1.0 0 0   1.0 1.0 1.0]'; % where ever there is zero we are doing mixed control
%Kd(find(pos_control_flag==0))= 0;
%Kp(find(pos_control_flag==0))= 10;
%         else
%            e_l(find(pos_control_flag==0)) = -torque;   
%            e_r(find(pos_control_flag==0)) = -torque;    
        end
        
        if(msg.grasp_type==msg.SANDIA_LEFT)
            publish(l_hand_joint_cmd_publisher,ts,[K_pos;K_vel;q_l;e_l]','L_HAND_JOINT_COMMANDS');
        elseif(msg.grasp_type==msg.SANDIA_RIGHT)
            publish(r_hand_joint_cmd_publisher,ts,[K_pos;K_vel;q_r;e_r]','R_HAND_JOINT_COMMANDS');
        elseif(msg.grasp_type==msg.SANDIA_BOTH)
            publish(l_hand_joint_cmd_publisher,ts,[K_pos;K_vel;q_l;e_l]','L_HAND_JOINT_COMMANDS');
            publish(r_hand_joint_cmd_publisher,ts,[K_pos;K_vel;q_r;e_r]','R_HAND_JOINT_COMMANDS');
        end

   
        
    end
end %end while


end

%============ GRAVEYARD =====================================
        
              
%         if(sum(msg.r_joint_position)>0) %do mixed torque control
%             fprintf('torque control\n');
%             for(i=1:length(Kp2))
%                if (Kp2(i)==0),
%                  q_l(6+i)=1;
%                  q_r(6+i)=1;
%                end
%             end
%             if(msg.grasp_type==msg.SANDIA_LEFT)
%                 publish(l_hand_joint_cmd_publisher,ts,[Kp2;Kd2;q_l]','L_HAND_JOINT_COMMANDS');
%             elseif(msg.grasp_type==msg.SANDIA_RIGHT)
%                 publish(r_hand_joint_cmd_publisher,ts,[Kp2;Kd2;q_r]','R_HAND_JOINT_COMMANDS');
%             end
%         else
%             if(msg.grasp_type==msg.SANDIA_LEFT)
%                 publish(l_hand_joint_cmd_publisher,ts,[Kp;Kd;q_l]','L_HAND_JOINT_COMMANDS');
%             elseif(msg.grasp_type==msg.SANDIA_RIGHT)
%                 publish(r_hand_joint_cmd_publisher,ts,[Kp;Kd;q_r]','R_HAND_JOINT_COMMANDS');
%             end
%             
%         end
        
