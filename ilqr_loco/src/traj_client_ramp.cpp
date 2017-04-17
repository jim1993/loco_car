#include "traj_client.h"

ilqr_loco::TrajExecGoal TrajClient::rampGenerateTrajectory(nav_msgs::Odometry prev_state,
                                                           nav_msgs::Odometry cur_state) {

  // std::cout << timeout_ << '\n';

  double dt = (cur_state.header.stamp).toSec() - (prev_state.header.stamp).toSec();
  double yaw = tf::getYaw(cur_state.pose.pose.orientation);
  ROS_INFO("Yaw = %f", yaw);

  // PID control for vehicle heading
  double error = 0 - yaw;
  cur_integral_ += error*dt;
  double output = kp_*error + std::max(-0.25,std::min(ki_*cur_integral_,0.25)) + std::max(-0.1,std::min(kd_*(error-prev_error_)/dt,0.1));
  ROS_INFO("P = %f,  |  I = %f,  |  D = %f", kp_*error, std::max(-0.25,std::min(ki_*cur_integral_,0.25)), std::max(-0.1,std::min(kd_*(error-prev_error_)/dt,0.1)));
  ROS_INFO("Output = %f", output);
  prev_error_ = error;


  // Generate goal
  cur_vel_ += accel_*dt;
  double v = cur_state.twist.twist.linear.x + accel_*dt + 0.5;
  v = cur_vel_<target_vel_ ? cur_vel_ : target_vel_;
  ROS_INFO("v = %f", v);

  ilqr_loco::TrajExecGoal goal;

  geometry_msgs::Twist control_msg;
  FillTwistMsg(control_msg, v, output);
  goal.traj.commands.push_back(control_msg);

  nav_msgs::Odometry state_msg;
  double expected_x = start_state_.pose.pose.position.x + 0.5*accel_*start_time_.toSec()*start_time_.toSec();
  double expected_y = start_state_.pose.pose.position.y;

  FillOdomMsg(state_msg, expected_x, expected_y, 0, v, 0, 0); //0s: yaw, vy, w
  goal.traj.states.push_back(state_msg);

  ++T_;
  ramp_goal_flag_ = (v >= target_vel_) ? true : false;  // Ramp completion flag
  FillGoalMsgHeader(goal);

  return goal;
}


void TrajClient::rampPlan() {

  if(ros::Time::now() - start_time_ < ros::Duration(timeout_))
  {
    ilqr_loco::TrajExecGoal goal = rampGenerateTrajectory(prev_state_, cur_state_);
    SendTrajectory(goal);
  }
  else
  {
    // Stop car after ramp timeout
    ROS_INFO("Timeout exceeded, stopping car");
    SendZeroCommand();
    mode_ = 0;
  }
}
