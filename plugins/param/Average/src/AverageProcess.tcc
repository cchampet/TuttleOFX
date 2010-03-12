#include "AveragePlugin.hpp"
#include "AverageProcess.hpp"
#include <tuttle/common/image/gilGlobals.hpp>
#include <boost/gil/extension/numeric/pixel_numeric_operations.hpp>

namespace boost {
namespace gil {
namespace detail {

////////////////////////////////////////////////////////////////////////////////

/// \ingroup ChannelNumericOperations
/// \brief ch2 += ch1
/// structure for adding one channel to another
/// this is a generic implementation; user should specialize it for better performance
template <typename ChannelSrc,typename ChannelDst>
struct channel_plus_assign_t : public std::binary_function<ChannelSrc,ChannelDst,ChannelDst>
{
    typename channel_traits<ChannelDst>::reference
	operator()( typename channel_traits<ChannelSrc>::const_reference ch1,
                typename channel_traits<ChannelDst>::reference ch2 ) const
	{
        return ch2 += ChannelDst( ch1 );
    }
};

/// \ingroup PixelNumericOperations
/// \brief p2 += p1
template <typename PixelSrc, // models pixel concept
          typename PixelDst> // models pixel value concept
struct pixel_plus_assign_t
{
    PixelDst& operator()( const PixelSrc& p1,
                          PixelDst& p2 ) const
	{
        static_for_each( p1, p2,
                         channel_plus_assign_t<typename channel_type<PixelSrc>::type,
                                                 typename channel_type<PixelDst>::type>() );
        return p2;
    }
};

////////////////////////////////////////////////////////////////////////////////

/// \ingroup ChannelNumericOperations
/// \brief ch /= s
/// structure for dividing a channel by a scalar
/// this is a generic implementation; user should specialize it for better performance
template <typename Scalar, typename ChannelDst>
struct channel_divides_scalar_assign_t : public std::binary_function<Scalar,ChannelDst,ChannelDst>
{
    typename channel_traits<ChannelDst>::reference
	operator()( const Scalar& s,
	            typename channel_traits<ChannelDst>::reference ch ) const
	{
        return ch /= ChannelDst(s);
    }
};

/// \ingroup PixelNumericOperations
/// \brief p /= s
template <typename Scalar, // models a scalar type
	      typename PixelDst>  // models pixel concept
struct pixel_divides_scalar_assign_t
{
    PixelDst& operator()( const Scalar& s,
	                      PixelDst& p ) const
	{
        static_for_each( p, std::bind1st( channel_divides_scalar_assign_t<Scalar, typename channel_type<PixelDst>::type>(), s ) );
		return p;
    }
};

////////////////////////////////////////////////////////////////////////////////

/// \ingroup ChannelNumericOperations
/// \brief ch2 = min( ch1, ch2 )
/// structure for adding one channel to another
/// this is a generic implementation; user should specialize it for better performance
template <typename ChannelSrc,typename ChannelDst>
struct channel_assign_min_t : public std::binary_function<ChannelSrc,ChannelDst,ChannelDst>
{
    typename channel_traits<ChannelDst>::reference
	operator()( typename channel_traits<ChannelSrc>::const_reference ch1,
                typename channel_traits<ChannelDst>::reference ch2 ) const
	{
        return ch2 = std::min( ChannelDst( ch1 ), ch2 );
    }
};

/// \ingroup PixelNumericOperations
/// \brief p2 = min( p1, p2 )
template <typename PixelSrc, // models pixel concept
          typename PixelDst> // models pixel value concept
struct pixel_assign_min_t
{
    PixelDst& operator()( const PixelSrc& p1,
                          PixelDst& p2 ) const
	{
        static_for_each( p1, p2,
                         channel_assign_min_t<typename channel_type<PixelSrc>::type,
                                              typename channel_type<PixelDst>::type>() );
        return p2;
    }
};

////////////////////////////////////////////////////////////////////////////////

/// \ingroup ChannelNumericOperations
/// \brief ch2 = max( ch1, ch2 )
/// structure for adding one channel to another
/// this is a generic implementation; user should specialize it for better performance
template <typename ChannelSrc,typename ChannelDst>
struct channel_assign_max_t : public std::binary_function<ChannelSrc,ChannelDst,ChannelDst>
{
    typename channel_traits<ChannelDst>::reference
	operator()( typename channel_traits<ChannelSrc>::const_reference ch1,
                typename channel_traits<ChannelDst>::reference ch2 ) const
	{
        return ch2 = std::max( ChannelDst( ch1 ), ch2 );
    }
};

/// \ingroup PixelNumericOperations
/// \brief p2 = max( p1, p2 )
template <typename PixelSrc, // models pixel concept
          typename PixelDst> // models pixel value concept
struct pixel_assign_max_t
{
    PixelDst& operator()( const PixelSrc& p1,
                          PixelDst& p2 ) const
	{
        static_for_each( p1, p2,
                         channel_assign_max_t<typename channel_type<PixelSrc>::type,
                                              typename channel_type<PixelDst>::type>() );
        return p2;
    }
};

////////////////////////////////////////////////////////////////////////////////

}
}
}



