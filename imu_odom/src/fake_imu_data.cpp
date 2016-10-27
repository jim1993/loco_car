#include <ros/ros.h>
#include <ros/console.h>
#include <sensor_msgs/Imu.h>
#include <geometry_msgs/Quaternion.h>
#include <geometry_msgs/Vector3.h>

int main(int argc, char** argv){
  ros::init(argc,argv,"fake_imu_publisher");

  ros::NodeHandle nh;
  ros::Publisher imu_pub = nh.advertise<sensor_msgs::Imu>("fake_imu", 50);

  ros::Rate r(1.0);

  while(nh.ok()){
    //populate the imu message
    ros::Time imu_time = ros::Time::now();
    sensor_msgs::Imu imu_data;
    imu_data.header.stamp = imu_time;
    imu_data.header.frame_id = "imu_frame";

    geometry_msgs::Vector3 ang_vel;
    geometry_msgs::Vector3 lin_acc;

    ang_vel.x=0; ang_vel.y=0; ang_vel.z=0.1;
    lin_acc.x=0.05; lin_acc.y=0.0; lin_acc.z=0;

    imu_data.angular_velocity = ang_vel;
    imu_data.linear_acceleration = lin_acc;

    //Stuff we don't know
    geometry_msgs::Quaternion imu_orient;
    imu_orient.x = 0; imu_orient.y = 0; imu_orient.z = 0; imu_orient.w = 0;
    imu_data.orientation =  imu_orient;

    imu_data.orientation_covariance[0] = -1;
    imu_data.angular_velocity_covariance[0] = -1;
    imu_data.linear_acceleration_covariance[0] = -1;

    imu_pub.publish(imu_data);
    //ROS_INFO("Published IMU data.");
    r.sleep();
  }

  return 0;

}
