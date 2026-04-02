//
//	SegImage3D.cc	--	class for class for segmentation image
//
//	?14-oct-14 This class has ItkImage3D base which contains main (native) image (GreyType) but 
//	GetNativeImage() only used once to get spacing.  Also has GreyWrapper.  
//	Possibly should consider changing.
//	
//	29-dec-11	bhb	subclass of ItkImage3D, following snap/Logic/Framework/SNAPImageData &
//					snap/Logic/Framework/GenericImageData
//	Modified:
//	06-jan-12  bhb	make m_CurrentSnakeParameters a pointer
//	25-jan-12  bhb	add m_LabelWrapper & segmentaton functions from ItkImage3D
//	02-feb-12  bhb	changed name from AsegImage to SegImage
//	04-may-12  bhb	add IsThreshLoaded()
//	05-jun-12  bhb	add insertWallPt(), deleteWallPt(), hasCurWalls(), 
//						getSegmentationVolume(), getPolgon()
//	19-jul-12  bhb	add getMyoPixels()
//	21-jul-12  bhb	Add SegImage3D::adjustWallIndex()
//	02-aug-12  bhb	need >1 WallType in a slice, add WallSet
//	08-aug-12  bhb	add m_WallColors
//	22-aug-12  bhb	add hasWall( uint wt)
//	02-dec-13  bhb	add clearSegmentation()
//	03-dec-13  bhb	add setLabelColor()
//	13-oct-14  bhb	add FillBubbles() from code in InitializeSegmentation()
//	14-oct-14  bhb	add AcweSegmentation()
//	17-nov-14  bhb	add InitLevelsetFromMesh() change FillBubbles() name to 
//					InitLevelsetFromBubbles()
//	23-mar-15  bhb	change ACWE segmentation to GeodesicActiveContour segmentation
//	10-jun-15  bhb	remove InitLevelsetFromMesh(), don't need
//	01-oct-15  bhb	add InitializeLevelSet(), GetHistogram()
//	07-oct-15  bhb	For 2D seg: add 2d wrappers, segment2D(), GetSnake2D(), LevelSetDriver2D(),
//					GetElapsedSegmentationIterations2D(), GetLevelSetImage2D(),
//					GetLevelSetFunction2D()
//	12-nov-15  bhb	remove GeodesicActiveContour()
//	15-jun-17  bhb	add InitializeSeeds()
//	12-jul-17  bhb	remove 2D segmentation operation
//	19-jul-17  bhb	add checkPCAFiles()
//	31-aug-17  bhb	rename InitializeFastMarchingFromSeeds to InitializeFastMarchingFromBubbles
//	13-sep-17  bhb  add m_ShapeWrapper, getInitialShape()
//	10-nov-20  bhb	use itkMacro.h instead of itkExceptionObject.h, got compile error
//	
// ???had to put these three here to avoid later itkxxx.h #include 'not found' errors
#include "vxl_config.h"
#include "vnl_math.h"
#include "vnl_vector_ref.h"

#include "SegImage3D.h"
#include "Mesh/MeshObject.h"
#include <ITK/VecToITKConversion.h>			// Vec3To_itkIndex
#include <CXlib/cxlib.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <assert.h>
#include <string>
#include <ITK/EdgePreprocessingImageFilter.h>		// #define USEGPU
#include <ITK/SmoothBinaryThresholdImageFilter.h>
#include "itkExtractImageFilter.h"
#include "itkContourExtractor2DImageFilter.h"
#include "itkMacro.h"
#include "itkImageFileWriter.h"
#include "itkMetaDataObject.h"
//#include "itkGeodesicActiveContourLevelSetImageFilter.h"
#include "itkImageRegionConstIteratorWithIndex.h"	// InitializeLevelSet(), GetHistogram()
#include "itkChangeInformationImageFilter.h"
#include "itkImageAlgorithm.h"				// for itkImage -> itkGPUImage copy

// for getInitialShape()
#include "itkGeodesicActiveContourShapePriorLevelSetImageFilter.h"
#include "itkPCAShapeSignedDistanceFunction.h"
#include "itkSpatialFunctionImageEvaluatorFilter.h"	// for initial shape
#include "itkBinaryThresholdImageFilter.h"

#include <CXlib/Timer.h>
#include <Plot/drawFunc.h>					// setColorV()
#include <ColorsPacked.h>
#include <Fltk/Draw.h>						// drawNum()
#include <fltk/ask.h>

using namespace itk;
using namespace std;
using namespace fltk;

#define CLOSE_THRESH	float(3.0)					// for closestPoint()
#define PT_SIZE	 		float(4.0)					// for drawing points
#define TIM(x)	x		// DoEdgePreprocessing(), InitializeSegmentation() timing

//
//	Constructor
//
SegImage3D::SegImage3D() : ItkImage3D(), m_LevelSetDriver(0), m_InOutPreprocessingDone(false),
	m_EdgePreprocessingDone(false), m_SegmentatonDone(false), m_CurWall( NOWALL), 
	m_CurWallSet(-1), m_EditWall(false)
{
	setColorV( m_WallColors[0], GREEN_P);		// Epi
	setColorV( m_WallColors[1], REDDISH_P);		// LaEndo
	setColorV( m_WallColors[2], BLUISH_P);		// RaEndo
	setColorV( m_WallColors[3], RED_P);			// LvEndo
	setColorV( m_WallColors[4], BLUE_P);		// RvEndo
}

SegImage3D::~SegImage3D() 
{
}

void
SegImage3D::SetGreyImage( GreyImageType *newGreyImage, 
							const ImageCoordinateGeometry &newGeometry,
							const GreyTypeToNativeFunctor &native)
{
	ItkImage3D::SetGreyImage( newGreyImage, newGeometry, native);

	m_LabelWrapper.InitializeToWrapper( &m_GreyWrapper, (LabelTypeUS)0);
	SetImageGeometry( &m_LabelWrapper, newGeometry);
}

//
//	Called from Aseg::EdgeFilterApply()
//	
void
SegImage3D::DoEdgePreprocessing( const EdgePreprocessingSettings &settings, 
	itk::Command *progressCB)
{
	typedef itk::GPUImage<GreyType, 3> GPUGreyImageType;
	typedef itk::GPUImage<float, 3> GPUFloatImageType;

	// Define an edge filter to use for preprocessing
#ifdef USEGPU
	typedef EdgePreprocessingImageFilter<GPUGreyImageType, GPUFloatImageType> FilterType;
#else
	typedef EdgePreprocessingImageFilter<GreyImageType, SpeedImageWrapper3D::ImageType>
			 FilterType;
#endif

	// Copy GreyImage to GPUImage.  From 'itkImageDuplicator::Update()'.
	GreyImageType *input = GreyImageType::New();
	input = GetGrey()->GetImage();
#ifdef USEGPU
	GPUGreyImageType::Pointer GPUImg = GPUGreyImageType::New();

	GPUImg->CopyInformation( input );
	GPUImg->SetRequestedRegion( input->GetRequestedRegion() );
	GPUImg->SetBufferedRegion( input->GetBufferedRegion() );
	GPUImg->Allocate();
	typename GreyImageType::RegionType region = input->GetLargestPossibleRegion();
	ImageAlgorithm::Copy( input, GPUImg.GetPointer(), region, region);
#endif

	// Configure the edge filter
	FilterType::Pointer filter = FilterType::New();

	// Pass the settings to the filter
	filter->SetEdgePreprocessingSettings( settings);

	// Set the filter's input
#ifdef USEGPU
	filter->SetInput( GPUImg);
#else
	filter->SetInput( input);
#endif
	// Provide a progress callback (if one is provided)
	if ( progressCB)
		filter->AddObserver( ProgressEvent(),progressCB);

	// Run the filter on the whole image
TIM(Timer tim;)
	filter->UpdateLargestPossibleRegion();
TIM(int mins = tim.sec() / 60;)
TIM(int sec = tim.sec() - (mins * 60);)
TIM(cout << "DoEdgePreprocessing: " << mins << ":" << sec << " min" << endl;)

	// Pass the output of the filter to the speed wrapper
	m_SpeedWrapper.SetImage( filter->GetOutput());
	m_SpeedWrapper.SetColorMap( SpeedColorMap::GetPresetColorMap( COLORMAP_BLACK_BLACK_WHITE));

	// debug: display min/max
	float min = m_SpeedWrapper.GetImageMin();
	float max = m_SpeedWrapper.GetImageMax();
	cout << "SegImage3D::DoEdgePreprocessing: min = " << min << ", max = " << max << endl;

	// Dismantle this pipeline
	m_SpeedWrapper.GetImage()->DisconnectPipeline();

	m_InOutPreprocessingDone = false;
	m_EdgePreprocessingDone = true;
}

