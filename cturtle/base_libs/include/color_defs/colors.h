/*******************************************************************************
 *
 *      colors
 * 
 *      Copyright (c) 2010, edward
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

#ifndef COLORS_H_
#define COLORS_H_

#include <opencv/cv.h>
#include <common_utils/math.h>
#include <array>
#include <string>

typedef unsigned int _DimType;

template<class __DataType, _DimType __Dim__ = 1>
class VecBase
{
public:

	typedef __DataType _DataType;
	typedef std::array<__DataType, __Dim__> _ArrayType;
	const static _DimType _Dim_ = __Dim__;

	_ArrayType data_;

	VecBase( const _ArrayType data ) :
	data_( data )
	{
		//
	}

	VecBase()
	{
		//
	}
};

template<class __DataType>
class VecBase<__DataType, 1>
{
public:
	typedef __DataType _DataType;
	const static _DimType _Dim_ = 1;

	__DataType data_;

	VecBase( const __DataType & data = __DataType( 0 ) ) :
			data_( data )
	{
		//
	}
};

template<class __DataType, _DimType __Dim__ = 1>
class Vec: public VecBase<__DataType, __Dim__> {
public:
typedef VecBase<__DataType, __Dim__> _VecBase;
typedef std::array<__DataType, __Dim__> _ArrayType;
typedef __DataType _DataType;

Vec( const _ArrayType data ) : _VecBase( data )
{
	//
}

Vec() : _VecBase()
{
	//
}
};

template<class __DataType>
class Vec<__DataType, 1> : public VecBase<__DataType, 1>
{
public:
	typedef VecBase<__DataType, 1> _VecBase;
	typedef __DataType _DataType;

	Vec( const __DataType & data = __DataType( 0 ) ) :
			_VecBase( data )
	{
		//
	}
};

template<class __DataType>
class Vec<__DataType, 3> : public VecBase<__DataType, 3>
{
public:
	typedef VecBase<__DataType, 3> _VecBase;
	typedef std::array<__DataType, _VecBase::_Dim_> _ArrayType;
	typedef __DataType _DataType;

	__DataType * i_;
	__DataType * j_;
	__DataType * k_;

	Vec( const _ArrayType data ) :
			_VecBase( data ), i_( &this->data_[0] ), j_( &this->data_[1] ), k_( &this->data_[2] )
	{
		//
	}

	Vec( const __DataType & i,
	     const __DataType & j,
	     const __DataType & k ) :
			_VecBase( { i, j, k } ), i_( &this->data_[0] ), j_( &this->data_[1] ), k_( &this->data_[2] )
	{
		//
	}

	Vec() :
			_VecBase(), i_( &this->data_[0] ), j_( &this->data_[1] ), k_( &this->data_[2] )
	{

	}
};

struct DistanceType
{
	const static _DimType EUCLIDIAN = 0;
	const static _DimType GAUSSIAN = 1;
};

// default template for FeatureBase with decimal accuracy
template<class __DataType>
class FeatureBase
{
public:

	typedef __DataType _DataType;
	typedef FeatureBase<__DataType> _FeatureBase;

	__DataType data_;

	FeatureBase( const __DataType & data = __DataType( 0 ) ) :
	data_( data )
	{
		//
	}

	__DataType distanceTo( const _FeatureBase & other, const __DataType & radius = __DataType( 0 ) ) const
	{
		// 0 - 179 = -179
		__DataType dist = other.data_ - data_;
		if ( radius > 0 && fabs( dist ) > radius )
	    {
	    	dist = math_utils::mod( fabs( dist ),
	    	                        radius ) - radius;
	    }
	    return dist;
	}

	static __DataType caltulateEuclidianDistanceComponent( const __DataType & dist )
	{
		return dist;
	}

	template<class _WeightType>
	static __DataType calculateGaussianDistanceComponent( const __DataType & dist, const _WeightType & variance )
	{
		if( variance == 0 ) return dist == 0 ? 1.0 : 0.0;
		return /*( 1 / sqrt( 2 * M_PI * variance ) ) * */ pow( M_E, -pow( dist, 2 ) / ( 2 * variance ) );
	}

	template<class _WeightType>
	static void calculateDistanceComponent( __DataType & total_distance, const _FeatureBase & current, const _FeatureBase & other, const __DataType & radius, const _WeightType & weight, const _WeightType & variance, _DimType distance_type, bool initialize_total_distance )
	{
		__DataType dist = current.distanceTo( other, radius );

		switch( distance_type )
		{
		case DistanceType::EUCLIDIAN:
			if( initialize_total_distance ) total_distance = 0.0;
			total_distance += pow( caltulateEuclidianDistanceComponent( dist ) * weight, 2 );
			break;
		case DistanceType::GAUSSIAN:
			if( initialize_total_distance ) total_distance = 0.0;
			total_distance += calculateGaussianDistanceComponent( dist, variance ) * weight;
			break;
		}
	}
};

