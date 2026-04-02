//
//	ThresholdFilter.cc - class for managing ITK intensity filter & plotting it.
//	
//	Modeled after Snap UserInterface/MainComponents/PreprocessingUILogic
//	Uses	libs/ITK/Filters/SmoothBinaryThresholdImageFilter - SmoothBinaryThresholdFunctor
//			libs/ITK/Filters/SmoothBinaryThresholdSettings - defaults set in loadImage()
//			libs/ITK/Common/Common.h - GreyTypeToNativeFunctor, NativeToGreyTypeFunctor
//	
//	13-dec-11	bhb
//	Modified:
//	02-dec-13  bhb	Add load/saveThresholdSettings()
//	10-nov-15  bhb	Modify to do 2D preview
//	10-mar-16  bhb	Remove thresholdOverlayChange()
//
#include "ThresholdFilter.h"
#include <fltk/run.h>				// wait()
#include <fltk/FileChooser.h>

using namespace std;
using namespace fltk;

ThresholdFilter::ThresholdFilter() : _fileChooser(0), ThresholdFilterUI()
{
	_directory = getenv( "IMAGE_DIR");
}

//
//	public functions, called from parent
//	
void
ThresholdFilter::initialize()
{
	_functionPlot->show();
	
	// Get the threshold parameters
	_settings = parent()->getThresholdSettings();

	// Shorthands
	GreyTypeToNativeFunctor g2n = parent()->image()->GetGrey()->GetNativeMapping();
	float lower = g2n((GreyType)settings().GetLowerThreshold());
	float upper = g2n((GreyType)settings().GetUpperThreshold());

	// Set the ranges for the two thresholds.	These ranges do not require the
	// lower slider to be less than the upper slider, that will be corrected
	// dynamically as the user moves the sliders
	double iMin = parent()->image()->GetGrey()->GetImageMinNative();
	double iMax = parent()->image()->GetGrey()->GetImageMaxNative();

	_lowerThresholdSlider->minimum(iMin);
	_lowerThresholdSlider->maximum(iMax);

	_upperThresholdSlider->minimum(iMin);
	_upperThresholdSlider->maximum(iMax);

	_thresholdSteepnessSlider->minimum(1);
	_thresholdSteepnessSlider->maximum(10);

	// Make sure that the specified range is valid
	if (lower > upper)
	{
		lower = 0.67 * iMin + 0.33 * iMax;
		upper = 0.33 * iMin + 0.67 * iMax;
	}

	// Make sure the current values of the upper and lower threshold are 
	// within the bounds (Nathan Moon)
	_lowerThresholdSlider->value( lower);
	_upperThresholdSlider->value( upper);

	_thresholdSteepnessSlider->value( settings().GetSmoothness());

	// Set the radio buttons
	if ( settings().IsLowerThresholdEnabled() && settings().IsUpperThresholdEnabled())
		_thresholdBelowAndAboveButton->setonly();
	else if (settings().IsLowerThresholdEnabled())
		_thresholdBelowButton->setonly();
	else
		_thresholdAboveButton->setonly();

	// Position the window and show it
//?	parent()->CenterChildWindowInMainWindow(m_WinInOut);

#ifdef USE_3D_SPEED
	// Get a handle to the snap image data
	SegImage3D *snapData = parent()->snapImage();

	// Initialize the speed image if necessary and assign it a color map
	if ( !snapData->IsSpeedLoaded())
		snapData->InitializeSpeed();

	snapData->GetSpeed()->SetColorMap(
		SpeedColorMap::GetPresetColorMap( COLORMAP_BLUE_BLACK_WHITE));

	// Set the speed image to In/Out mode
	snapData->GetSpeed()->SetModeToInsideOutsideSnake();
#endif

	// Apply the automatic preview preference
	_previewButton->value( 1); //? parent()->GetShowPreprocessedInOutPreview() ? 1 : 0);
	thresholdPreviewChange();

	// Use a callback method to enable / disable sliders
	thresholdDirectionChange();

	// Set up the plot range, etc
//?	FunctionPlot2DSettings &plotSettings = 
//?		m_BoxThresholdFunctionPlot->GetPlotter().GetSettings();

	// Compute the plot to be displayed
	updateThresholdPlot();
}

void
ThresholdFilter::close()
{
	// If in preview mode, disconnect and destroy the preview filters
#ifdef USE_3D_SPEED
	_InOutPreviewFilter[0] = 0;
	_InOutPreviewFilter[1] = 0;
	_InOutPreviewFilter[2] = 0;

	// Revert to the old speed image
	// TODO: We're reverting to the last APPLY.	The user may get confused.
	parent()->snapImage()->GetSpeed()->RemoveSliceSourcesForPreview();
#else
	_InOutPreviewFilter = 0;

	// Revert to the old speed image
	// TODO: We're reverting to the last APPLY.	The user may get confused.
//	parent()->snapImage()->GetSpeed2D()->RemoveSliceSourcesForPreview();
#endif
}

