/*******************************************************************************
 *
 *      color_blob_finder
 * 
 *      Copyright (c) 2011, Edward T. Kaszubski ( ekaszubski@gmail.com )
 *      All rights reserved.
 *
 *      Redistribution and use in source and binary forms, with or without
 *      modification, are permitted provided that the following conditions are
 *      met:
 *      
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following disclaimer
 *        in the documentation and/or other materials provided with the
 *        distribution.
 *      * Neither the name of "seabee3-ros-pkg" nor the names of its
 *        contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *      
 *      THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *      "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *      LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *      A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *      OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *      SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *      LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *      DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *      THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *      (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *      OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *******************************************************************************/

#ifndef COLOR_BLOB_FINDER_H_
#define COLOR_BLOB_FINDER_H_

/* stl */
#include <string>
#include <vector>

/* ros */
#include <common_utils/opencv.h>

// message filters
#include <message_filters/subscriber.h>
#include <message_filters/synchronizer.h>
#include <message_filters/sync_policies/approximate_time.h>
// for CvBridge
#include <cv_bridge/CvBridge.h>
#include <cv_bridge/cv_bridge.h>

/* msgs */
#include <base_libs/ComponentImageArray.h>
#include <color_blob_finder/ColorBlobArray.h>

/* cfgs */
#include <color_blob_finder/ColorBlobFinderConfig.h>

/* others */
#include <base_node/base_node.h>
// for a ton of boost-related content
#include <boost/thread.hpp>

typedef color_blob_finder::ColorBlobFinderConfig _ReconfigureType;
typedef BaseNode<_ReconfigureType> _BaseNode;
typedef unsigned int _DimType;

typedef base_libs::ComponentImageArray _ImageArrayMessageType;

typedef message_filters::sync_policies::ApproximateTime<_ImageArrayMessageType, _ImageArrayMessageType> _SyncPolicy;

class ColorBlobFinder: public _BaseNode
{
public:
	/* subs */
	message_filters::Subscriber<_ImageArrayMessageType> source1_sub_;
	message_filters::Subscriber<_ImageArrayMessageType> source2_sub_;

	/* pubs */
	ros::Publisher color_blob_array_pub_;
	image_transport::Publisher output_image_pub_;

	/* others */
	message_filters::Synchronizer<_SyncPolicy> sync_;
	IplImage * current_image_;
	IplImage * output_image_;
	IplImage * current_image_converted_;
	image_transport::ImageTransport image_transport_;
	boost::mutex image_sources_mutex_;
	sensor_msgs::CvBridge cv_bridge_;

	/* dynamic reconfigure */
	_ReconfigureType reconfigure_params_;

	ColorBlobFinder( const ros::NodeHandle & nh ) :
			_BaseNode( nh ), source1_sub_( nh_local_,
			                               "source1",
			                               1 ),
			                 source2_sub_( nh_local_,
			                               "source2",
			                               1 ),
			                 sync_( _SyncPolicy( 2 ),
			                        source1_sub_,
			                        source2_sub_ ),
			                 current_image_( NULL ),
			                 output_image_( NULL ),
			                 current_image_converted_( NULL ),
			                 image_transport_( nh_local_ )
	{
		sync_.registerCallback( boost::bind( &ColorBlobFinder::imageSourcesCB,
		                                     this,
		                                     _1,
		                                     _2 ) );
		color_blob_array_pub_ = nh_local_.advertise < color_blob_finder::ColorBlobArray > ( "color_blobs", 1 );
		output_image_pub_ = image_transport_.advertise( "output_image",
		                                                1 );

		initCfgParams();

		cv::namedWindow( "depth_display", CV_WINDOW_AUTOSIZE );
	}