//
//	Called from Aseg::ThresholdFilterApply()
//	
void
SegImage3D::DoInOutPreprocessing( const ThresholdSettings &settings, itk::Command *progressCB)
{
	// Define a threshold filter to use for preprocessing
	typedef SmoothBinaryThresholdImageFilter<GreyImageType, SpeedImageWrapper3D::ImageType>
												FilterType;

	// Create a threshold filter for whole-image preprocessing
	FilterType::Pointer filter = FilterType::New();

	// Pass the settings to the filter
	filter->SetThresholdSettings( settings);

	// Set the filter's input
	filter->SetInput( GetGrey()->GetImage());

	// Provide a progress callback (if one is provided)
	if ( progressCB)
		filter->AddObserver( ProgressEvent(), progressCB);

	// Run the filter
	filter->UpdateLargestPossibleRegion();

	// Pass the output of the filter to the speed wrapper
	m_SpeedWrapper.SetImage( filter->GetOutput());
	m_SpeedWrapper.SetColorMap( SpeedColorMap::GetPresetColorMap( COLORMAP_BLUE_BLACK_WHITE));

	// Dismantle this pipeline
	m_SpeedWrapper.GetImage()->DisconnectPipeline();

	m_EdgePreprocessingDone = false;
	m_InOutPreprocessingDone = true;
}

bool
SegImage3D::SetSegmentationImage( LabelImageType *newLabelImage) 
{
	typedef itk::ImageRegion<3>	RegionType;

	if ( newLabelImage == 0)
		return false;

	// Check that the image matches the size of the grey image
	assert( m_GreyWrapper.IsInitialized());	// &&
//		m_GreyWrapper.GetBufferedRegion() == newLabelImage->GetBufferedRegion());

	RegionType greyRegion = m_GreyWrapper.GetBufferedRegion();
	RegionType labelRegion = newLabelImage->GetBufferedRegion();

	if ( greyRegion != labelRegion)
	{
		cerr << "SegImage3D::SetSegmentationImage: greyRegion != labelRegion" << endl;
		cerr << "Make sure same images loaded as when made gipl" << endl;
		cerr << "gipl size: " << labelRegion.GetSize() << endl;
		cerr << "Grey size: " << greyRegion.GetSize() << endl;
		return false;
	}
	
	LabelImageType::SizeType size = labelRegion.GetSize();
	cout << "SegImage3D::SetSegmentationImage: Seg image size: " << size << endl;

	// Pass the image to the segmentation wrapper
	m_LabelWrapper.SetImage( newLabelImage);

//	cout << "min: " << m_LabelWrapper.GetImageMin() << ", max: " 
//		<< m_LabelWrapper.GetImageMax() << endl;

	// Sync up params between the main and label image
	newLabelImage->SetSpacing( m_GreyWrapper.GetImageBase()->GetSpacing());
	newLabelImage->SetOrigin( m_GreyWrapper.GetImageBase()->GetOrigin());
	newLabelImage->SetDirection( m_GreyWrapper.GetImageBase()->GetDirection());
	return true;
}

//
//	Not used currently.
//	
void 
SegImage3D::SetSegmentationVoxel(const Vec3<uint> &index, LabelTypeUS value)
{
	// Make sure that the main image data and the segmentation data exist
	assert(m_GreyWrapper.IsInitialized() && m_LabelWrapper.IsInitialized());

	// Store the voxel
	m_LabelWrapper.GetVoxelForUpdate(index) = value;

	// Mark the image as modified
	m_LabelWrapper.GetImage()->Modified();
}

//
//	Called from Aseg::resetSegmentation().
//	
bool
SegImage3D::InitializeSegmentation( const SegParameters *parameters,
				const vector<Sphere<uint> > &bubbles, 
				LabelTypeUS labelColor, itk::Command *progressCB)
{
TIM(Timer tim;)
	if ( !m_SpeedWrapper.IsInitialized())
		return false;

	// set parameters
	m_SegParameters = parameters;
	
	// Store the label color
	m_SnakeColorLabel = labelColor;

	unsigned long nInitVoxels = 0;

	// If doing GeoActContShape, check PCA files, center SpeedWrapper
	if ( m_SegParameters->GetSolver() == SegParameters::GEO_ACT_CONT_SHAPE)
	{
		if ( !checkPCAFiles())
			return false;

		// Following ITK GeodesicActiveContourShapePriorLevelSetImageFilter.cxx example
		// we need to center the input image - move origin to center.
		typedef itk::ChangeInformationImageFilter< FloatImageType>  CenterFilterType;
		typename CenterFilterType::Pointer center = CenterFilterType::New();
		center->CenterImageOn();
		center->SetInput( m_SpeedWrapper.GetImage());		// edge (sigmoid) image
		center->Update();

		// replace speedWrapper image
		m_SpeedWrapper.SetImage( center->GetOutput());
	}
	
	InitializeLevelSet( nInitVoxels);		// copy region from labelImage (registered mesh or
											// loaded segmentaton
TIM(cout << "InitializeLevelSet: " << tim.msec() << " msec" << endl; tim.init();)

	if ( m_SegParameters->GetSolver() == SegParameters::GEO_ACT_CONT_SHAPE)
	{
		// Replace m_SnakeInitializationWrapper image w/ fast marching w/ seeds.
		if ( InitializeFastMarchingFromBubbles( bubbles))
			nInitVoxels++;
TIM(	cout << "init fast marching: " << tim.sec() << " sec" << endl; tim.init();)
	}
	else
	{
		InitLevelsetFromBubbles( bubbles, nInitVoxels);
TIM(	cout << "fill bubbles: " << tim.msec() << endl; tim.init();)
	}

	// At this point, we should have an initialization image. 
	// End the routine if there are no initialization voxels.
	if (nInitVoxels == 0)
	{
		alert( "Error initializing level set.");
		m_SnakeInitializationWrapper.Reset();
		return false;
	}

	// Initialize the snake driver
	InitalizeSnakeDriver( parameters, progressCB);
TIM(cout << "InitalizeSnakeDriver: " << tim.sec() << " sec" << endl; tim.init();)

	// get initial shape for translation
	if ( !getInitialShape())
		return false;
TIM(cout << "getInitialShape: " << tim.sec() << " sec" << endl;)

	// Success
	return true;
}	// InitializeSegmentation()

//
//	Check existance of PCA (Principle Component Analysis) files
//	
bool
SegImage3D::checkPCAFiles()
{
	const char *filename = m_SegParameters->GetShapeMeanInput();
	if ( !fileExists( filename))
	{
		alert( "Missing %s PCA file.", filename);
		message( "Add file to current directory and check name in 'Segmentation Parameters'.");
		return false;
	}

	uint nModes = m_SegParameters->GetNumPCAModes();
	const char *modeformat = m_SegParameters->GetShapeModeFormat();
	for ( uint i=0; i<nModes; i++)
	{
		char modeName[256];
		sprintf( modeName, modeformat, i);
		if ( !fileExists( modeName))
		{
			alert( "Missing %s PCA file", modeName);
			message( "Add file to current directory and check name in 'Segmentation Parameters'.");
			return false;
		}
	}
	return true;
}

