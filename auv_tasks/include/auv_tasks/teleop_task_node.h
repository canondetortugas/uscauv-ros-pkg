/***************************************************************************
 *  include/auv_tasks/teleop_task_node.h
 *  --------------------
 *
 *  Copyright (c) 2013, Dylan Foster
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

/// ROS
#include <ros/ros.h>

/// generic task
#include <auv_tasks/task_executor.h>
#include <uscauv_joystick/joystick_policy.h>

/// uscauv
#include <uscauv_common/timing.h>
#include <uscauv_common/pose_integrator.h>

/// transforms
#include <tf/transform_listener.h>

/// msgs
#include <auv_msgs/MatchedShape.h>
#include <auv_msgs/MatchedShapeArray.h>

/// Seabee
/// TODO: Stop using old seabee movement API
#include <seabee3_msgs/MotorVals.h>
#include <seabee3_msgs/Depth.h>
#include <seabee3_common/movement.h>

#include <algorithm>

typedef seabee3_msgs::MotorVals _MotorValsMsg;
typedef seabee3_msgs::Depth _DepthMsg;
typedef auv_msgs::MatchedShape _MatchedShape;
typedef auv_msgs::MatchedShapeArray _MatchedShapeArray;

using namespace seabee3_common;

/**
 * To Implement:
 * Function/Option to normalize the motor vals for an axis
 * Scale for axes (within joystick policy)
 * Axis motor setter function within seabee3 movement
 * Some sort of generic axis assigner that will work with the new robot without changing anything in this project
 * Sensor policy? Get IMU, pressure
 * Pose Integrator
 */

class TeleopTaskNode: public TaskExecutorNode, public JoystickPolicy
{
 private:
  /// ROS interfaces
  ros::NodeHandle nh_rel_;
  tf::TransformListener tf_listener_;
  ros::Publisher motor_vals_pub_;
  ros::Subscriber matched_shape_sub_;
  ros::Subscriber depth_sub_;

 private:
  std::string imu_frame_name_;
  int follow_timeout_;
  
  /// for pitch toggle button
  int pitch_setpoint_;
  
  /// depth control
  float ambient_depth_;
  
  /// for pipe following
  _MatchedShape last_matched_pipe_;
  _DepthMsg last_depth_msg_;
  
  float first_match_scale_;
  bool follow_pipe_mode_;
  bool lock_to_pipe_;
  /// TODO: Get rid of this class - It is completely broken
  uscauv::AsynchronousTimer<std::chrono::milliseconds> follow_timer_;
  uscauv::PoseIntegrator z_pose_;
  
 public:
 TeleopTaskNode(): JoystickPolicy("teleop_task"), nh_rel_("~"),
    pitch_setpoint_(0), follow_pipe_mode_(false), lock_to_pipe_(false),
    follow_timer_( &TeleopTaskNode::followTimeoutCallback, this)
      {
	last_depth_msg_.value = 0.0f;
      }

 private:
  void spinFirst()
  {
    /// all setpoints are initializes to zero
    controller_.init("linear/x", "linear/y", "linear/z",
    		     "angular/yaw", "angular/pitch", "angular/roll");

    initJoystick( &TeleopTaskNode::joyCallback, this );

    /// TODO: Load this as param
    imu_frame_name_ = "/seabee3/sensors/imu";
    follow_timeout_ = 5000; // milliseconds
    ambient_depth_ = -1;

    motor_vals_pub_ = nh_rel_.advertise<_MotorValsMsg>("motor_vals", 1);

    matched_shape_sub_ = nh_rel_.subscribe("matched_shapes", 10, 
					   &TeleopTaskNode::matchedShapeCallback, this);
    depth_sub_ = nh_rel_.subscribe("/seabee3/depth", 1, 
					   &TeleopTaskNode::depthCallback, this);

  }