	void imageSourcesCB( const _ImageArrayMessageType::ConstPtr & image_array_msg1,
	                     const _ImageArrayMessageType::ConstPtr & image_array_msg2 )
	{
		printf("Got imageSourcesCB\n");
		if( !image_sources_mutex_.try_lock() ) return;

		std::vector<base_libs::ComponentImage> images;
		color_blob_finder::ColorBlobArray::Ptr color_blobs( new color_blob_finder::ColorBlobArray );

		unsigned int total_images = image_array_msg1->images.size() + image_array_msg2->images.size();
		images.reserve( total_images );

		printf("Anticipating %u images\n", total_images );

		for ( _DimType i = 0; i < image_array_msg1->images.size(); ++i )
		{
			images.push_back( image_array_msg1->images[i] );
		}
		for ( _DimType i = 0; i < image_array_msg2->images.size(); ++i )
		{
			images.push_back( image_array_msg2->images[i] );
		}

		// for each image
		for ( _DimType i = 0; i < images.size(); ++i )
		{
			/*const sensor_msgs::Image::ConstPtr current_image_msg_ptr ( &images[i].image );
			const IplImage current_image = IplImage ( cv_bridge::toCvShare( current_image_msg_ptr )->image );
			const IplImage * current_image_ptr = &current_image;*/

			cv_bridge_.fromImage( images[i].image );
			const IplImage * current_image_ptr = cv_bridge_.toIpl();

			std::vector<std::vector<cv::Point> > contours = processImage( current_image_ptr );

			color_blob_finder::ColorBlob color_blob;
			color_blob.color_id = images[i].id;

			// for each contour
			for ( _DimType j = 0; j < contours.size(); ++j )
			{
				color_blob_finder::Contour contour;
				contour.header = images[i].image.header;
				// for each point in the contour
				for ( _DimType k = 0; k < contours[j].size(); ++k )
				{
					color_blob_finder::Point2D point;

					point.x = contours[j][k].x;
					point.y = contours[j][k].y;

					contour.points.push_back( point );
				}
				color_blob.contour = contour;
			}
			color_blobs->blobs.push_back( color_blob );
		}

		color_blob_array_pub_.publish( color_blobs );
		output_image_pub_.publish( sensor_msgs::CvBridge::cvToImgMsg( output_image_ ) );
		printf("unlocking image_sources mutex\n");
		image_sources_mutex_.unlock();
	}

	virtual ~ColorBlobFinder()
	{
		if( current_image_ ) cvReleaseImage( &current_image_ );
		if( output_image_ ) cvReleaseImage( &output_image_ );
		if( current_image_converted_ ) cvReleaseImage( &current_image_converted_ );
	}

	void reconfigureCB( _ReconfigureType &config,
	                    uint32_t level )
	{
		reconfigure_params_ = config;
	}

	std::vector<std::vector<cv::Point> > processImage( const IplImage * ipl_image )
	{
		std::vector<std::vector<cv::Point> > contours;
		if ( !current_image_ ) current_image_ = cvCreateImage( cvSize( ipl_image->width,
		                                                               ipl_image->height ),
		                                                       ipl_image->depth,
		                                                       ipl_image->nChannels );

		if ( !current_image_converted_ ) current_image_converted_ = cvCreateImage( cvSize( ipl_image->width,
		                                                                                   ipl_image->height),
		                                                                           IPL_DEPTH_8U,
		                                                                           1 );
		if ( !output_image_ ) output_image_ = cvCreateImage( cvSize( ipl_image->width,
		                                                             ipl_image->height ),
		                                                     IPL_DEPTH_8U,
		                                                     3 );

		// our incoming probability images have a potentially very small probability range
		cvNormalize( ipl_image,
		             current_image_,
		             0.0,
		             255.0,
		             CV_MINMAX );

		cvConvertScale( current_image_, current_image_converted_ );

		if( reconfigure_params_.draw_distance_image ) cvCvtColor( current_image_converted_, output_image_, CV_GRAY2BGR );
		else cvFillImage( output_image_, 0.0 );

		cvThreshold( current_image_converted_,
		             current_image_converted_,
		             reconfigure_params_.threshold_value,
		             reconfigure_params_.threshold_max_value,
		             reconfigure_params_.threshold_type );

		//cvCvtColor( current_image_converted_, output_image_, CV_GRAY2BGR );

		cv::Mat current_image_converted_mat( current_image_converted_ );
		cv::Mat output_image_mat( output_image_ );

		cv::findContours( current_image_converted_mat,
		                  contours,
		                  CV_RETR_LIST,
		                  CV_CHAIN_APPROX_SIMPLE );

		printf( "Found %zu contours\n",
		        contours.size() );

		cv::drawContours( output_image_mat,
		                  contours,
		                  -1,
		                  cv::Scalar( 0,
		                              255,
		                              0 ),
		                  2 );

		//output_image_pub_.publish( sensor_msgs::CvBridge::cvToImgMsg( current_image_converted_ ) );

		return contours;
	}
};

#endif /* COLOR_BLOB_FINDER_H_ */