//
//	Called from InitializeSegmentation().
//	Get initial shape from SegLevelSetDriver.  Must be ready to do segmentation.
//	
bool
SegImage3D::getInitialShape()
{
	RunSegmentation( 1);		// do one iteration to get initial shape

	// Get initial shape.
	// Type definition for the shape function
	typedef itk::PCAShapeSignedDistanceFunction< double, 3, FloatImageType>
															ShapeFunctionType;
	typedef itk::SpatialFunctionImageEvaluatorFilter< ShapeFunctionType, 
					FloatImageType, FloatImageType>			EvaluatorFilterType;
	EvaluatorFilterType::Pointer evaluator = EvaluatorFilterType::New();
	evaluator->SetInput( GetLevelSetImage());
	evaluator->SetFunction( LevelSetDriver()->GetShape());

	typedef itk::GeodesicActiveContourShapePriorLevelSetImageFilter<
		FloatImageType, FloatImageType>	GeodesicActiveContourPriorFilterType;

	GeodesicActiveContourPriorFilterType * geodesicActiveContour = 
		(GeodesicActiveContourPriorFilterType *)LevelSetDriver()->GetLevelSetFilter();

	LevelSetDriver()->GetShape()->SetParameters(geodesicActiveContour->GetInitialParameters());
	evaluator->Update();

//	GetMinMax<FloatImageType>( evaluator->GetOutput(), fmin, fmax, "Init Seg min/max");
	typedef itk::BinaryThresholdImageFilter< FloatImageType, LabelImageType>
															ThresholdFilterType;
	ThresholdFilterType::Pointer thresholdFilter = ThresholdFilterType::New();

	thresholdFilter->SetInput( evaluator->GetOutput());
	thresholdFilter->SetLowerThreshold( -1000.F);
	thresholdFilter->SetUpperThreshold( 0.F);
	thresholdFilter->SetInsideValue( 255);		// between Lower & Upper
	thresholdFilter->SetOutsideValue( 0);		// otherwise
	thresholdFilter->Update();
	m_ShapeWrapper.InitializeToWrapper( &m_GreyWrapper, thresholdFilter->GetOutput());
	m_ShapeWrapper.SetLabelColorTable( m_LabelWrapper.GetLabelColorTable());
//	if ( _initSegImg)
//		imgView()->setInitSegImage( _initSegImg);
//	else
//		cout << "Error getting init seg min max" << endl;
	return true;
}

//
//	Called from SegImage3D::InitializeSegmentation() above.
//	
void
SegImage3D::InitializeLevelSet( unsigned long &nInitVoxels)
{
	// Inside/outside values
	const float INSIDE_VALUE = -1.0F, OUTSIDE_VALUE = 1.0F;	// snap v3 (SNAPImageData.cxx) 

	// Initialize the level set initialization wrapper, set pixels to OUTSIDE_VALUE
	// Just uses m_GreyWrapper for size, origin, spacing, & transforms.
	m_SnakeInitializationWrapper.InitializeToWrapper( &m_GreyWrapper, OUTSIDE_VALUE);

	// Create the initial level set image by merging the segmentation data from
	// IRIS region.
	LabelImageType::Pointer imgInput = m_LabelWrapper.GetImage();
	FloatImageType::Pointer imgLevelSet = m_SnakeInitializationWrapper.GetImage();

	// Get the target region. This really should be a region relative to the IRIS image
	// data, not an image into a needless copy of an IRIS region.
	LabelImageType::RegionType region = imgInput->GetBufferedRegion();

	// Create iterators to perform the copy
	typedef itk::ImageRegionConstIterator<LabelImageType> SourceIterator;
	typedef itk::ImageRegionIteratorWithIndex<FloatImageType> TargetIterator;
	SourceIterator itSource( imgInput, region);
	TargetIterator itTarget( imgLevelSet, region);

	// Convert the input label image into a binary function whose 0 level set
	// is the boundary of the current label's region
	while( !itSource.IsAtEnd())
	{
//		if ( itSource.Value() == m_SnakeColorLabel)
		if ( itSource.Value() > 0)
		{	
			nInitVoxels++;					// Increase the number of initialization voxels
			itTarget.Value() = INSIDE_VALUE;	// Set the target value to inside
		}
		++itTarget; ++itSource;					// Go to the next pixel
	}

	// Make sure that the correct color label is being used
	m_SnakeInitializationWrapper.SetColorLabel( m_ColorLabel);
}

void
SegImage3D::InitLevelsetFromBubbles( const vector<Sphere<uint> > &bubbles,
	unsigned long &nInitVoxels)
{
	const float INSIDE_VALUE = -1.0F;

	FloatImageType::Pointer imgLevelSet = m_SnakeInitializationWrapper.GetImage();
	LabelImageType::RegionType region = imgLevelSet->GetBufferedRegion();
	
	// Create iterator to perform the copy
	typedef itk::ImageRegionIteratorWithIndex<FloatImageType> TargetIterator;

	// Fill in the bubbles by computing their extents
	for ( unsigned int iBubble=0; iBubble < bubbles.size(); iBubble++)
	{
		// Compute the extents of the bubble
		typedef itk::Point<double,3> PointType;
		PointType ptCenter;

		// Compute the physical position of the bubble center
		imgLevelSet->TransformIndexToPhysicalPoint(
			Vec3To_itkIndex(bubbles[iBubble].center), ptCenter);

		// Extents of the bounding box
		FloatImageType::IndexType idxLower = Vec3To_itkIndex(bubbles[iBubble].center);
		FloatImageType::IndexType idxUpper = Vec3To_itkIndex(bubbles[iBubble].center);

		// Map all vertices in a cube of radius r around the physical center of
		// the bubble into index space, and compute a bounding box
		for(int jx=-1; jx<=1; jx+=2)
			for(int jy=-1; jy<=1; jy+=2)
				for(int jz=-1; jz<=1; jz+=2)
				{
					PointType ptTest;
					ptTest[0] = ptCenter[0] + jx * bubbles[iBubble].radius;
					ptTest[1] = ptCenter[1] + jy * bubbles[iBubble].radius;
					ptTest[2] = ptCenter[2] + jz * bubbles[iBubble].radius;

					FloatImageType::IndexType idxTest;
					imgLevelSet->TransformPhysicalPointToIndex(ptTest,idxTest);

					for(unsigned int k=0; k<3; k++)
					{
						if (idxLower[k] > idxTest[k])
							idxLower[k] = idxTest[k];
						if (idxUpper[k] < idxTest[k])
							idxUpper[k] = idxTest[k];
					}
				}

		// Create a region
		FloatImageType::SizeType szBubble;
		szBubble[0] = 1 + idxUpper[0] - idxLower[0];
		szBubble[1] = 1 + idxUpper[1] - idxLower[1];
		szBubble[2] = 1 + idxUpper[2] - idxLower[2];
		FloatImageType::RegionType regBubble(idxLower,szBubble);
		regBubble.Crop(region);

		// Create an iterator with an index to fill out the bubble
		TargetIterator itThisBubble( imgLevelSet, regBubble);

		// Need the squared radius for this
		// 14-feb-12	bhb, seems like could save Index->Phys convert if convert radius
		// to physical coord units
		float r2 = bubbles[iBubble].radius * bubbles[iBubble].radius;

		// Fill in the bubble
		while(!itThisBubble.IsAtEnd())
		{
			PointType pt;
			imgLevelSet->TransformIndexToPhysicalPoint( itThisBubble.GetIndex(), pt);

			if ( pt.SquaredEuclideanDistanceTo(ptCenter) <= r2)
			{
				itThisBubble.Value() = INSIDE_VALUE;
				nInitVoxels++;
			}

			++itThisBubble;
		}
	}
}	// InitLevelsetFromBubbles()