template<class __DataType, _DimType __Dim__ = 1>
class Feature: public Vec<FeatureBase<__DataType >, __Dim__>
{
public:
	typedef Vec<FeatureBase<__DataType >, __Dim__> _Vec;
	typedef std::array<__DataType, __Dim__> _ArrayType;
	typedef __DataType _DataType;
	typedef Feature<_DataType, __Dim__> _Feature;
	typedef FeatureBase<_DataType> _FeatureBase;

	Feature( const _ArrayType data ) :
	_Vec()
	{
		for ( _DimType i = 0; i < __Dim__; ++i )
		{
			this->data_[i] = _FeatureBase( data[i] );
		}
	}

	Feature() :
	_Vec()
	{

	}

	template<class _WeightType>
	__DataType distance( const _Feature & other, _ArrayType & radii, std::array<_WeightType, __Dim__> & weights, std::array<_WeightType, __Dim__> variances = std::array<_WeightType, __Dim__>(), _DimType distance_type = DistanceType::EUCLIDIAN )
	{
		__DataType total_distance = 0;

		_WeightType total_weight = 0.0;
		for ( _DimType i = 0; i < __Dim__; ++i )
		{
			total_weight += weights[i];
		}

		const _WeightType weight_norm_scale = 1.0 / total_weight;

		for ( _DimType i = 0; i < __Dim__; ++i )
		{
			if( weights[i] > 0 ) _FeatureBase::calculateDistanceComponent( total_distance, this->data_[i], other.data_[i], radii[i], weight_norm_scale * weights[i], variances[i], distance_type, i == 0 );
		}

		return sqrt( total_distance );
	}

/*	template<class _WeightType>
	__DataType distance( const _Feature & other, _ArrayType & radii, const _WeightType & uniform_weight = _WeightType( 1 ), _DimType distance_type = DistanceType::EUCLIDIAN )
	{
		__DataType total_distance;

		for ( _DimType i = 0; i < __Dim__; ++i )
		{
			_FeatureBase::calculateDistanceComponent( total_distance, this->data_[i], other.data_[i], radii[i], uniform_weight, distance_type, i == 0 );
		}

		return sqrt( total_distance );
	}

	template<class _WeightType>
	__DataType distance( const _Feature & other, const __DataType & uniform_radius = __DataType( 0 ), const _WeightType & uniform_weight = _WeightType( 1 ), _DimType distance_type = DistanceType::EUCLIDIAN )
	{
		__DataType total_distance;

		for ( _DimType i = 0; i < __Dim__; ++i )
		{
			_FeatureBase::calculateDistanceComponent( total_distance, this->data_[i], other.data_[i], uniform_radius, uniform_weight, distance_type, i == 0 );
		}

		return sqrt( total_distance );
	}

	template<class _WeightType>
	__DataType distance( const _Feature & other, const __DataType & uniform_radius, std::array<_WeightType, __Dim__> weights, _DimType distance_type = DistanceType::EUCLIDIAN )
	{
		__DataType total_distance;

		for ( _DimType i = 0; i < __Dim__; ++i )
		{
			_FeatureBase::calculateDistanceComponent( total_distance, this->data_[i], other.data_[i], uniform_radius, weights[i], distance_type, i == 0 );
		}

		return sqrt( total_distance );
	}*/
};

template<class __DataType>
class Feature<__DataType, 1>
{
public:
	typedef __DataType _DataType;
	typedef FeatureBase<_DataType> _FeatureBase;
	typedef Feature<__DataType, 1> _Feature;

	const static _DimType _Dim_ = 1;
	_FeatureBase data_;

	Feature( const __DataType & data = __DataType( 0 ) ) :
			data_( data )
	{
		//
	}

	__DataType distance( const _Feature & other,
	                     __DataType radius = __DataType( 0 ) )
	{
		return data_.distanceTo( other.data_, radius );
	}
};

// struct designed solely to hold RGB values to be returned by other classes
struct OutputColorRGB
{
	typedef cv::Vec3b _CvColorType;

	const static _DimType NUM_COLORS = 7;

	struct Id
	{
		const static _DimType red = 0;
		const static _DimType orange = 1;
		const static _DimType yellow = 2;
		const static _DimType green = 3;
		const static _DimType blue = 4;
		const static _DimType black = 5;
		const static _DimType white = 6;
		const static _DimType unknown = 7;
	};

	struct Value
	{
		static _CvColorType red, orange, yellow, green, blue, black, white, unknown;
	};

