/***************************************************************************
 *  include/auv_controls/control_server_node.h
 *  --------------------
 *
 *  Software License Agreement (BSD License)
 *
 *  Copyright (c) 2013, Dylan Foster (turtlecannon@gmail.com)
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of USC AUV nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 **************************************************************************/


#ifndef USCAUV_AUVCONTROLS_CONTROLSERVER
#define USCAUV_AUVCONTROLS_CONTROLSERVER

// ROS
#include <ros/ros.h>

// uscauv
#include <uscauv_common/base_node.h>
#include <uscauv_common/multi_reconfigure.h>
#include <uscauv_common/defaults.h>

#include <auv_controls/ControlServerConfig.h>
#include <auv_controls/controller.h>

#include <tf/transform_listener.h>
/* #include <auv_physics/thruster_axis_model.h> */

#include <auv_msgs/MaskedTwist.h>
#include <auv_msgs/MotorPowerArray.h>
#include <geometry_msgs/Twist.h>

#include <eigen_conversions/eigen_msg.h>

static std::string const DESIRED_FRAME_NAME = uscauv::defaults::DESIRED_LINK;
static std::string const MEASUREMENT_FRAME_NAME = uscauv::defaults::MEASUREMENT_LINK;

typedef auv_controls::ControlServerConfig _ControlServerConfig;

class ControlServerNode: public BaseNode, public uscauv::PID6D, MultiReconfigure
{
 public:
  typedef Eigen::Matrix<double, 6, 1> AxisValueVector;
  typedef Eigen::Matrix<bool, 6, 1> AxisMaskVector;
 private:
  typedef auv_msgs::MaskedTwist _MaskedTwistMsg;
  
 private:

  /* uscauv::ReconfigurableThrusterAxisModel<uscauv::ThrusterModelSimpleLookup> thruster_axis_model_; */

  _ControlServerConfig * config_;
  
  AxisValueVector axis_command_value_; /* pose_command_value_; */
  AxisMaskVector axis_command_mask_;

  /// ROS
  ros::NodeHandle nh_rel_;
  ros::Subscriber axis_command_sub_;
  ros::Publisher axis_pub_;
  tf::TransformListener tf_listener_;
  
 public:
 ControlServerNode(): BaseNode("ControlServer"), /* thruster_axis_model_("model/thrusters"),  */
    axis_command_value_( AxisValueVector::Zero() ), /* pose_command_value_( AxisValueVector::Zero() ), */
    axis_command_mask_( AxisMaskVector::Zero() ),
    nh_rel_("~") {}

 private:

  // Running spin() will cause this function to be called before the node begins looping the spinOnce() function.
  void spinFirst()
  {
    axis_command_sub_ = nh_rel_.subscribe( "axis_cmd", 10, &ControlServerNode::axisCommandCallback, this );

    axis_pub_ = nh_rel_.advertise<geometry_msgs::Twist>( "axis_out", 10 );
       
    addReconfigureServer<_ControlServerConfig>( "scale" );
    config_ = &getLatestConfig<_ControlServerConfig>("scale");

    /* /// Load thruster models */
    /* thruster_axis_model_.load("robot/thrusters"); */

    /// Load PIDs
    loadController();

    setObserved<PID6D::Axes::SURGE> (0.0f);
    setObserved<PID6D::Axes::SWAY>  (0.0f);
    setObserved<PID6D::Axes::HEAVE> (0.0f);
    setObserved<PID6D::Axes::ROLL>  (0.0f);
    setObserved<PID6D::Axes::PITCH> (0.0f);
    setObserved<PID6D::Axes::YAW>   (0.0f);
  }  

  // Running spin() will cause this function to get called at the loop rate until this node is killed.
  void spinOnce()
  {
    /// get latest transforms
    updatePoseCommand();

    AxisValueVector pose_control = updateAllPID();

    /// apply scaling
    pose_control.block(0,0,3,1) *= config_->pose_scale_linear;
    pose_control.block(3,0,3,1) *= config_->pose_scale_angular;

    AxisValueVector all_control = pose_control + axis_command_value_;
    
    geometry_msgs::Twist control_out;
    control_out.linear.x = all_control(0);
    control_out.linear.y = all_control(1);
    control_out.linear.z = all_control(2);
    control_out.angular.x = all_control(3);
    control_out.angular.y = all_control(4);
    control_out.angular.z = all_control(5);
    
    axis_pub_.publish( control_out );
  }
  
 private:
  
  void axisCommandCallback(_MaskedTwistMsg::ConstPtr const & msg)
  {
    /// kinda hack-y, msg.mask's numeric variables are floats, when they should be bools
    AxisValueVector input_value, input_mask_float;
    AxisMaskVector input_mask_bool;
    
    tf::twistMsgToEigen( msg->twist, input_value );
    tf::twistMsgToEigen( msg->mask, input_mask_float );
    input_mask_bool = input_mask_float.cast<bool>();

    input_value.block(0,0,3,1) *= config_->axis_scale_linear;
    input_value.block(3,0,3,1) *= config_->axis_scale_angular;

    for(unsigned int idx = 0; idx < 6; ++idx)
      {
	if( input_mask_bool( idx ) )
	  {
	    axis_command_mask_( idx ) = true;
	    axis_command_value_( idx ) = input_value( idx );
	  }
      }
    
  }

  void updatePoseCommand()
  {
    tf::StampedTransform world_to_desired_tf, world_to_measurement_tf;
    
    if( tf_listener_.canTransform( "/world", MEASUREMENT_FRAME_NAME, ros::Time(0) ))
      {
	try
	  {
	    tf_listener_.lookupTransform( "/world", MEASUREMENT_FRAME_NAME, ros::Time(0), world_to_measurement_tf);

	  }
	catch(tf::TransformException & ex)
	  {
	    ROS_ERROR( "%s", ex.what() );
	    return;
	  }
      }
    else return;
    
    if( tf_listener_.canTransform( "/world", DESIRED_FRAME_NAME, ros::Time(0) ))
      {
	try
	  {
	    tf_listener_.lookupTransform( "/world", DESIRED_FRAME_NAME, ros::Time(0), world_to_desired_tf);

	  }
	catch(tf::TransformException & ex)
	  {
	    ROS_ERROR( "%s", ex.what() );
	    return;
	  }
      }
    else return;

    tf::Transform error_tf = world_to_measurement_tf.inverse() * world_to_desired_tf;
    double roll, pitch, yaw;
    
    error_tf.getBasis().getRPY( roll, pitch, yaw );

    tf::Vector3 error_vec = error_tf.getOrigin();

    AxisValueVector error_pose_value;
    error_pose_value << error_vec.x(), error_vec.y(), error_vec.z(), roll, pitch, yaw;

    /* error_pose_value.block(0,0,3,1) *= config_->pose_scale_linear; */
    /* error_pose_value.block(3,0,3,1) *= config_->pose_scale_angular; */
    
    setSetpoint<PID6D::Axes::SURGE>( error_pose_value(0) );
    setSetpoint<PID6D::Axes::SWAY> ( error_pose_value(1) );
    setSetpoint<PID6D::Axes::HEAVE>( error_pose_value(2) );
    setSetpoint<PID6D::Axes::ROLL> ( error_pose_value(3) );
    setSetpoint<PID6D::Axes::PITCH>( error_pose_value(4) );
    setSetpoint<PID6D::Axes::YAW>  ( error_pose_value(5) );
  }
  
};

#endif // USCAUV_AUVCONTROLS_CONTROLSERVER