  void spinOnce()
  {
    z_pose_.updatePose();

    if( tf_listener_.canTransform( "/world", imu_frame_name_, ros::Time(0) ))
      {
	tf::StampedTransform world_to_imu_tf;
	    
	try
	  {
	    tf_listener_.lookupTransform( "/world", imu_frame_name_, ros::Time(0), world_to_imu_tf);

	  }
	catch(tf::TransformException & ex)
	  {
	    ROS_ERROR( "%s", ex.what() );
	    return;
	  }

	double roll, pitch, yaw;
	    
	/* Yaw around Z, pitch around Y, roll around X */
	world_to_imu_tf.getBasis().getEulerZYX( yaw, pitch, roll );
	    
	roll *= 180 / M_PI;
	pitch *= 180 / M_PI;
	yaw *= 180 / M_PI;
	   

	/// TODO: Add enum-type thing to PID6D such that axes 0-6 are called PID6D::YAW etc.
	/* controller_.setObserved<3>(yaw); */
	controller_.setObserved<4>(pitch);
	controller_.setObserved<5>(roll);
	    
      }
    controller_.setObserved<2>(last_depth_msg_.value);
    controller_.setSetpoint<2>(ambient_depth_);
    
    if( getButton("enable" ) )
      {
	/// Apply setpoints from joystick
	
	if( follow_pipe_mode_ )
	  {
	    if( lock_to_pipe_)
	      {
		/// FRANCESCA/TURNER: Play around with the signs on these four setsetPoint arguments if you run into direction issues
		controller_.setSetpoint<0>( 0 );
		controller_.setSetpoint<1>( 0 );
		/* controller_.setSetpoint<2>(first_match_scale_); */
		controller_.setSetpoint<3>( 0 );

		/// Negation because positive position indicates that seabee has a negative position with respect to the object frame
		controller_.setObserved<0>( -last_matched_pipe_.y );
		controller_.setObserved<1>( last_matched_pipe_.x );
		/* controller_.setObserved<2>(last_matched_pipe_.scale);  */
		controller_.setObserved<3>( -last_matched_pipe_.theta*180/M_PI );
	      }
	    else
	      {
		/// Go forward at medium speed until we find something
		controller_.setSetpoint<0>( 60 );
		controller_.setObserved<0>( 0  );
	      }
	  }
	else
	  {
	    if( getButtonAcquired("increment_pitch") )
	      {
		if ( !pitch_setpoint_)
		  pitch_setpoint_ = 90;
		else
		  pitch_setpoint_ = 0;
		controller_.setSetpoint<4>(pitch_setpoint_);
		ROS_INFO("New pitch setpoint [ %d ]", pitch_setpoint_);
	      }

	    controller_.setSetpoint<0>( getAxis("linear.x")*100 );
	    controller_.setSetpoint<1>( getAxis("linear.y")*100 );
	    float z = z_pose_.getPose().getOrigin().z();
	    z = ( z > 0 ) ? 0 : z; /// If z > 0, the robot is not in the pool
	    /* controller_.setSetpoint<2>( z ); */
	    /* ROS_INFO("Setting z setpoint to %f.", z); */
	
	    controller_.setSetpoint<3>( getAxis("angular.z")*180 );
	
	    if( getButton("barrel_roll"))
	      {
		/* ROS_INFO("Doing a barrel roll..."); */
		controller_.setSetpoint<5>(180);
		controller_.setObserved<5>(0);
	      }
	
	    if(getButtonAcquired("barrel_roll"))
	      {
		ROS_INFO("Entering barrel roll...");
	      }
	    else if( getButtonReleased("barrel_roll") )
	      {
		controller_.setSetpoint<5>(0);
		ROS_INFO("Halted barrel roll.");
	      }
	
	    geometry_msgs::Twist twist;
	
	    twist.linear.z = getAxis("linear.z");

	    /* pose_integrator_.setVelocity(twist); */
	  }

		
      }
    else
      {
	controller_.setObserved<0>(0);
	controller_.setObserved<1>(0);
	/* controller_.setObserved<2>(last_depth_msg_.value); */
	controller_.setObserved<3>(0);

	controller_.setSetpoint<0>(0);
	controller_.setSetpoint<1>(0);
	/* controller_.setSetpoint<2>(ambient_depth_); */
	controller_.setSetpoint<3>(0);
	controller_.setSetpoint<4>(0);
	controller_.setSetpoint<5>(0);
	
      }
	
    publishMotors();
  }
   
  void joyCallback()
  {
    if ( getButtonAcquired("follow_pipe") && getButton( "enable" ))
      {

	if( follow_pipe_mode_ )
	  {
	    ROS_INFO("Transitioning to manual control state.");
	    controller_.setObserved<0>(0);
	    controller_.setObserved<1>(0);
	    controller_.setObserved<2>(0);
	    controller_.setObserved<3>(0);
	    controller_.setSetpoint<0>(0);
	    controller_.setSetpoint<2>(0);
	  }
	else
	  {
	    if( lock_to_pipe_ )
	      {
		ROS_INFO("Transitioning to follow pipe state.");
	      }
	    else
	      {
		ROS_INFO("Transitioning to pipe search state");
	      }
	  }
	follow_pipe_mode_ = !follow_pipe_mode_;
      }
    
    if( getButton( "enable" ) )
      {
	geometry_msgs::Twist vel;
	vel.linear.z = getAxis("linear.z");
	z_pose_.setVelocity(vel);
      }
    
  }
  
  void depthCallback( _DepthMsg::ConstPtr const & msg )
  {
    last_depth_msg_ = *msg;
    last_depth_msg_.value = -last_depth_msg_.value;
  }

