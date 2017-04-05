#include "traj_client.h"

// Constructor
TrajClient::TrajClient(): ac_("traj_executer", true)
{
  state_sub_  = nh_.subscribe("odometry/filtered", 1, &TrajClient::stateCb, this);
  obs_sub_ = nh_.subscribe("cluster_center", 1, &TrajClient::obsCb, this);
  mode_sub_ = nh_.subscribe("client_command", 1, &TrajClient::modeCb, this);

  ROS_INFO("Waiting for action server to start.");
  ac_.waitForServer(); //will wait for infinite time
  ROS_INFO("Action server started. Send me commands from teleop_keyboard!");

  switch_flag_ = false;
  ramp_goal_flag_ = false;
  state_estimate_received_ = false;
  cur_integral_ = 0.0;
  prev_error_ = 0.0;
  T_ = 0;
  start_time_ = ros::Time::now();
  cur_vel_ = 0.5;

  //After this, this node will wait for a state estimate to start ramping up,
  //then switch to iLQR after an obstacle is seen.
}

void TrajClient::RampAndiLQR()
{
  while(!switch_flag_)
  {
    rampPlan();
    ros::spinOnce(); // Do we need this?
  }
  ilqgPlan();
}

void TrajClient::stateCb(const nav_msgs::Odometry &msg)
{
  prev_state_ = cur_state_;
  cur_state_ = msg;

  //process odometry message to turn velocities into world frame
  double theta = tf::getYaw(cur_state_.pose.pose.orientation);
  double old_vx = cur_state_.twist.twist.linear.x;
  double old_vy = cur_state_.twist.twist.linear.y;
  cur_state_.twist.twist.linear.x = cos(theta)*old_vx + sin(theta)*old_vy;
  cur_state_.twist.twist.linear.y = cos(theta+PI/2)*old_vx + sin(theta+PI/2)*old_vy;
  std::cout << "velocities: " << cur_state_.twist.twist.linear.x << ' '
    << cur_state_.twist.twist.linear.y << '\n';
  state_estimate_received_ = true;
}

void TrajClient::obsCb(const geometry_msgs::PointStamped &msg)
{
  if (msg.point.x<100)
  {
    obs_pos_.x = msg.point.x;
    obs_pos_.y = msg.point.y;
    ROS_INFO("Received obstacle message: x = %f, y = %f", obs_pos_.x, obs_pos_.y);
    switch_flag_ = true;
  }
}

void TrajClient::modeCb(const geometry_msgs::Point &msg)
{
  int command = msg.x;
  ROS_INFO("Received command %d", command);
  std::cout << state_estimate_received_ << '\n';
  if (!state_estimate_received_){
    ROS_INFO("Haven't received state info yet.");
    return;
  }
  switch (command)
  {
    case 1: {
      rampPlan();
      break;
    }
    case 2: {
      ilqgPlan();
      break;
    }
    case 3: {
      RampAndiLQR();
      break;
    }
  }
}


// void TrajClient::activeCb() {}
//
// void TrajClient::feedbackCb(const ilqr_loco::TrajExecFeedbackConstPtr& feedback) {}
//
// void TrajClient::doneCb(const actionlib::SimpleClientGoalState& state,
//                         const ilqr_loco::TrajExecResultConstPtr& result) {}

void TrajClient::SendTrajectory(ilqr_loco::TrajExecGoal &goal)
{
  ROS_INFO("Sending trajectory.");
  ac_.sendGoal(goal);
              //  ,boost::bind(&TrajClient::doneCb, this, _1, _2),
              //  boost::bind(&TrajClient::activeCb, this),
              //  boost::bind(&TrajClient::feedbackCb, this, _1));
}

//////////////////////// GENERATE RAMP START //////////////////////////////////