//
//	Replaces m_SnakeInitializationWrapper image w/ fast marching w/ seeds.
//	
bool
SegImage3D::InitializeFastMarchingFromBubbles( const vector<Sphere<uint> > &bubbles)
{
	FloatImageType::Pointer imgLevelSet = m_SnakeInitializationWrapper.GetImage();
	FloatImageType::RegionType region = imgLevelSet->GetBufferedRegion();

	//	Make sure origin same as m_SpeedWrapper
	imgLevelSet->SetOrigin( m_SpeedWrapper.GetImage()->GetOrigin());
	
	//	Get seeds in NodeContainer
	typedef FastMarchingFilterType::NodeType			NodeType;

	//  The list of nodes is initialized and then every node is inserted using
	//  InsertElement().
	NodeContainer::Pointer seeds = NodeContainer::New();
	seeds->Initialize();

	// need to invert seed y val
	FloatImageType::SizeType size = region.GetSize();

	for ( uint i=0; i<bubbles.size(); i++)
	{
		NodeType node;
		node.SetValue( -m_SegParameters->GetSeedRadius());		// neg radius why?
//		node.SetValue( m_SegParameters->GetSeedRadius());
		IndexType seedInv = Vec3To_itkIndex(bubbles[i].center);
		seedInv[1] = size[1] - seedInv[1];	// invert y
		node.SetIndex( seedInv);
		seeds->InsertElement( i, node);
	}
	
	FastMarchingFilterType::Pointer fastMarching = FastMarchingFilterType::New();
	fastMarching->SetTrialPoints( seeds);

	//  Since the FastMarchingImageFilter is used here just as a Distance Map generator.
	//  It does not require a speed image as input.
	//  Instead the constant value 1.0 is passed using the SetSpeedConstant() method.
	//
	fastMarching->SetSpeedConstant( 1.0);
	fastMarching->SetOutputRegion( region);
	fastMarching->SetOutputSpacing( imgLevelSet->GetSpacing());
	fastMarching->SetOutputOrigin( imgLevelSet->GetOrigin());

	try
	{
		fastMarching->Update();
	}
	catch( itk::ExceptionObject &excep)
	{
		const char *desc = excep.GetDescription();
		alert( "Exception caught updating FastMarchingFilter: %s", desc);
		return false;
	}

	// Create iterator to perform the copy
	typedef itk::ImageRegionConstIterator< FloatImageType> ConstIteratorType;
	typedef itk::ImageRegionIterator< FloatImageType> IteratorType;

	// Init iterators to copy fast marching to imgLevelSet
	ConstIteratorType sourceIt( fastMarching->GetOutput(), region);
	IteratorType targetIt( imgLevelSet, region);

	while( !sourceIt.IsAtEnd())
	{
		targetIt.Set( sourceIt.Get());
		++sourceIt;
		++targetIt;
	}
	return true;
}

void
SegImage3D::InitalizeSnakeDriver( const SegParameters *p, itk::Command *progressCB)
{
	// Create a new level set driver, deleting the current one if it's there
	if (m_LevelSetDriver)
		delete m_LevelSetDriver;

	// This is a good place to check that the parameters are valid
	if ( p->GetSpeedType() == SegParameters::REGION_SNAKE)
	{
		// There is no advection
//		assert(p->GetAdvectionWeight() == 0);
		if ( p->GetAdvectionWeight() != 0.F)
			alert( "For threshold (in/out) segmentation, advection supposed to equal 0");

		// There is no curvature speed
		assert(p->GetCurvatureSpeedExponent() == -1);

		// Propagation is modulated by probability
		assert(p->GetPropagationSpeedExponent() == 1);

		// There is no smoothing speed
		assert(p->GetLaplacianSpeedExponent() == 0);
	}

	// Initialize the snake driver and pass the parameters
	m_LevelSetDriver = new SegLevelSetDriver<3>(
		m_SnakeInitializationWrapper.GetImage(), m_SpeedWrapper.GetImage(),
		m_SegParameters, m_ExternalAdvectionField, progressCB);

	// Initialize the level set wrapper with the image from the level set
	// driver and other settings from the other wrappers
	m_SnakeWrapper.InitializeToWrapper( &m_GreyWrapper, LevelSetDriver()->GetCurrentState());
	m_SnakeWrapper.GetImage()->SetOrigin( m_GreyWrapper.GetImage()->GetOrigin());
	m_SnakeWrapper.GetImage()->SetSpacing( m_GreyWrapper.GetImage()->GetSpacing());

	// Make sure that the correct color label is being used
	m_SnakeWrapper.SetColorLabel( m_ColorLabel);
}	// InitalizeSnakeDriver()

//
//	Called from Aseg: segmentation thread function.
//	
void
SegImage3D::RunSegmentation( unsigned int nIterations)
{
	// Should be in level set mode
	assert( m_LevelSetDriver);

	// Pass through to the level set driver
	LevelSetDriver()->Run( nIterations);
}

void
SegImage3D::RestartSegmentation()
{
	// Should be in level set mode
	assert( m_LevelSetDriver);

	// Pass through to the level set driver
	LevelSetDriver()->Restart();

	// Update the image pointed to by the snake wrapper
	m_SnakeWrapper.SetImage( LevelSetDriver()->GetCurrentState());
}

void
SegImage3D::TerminateSegmentation()
{
	// Should be in level set mode
	assert( m_LevelSetDriver);

	// Delete the level set driver and all the problems that go along with it
	delete m_LevelSetDriver; m_LevelSetDriver = NULL;
	m_SegmentatonDone = true;

}

//*****************************************************************************
//
//	Wall functions
//	
//*****************************************************************************
void
SegImage3D::addWall( uint wt)
{
	uint curSlice = getSlice();
	
	curWall( wt);			// set m_CurWall

	// initialize m_Walls if needed
	if ( m_Walls.size() == 0)
	{
		vector<WallSet > walls;
		Vec3<uint> size = GetGrey()->GetSize();
		uint nSlices = size[2];
		m_Walls.assign( nSlices, walls);
	}

	// add WallSet if needed, set m_CurWallSet
	m_CurWallSet = m_Walls[curSlice].size() - 1;
	if ( m_CurWallSet < 0 || m_Walls[curSlice][m_CurWallSet].wall[wt].size() > 0)
	{
		WallSet ws;
		m_Walls[curSlice].push_back( ws);
		m_CurWallSet++;
	}
	m_EditWall = true;
}

void
SegImage3D::addWallPt( Pt3<float> &pt)
{
	uint curSlice = getSlice();

	if ( wallSelected() && m_CurWallSet >= 0 && m_CurWallSet < m_Walls[curSlice].size())
	{
		m_Walls[curSlice][m_CurWallSet].wall[curWall()].addPt( pt);
	}
}

void
SegImage3D::insertWallPt( Pt3<float> &pt)
{
	uint curSlice = getSlice();

	if ( wallSelected() && m_CurWallSet >= 0)
	{
		m_Walls[curSlice][m_CurWallSet].wall[curWall()].insertPt( pt);
	}
}

void
SegImage3D::deleteWallPt( Pt3<float> &pt)
{
	uint curSlice = getSlice();

	if ( wallSelected() && m_CurWallSet >= 0)
	{
		m_Walls[curSlice][m_CurWallSet].wall[curWall()].deletePt( pt);
	}
}

void
SegImage3D::closeWall()
{
	SplineCurve< Pt3<float>, float > *wall = &m_Walls[getSlice()][m_CurWallSet].wall[curWall()];
	if ( wall->size())
	{
		wall->setClosed( true);
		if ( wall->clockwise() < 0)
		{
			wall->reverse();
			alert( "Wall reversed to be clockwise, all walls need to have same orientation.");
		}
	}
	m_EditWall = false;
}

//
//	Search currently selected wall for closest point
//	
Pt3<float> *
SegImage3D::closestPoint( Pt3<float> &pt)
{
	uint curSlice = getSlice();

	if ( m_Walls.size() <= curSlice || m_Walls[curSlice].size() == 0)
		return 0;

	Pt3<float> *tmpPt = 0;
	Pt3<float> *retPt = 0;
	float thresh = m_EditWall ? CLOSE_THRESH * float(3) : CLOSE_THRESH;
	WallType wallType = NOWALL;
	int wallset = -1;
	float	dst;
		
	if (	m_Walls[curSlice][m_CurWallSet].wall[m_CurWall].size() && 
			(tmpPt = m_Walls[curSlice][m_CurWallSet].wall[m_CurWall].closestPt(pt)))
	{
		if ( (dst = dist_sq( pt, *tmpPt)) < thresh)
		{
			retPt = tmpPt;
			thresh = dst;
		}
	}
	return retPt;
}

//
//	If pt within threshold of a wall, set m_CurWall & return index of closest edge,
//	else return -1
//	
int
SegImage3D::closestEdge( Pt3<float> &pt)
{
	uint curSlice = getSlice();
	float thresh = m_EditWall ? CLOSE_THRESH * float(3) : CLOSE_THRESH;
	WallType wallType = NOWALL;
	int wallset = -1;
	float	dst, minDst = HUGE;
	uint edge;
	
	if ( m_Walls[curSlice].size() == 0)
		return 0;
		
	for ( uint ws = 0; ws < m_Walls[curSlice].size(); ws++)
	{
		for ( uint wt = 0; wt < 5; wt++)
		{
			if ( m_Walls[curSlice][ws].wall[wt].size() && 
					(edge = m_Walls[curSlice][ws].wall[wt].closestEdge(pt, dst) > -1))
			{
				if ( dst < thresh && dst < minDst)
				{
					minDst = dst;
					wallset = ws;
					wallType = WallType(wt);
				}
			}
		}
	}
	if ( wallType != NOWALL)
	{
		m_CurWallSet = wallset;
		m_CurWall = wallType;
		return edge;
	}
	return -1;	
}