  void matchedShapeCallback( _MatchedShapeArray::ConstPtr const & msg )
  {
    /* std::chrono::time_point<std::chrono::system_clock> start, end; */
    /* start = std::chrono::system_clock::now(); */

    _MatchedShape match;
    float candidate_scale = 0.0f;
    /// find the biggest matching shape
    for( std::vector<_MatchedShape>::const_iterator shape_it= msg->shapes.begin();
	 shape_it != msg->shapes.end(); ++shape_it)
      {
	if( shape_it->color == "blaze_orange" && 
	    shape_it->type == "pipe" &&
	    shape_it->scale > candidate_scale)
	  {
	    candidate_scale = shape_it->scale;
	    match = *shape_it;
	  }
      }
    /// no matches
    if( candidate_scale <= 0.0f)
      return;
    else
      {
	last_matched_pipe_ = match;

	if( lock_to_pipe_)
	  {
	    follow_timer_.stop();
	    follow_timer_.start( std::chrono::milliseconds( follow_timeout_ ));
	  }	
	else
	  {
	    ROS_INFO("Detected new pipe.");
	    first_match_scale_ = match.scale;
	    follow_timer_.stop();
	    follow_timer_.start( std::chrono::milliseconds( follow_timeout_ ));
	    lock_to_pipe_ = true;
	  }
      }
    /* end = std::chrono::system_clock::now(); */

    /* int elapsed_seconds = std::chrono::duration_cast<std::chrono::milliseconds> */
    /*   (end-start).count(); */
 
    /* ROS_INFO_STREAM( "elapsed time: " << elapsed_seconds); */
  }

  void followTimeoutCallback()
  {
    ROS_INFO("Lost pipe.");
    lock_to_pipe_ = false;

    /// Reset XYZ and yaw
    if( getButton("enable") && follow_pipe_mode_ )
      {
	ROS_INFO("Transitioning from follow pipe state to search pipe state.");
	controller_.setObserved<0>(0);
	controller_.setObserved<1>(0);
	controller_.setObserved<2>(0);
	controller_.setObserved<3>(0);
	controller_.setSetpoint<2>(0);
      }
    
  }

 private:
  void publishMotors()
  {
    _MotorValsMsg msg;
    
    /* ROS_INFO("Speed has pid value: %f", controller_.update<0>()); */
    /* ROS_INFO("Strafe has pid value: %f", controller_.update<1>()); */
    /* ROS_INFO("Depth has pid value: %f", controller_.update<2>()); */
    /* ROS_INFO("Yaw has pid value: %f", controller_.update<3>()); */
    
    applyMotors(msg, movement::Axes::SPEED, controller_.update<0>() );
    applyMotors(msg, movement::Axes::STRAFE, controller_.update<1>() );
    applyMotors(msg, movement::Axes::DEPTH, controller_.update<2>() );
    applyMotors(msg, movement::Axes::YAW, controller_.update<3>() );
    applyMotors(msg, movement::Axes::PITCH, controller_.update<4>() );
    applyMotors(msg, movement::Axes::ROLL, controller_.update<5>() );
    
    /// TODO: THrottle down to 100 here so that the sum is < 100
    /// TODO: Should really normalize instead of clipping
    normalizeMotors( msg );
    motor_vals_pub_.publish( msg );
    
  }
  
  void normalizeMotors(_MotorValsMsg & msg )
  {
    normalizeAxis(msg, movement::Axes::SPEED );
    normalizeAxis(msg, movement::Axes::STRAFE);
    normalizeAxis(msg, movement::Axes::DEPTH );
  }

  void normalizeAxis(_MotorValsMsg & msg, int const & axis)
  {
    int motor1_id = movement::ThrusterPairs::values[axis][0];
    int motor2_id = movement::ThrusterPairs::values[axis][1];

    double max = std::max( abs(msg.motors[motor1_id]), abs(msg.motors[motor2_id]) );
    
    if(max <= 100 )
      return;
    
    msg.motors[motor1_id] *= 100/max;
    msg.motors[motor2_id] *= 100/max;
  }

  /// TODO:Redo this entire function
  void applyMotors(_MotorValsMsg & msg, int const & axis, double val)
  {
    
    int motor1_id = movement::ThrusterPairs::values[axis][0];
    int motor2_id = movement::ThrusterPairs::values[axis][1];

    msg.mask[motor1_id] = 1;
    msg.mask[motor2_id] = 1;
    
    switch(axis)
      {
      case movement::Axes::YAW:
	{
	  msg.motors[motor1_id] += val;
	  msg.motors[motor2_id] += -val;
	  break;
	}
      case movement::Axes::PITCH:
	{
	  msg.motors[motor1_id] += -val;
	  msg.motors[motor2_id] += val;
	  break;
	}
      case movement::Axes::ROLL:
	{
	  msg.motors[motor1_id] += -val;
	  msg.motors[motor2_id] += -val;
	  break;
	}
      case movement::Axes::SPEED:
	{
	  msg.motors[motor1_id] += val;
	  msg.motors[motor2_id] += val;
	  break;
	}
      case movement::Axes::STRAFE:
	{
	  msg.motors[motor1_id] += val;
	  msg.motors[motor2_id] += -val;
	  break;
	}
      case movement::Axes::DEPTH:
	{
	  msg.motors[motor1_id] += val;
	  msg.motors[motor2_id] += val;
	  break;
	}
      }
    
  }



};
