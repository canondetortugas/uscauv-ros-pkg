#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <boost/thread.hpp>
#include <sensor_msgs/CameraInfo.h>
#include <opencv/cv.h>
#include <cv_bridge/CvBridge.h>
#include <sensor_msgs/Image.h>
#include <dynamic_reconfigure/server.h>
#include <sparse_stereo/SparseStereoConfig.h>
#include <string>
#include <limits>

boost::mutex l_img_mutex_, r_img_mutex_, l_info_mutex_, r_info_mutex_, flag_mutex_;

bool new_left_img_ = false;
bool processed_left_img = false;
bool new_right_img_ = false;
bool processed_right_img = false;
bool new_left_info_ = false;
bool new_right_info_ = false;

cv::SURF mSurf;
std::vector<cv::KeyPoint> leftKeyPoints, rightKeyPoints;
std::vector<float> leftDescriptors, rightDescriptors;

double norm_threshold, yspace_diff_threshold, gauss_mean, gauss_variance;

sensor_msgs::ImagePtr left_img_, right_img_;
sensor_msgs::CameraInfo left_info_, right_info_;
image_transport::Publisher *disparity_pub;
sensor_msgs::CvBridge bridge;

cv::Mat rightImage, leftImage, imageMask, combined;
std::vector<std::pair<size_t, size_t> > matches;

bool initialized = false;

double gaussian( double x, double mean, double variance )
{
	return variance != 0.0 ? ( 1.0 / sqrt( 2.0 * M_PI * variance ) ) * pow( M_E, -pow( x - mean, 2.0 ) / ( 2.0 * variance ) ) : 0;
}

