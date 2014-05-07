#include "ros/ros.h"
#include "uos_3dscanner/ServoCommand.h"
#include "std_msgs/String.h"
#include "sensor_msgs/JointState.h"
#include "uos_3dscanner/servo_control.h"
#include <sstream>
#include "USBInterface.h"
#include "clock.h"
#include <iostream>
#include <boost/thread.hpp>
#define _USE_MATH_DEFINES

#define RAD(GRAD) ((GRAD * (float)M_PI) / (float)180)

#define SERVO_RANGE 1000
#define SERVO_MIN 1000

static ros::Publisher state_pub;

static sensor_msgs::JointState currentState;

static double min_pos[5] =
{
    RAD(-50.0),//fertig
    RAD(-65.0),//fertig
    RAD(-65.0),//fertig
    RAD(-50.0),//fertig
    RAD(-65.0) //fertig
};
static double max_pos[5] =
{
    RAD(60.0),
    RAD(45.0),
    RAD(45.0),
    RAD(60.0),
    RAD(45.0)
};

static std::string jointNames[5] =
{ "drehobjekt_1_to_balken_1",
  "servo_1_a_to_wing_1_a",
  "servo_1_b_to_wing_1_b",
  "servo_2_a_to_wing_2_a",
  "servo_2_b_to_wing_2_b"};


boost::mutex& mut()
{
    static boost::mutex m;
    return m;
}

void setJointState(int channel, double angle)
{
    currentState.header.stamp = ros::Time::now();


    if(channel == 2 || channel == 4|| channel == 0) angle *= -1.0;

    currentState.position[channel] = angle;
}

void moveServo(int channel, double angle, int speed, bool secure)
{
    boost::lock_guard<boost::mutex> _(mut());

    USBPololuInterface usb;

    uscSettings settings;
    usb.setUscSettings(settings);

    usb.setSpeed(channel, speed);

    double target = (double)angle;
    target -= min_pos[channel];

    target /=  (max_pos[channel] - min_pos[channel]);

    target *= SERVO_RANGE;

    target += SERVO_MIN;

    if(secure)
    {
        usb.moveToTarget(channel, target);
    }
    else
    {
        usb.setTarget(channel, target);
    }
    // As the board position information seems biased we just assume the requested angle was reached.
    setJointState(channel, angle);

    state_pub.publish(currentState);

    std::cout << "Reported servo status: " << std::endl;
}


void servoCallback(const uos_3dscanner::servo_control::ConstPtr& req)
{
     ROS_INFO("A servo movement was requested by topic");
     ROS_INFO("Channel: [%i] Target: [%f] Speed: [%i]",req->channel,req->angle,req->speed);
     if(req->channel == 0) return;

     double target = req->angle;

     if(req->angle < min_pos[req->channel])
     {
         ROS_INFO("Required Angle out of range!");
         target = min_pos[req->channel];
     }

     if(req->angle  > max_pos[req->channel])
     {
         ROS_INFO("Required Angle out of range!");
         target = max_pos[req->channel];
     }

     moveServo(req->channel, target, req->speed, false);

}


bool nod(uos_3dscanner::ServoCommand::Request  &req,
         uos_3dscanner::ServoCommand::Response &res)
{
  ROS_INFO("A servo movement was requested by service");
  ROS_INFO("Channel: [%i] Target: [%f] Speed: [%i]",req.channel,req.angle,req.speed);


  double target = req.angle;

  if(req.angle < min_pos[req.channel])
  {
      ROS_INFO("Required Angle out of range!");
      target = min_pos[req.channel];
  }

  if(req.angle  > max_pos[req.channel])
  {
      ROS_INFO("Required Angle out of range!");
      target = max_pos[req.channel];
  }

  moveServo(req.channel, target, req.speed, true);

  res.finished = true;
  return true;
}

int main(int argc, char **argv)
{
  ros::init(argc, argv, "servo_node");

  ros::NodeHandle n;

  // inits the service
  ros::ServiceServer service = n.advertiseService("servo_node", nod);

  // inits the publisher
  state_pub = n.advertise<sensor_msgs::JointState>("joint_states", 1);

  // inits the subscriber
  ros::Subscriber sub = n.subscribe("servo_control", 5, servoCallback);


  // init the current State
  currentState = sensor_msgs::JointState();
  currentState.name.resize(5);
  currentState.position.resize(5);

  // populate the current State with the values of the servos
  for (int i = 0; i < 5; i++)
  {
      setJointState(i, 0.0);
      currentState.name[i] = jointNames[i];
  }


  ROS_INFO("Ready to control servos.");

  while(ros::ok())
  {
      {
          boost::lock_guard<boost::mutex> _(mut());

          currentState.header.stamp = ros::Time::now();
          state_pub.publish(currentState);
      }

      ros::spinOnce();
  }

  return 0;
}