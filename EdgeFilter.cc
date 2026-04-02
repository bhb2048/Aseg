//
//	EdgeFilter.cc - class for getting ITK/EdgePreprocessingImageFilter settings
//	
//	26-mar-15  bhb
//	Modified:
//	03-nov-15  bhb	Modify to do 2D preview
//	24-oct-18  bhb	Add 'USEGPU', work w/ GPUImages
//	
#include "Aseg.h"
#include "EdgeFilter.h"
#include <fltk/run.h>				// wait()
#include <fltk/FileChooser.h>

using namespace std;
using namespace fltk;

EdgeFilter::EdgeFilter() : _fileChooser(0), EdgeFilterUI()
{
	_directory = getenv( "IMAGE_DIR");
}

//
//	public functions, called from parent
//	
void
EdgeFilter::initialize()
{
#ifndef SIGMOID
	_FunctionPlot->show();
#endif
	// Get the Edge parameters
	_settings = parent()->getEdgeSettings();

#ifdef SIGMOID
	_EdgeSigmaSlider->value( settings().GetSigma());
	_EdgeAlphaSlider->value( settings().GetAlpha());
	_EdgeBetaSlider->value( settings().GetBeta());
#else
	_EdgeSigmaSlider->value( settings().GetGaussianBlurScale());
	_EdgeKappaSlider->value( settings().GetRemappingSteepness());
	_EdgeExponentSlider->value( settings().GetRemappingExponent());
#endif
	// Position the window and show it
//?	parent()->CenterChildWindowInMainWindow(m_WinInOut);

	// Apply the automatic preview preference
	_previewButton->value( 1);	// need 'true' so sets SegImage3D::m_SpeedWrapper2D
	EdgePreviewChange();
	_previewButton->value( 0);	// but default to 'false'
	EdgePreviewChange();		// just sets filters to 0
	// Set up the plot range, etc
//?	FunctionPlot2DSettings &plotSettings = 
//?		m_BoxEdgeFunctionPlot->GetPlotter().GetSettings();

#ifndef SIGMOID
	// Compute the plot to be displayed.GetPointer()
	UpdateEdgePlot();
#endif
}

void
EdgeFilter::close()
{
	// If in preview mode, disconnect and destroy the preview filters
	_EdgePreviewFilter = 0;

	// Revert to the old speed image
	// TODO: We're reverting to the last APPLY.	The user may get confused.
//	parent()->snapImage()->GetSpeed2D()->RemoveSliceSourcesForPreview();
}

//
//	UI "Edge Settings" button
//	
void
EdgeFilter::SetEdgeSettings()
{
	// Pass the current GUI settings to the filter
#ifdef SIGMOID
	settings().SetSigma( _EdgeSigmaSlider->value());
	settings().SetAlpha( _EdgeAlphaSlider->value());
	settings().SetBeta( _EdgeBetaSlider->value());
#else
	settings().SetGaussianBlurScale( _EdgeSigmaSlider->value());
	settings().SetRemappingSteepness( _EdgeKappaSlider->value());
	settings().SetRemappingExponent( _EdgeExponentSlider->value());
#endif
	// Store the settings globally
	parent()->setEdgeSettings( settings());

#ifndef SIGMOID
	// Compute the plot to be displayed
	UpdateEdgePlot();
#endif
	// Apply the settings to the filter but only if we are in preview mode
	if ( _previewButton->value())
	{
		if ( _EdgePreviewFilter)
				_EdgePreviewFilter->SetEdgePreprocessingSettings(settings());

		// Repaint the slice windows
		parent()->redraw();
	}	
}

//
//	Called by UI "Preview result" button and from initialize().
//	
void
EdgeFilter::EdgePreviewChange()
{
	bool preview = (_previewButton->value() == 1);
	
	if ( preview)
	{
		// Initialize preview filter
		// Get a handle to the snap image data
		SegImage3D *snapImg = parent()->segImage3D();

		// Make sure the preview filter is deallocated
		assert( !_EdgePreviewFilter);

		// Create the filter
		_EdgePreviewFilter = EdgeFilterType::New();

		// Give it an input
		snapImg->GetGrey()->GetSlicer(0)->Modified();
		snapImg->GetGrey()->GetSlicer(0)->Update();
#ifdef USEGPU
		// Copy GreyImage to GPUImage.  From 'itkImageDuplicator::Update()'.
		GreyImageType *input = snapImg->GetGrey()->GetSlice(0);
		GPUGreyImageType::Pointer GPUImg = GPUGreyImageType::New();

		GPUImg->CopyInformation( input );
		GPUImg->SetRequestedRegion( input->GetRequestedRegion() );
		GPUImg->SetBufferedRegion( input->GetBufferedRegion() );
		GPUImg->Allocate();
		typename GreyImageType::RegionType region = input->GetLargestPossibleRegion();
		ImageAlgorithm::Copy( input, GPUImg.GetPointer(), region, region);

		_EdgePreviewFilter->SetInput( GPUImg);
#else
		_EdgePreviewFilter->SetInput( snapImg->GetGrey()->GetSlice(0));
#endif	

		// Pass the current settings to the filter
		_EdgePreviewFilter->SetEdgePreprocessingSettings( settings());

		// Attach the preview filters to the corresponding slicer
//		snapImg->GetSpeed2D()->SetSliceSourceForPreview(
//			j, _EdgePreviewFilter->GetOutput());

		_EdgePreviewFilter->Update();

		// Set SegImage3D's m_SpeedWrapper2D
		snapImg->InitializeSpeed2D( _EdgePreviewFilter->GetOutput());

		// and init colormap
		snapImg->GetSpeed2D()->SetColorMap(
			SpeedColorMap::GetPresetColorMap( COLORMAP_BLACK_BLACK_WHITE));

		// Set the speed image to Edge mode
		snapImg->GetSpeed2D()->SetModeToEdgeSnake();

		// debug: display min/max
		float min = snapImg->GetSpeed2D()->GetImageMin();
		float max = snapImg->GetSpeed2D()->GetImageMax();
		cout << "EdgeFilter::EdgePreviewChange: min = " << min << ", max = " << max << endl;
	}
	else
	{
		// Clear the preview filter
		_EdgePreviewFilter = 0;

		// Revert to the old speed image
		// TODO: We're reverting to the last APPLY.	The user may get confused.
//		snapImg->GetSpeed2D()->RemoveSliceSourcesForPreview();
	}

	// Notify parent of the update
	parent()->preprocessingPreview( preview);	// just sets DisplayType to DT_SPEED
}