void process_images()
{
	//ROS_INFO( "Trying to process available resources..." );
	if ( !initialized && new_right_img_ && new_left_img_ )
	{
		boost::lock_guard<boost::mutex> limgguard( l_img_mutex_ );
		boost::lock_guard<boost::mutex> rimgguard( r_img_mutex_ );

		leftImage = cv::Mat( bridge.imgMsgToCv( left_img_ ) );
		rightImage = cv::Mat( bridge.imgMsgToCv( right_img_ ) );

		int width, height;

		width = rightImage.size().width;
		height = std::max( rightImage.size().height, leftImage.size().height );

		imageMask = cv::Mat::ones( height, width, CV_8U);
		combined = cv::Mat( cv::Size( 2 * width, height ), leftImage.type(), cv::Scalar( 0 ) );

		ROS_INFO( "Initialized." );
		initialized = true;
	}
	else if ( !initialized ) return;

	if ( new_left_img_ && !processed_left_img )
	{
		//ROS_INFO( "processing left image..." );
		boost::lock_guard<boost::mutex> limgguard( l_img_mutex_ );
		leftImage = cv::Mat( bridge.imgMsgToCv( left_img_ ) );

		for ( int y = 0; y < combined.size().height; y++ )
		{
			for ( int x = 0; x < leftImage.size().width; x++ )
			{
				cv::Vec3b data = leftImage.at<cv::Vec3b> ( cv::Point( x, y ) );
				combined.at<cv::Vec3b> ( cv::Point( x, y ) ) = data;
			}
		}

		cv::Mat leftImageBw;
		cv::cvtColor( leftImage, leftImageBw, CV_BGR2GRAY);

		mSurf( leftImageBw, imageMask, leftKeyPoints, leftDescriptors );

		for ( size_t i = 0; i < leftKeyPoints.size(); i++ )
		{
			cv::circle( combined, leftKeyPoints[i].pt, 3, cv::Scalar( 255, 0, 0 ) );
		}
		//ROS_INFO( "found %d keypoints in left image; %d descriptors", leftKeyPoints.size(), leftDescriptors.size() );
		processed_left_img = true;
	}

	if ( new_right_img_ && !processed_right_img )
	{
		//ROS_INFO( "processing right image..." );
		boost::lock_guard<boost::mutex> rimgguard( r_img_mutex_ );
		rightImage = cv::Mat( bridge.imgMsgToCv( right_img_ ) );

		for ( int y = 0; y < combined.size().height; y++ )
		{
			for ( int x = 0; x < rightImage.size().width; x++ )
			{
				cv::Vec3b data = rightImage.at<cv::Vec3b> ( cv::Point( x, y ) );
				combined.at<cv::Vec3b> ( cv::Point( x + leftImage.size().width, y ) ) = data;
			}
		}

		cv::Mat rightImageBw;
		cv::cvtColor( rightImage, rightImageBw, CV_BGR2GRAY);
		mSurf( rightImageBw, imageMask, rightKeyPoints, rightDescriptors );

		for ( size_t i = 0; i < rightKeyPoints.size(); i++ )
		{
			cv::Point temp = cv::Point( rightImage.size().width + rightKeyPoints[i].pt.x, rightKeyPoints[i].pt.y );
			cv::circle( combined, temp, 3, cv::Scalar( 255, 0, 0 ) );
		}


		//ROS_INFO( "found %d keypoints in right image; %d descriptors", rightKeyPoints.size(), rightDescriptors.size() );
		processed_right_img = true;
	}

	if ( new_left_img_ && new_right_img_ && new_left_info_ && new_right_info_ )
	{
		//ROS_INFO( "Checking locks..." );
		boost::lock_guard<boost::mutex> limgguard( l_img_mutex_ );
		boost::lock_guard<boost::mutex> rimgguard( r_img_mutex_ );
		boost::lock_guard<boost::mutex> linfoguard( l_info_mutex_ );
		boost::lock_guard<boost::mutex> rinfoguard( r_info_mutex_ );


		//ROS_INFO( "Matching points across images..." );

		for ( size_t lIdx = 0; lIdx < leftKeyPoints.size(); lIdx++ )
		{
			bool foundFirst = false;
			double bestValue = 0;
			size_t bestIdx = 0;

			for ( size_t rIdx = 0; rIdx < rightKeyPoints.size(); rIdx++ )
			{
				double norm = 0;
				double yspace_diff = fabs( leftKeyPoints[lIdx].pt.y - rightKeyPoints[rIdx].pt.y );

				if ( yspace_diff > yspace_diff_threshold ) continue;

				for ( int i = 0; i < 128; i++ )
				{
					norm += pow( leftDescriptors[lIdx * 128 + i] - rightDescriptors[rIdx * 128 + i], 2 );
				}

				//ROS_INFO( "norm %f", norm );

				norm /= gaussian( yspace_diff, gauss_mean, gauss_variance );

				//ROS_INFO( "norm w/ gauss %f", norm );

				if ( norm < bestValue || !foundFirst )
				{
					foundFirst = true;
					bestValue = norm;
					bestIdx = rIdx;
				}
			}

			if ( foundFirst ) ROS_INFO( "best value: %f", bestValue );

			if ( bestValue < norm_threshold && foundFirst )
			{
				matches.push_back( std::make_pair( lIdx, bestIdx ) );
			}
			//std::cout << "Best Match for L"<<lIdx/128<< " = R"<<leftMatches[lIdx/128] << std::endl;
		}

		for ( size_t i = 0; i < matches.size(); i++ )
		{
			cv::Point lPoint = leftKeyPoints[matches[i].first].pt;
			cv::Point rPoint = rightKeyPoints[matches[i].second].pt;
			rPoint.x += leftImage.size().width;
			cv::line( combined, lPoint, rPoint, cv::Scalar( 0, 0, 255 ) );
		}

		//ROS_INFO( "Publishing disparity image" );

		IplImage * combined_ipl = & ( (IplImage) combined );


		//ROS_INFO( "width: %d height: %d", combined_ipl->width, combined_ipl->height );
		//ROS_INFO( "width: %d height: %d", combined.size().width, combined.size().height );

		disparity_pub->publish( bridge.cvToImgMsg( combined_ipl ) );


		//Reset new flags
		new_left_img_ = false;
		new_right_img_ = false;
		new_left_info_ = false;
		new_right_info_ = false;
		processed_left_img = false;
		processed_right_img = false;
		matches.clear();

		ROS_INFO( "done" );
	}
}

