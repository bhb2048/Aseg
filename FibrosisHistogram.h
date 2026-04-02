//
//	FibrosisHistogram.h - superclass to display and analyze Fibrosis histogram.
//
//	02-apr-12  bhb
//	Modified:
//
#ifndef FibrosisHistogram_H
#define FibrosisHistogram_H
#include "UI/FibrosisHistogramUI.h"
#include <ITK/Common.h>
#include <itkImageRegion.h>
#include <vector>
#include <string>

class FibrosisHistogram : public FibrosisHistogramUI
{
	public:
		FibrosisHistogram() {}
		virtual ~FibrosisHistogram() {}

		GreyType		minPixel()			{ return _minPixel;	}
		GreyType		maxPixel()			{ return _maxPixel;	}
		GreyType		getThreshold()		{ return _threshold;	}
		
		void			setPixels( std::vector<GreyType> *pix)	{ _pixels = pix;	}
		void			setAboveThresh( GreyType thr)	{ _aboveThreshOutput->value( thr);	}
		GreyType		getAboveThresh()	{ return (GreyType)_aboveThreshOutput->value();	}
		void			setPercentAboveThresh( float pc) { _perCentAboveThreshOutput->value( pc);	}
		float			getPerCentAboveThresh()	{ return (float)_perCentAboveThreshOutput->value();	}
		virtual void	getHistogram();
		
	private:
		std::vector<GreyType> *		_pixels;
		std::vector<unsigned int> 	_histogram;
		std::vector<unsigned int> 	_histogramSm;	// with smoothed part
	
		uint			_shift;
		float			_mean;
		float			_stdDev;
		GreyType		_threshold;
		GreyType		_minPixel, _maxPixel;
		uint			_meanXval;
		uint			_stdDevXval;
		
		virtual void	getMeanStd();
		virtual void	setThreshold();
		virtual void	smoothHistogram();
};
#endif