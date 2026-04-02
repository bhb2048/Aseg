//
//	ImageIntensity.h - class for managing ITK intensity filter.
//	
//	31-may-12	bhb
//	Modified:
//	
#include "Aseg.h"
#include "ImageIntensity.h"

ImageIntensity::ImageIntensity( Aseg *parent) : ImageIntensityUI(), _Aseg(parent)
{
}

void
ImageIntensity::intensityChanged()
{
	parent()->intensityChanged();
}