ilqr_loco::TrajExecGoal TrajClient::rampGenerateTrajectory(nav_msgs::Odometry prev_state,
                                                           nav_msgs::Odometry cur_state) {

  ilqr_loco::TrajExecGoal goal;
  goal.traj.header.seq = T_;
  goal.traj.header.stamp = ros::Time::now();
  goal.traj.header.frame_id = "/base_link";

  double dt = (cur_state.header.stamp).toSec() - (prev_state.header.stamp).toSec();
  double yaw = tf::getYaw(cur_state.pose.pose.orientation);
  // ROS_INFO("yaw = %f",yaw);

  // PID control for vehicle heading
  double error = 0 - yaw;
  cur_integral_ += error*dt;
  double output = kp_*error + (std::abs(cur_integral_)<1 ? ki_*cur_integral_ : 0) + (dt>0.01 ? kd_*(error-prev_error_)/dt : 0);
  // ROS_INFO("P = %f | I = %f | D = %f",kp_*error, ki_*cur_integral_, kd_*(error-prev_error_)/dt);
  // ROS_INFO("PID output = %f",output);
  prev_error_ = error;

  // Generate goal
  // ROS_INFO("Cur vel = %f,    dt = %f",cur_state.twist.twist.linear.x,dt);
  cur_vel_ += accel_*dt;
  double v = cur_state.twist.twist.linear.x + accel_*dt + 0.75;
  //v = v<target_vel_ ? v : target_vel_;
  v = cur_vel_<target_vel_ ? cur_vel_ : target_vel_;

  geometry_msgs::Twist control_msg;
  control_msg.linear.x = v;
  control_msg.angular.z = output;
  goal.traj.commands.push_back(control_msg);

  nav_msgs::Odometry traj_msg = cur_state;
  traj_msg.pose.pose.orientation.w = 1;
  traj_msg.pose.pose.orientation.x = 0;
  traj_msg.pose.pose.orientation.y = 0;
  traj_msg.pose.pose.orientation.z = 0;
  traj_msg.twist.twist.linear.x = v;
  traj_msg.twist.twist.linear.y = 0;
  traj_msg.twist.twist.angular.z = 0;
  goal.traj.states.push_back(traj_msg);

  ++T_;
  ramp_goal_flag_ = v>=target_vel_ ? true : false;  // Ramp completion flag

  return goal;
}


void TrajClient::rampPlan() {

  if(ros::Time::now() - start_time_ < ros::Duration(timeout_)) {
    ilqr_loco::TrajExecGoal goal = TrajClient::rampGenerateTrajectory(prev_state_, cur_state_);
    TrajClient::SendTrajectory(goal);
  }
  else {
    // Stop car after ramp timeout
    ROS_INFO("Timeout exceeded, stopping car");
    ilqr_loco::TrajExecGoal end_goal;
    geometry_msgs::Twist control_msg;
    control_msg.linear.x = 0.0;
    control_msg.angular.z = 0.0;
    end_goal.traj.commands.push_back(control_msg);
    end_goal.traj.commands.push_back(control_msg);
    end_goal.traj.states.push_back(cur_state_);
    end_goal.traj.states.push_back(cur_state_);
    TrajClient::SendTrajectory(end_goal);
    switch_flag_ = true;
  }
}

//////////////////////// GENERATE RAMP END //////////////////////////////////

//////////////////////// GENERATE iLQG START //////////////////////////////////

// Calls iLQG_mpc.c to generate new trajectory
ilqr_loco::TrajExecGoal TrajClient::ilqgGenerateTrajectory(nav_msgs::Odometry cur_state)
{
  ROS_INFO("Generating iLQG trajectory.");
  ilqr_loco::TrajExecGoal goal;
  goal.traj.header.seq = T_;
  goal.traj.header.stamp = ros::Time::now(); //Makes sure that action server can account for planning delay.
  goal.traj.header.frame_id = "/base_link";
  goal.traj.timestep = timestep_;

  double xd[] = {3, 0, 0, 0, 0, 0};
  std::vector<double> x_des(xd, xd+6); // Maybe this should be a member variable too?
  iLQR_gen_traj(cur_state, x_des, obs_pos_, 50, goal);
  ++T_;

  return goal;
}

void TrajClient::ilqgPlan()
{
  ilqr_loco::TrajExecGoal goal = ilqgGenerateTrajectory(cur_state_);
  SendTrajectory(goal);
}

//////////////////////// GENERATE iLQG END //////////////////////////////////


int main(int argc, char** argv)
{
  ros::init(argc, argv, "test_client");
  TrajClient client;
  ros::spin();

  return 0;
}