	static _CvColorType & getColorRGB( const _DimType & color_id )
	{
		switch ( color_id )
		{
		case OutputColorRGB::Id::red:
			return OutputColorRGB::Value::red;
		case OutputColorRGB::Id::orange:
			return OutputColorRGB::Value::orange;
		case OutputColorRGB::Id::yellow:
			return OutputColorRGB::Value::yellow;
		case OutputColorRGB::Id::green:
			return OutputColorRGB::Value::green;
		case OutputColorRGB::Id::blue:
			return OutputColorRGB::Value::blue;
		case OutputColorRGB::Id::black:
			return OutputColorRGB::Value::black;
		case OutputColorRGB::Id::white:
			return OutputColorRGB::Value::white;
		}
		return OutputColorRGB::Value::unknown;
	}

	const static std::string getColorName( const _DimType & color_id )
	{
		switch ( color_id )
		{
		case OutputColorRGB::Id::red:
			return "red";
		case OutputColorRGB::Id::orange:
			return "orange";
		case OutputColorRGB::Id::yellow:
			return "yellow";
		case OutputColorRGB::Id::green:
			return "green";
		case OutputColorRGB::Id::blue:
			return "blue";
		case OutputColorRGB::Id::black:
			return "black";
		case OutputColorRGB::Id::white:
			return "white";
		}
		return "unknown";
	}
};

template<class __DataType = uchar, _DimType __Dim__ = 1>
class Color: public Feature<__DataType, __Dim__> {
public:
typedef __DataType _DataType;
typedef std::array<__DataType, __Dim__> _ArrayType;
typedef Feature<__DataType, __Dim__> _Feature;

template<class __InputDataType>
Color( __InputDataType * data ) :
_Feature()
{
	//std::memcpy( this->data_.begin(), data, __Dim__ * sizeof( __InputDataType ) );
	std::copy( data, data + __Dim__, this->data_.begin() );
}

Color( const _ArrayType data ) :
_Feature( data )
{
	//
}

Color() :
_Feature()
{

}

};

// template specialization for a single-channel color vector
template<class __DataType>
class Color<__DataType, 1> : public Feature<__DataType, 1>
{
public:
	typedef __DataType _DataType;
	typedef Feature<__DataType, 1> _Feature;

	template<class __InputDataType>
	Color( const __InputDataType & data = __InputDataType( 0 ) ) :
			_Feature( (__InputDataType ) data )
	{
		//
	}
};

// class to handle input from cv::Vec types
// __CvColorType must be some cv::Vec<typename _Tp, int cn>
template<class __CvColorType = cv::Vec3b>
class ColorCv: public Color<typename __CvColorType::value_type, __CvColorType::channels>
{
public:
	typedef Color<typename __CvColorType::value_type, __CvColorType::channels> _Color;
	typedef __CvColorType _CvColorType;
	typedef typename __CvColorType::value_type _DataType;

	ColorCv( const __CvColorType & cv_color = __CvColorType() ) :
			_Color( cv_color.val )
	{
		//
	}
};

//template <class __DataType>
//typedef Color<__DataType, 3> _Color3;

typedef ColorCv<cv::Vec3b> _ColorCv3b;
typedef ColorCv<cv::Vec3f> _ColorCv3f;

typedef Color<unsigned char, 3> _Color3b;
typedef Color<float, 3> _Color3f;

template<class __DataType, _DimType __Dim__ = 1>
class Threshold: public Vec<__DataType, __Dim__> {
public:
typedef Vec<__DataType, __Dim__> _Vec;
typedef std::array<__DataType, __Dim__> _ArrayType;

Threshold( _ArrayType data ) : _Vec( data )
{
	//
}

Threshold( ) : _Vec( )
{
	// empty constructor sets all arguments to 0
}
};

template<class __DataType>
class Threshold<__DataType, 1>
{
public:
	typedef __DataType _DataType;

	const static _DimType _Dim_ = 1;

	__DataType data_;

	Threshold( const __DataType & data = __DataType( 0 ) ) :
			data_( data )
	{
		//
	}
};

template<class __ColorType>
class ThresholdColor: public Threshold<typename __ColorType::_DataType, __ColorType::_Dim_>
{
public:
	typedef Threshold<typename __ColorType::_DataType, __ColorType::_Dim_> _Threshold;

	ThresholdColor() :
			_Threshold()
	{
		// empty constructor sets all arguments at 0
	}
};

template<class __DataType>
__DataType normalizeHue( __DataType hue, __DataType min, __DataType max )
{
	__DataType result;
	const __DataType radius = max - min;

	if( hue > radius ) result = math_utils::mod( result, radius );
	result += min;

	return result;
}

struct ThresholdedColor
{
	_Color3f color;
	float threshold;
	Vec<float, 3> weight;
	Vec<float, 3> variance;
	bool enabled;
};

#endif /* COLORS_H_ */
