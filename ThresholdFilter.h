//
//	ThresholdFilter.h - class for managing ITK intensity filter & plotting it.
//	
//	Modeled after Snap UserInterface/MainComponents/PreprocessingUILogic
//	
//	13-dec-11  bhb
//	Modified:
//	02-dec-13  bhb	Add load/saveThresholdSettings()
//	10-nov-15  bhb	Modify to do 2D preview
//	10-mar-16  bhb	Remove thresholdOverlayChange()
//	
#ifndef _INTENSITYFILTER_H
#define _INTENSITYFILTER_H

#include "Aseg.h"
#include "UI/ThresholdFilterUI.h"
#include <ITK/SmoothBinaryThresholdImageFilter.h>
#include <string>

class ThresholdFilter : public ThresholdFilterUI
{
	public:
		ThresholdFilter();
		virtual ~ThresholdFilter() {}

		void		setParent( Aseg *p)				{ _Aseg = p;	}
		Aseg *		parent()						{ return _Aseg;	}
		void		setDirectory( std::string &d)	{ _directory = d;	}
						 
		ThresholdSettings &	settings()				{ return _settings;	}
		bool		showPreview()					{ return _previewButton->value();	}
		
		void		initialize();
		void		close();
		void		thresholdPreviewUpdate();
		
	private:

		Aseg *				_Aseg;					// parent app
		ThresholdSettings	_settings;

		/** The image types used for preprocessing */
#ifdef USE_3D_SPEED
		enum { Dimension =  3};
#else
		enum { Dimension =  2};
#endif
		typedef itk::Image<GreyType,Dimension> GreyImageType;
		typedef itk::Image<float,Dimension> SpeedImageType;

		/** The filter type for in/out processing */
		typedef SmoothBinaryThresholdImageFilter< GreyImageType, SpeedImageType> 
															InOutFilterType;
		typedef itk::SmartPointer<InOutFilterType> InOutFilterPointer;
		
		/** The filter used for in-out thresholding */
#ifdef USE_3D_SPEED
		InOutFilterPointer _InOutPreviewFilter[3];
#else
		InOutFilterPointer _InOutPreviewFilter;
#endif
		fltk::FileChooser *	_fileChooser;
		std::string			_directory;

		virtual void setLowerThreshold();
		virtual void setUpperThreshold();
		virtual void setThresholdSettings();
		virtual void thresholdDirectionChange();
		virtual void thresholdPreviewChange();
		virtual void loadThresholdSettings();
		virtual void saveThresholdSettings();
		
		void		updateThresholdPlot();
};
#endif