//
//	For slice change, called from Aseg::sliceChange()
//	
void
EdgeFilter::EdgePreviewUpdate()
{
	static bool change = true;	// kludge to make image change when change slice slider
	if ( showPreview())			// see log 2014-06-19 (and previous)
	{
#ifdef SIGMOID
		if ( change)
			settings().SetSigma( _EdgeSigmaSlider->value()+0.001F);
		else
			settings().SetSigma( _EdgeSigmaSlider->value());
#else
		if ( change)
			settings().SetGaussianBlurScale( _EdgeSigmaSlider->value()+0.001F);
		else
			settings().SetGaussianBlurScale( _EdgeSigmaSlider->value());
#endif
		change = !change;
		if ( _EdgePreviewFilter)
			_EdgePreviewFilter->SetEdgePreprocessingSettings( settings());
	}
}

#ifndef SIGMOID
void
EdgeFilter::UpdateEdgePlot()
{
	// Create a functor object used in the filter
	EdgeFilterType::FunctorType functor;

	// Pass the settings to the functor
	functor.SetParameters( 0.F,1.0F, settings().GetRemappingExponent(), 
		settings().GetRemappingSteepness());

	// Compute the function for a range of values
	const unsigned int nSamples = 200;
	float x[nSamples];
	float y[nSamples];

	for(unsigned int i=0;i<nSamples;i++) 
	{
		x[i] = i * 1.0F / (nSamples-1);
		y[i] = functor(x[i]);
	}

	// Pass the results to the plotter
	_FunctionPlot->setDataPoints( x, y, nSamples);

	// Redraw the box
	_FunctionPlot->redraw();
}
#endif

void
EdgeFilter::loadEdgeSettings()
{
	if ( _fileChooser == 0)
	{
		_fileChooser = new FileChooser( _directory.c_str(), "*.edg",
					FileChooser::SINGLE, "Load Edge Settings");
	}
	else
	{
		_fileChooser->type( FileChooser::SINGLE);
		_fileChooser->label("Load Edge Settings");
		_fileChooser->filter( "*.edg");
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
			getline( inFile, line);		// should be 'Edge Settings'
			inFile >> val;
			_EdgeSigmaSlider->value( val);
			inFile >> val;
#ifdef SIGMOID
			_EdgeAlphaSlider->value( val);
			inFile >> val;
			_EdgeBetaSlider->value( val);
#else
			_EdgeKappaSlider->value( val);
			inFile >> val;
			_EdgeExponentSlider->value( val);
#endif
			inFile.close();
			
			SetEdgeSettings();
		}
		else
		{
			alert( "Error opening %s", _fileChooser->value());
		}
	}
}

void
EdgeFilter::saveEdgeSettings()
{
	if ( _fileChooser == 0)
	{
		_fileChooser = new FileChooser( _directory.c_str(), "*.edg",
					FileChooser::CREATE, "Save Edge Settings");
	}
	else
	{
		_fileChooser->type( FileChooser::CREATE);
		_fileChooser->label("Save Edge Settings");
		_fileChooser->filter( "*.edg");
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
			outFile << "Edge Settings" << endl;
			outFile << _EdgeSigmaSlider->value() << endl;
#ifdef SIGMOID
			outFile << _EdgeAlphaSlider->value() << endl;
			outFile << _EdgeBetaSlider->value() << endl;
#else
			outFile << _EdgeKappaSlider->value() << endl;
			outFile << _EdgeExponentSlider->value() << endl;
#endif
			outFile.close();
		}
		else
		{
			alert( "Error opening %s", _fileChooser->value());
		}
	}
}