namespace tuttle {
namespace plugin {
namespace average {

template<class View>
AverageProcess<View>::AverageProcess( AveragePlugin &instance )
: ImageGilFilterProcessor<View>( instance )
, _plugin( instance )
{
	this->setNoMultiThreading( );
}

template<class View>
void AverageProcess<View>::setup( const OFX::RenderArguments &args )
{
	using namespace boost::gil;
	using namespace boost::gil::detail;

	ImageGilFilterProcessor<View>::setup( args );

	// recovery parameters values
	OfxRectD srcRod = this->_srcClip->getCanonicalRod( args.time );
	_processParams = _plugin.getProcessParams( srcRod );

	Point2 rectSize( std::abs( _processParams._rect.x2 - _processParams._rect.x1 ),
					 std::abs( _processParams._rect.y2 - _processParams._rect.y1 ) );

	COUT_VAR( _processParams._rect );
	COUT_VAR( rectSize );

	typedef pixel<bits32f, layout<typename color_space_type<View>::type> > Pixel32f;
	typedef pixel<typename channel_type<View>::type, layout<gray_t> > PixelGray;

	// declare values and init
	Pixel firstPixel = this->_srcView[0]; // for initialization only
	PixelGray firstPixelGray;
	color_convert( firstPixel, firstPixelGray );

	Pixel32f average;
	pixel_zeros_t<Pixel32f>()( average );
	Pixel minChannel = firstPixel;
	Pixel maxChannel = firstPixel;
	Pixel minLuminosity = firstPixel;
	PixelGray minLuminosityGray = firstPixelGray;
	Pixel maxLuminosity = firstPixel;
	PixelGray maxLuminosityGray = firstPixelGray;

	for( int y = _processParams._rect.y1;
		 y < _processParams._rect.y2;
		 ++y )
	{
		typename View::x_iterator src_it = this->_srcView.x_at( _processParams._rect.x1, y );
		Pixel32f lineAverage;
		pixel_assigns_t<Pixel, Pixel32f>()( *(src_it++), lineAverage ); // init with first value
		
		for( int x = _processParams._rect.x1+1;
			 x < _processParams._rect.x2;
			 ++x, ++src_it )
		{
			// for average : accumulate
			pixel_plus_assign_t<Pixel, Pixel32f>()( ( *src_it ), lineAverage ); // lineAverage += src_it;

			// search min for each channel
			pixel_assign_min_t<Pixel, Pixel>()( *src_it, minChannel );
			// search max for each channel
			pixel_assign_max_t<Pixel, Pixel>()( *src_it, maxChannel );
			
			PixelGray grayCurrentPixel; // current pixel in gray colorspace
			color_convert( *src_it, grayCurrentPixel );
			// search min luminosity
			if( get_color( grayCurrentPixel, gray_color_t() ) < get_color( minLuminosityGray, gray_color_t() ) )
			{
				minLuminosity = *src_it;
				minLuminosityGray = grayCurrentPixel;
			}
			// search max luminosity
			if( get_color( grayCurrentPixel, gray_color_t() ) > get_color( maxLuminosityGray, gray_color_t() ) )
			{
				maxLuminosity = *src_it;
				maxLuminosityGray = grayCurrentPixel;
			}
		}
		// for average : divide by number of accumulated pixels
		pixel_divides_scalar_assign_t<double, Pixel32f>()( rectSize.x, lineAverage ); // lineAverage /= rectSize.x;
		// for average : accumulate each line
		pixel_plus_assign_t<Pixel32f, Pixel32f>()( lineAverage, average ); // _average += lineAverage;
	}
	// for average : divide by number of accumulated lines
	pixel_divides_scalar_assign_t<double, Pixel32f>()( rectSize.y, average ); // _average /= rectSize.y;


	rgba32f_pixel_t rgbaParamAverage;
	color_convert( average, rgbaParamAverage );

	_plugin._outputAverage->setValueAtTime( args.time,
	                                        get_color( rgbaParamAverage, red_t() ),
	                                        get_color( rgbaParamAverage, green_t() ),
	                                        get_color( rgbaParamAverage, blue_t() ),
	                                        get_color( rgbaParamAverage, alpha_t() )
	                                      );
	_plugin._outputChannelMin->setValueAtTime( args.time,
	                                        get_color( minChannel, red_t() ),
	                                        get_color( minChannel, green_t() ),
	                                        get_color( minChannel, blue_t() ),
	                                        get_color( minChannel, alpha_t() )
	                                      );
	_plugin._outputChannelMax->setValueAtTime( args.time,
	                                        get_color( maxChannel, red_t() ),
	                                        get_color( maxChannel, green_t() ),
	                                        get_color( maxChannel, blue_t() ),
	                                        get_color( maxChannel, alpha_t() )
	                                      );
	_plugin._outputLuminosityMin->setValueAtTime( args.time,
	                                        get_color( minLuminosity, red_t() ),
	                                        get_color( minLuminosity, green_t() ),
	                                        get_color( minLuminosity, blue_t() ),
	                                        get_color( minLuminosity, alpha_t() )
	                                      );
	_plugin._outputLuminosityMax->setValueAtTime( args.time,
	                                        get_color( maxLuminosity, red_t() ),
	                                        get_color( maxLuminosity, green_t() ),
	                                        get_color( maxLuminosity, blue_t() ),
	                                        get_color( maxLuminosity, alpha_t() )
	                                      );
	
	switch( _processParams._chooseOutput )
	{
		case eChooseOutputSource:
			break;
		case eChooseOutputAverage:
			color_convert( average, _outputPixel );
			break;
		case eChooseOutputChannelMin:
			color_convert( minChannel, _outputPixel );
			break;
		case eChooseOutputChannelMax:
			color_convert( maxChannel, _outputPixel );
			break;
		case eChooseOutputLuminosityMin:
			color_convert( minLuminosity, _outputPixel );
			break;
		case eChooseOutputLuminosityMax:
			color_convert( maxLuminosity, _outputPixel );
			break;
	}
}

/**
 * @param[in] procWindow  Processing window
 */
template<class View>
void AverageProcess<View>::multiThreadProcessImages( const OfxRectI& procWindow )
{
	using namespace boost::gil;
	if( _processParams._chooseOutput == eChooseOutputSource )
	{
		for( int y = procWindow.y1;
			 y < procWindow.y2;
			 ++y )
		{
			typename View::x_iterator src_it = this->_srcView.x_at( procWindow.x1, y );
			typename View::x_iterator dst_it = this->_dstView.x_at( procWindow.x1, y );
			for( int x = procWindow.x1;
				 x < procWindow.x2;
				 ++x, ++src_it, ++dst_it )
			{
				(*dst_it) = (*src_it);
			}
			if( this->progressForward() )
				return;
		}
	}
	else
	{
		for( int y = procWindow.y1;
			 y < procWindow.y2;
			 ++y )
		{
			typename View::x_iterator dst_it = this->_dstView.x_at( procWindow.x1, y );
			for( int x = procWindow.x1;
				 x < procWindow.x2;
				 ++x, ++dst_it )
			{
				( *dst_it ) = _outputPixel;
			}
			if( this->progressForward( ) )
				return;
		}
	}
}

}
}
}