void
SegImage3D::deleteWalls()
{
	m_Walls.clear();
}

void
SegImage3D::drawWalls()
{
	if ( m_Walls.size() == 0)
		return;
	uint curSlice = getSlice();

	glLineWidth(2.0);
	for ( uint ws = 0; ws < m_Walls[curSlice].size(); ws++)
	{
		for ( uint wt = 0; wt < 5; wt++)
		{
			if ( m_Walls[curSlice][ws].wall[wt].size())
			{
				glColor4fv( m_WallColors[wt]);
				m_Walls[curSlice][ws].wall[wt].draw2d();

				if ( m_EditWall && ws == m_CurWallSet && wt == m_CurWall)
				{
					m_Walls[curSlice][ws].wall[wt].drawPts2d(PT_SIZE);
					for ( uint j=0; j < m_Walls[curSlice][ws].wall[wt].size(); j+=5)
						drawNum( m_Walls[curSlice][ws].wall[wt][j], j);
				}
			}
		}
	}
	glLineWidth(1.0);
}

//
//	Return true if have walls for any slice
//	
bool
SegImage3D::hasWalls()
{
	uint n = 0;
	Vec3<uint> size = GetGrey()->GetSize();
	uint nSlices = size[2];
	
	if ( m_Walls.size() == 0)
		return 0;

	for ( uint s=0; s<nSlices; s++)		// for each slice
			n += m_Walls[s].size();
	return n > 0;
}

//
//	Return true if has at least one wall in any slice of type wt
//	
bool
SegImage3D::hasWall( uint wt)
{
	uint nSlices = m_Walls.size();

	for ( uint s = 0; s < nSlices; s++)
		if ( m_Walls[s].size() && m_Walls[s][0].wall[wt].size())
			return true;
	return false;
}

//
//	Return true if have any walls for this slice
//	
bool
SegImage3D::hasCurWalls()
{
	uint curSlice = getSlice();
	
	if ( m_Walls.size() == 0 || m_Walls[curSlice].size() == 0)
		return 0;

	uint n = 0;
	for ( uint i=0; i<5; i++)
		if ( m_Walls[curSlice][0].wall[i].size())		// just check first WallSet
			n++;
	return n > 0;
}

bool
SegImage3D::hasPrevWalls()
{
	uint curSlice = getSlice();
	if ( curSlice > 0)
		curSlice--;
	else
		return false;

	if ( m_Walls.size() <= curSlice || m_Walls[curSlice].size() == 0)
		return false;
	
	uint n = 0;
	for ( uint i=0; i<5; i++)
		if ( m_Walls[curSlice][0].wall[i].size())
			n++;
	return n > 0;
}

bool
SegImage3D::hasNextWalls()
{
	uint curSlice = getSlice() + 1;		// next wall slice
	unsigned int axis = 2;		// GetGrey()->GetSlicer(0)->GetSliceDirectionImageAxis();
	Vec3<uint> size = GetGrey()->GetSize();
	uint nSlices = size[axis];
	if ( curSlice >= nSlices)
		return false;
		
	if ( m_Walls.size() == 0 || m_Walls[curSlice].size() == 0)
		return false;
	
	uint n = 0;
	for ( uint i=0; i<5; i++)
		if ( m_Walls[curSlice][0].wall[i].size())
			n++;
	return n > 0;
}

//
//	Walls stored by slice, then WallType for each slice.
//	
void
SegImage3D::loadWalls( ifstream &ifstrm, Pt3<float> &indxPt)
{
	WallType type;
	unsigned int axis = 2;		// GetGrey()->GetSlicer(0)->GetSliceDirectionImageAxis();
	Vec3<uint> size = GetGrey()->GetSize();
	uint nSlices = size[axis];
	SplineCurve<Pt3<float>, float> wall( SplineCurve<Pt3<float>, float>::CATMULL_ROM);
	bool done = false;
	vector<WallSet> walls;
	m_Walls.clear();
	m_Walls.assign( nSlices, walls);
	
	do
	{
		// read slice num
		bool inSlice;
		uint slice;
		string str;
		string::size_type loc;
		if ( ifstrm >> str)
			loc = str.find( ":", 0);
		else
			loc = string::npos;
		if ( loc != string::npos)
		{	
			str.erase( loc, 1);
			slice = atoi( str.c_str());
			inSlice = true;
			bool inWallSet;

			// get WallSets for this Slice
			do
			{
				streampos mark = ifstrm.tellg();
				if ( ifstrm >> str)
					loc = str.find( "WallSet:", 0);
				else
					loc = string::npos;
				if ( loc == string::npos)
				{
					ifstrm.seekg( mark);
					inSlice = false;		// next slice
				}
				else
				{
					WallSet ws;
					m_Walls[slice].push_back( ws);
					inWallSet = true;

					// get Walls for this WallSet
					do
					{
						type = whichWall( ifstrm);
						if ( type != NOWALL)
						{
							uint n = type;
							ifstrm >> m_Walls[slice].back().wall[n];
						}
						else
							inWallSet = false;
					} while ( inWallSet);
				}
			} while ( inSlice);
		}
		else
			done = true;
	} while ( !done);

	adjustWallIndex( indxPt);			// indxPt should have negative values, so subtracts
}

//
//	Walls stored by slice, then WallType for each slice.
//	
void
SegImage3D::saveWalls( ofstream &ofstrm, Pt3<float> &indxPt)
{
	string wallNames[] = { "Epi", "LaEndo", "RaEndo", "LvEndo", "RvEndo" };

	adjustWallIndex( indxPt);

	Vec3<uint> size = GetGrey()->GetSize();
	uint nSlices = size[2];
	
	for ( uint slice=0; slice < nSlices; slice++)		// for each slice
	{
		uint nWallSets = m_Walls[slice].size();
		if ( nWallSets > 0)
		{	
			for ( uint ws = 0; ws < nWallSets; ws++)	// for each WallSet
			{
				if (	m_Walls[slice][ws].hasWall())
				{
					if ( ws == 0)
						ofstrm << slice << ":" << endl;
					
					ofstrm << "WallSet:" << endl;
					for ( uint wt = 0; wt < 5; wt++)		// for each wall in this WallSet
					{
						if (	m_Walls[slice][ws].wall[wt].size())
						{
							ofstrm << wallNames[wt] << endl;
							ofstrm << m_Walls[slice][ws].wall[wt];
						}
					}
				}
			}
		}
	}
	indxPt = -indxPt;
	adjustWallIndex( indxPt);			// adjust back
}

//
//	For ROI change, adds index point, so if have ROI, should be negative.	If already adjusted
//	for ROI and need no ROI, should be positive.
//	
void
SegImage3D::adjustWallIndex( Pt3<float> &indxPt)
{
	if ( indxPt[0] == 0)
		return;
	if ( !hasWalls())
		return;
	Vec3<uint> size = GetGrey()->GetSize();
	uint nSlices = size[2];
	
	for ( uint s=0; s<nSlices; s++)		// for each slice
	{
		uint nWallSets = m_Walls[s].size();
		for ( uint ws=0; ws<nWallSets; ws++)
		{
			for ( uint wt = 0; wt < 5; wt++)
			{
				if ( m_Walls[s][ws].wall[wt].size())
				{
					for ( vector<Pt3<float> >::iterator pi = m_Walls[s][ws].wall[wt].begin();
							pi != m_Walls[s][ws].wall[wt].end(); pi++)
						*pi += indxPt;
					m_Walls[s][ws].wall[wt].setChanged();
				}
			}
		}
	}
}

