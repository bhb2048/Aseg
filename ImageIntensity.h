//
//	ImageIntensity.h
//	
//	30-may-12  bhb
//	Modified:
//	
#ifndef IntensityFilter_H
#define IntensityFilter_H
#include "UIFl/ImageIntensityUI.h"

class Aseg;


class ImageIntensity : public ImageIntensityUI
{
	public:
		ImageIntensity( Aseg* parent);
		virtual ~ImageIntensity() {}

		Aseg *		parent()					{ return _Aseg;	}

	private:

		Aseg *				_Aseg;					// parent app
		virtual void intensityChanged();
};
#endif