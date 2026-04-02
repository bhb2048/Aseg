//
//	EdgeFilter.h - class for getting ITK/EdgePreprocessingImageFilter settings
//	
//	26-mar-15  bhb
//	Modified:
//	03-nov-15  bhb	Modify to do 2D preview
//	24-oct-18  bhb	Add 'USEGPU', work w/ GPUImages
//	
#ifndef _EdgeFilter_H
#define _EdgeFilter_H

#include "UI/EdgeFilterUI.h"
#include <ITK/EdgePreprocessingImageFilter.h>	// #define USEGPU

//	Note: using EdgePreprocessingImageFilter in 2D here.  Is 3D in 
//	SegImage::DoEdgePreprocessing.

class Aseg;

class EdgeFilter : public EdgeFilterUI
{
	public:
		EdgeFilter();
		virtual ~EdgeFilter()  {}

		void		setParent( Aseg *p)				{ _Aseg = p;	}
		Aseg *		parent()						{ return _Aseg;	}
		void		setDirectory( std::string &d)	{ _directory = d;	}
						 
		EdgePreprocessingSettings &	settings()		{ return _settings;	}
		bool		showPreview()					{ return _previewButton->value();	}
		
		void		initialize();
		void		close();
		void		EdgePreviewUpdate();
		
	private:
		Aseg *				_Aseg;					// parent app
		EdgePreprocessingSettings	_settings;

		/** The image types used for preprocessing */
		enum { Dimension =  2};
		typedef itk::Image<GreyType, Dimension>	GreyImageType;
		typedef itk::Image<float, Dimension>	SpeedImageType;
#ifdef USEGPU
		typedef itk::GPUImage<GreyType, Dimension>	GPUGreyImageType;
		typedef itk::GPUImage<float, Dimension>	GPUSpeedImageType;
		/** The filter type for edge processing */
		typedef EdgePreprocessingImageFilter<GPUGreyImageType, GPUSpeedImageType>																		EdgeFilterType;
#else
		/** The filter type for edge processing */
		typedef EdgePreprocessingImageFilter<GreyImageType, SpeedImageType>	EdgeFilterType;
#endif
		typedef itk::SmartPointer<EdgeFilterType> 						EdgeFilterPointer;
		
		/** The filter used for edge processing */
		EdgeFilterPointer _EdgePreviewFilter;

		fltk::FileChooser *	_fileChooser;
		std::string			_directory;

#ifndef SIGMOID
		virtual void	UpdateEdgePlot();
#endif		
		virtual void	SetEdgeSettings();
		virtual void	EdgePreviewChange();
		virtual void	loadEdgeSettings();
		virtual void	saveEdgeSettings();
};
#endif