//
//	UI control functions, private
//	
void
ThresholdFilter::setLowerThreshold()
{
	// There may be a need to shift the upper bound
	if ( _upperThresholdSlider->value() < _lowerThresholdSlider->value())
	{
		_upperThresholdSlider->value( _lowerThresholdSlider->value());
	}

	// Call the generic callback
	setThresholdSettings();
}

void
ThresholdFilter::setUpperThreshold()
{
	// There may be a need to shift the lower bound
	if ( _upperThresholdSlider->value() < _lowerThresholdSlider->value())
	{
		_lowerThresholdSlider->value( _upperThresholdSlider->value());
	}

	// Call the generic callback
	setThresholdSettings();
}

//
//	UI "Smoothness" button
//	
void
ThresholdFilter::setThresholdSettings()
{
	// Pass the current GUI settings to the filter
	GreyTypeToNativeFunctor g2n = parent()->image()->GetGrey()->GetNativeMapping();
	NativeToGreyTypeFunctor n2g(g2n);
	settings().SetLowerThreshold(n2g( _lowerThresholdSlider->value()));
	settings().SetUpperThreshold(n2g( _upperThresholdSlider->value()));
	settings().SetSmoothness( _thresholdSteepnessSlider->value());
	settings().SetLowerThresholdEnabled( _lowerThresholdSlider->active());
	settings().SetUpperThresholdEnabled( _upperThresholdSlider->active());	

	// Store the settings globally
	parent()->setThresholdSettings( settings());

	// Compute the plot to be displayed
	updateThresholdPlot();

	// Apply the settings to the filter but only if we are in preview mode
	if ( _previewButton->value())
	{ 
#ifdef USE_3D_SPEED
		_InOutPreviewFilter[0]->SetThresholdSettings(settings());
		_InOutPreviewFilter[1]->SetThresholdSettings(settings());
		_InOutPreviewFilter[2]->SetThresholdSettings(settings());
#else
		_InOutPreviewFilter->SetThresholdSettings(settings());
#endif
		// Repaint the slice windows
		parent()->redraw();
	}	
}

//
//	"Below", "Above", "Below and Above" buttons
//	
void
ThresholdFilter::thresholdDirectionChange()
{
	// Enable and disable the state of the sliders based on the
	// current button settings
	if ( _thresholdBelowAndAboveButton->value())
	{
		_lowerThresholdSlider->activate();
		_upperThresholdSlider->activate();
	}
	else if ( _thresholdAboveButton->value())
	{
		_lowerThresholdSlider->deactivate();
//		_lowerThresholdSlider->value( parent()->image()->GetGrey()->GetImageMinNative());
		_upperThresholdSlider->activate();
	}
	else	// _thresholdBelowButton
	{
		_lowerThresholdSlider->activate();
//		_upperThresholdSlider->value(	parent()->image()->GetGrey()->GetImageMaxNative());
		_upperThresholdSlider->deactivate();
	}

	// The settings have changed, so call that method
	setThresholdSettings();
}

//
//	Called by UI "Preview result" button and from initialize().
//	
void
ThresholdFilter::thresholdPreviewChange()
{
	bool preview = (_previewButton->value() == 1);

	if ( preview)
	{
		// Initialize preview filter

		// Get a handle to the snap image data
		SegImage3D *snapImg = parent()->segImage3D();

#ifdef USE_3D_SPEED
		for ( unsigned int i=0; i<3; i++)
		{
			// Make sure the preview filter is deallocated
			assert( !_InOutPreviewFilter[i]);

			// Create the filter
			_InOutPreviewFilter[i] = InOutFilterType::New();

			// Give it an input
			_InOutPreviewFilter[i]->SetInput( parent()->segImage3D()->GetGrey()->GetImage());

			// Pass the current settings to the filter
			_InOutPreviewFilter[i]->SetThresholdSettings( settings());
		}

		// Attach the preview filters to the corresponding slicers
		for ( unsigned int j=0; j<3; j++)
		{
			// What is the image axis corresponding to the j-th slicer?
			unsigned int iSliceAxis = 
				snapImg->GetSpeed()->GetDisplaySliceImageAxis(j);

			// Connect the previewer to that slicer
			snapImg->GetSpeed()->SetSliceSourceForPreview(
				j, _InOutPreviewFilter[iSliceAxis]->GetOutput());
		}
#else
		// Make sure the preview filter is deallocated
		assert( !_InOutPreviewFilter);

		// Create the filter
		_InOutPreviewFilter = InOutFilterType::New();

		// Give it an input
		snapImg->GetGrey()->GetSlicer(0)->Modified();
		snapImg->GetGrey()->GetSlicer(0)->Update();
		_InOutPreviewFilter->SetInput( snapImg->GetGrey()->GetSlice(0));

		// Pass the current settings to the filter
		_InOutPreviewFilter->SetThresholdSettings( settings());

		_InOutPreviewFilter->Update();
		
		// Set SegImage3D's m_SpeedWrapper2D
		snapImg->InitializeSpeed2D( _InOutPreviewFilter->GetOutput());

		// and init colormap
		snapImg->GetSpeed2D()->SetColorMap(
			SpeedColorMap::GetPresetColorMap( COLORMAP_BLUE_BLACK_WHITE));

		// Set the speed image to In/Out mode
		snapImg->GetSpeed2D()->SetModeToInsideOutsideSnake();

		// debug: display min/max
		float min = snapImg->GetSpeed2D()->GetImageMin();
		float max = snapImg->GetSpeed2D()->GetImageMax();
		cout << "EdgeFilter::EdgePreviewChange: min = " << min << ", max = " << max << endl;
#endif
	}
	else
	{
		// Clear the preview filters
#ifdef USE_3D_SPEED
		_InOutPreviewFilter[0] = 0;
		_InOutPreviewFilter[1] = 0;
		_InOutPreviewFilter[2] = 0;

		// Revert to the old speed image
		// TODO: We're reverting to the last APPLY.	The user may get confused.
		parent()->segImage3D()->GetSpeed()->RemoveSliceSourcesForPreview();
#else
		_InOutPreviewFilter = 0;
		// Revert to the old speed image
		// TODO: We're reverting to the last APPLY.	The user may get confused.
//		snapImg->GetSpeed2D()->RemoveSliceSourcesForPreview();
#endif
	}

	// Notify parent of the update
	parent()->preprocessingPreview( preview);
}