void
SegImage3D::copyWalls( uint n)
{
	uint curSlice = getSlice();
	uint otherSlice;
	
	if ( n == 0)	// copy prev walls
	{
		if ( !hasPrevWalls())
		{
			cerr << "SegImage3D::copyWalls: error no prev walls to copy" << endl;
			return;
		}
		otherSlice = curSlice - 1;
	}
	else			// copy next walls
	{
		if ( !hasNextWalls())
		{
			cerr << "SegImage3D::copyWalls: error no next walls to copy" << endl;
			return;
		}
		otherSlice = curSlice + 1;
	}

	uint nWallSets = m_Walls[otherSlice].size();

	// add WallSets to curSlice if needed
	WallSet ws;
	while ( m_Walls[curSlice].size() < nWallSets)
		m_Walls[curSlice].push_back( ws);
		
	for ( uint ws = 0; ws < nWallSets; ws++)
	{		
		for ( uint wt=0; wt<5; wt++)
			if ( m_Walls[otherSlice][ws].wall[wt].size())
				m_Walls[curSlice][ws].wall[wt] = m_Walls[otherSlice][ws].wall[wt];
	}
}

void
SegImage3D::setCurWallChanged()
{
	if ( m_CurWall == NOWALL)
		return;
	m_Walls[getSlice()][m_CurWallSet].wall[m_CurWall].setChanged();
}

//
//	protected Wall functions
//	
SegImage3D::WallType
SegImage3D::whichWall( ifstream &ifstrm)
{
	string word;
	string wallNames[] = { "Epi", "LaEndo", "RaEndo", "LvEndo", "RvEndo" };
	streampos mark = ifstrm.tellg();
	if ( ifstrm >> word)
	{
		for ( uint i=0; i<5; i++)
			if ( word == wallNames[i])
				return WallType(i);
		ifstrm.seekg( mark);
	}
	return NOWALL;
}

//
//	Public non-wall functions
//	

uint
SegImage3D::getSlice()
{
	unsigned int axis = 2;	// GetGrey()->GetSlicer(0)->GetSliceDirectionImageAxis();
	Vec3<uint> cursor = GetGrey()->GetSliceIndex();
	return cursor[axis];
}

void
SegImage3D::setSlice( uint slice)
{
	Vec3<uint> cursor = GetGrey()->GetSliceIndex();
	cursor[2] = slice;
	GetGrey()->SetSliceIndex( cursor);
}

//
//	For fibrosis analysis
//	
void
SegImage3D::SetThreshImage( const ImageCoordinateGeometry &geometry)
{
	m_ThreshLabelWrapper.InitializeToWrapper( &m_GreyWrapper, (LabelTypeUS)0);
	SetImageGeometry( &m_ThreshLabelWrapper, geometry);
	m_ThreshLabelWrapper.SetLabelColorTable( m_LabelWrapper.GetLabelColorTable());
}

bool
SegImage3D::getSegmentationVolume( char *errMsg)
{
	unsigned short nSlices;
	LabelImageWrapper3D::ImagePointer segimg;
	vector<Polygon<Pt2<float>, float> > polys;
	const itk::Vector<double, 3u> spacing = GetNativeImage()->GetSpacing();
	float spacingFactor = float(spacing[0] * spacing[1] * spacing[2]);

	if ( SegmentatonDone())
	{
		// extract polys from segmentation
		segimg = GetSegmentation()->GetImage();

		typedef itk::ExtractImageFilter< LabelImageType, LabelImage2dType > FilterType;
		FilterType::Pointer extractFilter = FilterType::New();
		LabelImageType::RegionType inputRegion = segimg->GetLargestPossibleRegion();

		LabelImageType::SizeType size = inputRegion.GetSize();
		cout << "SegImage3D::getSegmentationVolume Seg image size: " << size[0] << ", " << size[1] 
				<< ", " << size[2] << endl;

		nSlices = size[2];
		size[2] = 0;
		LabelImageType::IndexType start = inputRegion.GetIndex();

		extractFilter->SetInput( segimg);

		LabelImageType::RegionType desiredRegion;
		desiredRegion.SetSize( size);

		for ( int i=0; i < nSlices; i++)
		{
			Polygon<Pt2<float>, float> poly;
			start[2] = i;
			
			desiredRegion.SetIndex( start);

			extractFilter->SetExtractionRegion( desiredRegion);
			extractFilter->Update();

			if ( getPolgon( extractFilter->GetOutput(), m_SnakeColorLabel, poly))
				polys.push_back( poly);		// assumes all will be on contiguous slices
		}

		uint npoly = polys.size();
		float volume = float(0);
		if ( npoly < 2)
		{
			strcpy( errMsg, "Error finding polygons, unable to get volume");
			return false;
		}

		// calculate volume
		float a1 = polys[0].area2() * 0.5F;
		for ( uint i=1; i<npoly; i++)
		{
			float a2 = polys[i].area2() * 0.5F;			// area in pixels
			volume += (a1+a2) * spacingFactor;
			a1 = a2;
		}
		m_Volume[0] = volume;
		cout << "SegImage3D::getSegmentationVolume(): vol = " << volume << " mm^3" << endl;
		return true;
	}
	else if ( hasWalls())
	{
		string wallNames[] = { "Epi", "LaEndo", "RaEndo", "LvEndo", "RvEndo" };
		nSlices = m_Walls.size();

		// look for same endo WallType in consecutive slices
		for ( uint wt = 1; wt < 5; wt++)
		{
			m_Volume[wt-1] = float(0);
			uint startSlice, endSlice;
			
			// find single sequence of consecutive walls for this WallType
			startSlice = 0;
			while ( startSlice < nSlices && 
				(m_Walls[startSlice].size() == 0 || m_Walls[startSlice][0].wall[wt].size() == 0))
				startSlice++;
			if ( startSlice > nSlices-2)
				continue;

			endSlice = startSlice+1;
			while ( endSlice < nSlices && 
				(m_Walls[endSlice].size() && m_Walls[endSlice][0].wall[wt].size() > 0))
				endSlice++;
			if ( (endSlice - startSlice) < 2)
				continue;

			// calculate volume
			float a1 = float(0);
			uint nWallSets = m_Walls[startSlice].size();
			for ( uint ws = 0; ws < nWallSets; ws++)
			{
				if ( m_Walls[startSlice][ws].wall[wt].size())
				{
					m_Walls[startSlice][ws].wall[wt].evalCurve();		// make sure polygon generated
					a1 += m_Walls[startSlice][ws].wall[wt].poly()->area2() * 0.5F;	// pixel area
				}
			}
			for ( uint slice=startSlice+1; slice < endSlice; slice++)
			{
				float a2 = float(0);
				for ( uint ws = 0; ws < nWallSets; ws++)
				{
					if ( m_Walls[slice][ws].wall[wt].size())
					{
						m_Walls[startSlice][ws].wall[wt].evalCurve();	// make sure polygon generated
						a2 += m_Walls[startSlice][ws].wall[wt].poly()->area2() * 0.5F;
					}
				}
				m_Volume[wt-1] += (a1+a2) * spacingFactor;
				a1 = a2;
			}
			cout << "SegImage3D::getSegmentationVolume(): " << wallNames[wt] << " vol = " 
				<< m_Volume[wt-1] << " mm^3" << endl;
		}
		
		if ( (m_Volume[0] + m_Volume[1] + m_Volume[2] + m_Volume[3]) == float(0))
		{
			strcpy( errMsg, "Error, must have walls in at least 2 consecutive slices");
			return false;		
		}
	}
	else
	{
		strcpy( errMsg, "Error, either segmentation not done or no walls");
		return false;
	}
}	// getSegmentationVolume()

//
//	Used in getSegmentationVolume()
//	
bool
SegImage3D::getPolgon( const LabelImage2dType *image, LabelTypeUS val, 
					Polygon<Pt2<float>, float> &poly)
{
	/** Fast Marching filter use to evolve the contours */
	typedef itk::ContourExtractor2DImageFilter< LabelImage2dType> 
										ContourExtractorFilterType;
		
	int npoly;
	ContourExtractorFilterType::Pointer	 contourExtractorFilter;
	contourExtractorFilter = ContourExtractorFilterType::New();
	ContourExtractorFilterType::OutputPathType *contour;
	ContourExtractorFilterType::VertexListType const *vlist, *vl;

	contourExtractorFilter->SetInput( image);
	contourExtractorFilter->SetContourValue( val-1);
	contourExtractorFilter->Update();
	npoly = contourExtractorFilter->GetNumberOfOutputs();
	cout << "getPolgon: val=" << val << ", npoly=" << npoly; 

	if ( npoly)
	{
		// get largest poly with at least 10 points
		int len1 = 0;

		for ( int i=0; i<npoly; i++)
		{
			contour = contourExtractorFilter->GetOutput( i);
			vl = contour->GetVertexList();
			int len = vl->Size();
			if ( len < 10)
				continue;
			if ( len > len1)
			{
				len1 = len;
				vlist = vl;
			}
		}

		if ( len1 >= 10)
		{
			cout << ", size = " << len1 << endl;
			for ( int i=0; i < len1; i++)
			{
				ContourExtractorFilterType::VertexType vert = (*vlist)[i];
				Pt2<float> pt( (float)vert[0], (float)vert[1]);
				poly.push_back( pt);
			}
			return true;
		}
	}
	cout << endl;
	return false;
}

