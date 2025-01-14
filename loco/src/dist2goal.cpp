//
// MIT License
//
// Copyright (c) 2017 MRSD Team D - LoCo
// The Robotics Institute, Carnegie Mellon University
// http://mrsdprojects.ri.cmu.edu/2016teamd/
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "ros/ros.h"
#include "tf/transform_datatypes.h"
#include "move_base_msgs/MoveBaseActionGoal.h"
#include <tf/transform_listener.h>
#include "std_msgs/Float64.h"
#include <cmath>
#include <string>
#include "geometry_msgs/PointStamped.h"

double last_goal_x = 0.0;
double last_goal_y = 0.0;
ros::Subscriber goal_sub;
ros::Publisher dist_pub;
ros::Publisher circle_pub;

void GoalCallback(const move_base_msgs::MoveBaseActionGoal::ConstPtr& msg){
  ROS_INFO("dist2goal_node: received goal");
  last_goal_x = msg->goal.target_pose.pose.position.x;
  last_goal_y = msg->goal.target_pose.pose.position.y;
}

int main(int argc, char** argv)
{
  ros::init(argc, argv, "dist2goal_node");

  ros::NodeHandle nh;

  goal_sub = nh.subscribe("move_base/goal", 1, GoalCallback);
  dist_pub = nh.advertise<std_msgs::Float64>("dist2goal_metric", 1);
  circle_pub = nh.advertise<geometry_msgs::PointStamped>("circle",1);

  tf::TransformListener listener;

  ros::Rate r(10);

  while(ros::ok()){
    ros::spinOnce();

    double current_x, current_y;

    tf::StampedTransform transform;

    try{
       listener.lookupTransform("map", "base_link", ros::Time(0), transform);
       current_x = transform.getOrigin().x();
       current_y = transform.getOrigin().y();

       //ROS_INFO("Currently at: %f , %f ", current_x, current_y);
    }
    catch (tf::TransformException ex){
      ROS_ERROR("dist2goal_node: map to base_link transform not found on this cycle");
    }

    double distance = sqrt( pow((last_goal_x-current_x),2) + pow((last_goal_y-current_y),2) );

    ROS_INFO("error x:%.3f y:%.3f abs:%.3f ", (last_goal_x-current_x), (last_goal_y-current_y), distance);

    std_msgs::Float64 dist_msg;
    dist_msg.data = distance;
    dist_pub.publish(dist_msg);


	geometry_msgs::PointStamped circle_msg;
	circle_msg.point.x = last_goal_x;
	circle_msg.point.y = last_goal_y;
	circle_msg.point.z = 0.0;

	circle_msg.header.stamp = ros::Time::now();
	circle_msg.header.frame_id = "map";
	circle_pub.publish(circle_msg);


    r.sleep();


  }

  return 0;
}