//
//	For slice change, called from Aseg::sliceChange()
//	
void
ThresholdFilter::thresholdPreviewUpdate()
{
	static bool change = true;	// kludge to make image change when change slice slider
	if ( showPreview())			// see log 2014-06-19 (and previous)
	{
		if ( change)
			settings().SetSmoothness( _thresholdSteepnessSlider->value()+0.001F);
		else
			settings().SetSmoothness( _thresholdSteepnessSlider->value());
		change = !change;
#ifdef USE_3D_SPEED
		for ( unsigned int j=0; j<3; j++)
			_InOutPreviewFilter[j]->SetThresholdSettings(settings());
#else
		_InOutPreviewFilter->SetThresholdSettings(settings());
#endif
	}
}

void
ThresholdFilter::updateThresholdPlot()
{
	// Create a functor object used in the filter
	SmoothBinaryThresholdFunctor<float,float> functor;

	// We need to know the min/max of the image
	GreyTypeToNativeFunctor g2n =
		parent()->image()->GetGrey()->GetNativeMapping();
	NativeToGreyTypeFunctor n2g(g2n);
	float iMin = n2g(_lowerThresholdSlider->minimum());
	float iMax = n2g(_upperThresholdSlider->maximum());

	// Pass the settings to the functor
	functor.SetParameters(iMin,iMax,_settings);

	// Compute the function for a range of values
	const unsigned int nSamples = 200;
	float x[nSamples];
	float y[nSamples];

	for(unsigned int i=0;i<nSamples;i++) 
	{
		x[i] = iMin + i * (iMax-iMin) / float(nSamples-1);
		y[i] = functor(x[i]);
	}

	// Pass the results to the plotter
	_functionPlot->setDataPoints( x, y, nSamples);

	// Redraw the box
	_functionPlot->redraw();
}

void
ThresholdFilter::loadThresholdSettings()
{
	if ( _fileChooser == 0)
	{
		_fileChooser = new FileChooser( _directory.c_str(), "*.thr",
					FileChooser::SINGLE, "Load Threshold Settings");
	}
	else
	{
		_fileChooser->type( FileChooser::SINGLE);
		_fileChooser->label("Load Threshold Settings");
		_fileChooser->filter( "*.thr");
	}
	_fileChooser->show( 300, 300);

	while ( _fileChooser->visible())
		fltk::wait();

	if ( _fileChooser->count() > 0)
	{
		ifstream inFile;
		inFile.open( _fileChooser->value(), ios::in);
		if ( inFile)
		{
			int type;
			string line;
			double val;
			getline( inFile, line);		// should be 'Threshold Settings'
			inFile >> val;
			_lowerThresholdSlider->value( val);
			inFile >> val;
			_upperThresholdSlider->value( val);
			inFile >> val;
			_thresholdSteepnessSlider->value( val);
			inFile.close();
			
			setThresholdSettings();
		}
		else
		{
			alert( "Error opening %s", _fileChooser->value());
		}
	}
}

void
ThresholdFilter::saveThresholdSettings()
{
	if ( _fileChooser == 0)
	{
		_fileChooser = new FileChooser( _directory.c_str(), "*.thr",
					FileChooser::CREATE, "Save Threshold Settings");
	}
	else
	{
		_fileChooser->type( FileChooser::CREATE);
		_fileChooser->label("Save Threshold Settings");
		_fileChooser->filter( "*.thr");
	}
	_fileChooser->show( 300, 300);

	while ( _fileChooser->visible())
		fltk::wait();

	if ( _fileChooser->count() > 0)
	{
		ofstream outFile;
		outFile.open( _fileChooser->value(), ios::out);
		if ( outFile)
		{
			outFile << "Threshold Settings" << endl;
			outFile << _lowerThresholdSlider->value() << endl;
			outFile << _upperThresholdSlider->value() << endl;
			outFile << _thresholdSteepnessSlider->value() << endl;
			outFile.close();
		}
		else
		{
			alert( "Error opening %s", _fileChooser->value());
		}
	}
}