//
//	Fibrosis functions
//	

//
//	sort compare function (ascending x)
//	
bool	compareAscX( const Pt3<float> &p1, const Pt3<float> &p2)
{
	return p1[0] < p2[0];
}

//
//	Get myocardial pixels for all slices with epi & WallType (wt) endo walls
//	wt: 1 - LaEndo, 2 - RaEndo, 3 - LvEndo, 4 - RvEndo wall
//	
bool
SegImage3D::getMyoPixels( uint wt, uint viewId, vector<GreyType> &pixels)
{
	uint nSlices = m_Walls.size();
	uint curSlice = getSlice();		// for restoring current slice
	
	pixels.clear();
	_myoPixels[wt-1].clear();
	_myoPixels[wt-1].assign( nSlices, pixels);
	
	for ( uint slice = 0; slice < nSlices; slice++)
	{
		setSlice( slice);

		// for each WallSet this slice
		for ( vector<WallSet >::iterator wsi = m_Walls[slice].begin();
				wsi != m_Walls[slice].end(); wsi++)
		{
			SplineCurve<Pt3<float>, float> &epi = (*wsi).wall[0];
			if ( epi.size() == 0)
				continue;

			cout << "SegImage3D::getMyoPixels: slice " << slice << endl;
			epi.evalCurve();			// make sure polygon generated
			Bbox<Pt3<float>, float> *bbox = epi.bbox();
			Pt3<float> minPt = bbox->minPt();
			Pt3<float> maxPt = bbox->maxPt();
			Polygon<Pt3<float>, float> *epiPoly = epi.poly();
			Polygon<Pt3<float>, float> *endoPoly = 0;
			
			// walls in ModelView coordinates (0,0 in ll corner)
			// for endo wall w:
			// 	start at bottom (ModelView coords) and work up
			// 	for each y val
			// 		find epi isects (possibly only one)
			// 			if only one continue
			// 		if endo, find endo isects (possibly only one)
			// 		if zero endo isects
			// 			add voxels between epiEnter & exitPt
			// 		if only one endo isect, assign to both endoEnter & endoExit
			// 			add voxels between epiEnter & endoEnter
			// 			add voxels between endoExit & exitPt
			// 		if 2 epi and 2 endo isects
			// 			add voxels between first epi and first endo isect
			// 			add voxels between 2nd endo and 2nd epi isect
			// 		
			// 		
			// 		

			SplineCurve<Pt3<float>, float> &endo = (*wsi).wall[wt];
			if ( endo.size() > 0)
			{
				endo.evalCurve();
				endoPoly = endo.poly();
			}
			Pt3<float> startPt = minPt;
			startPt.incx( float(-1));
			Pt3<float> endPt( maxPt[0], minPt[1], minPt[2]);
			
			// 	for each y val
			while ( startPt[1] < maxPt[1])
			{
				vector<Pt3<float> > iptsEpi, iptsEndo;

				cout << "y = " << startPt[1] << endl;
				uint nEpiIsect = epiPoly->intersectLine( startPt, endPt, iptsEpi);
				if ( nEpiIsect > 2)
					cerr << "SegImage3D::getMyoPixels: num Epi isects = " << nEpiIsect << endl;
				if ( nEpiIsect < 2)
				{
					startPt.incy( float(1));
					endPt.incy( float(1));
					continue;
				}

				if ( nEpiIsect > 1)
					sort( iptsEpi.begin(), iptsEpi.end(), compareAscX);		// need ascending x order

				uint nEndoIsect = 0;
				if ( endoPoly)
					nEndoIsect = endoPoly->intersectLine( startPt, endPt, iptsEndo);

				if ( nEndoIsect > 1)
					sort( iptsEndo.begin(), iptsEndo.end(), compareAscX);	// need ascending x order

				// check for single isects
				if ( nEpiIsect & 1)
				{
					for ( vector<Pt3<float> >::iterator pi = iptsEpi.begin(); 
						pi != iptsEpi.end(); pi++)
					{
						Pt3<float> pt = *pi;
						pt.incx( float(2));
						if ( !epiPoly->inside(pt))
						{
							iptsEpi.erase(pi);
							nEpiIsect--;
							cout << "Erased epi pt" << endl;
							break;
						}
					}
				}
				if ( nEndoIsect & 1)
				{
					for ( vector<Pt3<float> >::iterator pi = iptsEndo.begin(); 
						pi != iptsEndo.end(); pi++)
					{
						Pt3<float> pt = *pi;
						pt.incx( float(2));
						if ( !endoPoly->inside(pt))
						{
							iptsEndo.erase(pi);
							nEndoIsect--;
							cout << "Erased endo pt" << endl;
							break;
						}
					}
				}
				if ( nEpiIsect & 1)
					cerr << "Error: nEpiIsect == " << nEpiIsect << endl;
				if ( nEndoIsect & 1)
					cerr << "Error: nEndoIsect == " << nEndoIsect << endl;
#if 0
				// delete single epi isects between adjacent endo
				if ( !checkSingleIsects( iptsEpi, iptsEndo, true))
				{
					alert( "Error slice %d: analyzing epi wall, redraw", slice);
					return false;
				}
				nEpiIsect = iptsEpi.size();
				
				// delete single endo isects between adjacent epi
				if ( !checkSingleIsects( iptsEndo, iptsEpi, false))
				{
					alert( "Error slice %d: analyzing endo wall, redraw", slice);
					return false;
				}
				nEndoIsect = iptsEndo.size();
#endif				
				uint epiIndx = 0;
				uint endoIndx = 0;

				Pt3<float> enterPt = iptsEpi[epiIndx++];
				Pt3<float> exitPt;
				while ( epiIndx < nEpiIsect)
				{
					// find next exitPt
					if ( nEndoIsect == 0)
					{
						// add voxels between enterPt & exitPt
						Pt3<float> exitPt = iptsEpi[epiIndx++];
						addVoxels( viewId, enterPt, exitPt, _myoPixels[wt-1][slice]);

						// set next enterPt
						enterPt = iptsEpi[epiIndx++];
					}
					else
					{
						if ( (endoIndx < nEndoIsect) && iptsEndo[endoIndx][0] < iptsEpi[epiIndx][0])
							exitPt = iptsEndo[endoIndx++];	// exit is endo
						else
							exitPt = iptsEpi[epiIndx++];	// exit is epi, next enter must be epi
						addVoxels( viewId, enterPt, exitPt, _myoPixels[wt-1][slice]);

						if ( epiIndx >= nEpiIsect)
							break;

						// find next enterPt
						if ( endoIndx < nEndoIsect && iptsEndo[endoIndx][0] < iptsEpi[epiIndx][0])
							enterPt = iptsEndo[endoIndx++];
						else
							enterPt = iptsEpi[epiIndx++];
					}
				}

				startPt.incy( float(1));
				endPt.incy( float(1));
			}	// each yval
			
			// save pixels for this slice in pixels
			pixels.insert( pixels.end(), _myoPixels[wt-1][slice].begin(), 
											_myoPixels[wt-1][slice].end());
		}	// each WallSet
	}	// each slice

	// restore current slice
	setSlice( curSlice);
	if ( pixels.size() == 0)
		return false;
	return true;
}	// getMyoPixels()


//
//	Add pixels between pt1, pt2, also add cursor points to _myoPoints.
//	
void
SegImage3D::addVoxels( uint viewId, Pt3<float> &pt1, Pt3<float> &pt2, vector<GreyType> &pixels)
{
	uint nadded = 0;
	uint slice = getSlice();		// need to set for slice, not image.	???
	ImageCoordinateTransform DisplayToImageTransform = 
		GetGrey()->GetDisplayToImageTransform( viewId);
	
	while (pt1.x() < pt2.x())
	{
		Vector3f xSlice( pt1[0], pt1[1], pt1[2]);
		xSlice = DisplayToImageTransform.TransformPoint( xSlice);
		Pt3<float> ipt( xSlice(0), xSlice(1), xSlice(2));
		Vec3<uint> cursor = to_uint( ipt);
		cursor[2] = slice;
		GreyType pixel = GetGrey()->GetVoxel( cursor);
		pixels.push_back( pixel);

		_myoPoints.push_back( cursor);		// set myo point

		pt1.incx( float(1));				// next point
		nadded++;
	}
	cout << "addVoxels: added " << nadded << endl;
}

