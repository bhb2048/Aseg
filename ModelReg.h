//
//	ModelReg.h -- model to image registration
//	
//	21-apr-14  bhb
//
//	Modified:
//	
#ifndef ModelReg_H
#define ModelReg_H
#include "itkEllipseSpatialObject.h"

class SpeedImageWrapper3D;

class ModelReg
{
	public:
		typedef itk::EllipseSpatialObject< 2>	EllipseType;

		ModelReg();
		virtual ~ModelReg();

		void			setImage( SpeedImageWrapper3D *img)	{ _image = img;	}
		SpeedImageWrapper3D *	image()						{ return _image;	}
//		EllipseType::Pointer	ellipse()					{ return _ellipse;	}
		void			initEllipse( double radius, double x, double y);
		bool			Register();
		
	protected:

	private:
		SpeedImageWrapper3D *	_image;
//		EllipseType::Pointer	_ellipse;
		double					_radius;
		double					_x, _y;			// initial ellipse center
};
#endif