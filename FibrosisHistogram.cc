//
//	FibrosisHistogram.cc - superclass to display and analyze Fibrosis histogram.
//
//	02-apr-12	bhb
//	Modified:
//
#include "FibrosisHistogram.h"
#include <fltk/events.h>
#include <fltk/ask.h>
#include <ColorsPacked.h>
#include <Math/MinMax.h>
#include <Math/StdDev.h>
#include <Math/recipes.h>
#include <fltk/run.h>
#include <fltk/ask.h>
#include <iostream>
#include <fstream>

using namespace std;
using namespace fltk;

void
FibrosisHistogram::getHistogram()
{
	// find pixel min, max
	uint npix = _pixels->size();

	_minPixel = _maxPixel = (*_pixels)[0];
	for ( uint i=1; i < npix; i++)
	{
		GreyType pix = (*_pixels)[i];
		if ( pix < _minPixel)
			_minPixel = pix;
		else if ( pix > _maxPixel)
			_maxPixel = pix;
	}
	
	// set bins
	uint span = _maxPixel - _minPixel + 1;
	_shift = 0;
	uint numBins = 64 << uint(_numBinsChoice->value());
	
	while ( span > numBins)
	{
		_shift++;
		span = span >> 1;
	}
	span++;			// need one more since zero based
	_histogram.clear();
	_histogram.assign( span, 0);

	cout << "FibrosisHistogram::getHistogram: span = " << span << endl;
	for ( uint i=1; i < npix; i++)
	{
		uint pix = (*_pixels)[i] - _minPixel;		// make positive
		pix = pix >> _shift;
		if ( pix > span)
		{
			cerr << "pix > span " << pix << ", pixel " << (*_pixels)[i] << endl;
			continue;
		}
		_histogram[pix]++;
	}

	// update UI
	_minPixelOutput->value( _minPixel);
	_maxPixelOutput->value( _maxPixel);
	_totalPixelsOutput->value( npix);
	setAboveThresh( 0);
	
	// reset UI outputs
	_meanOutput->value( 0.);
	_stdDevOutput->value( 0.);
	_thresholdOutput->value( 0.);
	_pixelThresholdOutput->value( 0.);
	
	// turn off markers
	for ( uint i=0; i<4; i++)
		_plot->setHorizMarkerOff( i);

	// init plot
	_plot->show();
	_plot->setDataPoints( _histogram);
	_smoothToggle->value( false);
	_plot->setHorizMarker(0, float(numBins) * 0.02F);		// 2%
	_plot->setHorizMarker(1, float(numBins) * 0.4F);		// 40%
}

//
//	Called from Calc button on FibrosisHistogramUI
//	
void
FibrosisHistogram::getMeanStd()
{
	if ( !(_plot->getHorizMarkerOn(0) && _plot->getHorizMarkerOn(1)))
	{
		alert( "Set both markers first");
		return;
	}

	uint starti = _plot->getHorizMarker( 0);
	uint endi	= _plot->getHorizMarker( 1);
	vector<float> hist;
	float minv, maxv;
	int minpt, maxpt;
	uint x1, x2;
	
	if ( _smoothToggle->value())
	{
		for ( uint i=starti; i<endi; i++)
			hist.push_back( float(_histogramSm[i]));
	}
	else
	{
		for ( uint i=starti; i<endi; i++)
			hist.push_back( float(_histogram[i]));
	}
	
	StdDev( &hist[0], endi-starti, _mean, _stdDev);

	// Find mean xval (intensity), search both directions, if diff, split diff
	// Still in bin units.	Assume _mean on uphill or near peak
	x1 = starti;
	uint meani = (uint)_mean;
	for ( ; _histogram[x1] < meani; x1++);
	x2 = endi;
	if ( _histogram[x2] > meani)
		for ( ; _histogram[x2] > meani; x2--);		// on downhill side
	else
		for ( ; _histogram[x2] < meani; x2--);		// on uphill side (mean near peak)
	_meanXval = (x1 + x2) >> 1;

	// Find std dev xval.	Normally mean would be center of bell curve but depends on
	// interval selected so look ahead of _meanXval to see whether curve going up or down.
	// Still in bin units.
	x1 = _meanXval;		// search both directions, if diff, split diff
	x2 = endi;
	uint threshi = uint(_mean + _stdDev);
	if ( _histogram[x2] > _histogram[_meanXval])	// going up
	{
		for ( ; _histogram[x1] < threshi; x1++);
		for ( ; _histogram[x2] > threshi; x2--);
	}
	else											// going down
	{
		for ( ; _histogram[x1] > threshi; x1++);
		for ( ; _histogram[x2] < threshi; x2--);
	}
	int meanX = _meanXval;		// use int and abs to avoid neg problems
	int xval = (x1 + x2) >> 1;
	_stdDevXval = abs( xval - meanX);	// this an intensity difference (bin units)
	
	// Set dialog values (y values - histogram pixel counts)
	_meanOutput->value( _mean);
	_stdDevOutput->value( _stdDev);

	setThreshold();
}

//
//	Called from getMeanStd() & Threshold Mult slider
//	
void
FibrosisHistogram::setThreshold()
{
	// Using intensities in bin units, convert to real with shift and add to min intenisty.
	uint threshi = _meanXval + uint(_thresholdSlider->value() * _stdDevXval);
	_threshold = _minPixel + (threshi << _shift);
	
	_thresholdOutput->value( double(threshi));
	_pixelThresholdOutput->value( _threshold);

	// find pixels above threshold
	uint nAbove = 0;
	for ( uint i = 0; i < _pixels->size(); i++)
		if ( (*_pixels)[i] > _threshold)
			nAbove++;

	setAboveThresh( nAbove);
	setPercentAboveThresh( ((float)nAbove / (float)_pixels->size()) * 100.0F);
	_plot->setHorizMarkerColor( 2, CYAN_P);
	_plot->setHorizMarker( 2, _meanXval);
	_plot->setHorizMarkerColor( 3, BLUISH_P);
	_plot->setHorizMarker( 3, threshi);
	_plot->redraw();
}

//
//	Called from FibrosisHistogramUI smooth button
//	
void
FibrosisHistogram::smoothHistogram()
{
	if ( _smoothToggle->value())
	{
		int ndata = _histogram.size();
		int nCoef = 3 + _nCoefsChoice->value();
		float *x = fvector(1, ndata);
		float *y = fvector(1, ndata);
		float *coefs = fvector(1, nCoef);
		float chisq;
		
		for ( int i=1; i<=ndata; i++)
		{
			int di = i-1;
			x[i] = di;
			y[i] = _histogram[di];
		}

		svdfitb( x, y, ndata, coefs, nCoef, &chisq);

//		_histogramSm.clear();
//		_histogramSm.reserve( ndata);
		_histogramSm = _histogram; 

		// debug
//		cout << "i\tx\ty\ty'" << endl;
		for ( int i=1; i<=ndata; i++)
		{
			float newy = 0.0;
			float xpow = 1.0;
			for ( int j=1; j<=nCoef; j++)
			{
				newy += coefs[j] * xpow;
				xpow *= x[i];
			}
			_histogramSm[i-1] = newy > float(0) ? (uint)newy : 0;
//			cout << i-1 << "\t" << x[i] << "\t" << y[i] << "\t" << newy << endl;
		}
		cout << "chisq = " << chisq << endl;
		cout << "coefs: ";
		for ( int i=1; i<=nCoef; i++)
			cout << coefs[i] << " ";
		cout << endl;

		_plot->setDataPoints( _histogramSm);
	}
	else
	{
		_plot->setDataPoints( _histogram);
	}
}