//
//	Make sure no single ipts1 between any two ipts2.
//	Return true if num ipts1 even (or zero).
//	
bool
SegImage3D::checkSingleIsects( vector<Pt3<float> > &ipts1, vector<Pt3<float> > &ipts2, bool epi)
{
	uint nipts1 = ipts1.size();
	uint nipts2 = ipts2.size();

	if ( nipts1 == 0)
		return true;
	if ( (nipts1 == 2) || (nipts2 == 0))
		return !(ipts1.size() & 1);
	
	uint indx1 = epi ? 1 : 0;
	uint indx2 = 0;
	for ( ; indx1 < nipts1-1; indx1++)
	{
		// find last ipts2 pt before this ipts1 pt
		while ( indx2+1 < nipts2 && (ipts2[indx2+1][0] < ipts1[indx1][0]))
			indx2++;

		if ( epi && indx2+1 == nipts2 && ((indx1 - nipts1) == 2))
		{	// have 2 epi after last endo pt, delete last epi
			ipts1.pop_back();
			cout << "SegImage3D::checkSingleIsects: deleted epi pt " << nipts1 << endl;
			nipts1--;
			indx1--;
		}
		// if next ipts1 point > next ipts2 point, have single ipts1 between two ipts2, so delete it
		else if ( ipts1[indx1+1][0] > ipts2[indx2+1][0])
		{
			vector<Pt3<float> >::iterator epii = find( ipts1.begin(), ipts1.end(), ipts1[indx1]);
			ipts1.erase( epii);
			if ( epi)
				cout << "SegImage3D::checkSingleIsects: deleted epi pt " << nipts1 << endl;
			else
				cout << "SegImage3D::checkSingleIsects: deleted endo pt " << nipts1 << endl;
			nipts1--;
			indx1--;
		}
		else
			indx1++;		// two ipts1 between so skip next ipts1
	}
	return !(ipts1.size() & 1);
}

void
SegImage3D::updateFibrosisThreshold( GreyType threshold)
{
	LabelImageWrapper3D *threshImage = GetThreshImage();
	LabelTypeUS color = 4;			// yellow
	
	for ( vector<Vec3<uint> >::iterator pi= _myoPoints.begin(); pi != _myoPoints.end(); pi++)
	{
		if ( GetGrey()->GetVoxel( *pi) >= threshold)
			threshImage->GetVoxelForUpdate( *pi) = color;
		else
			threshImage->GetVoxelForUpdate( *pi) = 0;		// zero pixel
	}
	threshImage->GetImage()->Modified();
	threshImage->UpdateColorMappingCache();
}

void
SegImage3D::saveFibrosis( ofstream &ofstrm, GreyType minPixel, GreyType maxPixel, 
		GreyType threshold, uint numAbove, float perCentAbove)
{
	Vec3<uint> size = GetGrey()->GetSize();
	uint nSlices = size[2];
	string wallNames[] = { "LaEndo", "RaEndo", "LvEndo", "RvEndo" };

	ofstrm << "Fibrosis threshold data" << endl;
	ofstrm << "Min pixel\tMax pixel\tThreshold\tNum myo pixels\tNum > threshold\tPer Cent Above" << endl;
	ofstrm << minPixel << "\t\t" << maxPixel << "\t\t" << threshold << 
		"\t\t" << _myoPoints.size() << "\t\t" << numAbove << "\t\t" << perCentAbove << endl;

	// Output data for each endo wall, each slice
	for ( uint i=0; i<4; i++)
	{
		if (	_myoPixels[i].size() == 0)
			continue;
		ofstrm << endl << wallNames[i] << ":" << endl;
		ofstrm << "Slice\tNum myo pixels\tNum > threshold\tPer Cent Above" << endl;
		for ( uint slice = 0; slice < nSlices; slice++)
		{
			if ( _myoPixels[i][slice].size() == 0)
				continue;
			uint nAbove = 0;
			for ( vector<GreyType>::iterator pi = _myoPixels[i][slice].begin();
					pi != _myoPixels[i][slice].end(); pi++)
				if ( *pi > threshold)
					nAbove++;
			ofstrm << slice << "\t" << _myoPixels[i][slice].size() << "\t\t" << nAbove
			 << "\t\t" << ((float)nAbove / (float)_myoPixels[i][slice].size()) * 100.0F	<< endl;
		}
	}
	ofstrm.close();
}

void
SegImage3D::setLabelColor( Pt3<float> &pos)
{
	// drill down into mesh
}

unsigned long
SegImage3D::countVoxelsColor( LabelImageWrapper3D::ImagePointer img, LabelTypeUS color)
{
	typedef itk::ImageRegionConstIterator<LabelImageType> SourceIterator;

	unsigned long nVoxels = 0;
	LabelImageType::RegionType region = img->GetBufferedRegion();
	SourceIterator itSource( img, region);
	
	while( !itSource.IsAtEnd())
	{
		if ( itSource.Value() == color)
			nVoxels++;
		++itSource;
	}
	return nVoxels;
}

//
template< typename ImgType>
void
SegImage3D::GetMinMax( ImgType *img, typename ImgType::PixelType &min, 
					typename ImgType::PixelType &max, const char *msg)
{
	typedef itk::MinimumMaximumImageCalculator<ImgType> MinMaxCalculatorType;
	typename MinMaxCalculatorType::Pointer MinMaxCalculator =  MinMaxCalculatorType::New();
	MinMaxCalculator->SetImage( img);
	MinMaxCalculator->Compute();

	min = MinMaxCalculator->GetMinimum();
	max = MinMaxCalculator->GetMaximum();

	if ( msg)
		cout << msg << ": " << int(min) << ", " << int(max) << endl;
}

//
//	Not currently used.
//	
template<typename ImgType>
void
SegImage3D::GetHistogram( ImgType *img, const char *msg)
{
	unsigned long histo[256];

	for ( uint i=0; i<256; i++)
		histo[i] = 0;

	// Create an iterator for parsing the image
	typedef itk::ImageRegionConstIterator<ImgType> InputIterator;
	InputIterator it( img, img->GetLargestPossibleRegion());

	// Parse through the image using an iterator and compute the bounding boxes
	for(it.GoToBegin();!it.IsAtEnd();++it)
	{
		// Get the intensity at current pixel
		float val = it.Value();
		LabelTypeUS label = it.Value();

		// Increment the histogram
		if ( val < 0.F)
			label = 0;
		else if ( val > 255.F)
			label = 255;
		else
			label = (unsigned short)val;
		histo[label]++;
	}

	cout << msg << ": " << endl;
	for ( uint i=0; i<256; i++)
		if ( histo[i] > 0)
			cout << i << ": " << histo[i] << endl;
}

//
//	Rescale to OutImgType.	Not currently used.
//	
template<typename InImgType, typename OutImgType>
typename OutImgType::Pointer
SegImage3D::rescaleImage( InImgType *inImg, typename OutImgType::PixelType outMin, 
					typename OutImgType::PixelType outMax)
{
	// rescale to unsigned short max
	typedef itk::RescaleIntensityImageFilter< InImgType, OutImgType> RescaleFilterType;

	typename RescaleFilterType::Pointer rescaleFilter = RescaleFilterType::New();
	rescaleFilter->SetInput( inImg);
	if ( outMax > 0)
	{
		rescaleFilter->SetOutputMinimum( outMin);
		rescaleFilter->SetOutputMaximum( outMax);
	}
	else
	{
		rescaleFilter->SetOutputMinimum( 0);
		typename InImgType::PixelType min, max;
		GetMinMax<InImgType>( inImg, min, max);
		if ( min < 0)
			max += -min;
		rescaleFilter->SetOutputMaximum( max);
	}
	rescaleFilter->Update();
	return rescaleFilter->GetOutput();
}