void img_cb_l( const sensor_msgs::ImageConstPtr& msg )
{
	ROS_INFO( "got left img" );
	l_img_mutex_.lock();
	left_img_ = boost::const_pointer_cast<sensor_msgs::Image>( msg );
	//img_left = msg;

	l_img_mutex_.unlock();

	boost::lock_guard<boost::mutex> guard( flag_mutex_ );
	new_left_img_ = true;

	process_images();
}

void img_cb_r( const sensor_msgs::ImageConstPtr& msg )
{
	ROS_INFO( "got right img" );
	r_img_mutex_.lock();
	right_img_ = boost::const_pointer_cast<sensor_msgs::Image>( msg );
	//img_right = msg;

	r_img_mutex_.unlock();

	boost::lock_guard<boost::mutex> guard( flag_mutex_ );
	new_right_img_ = true;

	process_images();
}

void info_cb_l( const sensor_msgs::CameraInfoConstPtr& msg )
{
	ROS_INFO( "got left info" );
	l_info_mutex_.lock();
	left_info_ = *msg;
	//info_left = boost::const_pointer_cast<sensor_msgs::CameraInfo>(msg);

	l_info_mutex_.unlock();

	boost::lock_guard<boost::mutex> guard( flag_mutex_ );
	new_left_info_ = true;

	process_images();
}

void info_cb_r( const sensor_msgs::CameraInfoConstPtr& msg )
{
	ROS_INFO( "got right info" );
	r_info_mutex_.lock();
	right_info_ = *msg;

	r_info_mutex_.unlock();

	boost::lock_guard<boost::mutex> guard( flag_mutex_ );
	new_right_info_ = true;

	process_images();
}

void reconfigureCallback( sparse_stereo::SparseStereoConfig &config, uint32_t level )
{
	//ROS_INFO( "Setting norm_threshold to %f", config.norm_threshold );
	//ROS_INFO( "Setting norm_threshold to %f", config.norm_threshold );
	norm_threshold = config.norm_threshold;
	yspace_diff_threshold = (double) config.yspace_diff_threshold;
	gauss_mean = config.gauss_mean;
	gauss_variance = config.gauss_variance;
}

int main( int argc, char * argv[] )
{
	ros::init( argc, argv, "sparse_stereo" );
	ros::NodeHandle n;
	ros::NodeHandle n_priv( "~" );
	ros::MultiThreadedSpinner spinner( 4 );

	std::string stereo_ns, image, left_ns, right_ns;

	image_transport::Subscriber img_sub_l, img_sub_r;
	ros::Subscriber info_sub_l, info_sub_r;
	image_transport::ImageTransport it( n_priv );

	n_priv.param( "stereo", stereo_ns, std::string( "/stereo" ) );
	n_priv.param( "image", image, std::string( "image_rect_color" ) );
	n_priv.param( "left", left_ns, std::string( "left" ) );
	n_priv.param( "right", right_ns, std::string( "right" ) );

	img_sub_l = it.subscribe( stereo_ns + "/" + left_ns + "/" + image, 1, img_cb_l );
	img_sub_r = it.subscribe( stereo_ns + "/" + right_ns + "/" + image, 1, img_cb_r );

	info_sub_l = n_priv.subscribe( stereo_ns + "/" + left_ns + "/camera_info", 1, info_cb_l );
	info_sub_r = n_priv.subscribe( stereo_ns + "/" + right_ns + "/camera_info", 1, info_cb_r );

	disparity_pub = new image_transport::Publisher;
	*disparity_pub = it.advertise( stereo_ns + "/disparity", 1 );

	dynamic_reconfigure::Server<sparse_stereo::SparseStereoConfig> srv;
	dynamic_reconfigure::Server<sparse_stereo::SparseStereoConfig>::CallbackType f = boost::bind( &reconfigureCallback, _1, _2 );
	srv.setCallback( f );

	spinner.spin();
}