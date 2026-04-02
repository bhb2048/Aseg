//
//  SegLevelSetDriver.txx
//
//  22-dec-11  bhb  From ItkSnap SNAPLevelSetDriver.txx
//  Modified:
//  06-jan-12  bhb change m_Parameters to be a pointer
//
#ifndef __SegLevelSetDriver_txx_
#define __SegLevelSetDriver_txx_

#include "SegLevelSetDriver.h"
#include "SegParameters.h"
#include <ITK/VectorTypesToITKConversion.h>

#include "itkCommand.h"
#include "itkSparseFieldLevelSetImageFilter.h"
#include "itkNarrowBandLevelSetImageFilter.h"
#include "itkDenseFiniteDifferenceImageFilter.h"
#include "itkGeodesicActiveContourLevelSetImageFilter.h"
#include "LevelSetExtensionFilter.h"

// for GeodesicActiveContourShapePriorLevelSetImageFilter
#include "itkImageFileReader.h"
#include "itkMetaImageIO.h"
#include "itkGeodesicActiveContourShapePriorLevelSetImageFilter.h"
#include "itkPCAShapeSignedDistanceFunction.h"
#include "itkEuler2DTransform.h"
#include "itkEuler3DTransform.h"
#include "itkOnePlusOneEvolutionaryOptimizer.h"
#include "itkNormalVariateGenerator.h"
#include "itkNumericSeriesFileNames.h"
#include <vector>

using namespace std;

//
//	'init' is m_SnakeInitializationWrapper image possibly init'd from bubbles or fast marching
//		w/ seeds.
//	'speed' is Edge or In/Out image
//	
template<unsigned int VDimension>
SegLevelSetDriver<VDimension>::SegLevelSetDriver( FloatImageType *init, FloatImageType *speed,
	const SegParameters *sparms, VectorImageType *externalAdvection, itk::Command *progressCB)				
{
	// Create the level set function
	m_LevelSetFunction = LevelSetFunctionType::New();

	// Pass the speed image to the function
	m_LevelSetFunction->SetSpeedImage( speed);

	// Set the external advection if any
	if ( externalAdvection)
	  m_LevelSetFunction->SetAdvectionField( externalAdvection);

	// Remember the input and output images for later initialization
	m_InitializationImage = init;

	// Pass the parameters to the level set function
	AssignParametersToPhi( sparms);

	// Create the filter
	DoCreateLevelSetFilter( progressCB);
}

template<unsigned int VDimension>
void
SegLevelSetDriver<VDimension>::AssignParametersToPhi(const SegParameters *p)
{
	// Set up the level set function

	// The sign of the advection term is flipped in our equation
	m_LevelSetFunction->SetAdvectionWeight( -p->GetAdvectionWeight());
	m_LevelSetFunction->SetAdvectionSpeedExponent( p->GetAdvectionSpeedExponent());

	// The curvature exponent for traditional/legacy reasons has a +1 value.
	// 08-aug-17 bhb, but value is -1 so now zero.
	m_LevelSetFunction->SetCurvatureSpeedExponent( p->GetCurvatureSpeedExponent()+1);
	m_LevelSetFunction->SetCurvatureWeight(p->GetCurvatureWeight());

	m_LevelSetFunction->SetPropagationWeight(p->GetPropagationWeight());
	m_LevelSetFunction->SetPropagationSpeedExponent( p->GetPropagationSpeedExponent());

	m_LevelSetFunction->SetLaplacianSmoothingWeight( p->GetLaplacianWeight());
	m_LevelSetFunction->SetLaplacianSmoothingSpeedExponent( p->GetLaplacianSpeedExponent());

	// We only need to recompute the internal images if the exponents to those
	// images have changed
	m_LevelSetFunction->CalculateInternalImages();

	// Call the initialize method
	typename LevelSetFunctionType::RadiusType radius;
	radius.Fill(1);
	m_LevelSetFunction->Initialize(radius);

	// Set the time step
	m_LevelSetFunction->SetTimeStepFactor(
		p->GetAutomaticTimeStep() ? 1.0 : p->GetTimeStepFactor());

	// Remember the parameters
	m_Parameters = p;
}

template<unsigned int VDimension>
void
SegLevelSetDriver<VDimension>::DoCreateLevelSetFilter( itk::Command *progressCB)
{
	// In this method we have the flexibility to create a level set filter
	// of any ITK solver type.  This way, we can plug in different solvers:
	// NarrowBand, ParallelSparseField, even Dense.
	if ( m_Parameters->GetSolver() == SegParameters::PARALLEL_SPARSE_FIELD_SOLVER)
	{
		// Define an extension to the appropriate filter class
		typedef itk::SparseFieldLevelSetImageFilter<FloatImageType, FloatImageType> 
			LevelSetFilterType;

		typedef typename LevelSetFilterType::Pointer LevelSetFilterPointer;
		LevelSetFilterPointer filter = LevelSetFilterType::New();

		// Cast this specific filter down to the lowest common denominator that is
		// a filter (FiniteDifferenceImageFilter)
		m_LevelSetFilter = filter.GetPointer();

		// Perform the special configuration tasks on the filter
		filter->SetInput( m_InitializationImage);
		filter->SetNumberOfLayers( 3);
		filter->SetIsoSurfaceValue( 0.0F);
		filter->SetDifferenceFunction( m_LevelSetFunction);
		filter->InPlaceOn();
	}
	else if ( m_Parameters->GetSolver() == SegParameters::DENSE_SOLVER)
	{
		// Define an extension to the appropriate filter class
		typedef itk::DenseFiniteDifferenceImageFilter<FloatImageType, FloatImageType>
			LevelSetFilterType;
		typedef LevelSetExtensionFilter<LevelSetFilterType> ExtensionFilter;
		typename ExtensionFilter::Pointer filter = ExtensionFilter::New();

		// Cast this specific filter down to the lowest common denominator that is
		// a filter (FiniteDifferenceImageFilter)
		m_LevelSetFilter = filter.GetPointer();

		// Perform the special configuration tasks on the filter
		filter->SetInput( m_InitializationImage);
		filter->SetDifferenceFunction( m_LevelSetFunction);
		filter->InPlaceOn();
	}
	else if ( m_Parameters->GetSolver() == SegParameters::GEO_ACT_CONT)
	{
		// Define an extension to the appropriate filter class
		typedef itk::GeodesicActiveContourLevelSetImageFilter< FloatImageType, FloatImageType>
														LevelSetFilterType;
		typedef LevelSetExtensionFilter<LevelSetFilterType> ExtensionFilter;
		typename ExtensionFilter::Pointer filter = ExtensionFilter::New();

		// Cast this specific filter down to the lowest common denominator that is
		// a filter (FiniteDifferenceImageFilter)
		m_LevelSetFilter = filter.GetPointer();

		// Perform the special configuration tasks on the filter
		filter->SetInput( m_InitializationImage);
		filter->SetFeatureImage( m_LevelSetFunction->GetSpeedImage());
		filter->SetAdvectionScaling( -m_Parameters->GetAdvectionWeight());	// neg?
		filter->SetCurvatureScaling( m_Parameters->GetCurvatureWeight());
		filter->SetPropagationScaling( m_Parameters->GetPropagationWeight());
		filter->SetMaximumRMSError( m_Parameters->GetRMSError());
		filter->SetDifferenceFunction( m_LevelSetFunction);		// added 02-aug-17, need test
		filter->InPlaceOn();		// ??? 02-aug-17 was commented out
		cout << "Advection:          " << filter->GetAdvectionScaling() << endl;
		cout << "Curvature:          " << filter->GetCurvatureScaling() << endl;
		cout << "Propagation:        " << filter->GetPropagationScaling() << endl;
	}
	else if ( m_Parameters->GetSolver() == SegParameters::GEO_ACT_CONT_SHAPE)
	{
		typedef itk::GeodesicActiveContourShapePriorLevelSetImageFilter<
			FloatImageType, FloatImageType>	LevelSetFilterType;
		typedef LevelSetExtensionFilter<LevelSetFilterType> ExtensionFilter;
		typename ExtensionFilter::Pointer filter = ExtensionFilter::New();


		// Cast this specific filter down to the lowest common denominator that is
		// a filter (FiniteDifferenceImageFilter)
		m_LevelSetFilter = filter.GetPointer();

		// Perform the special configuration tasks on the filter
		filter->SetInput( m_InitializationImage);					// fast marching w/ seeds
		filter->SetFeatureImage( m_LevelSetFunction->GetSpeedImage());	// gradient reciprocal
																			// from ITK guide:
		filter->SetAdvectionScaling( m_Parameters->GetAdvectionWeight());	// neg?
		filter->SetCurvatureScaling( m_Parameters->GetCurvatureWeight());		// 0.2
		filter->SetPropagationScaling( m_Parameters->GetPropagationWeight());	// 0.5
		filter->SetMaximumRMSError( m_Parameters->GetRMSError());
//		filter->InPlaceOn();
		cout << "Advection:          " << filter->GetAdvectionScaling() << endl;
		cout << "Curvature:          " << filter->GetCurvatureScaling() << endl;
		cout << "Propagation:        " << filter->GetPropagationScaling() << endl;

		filter->SetShapePriorScaling( m_Parameters->GetShapeScaling());  // 0.02 from ITK guide
		filter->SetNumberOfLayers( 4);	// incr # of sparse field layers, default 3 (dimension)

		// shape model:
		const unsigned int numberOfPCAModes = int( m_Parameters->GetNumPCAModes());
		m_Shape = ShapeFunctionType::New();
//		m_Shape->DebugOn();		// output very slow
		m_Shape->SetNumberOfPrincipalComponents( numberOfPCAModes);

		// Read the mean shape and principal mode images
		typedef  itk::ImageFileReader< FloatImageType> ReaderType;
		typename ReaderType::Pointer meanShapeReader = ReaderType::New();
		meanShapeReader->SetImageIO( itk::MetaImageIO::New());

		const char *filename = m_Parameters->GetShapeMeanInput();
		if ( !filename || strlen(filename) == 0 )
		{
			return;
		}

		meanShapeReader->SetFileName( filename);
		// Read mean shape image
		try
		{
			meanShapeReader->Update();
		}
		catch( itk::ExceptionObject & excep)
		{
			cerr << "Exception reading mean shape: " << excep.GetDescription() << endl;
			return;
		}
		cout << "Loaded " << filename << endl;

		typedef itk::Image< float, VDimension>					FloatImageType;
		typedef	typename FloatImageType::Pointer				FloatImageTypePointer;

		FloatImageTypePointer meanImage = meanShapeReader->GetOutput();
		cout << "Mean image origin: " << meanImage->GetOrigin() << endl;
		cout << "Mean image size: " << meanImage->GetBufferedRegion().GetSize() << endl;

		vector<FloatImageTypePointer> shapeModeImages( numberOfPCAModes);
		itk::NumericSeriesFileNames::Pointer fileNamesCreator =
				itk::NumericSeriesFileNames::New();

		fileNamesCreator->SetStartIndex( 0);
		fileNamesCreator->SetEndIndex( numberOfPCAModes - 1);
		fileNamesCreator->SetSeriesFormat( m_Parameters->GetShapeModeFormat());
		const vector<string> & shapeModeFileNames = fileNamesCreator->GetFileNames();

		// read principle mode images
		for ( unsigned int k = 0; k < numberOfPCAModes; k++)
		{
			typename ReaderType::Pointer shapeModeReader = ReaderType::New();
			shapeModeReader->SetImageIO( itk::MetaImageIO::New());
			shapeModeReader->SetFileName( shapeModeFileNames[k].c_str());
			try
			{
				shapeModeReader->Update();
			}
			catch( itk::ExceptionObject & excep)
			{
				cerr << "Exception: " << excep.GetDescription() << endl;
				return;
			}
			shapeModeImages[k] = shapeModeReader->GetOutput();
//			cout << "Mode image " << k << " origin: " << shapeModeImages[k]->GetOrigin() << endl;
//			cout << "Mode image " << k << " size: " <<
//			shapeModeImages[k]->GetBufferedRegion().GetSize() << endl;
		}

		m_Shape->SetMeanImage( meanShapeReader->GetOutput());
		m_Shape->SetPrincipalComponentImages( shapeModeImages);

		// Assume that the shape modes have been normalized by multiplying with the
		// corresponding singular value. Hence, we can set the principal component standard
		// deviations to all ones.
		typename ShapeFunctionType::ParametersType pcaStandardDeviations( numberOfPCAModes);
		pcaStandardDeviations.Fill( 1.0);
		m_Shape->SetPrincipalComponentStandardDeviations( pcaStandardDeviations);

		// Connect a Euler2D/3DTransform to the PCASignedDistanceFunction. The transform 
		// represents the pose of the shape. The parameters of the transform forms the set 
		// of pose parameters.
		typedef itk::Euler3DTransform<double>			TransformType3D;
		typename TransformType3D::Pointer transform = TransformType3D::New();
		m_Shape->SetTransform( transform);
				
		// Set up Cost Function:

		// Before updating the level set at each iteration, the parameters of the current
		// best-fit shape is estimated by minimizing the ShapePriorMAPCostFunction.
		// The cost function is composed of four terms: contour fit, image fit, shape prior
		// and pose prior.  The user can specify the weights applied to each term.
		typedef itk::ShapePriorMAPCostFunction< FloatImageType, float>	CostFunctionType;
		typename CostFunctionType::Pointer costFunction = CostFunctionType::New();

		typename CostFunctionType::WeightsType weights;
		weights[0] =  m_Parameters->GetContourWt();		// weight for contour fit term
		weights[1] =  m_Parameters->GetImageWt();		// weight for image fit term
		weights[2] =  m_Parameters->GetShapePriorWt();	// weight for shape prior term
		weights[3] =  m_Parameters->GetPosePriorWt(); 	// weight for pose prior term

		costFunction->SetWeights( weights);

		typename CostFunctionType::ArrayType mean(   m_Shape->GetNumberOfShapeParameters());
		typename CostFunctionType::ArrayType stddev( m_Shape->GetNumberOfShapeParameters());

		mean.Fill( 0.0);
		stddev.Fill( 1.0);
		costFunction->SetShapeParameterMeans( mean);
		costFunction->SetShapeParameterStandardDeviations( stddev);

			// Setup Optimizer:

		// Use OnePlusOneEvolutionaryOptimizer to optimize the cost function
		typedef itk::OnePlusOneEvolutionaryOptimizer			OptimizerType;
		OptimizerType::Pointer optimizer = OptimizerType::New();
		typedef itk::Statistics::NormalVariateGenerator GeneratorType;
		GeneratorType::Pointer generator = GeneratorType::New();

		generator->Initialize( 20020702);

		optimizer->SetNormalVariateGenerator( generator);
		OptimizerType::ScalesType scales( m_Shape->GetNumberOfParameters());
		scales.Fill( 1.0);
		for( unsigned int k = 0; k < numberOfPCAModes; k++)
		{
			scales[k] = 20.0;		// scales for the pca mode multiplier
		}
		scales[numberOfPCAModes] = 350.0;	// scale for 2D rotation
		optimizer->SetScales( scales);

		// Next, we specify the initial radius, the shrink and
		// grow mutation factors and termination criteria of the optimizer.
		// Since the best-fit shape is re-estimated each iteration of
		// the curve evolution, we do not need to spend too much time finding the true
		// minimizing solution each time; we only need to head towards it. As such,
		// we only require a small number of optimizer iterations.
		//
		double initRadius = 1.05;
		double grow = 1.1;
		double shrink = pow(grow, -0.25);
		optimizer->Initialize( initRadius, grow, shrink);

		optimizer->SetEpsilon(1.0e-6);		// minimal search radius

		optimizer->SetMaximumIteration(15);

		// Before starting the segmentation process we need to also supply the initial
		// best-fit shape estimate. Use the unrotated mean shape with the initial x and
		// y translation.
		typename ShapeFunctionType::ParametersType parameters( m_Shape->GetNumberOfParameters());
		parameters.Fill( 0.0);
		parameters[numberOfPCAModes + 1] = float( m_Parameters->GetShapeStartX());
		parameters[numberOfPCAModes + 2] = float( m_Parameters->GetShapeStartY());
		parameters[numberOfPCAModes + 3] = float( m_Parameters->GetShapeStartZ());

		// Connect all the components to the filter.
		filter->SetShapeFunction( m_Shape);
		filter->SetCostFunction( costFunction);
		filter->SetOptimizer( optimizer);
		filter->SetInitialParameters( parameters);
		filter->SetDifferenceFunction( m_LevelSetFunction);	// added 02-aug-17, not in Sticks
		// need to try with & without - bhb 21-sep-17.
		filter->InPlaceOn();
	}
	else
	{
		throw itk::ExceptionObject(__FILE__,__LINE__,"Unknown level set solver requested");
	}

	if ( progressCB)
		m_LevelSetFilter->AddObserver( itk::IterationEvent(), progressCB);

	// This code is common to all filters. It causes the filter to initialize
	// the necessary memory and sets the iteration counter to 0
	m_LevelSetFilter->SetManualReinitialization(true);
	m_LevelSetFilter->SetNumberOfIterations(0);

	// Update the largest possible region. The slicer may be changing the
	// requested region on this image, so it's important that we always
	// update the entire image
	m_LevelSetFilter->UpdateLargestPossibleRegion();
}	// DoCreateLevelSetFilter()

template<unsigned int VDimension>
void
SegLevelSetDriver<VDimension>::Restart()
{
	// Tell the filter to reinitialize next time that an update will
	// be performed, and set the number of iterations to 0
	m_LevelSetFilter->SetStateToUninitialized();
	m_LevelSetFilter->SetNumberOfIterations(0);

	// Update the largest possible region. The slicer may be changing the
	// requested region on this image, so it's important that we always
	// update the entire image
	m_LevelSetFilter->UpdateLargestPossibleRegion();
}

template<unsigned int VDimension>
void
SegLevelSetDriver<VDimension>::Run(unsigned int nIterations)
{
	// Increment the number of iterations
	unsigned int nElapsed = m_LevelSetFilter->GetElapsedIterations();
	m_LevelSetFilter->SetNumberOfIterations(nElapsed + nIterations);

	// Update the largest possible region. The slicer may be changing the
	// requested region on this image, so it's important that we always
	// update the entire image
	m_LevelSetFilter->UpdateLargestPossibleRegion();
}

template<unsigned int VDimension>
typename SegLevelSetDriver<VDimension>::FloatImageType *
SegLevelSetDriver<VDimension>::GetCurrentState()
{
	// Fix the spacing of the level set filter's output (huh?)
	m_LevelSetFilter->GetOutput()->SetDirection( m_InitializationImage->GetDirection());
	m_LevelSetFilter->GetOutput()->SetSpacing( m_InitializationImage->GetSpacing());
	m_LevelSetFilter->GetOutput()->SetOrigin( m_InitializationImage->GetOrigin());

	// Return the filter's output
	return m_LevelSetFilter->GetOutput();
}

template<unsigned int VDimension>
unsigned int
SegLevelSetDriver<VDimension>::GetElapsedIterations() const
{
	return m_LevelSetFilter->GetElapsedIterations();
}

template<unsigned int VDimension>
void
SegLevelSetDriver<VDimension>::CleanUp()
{
	// Basically, the filter is finished, and we can finally return
	// from running the filter.  Let's clear the level set and the
	// function to free memory
	m_LevelSetFilter = 0;
	m_LevelSetFunction = 0;
}

//
//	14-jun-17 not called anywhere
//	
template<unsigned int VDimension>
void
SegLevelSetDriver<VDimension>::SetSegParameters(const SegParameters *sparms)
{
	// Parameter setting can be destructive or passive.  If the solver has
	// has changed, then it's destructive, otherwise it's passive
	bool destructive = sparms->GetSolver() != m_Parameters->GetSolver();

	// First of all, pass the parameters to the phi function, which may or
	// may not cause it to recompute it's images
	AssignParametersToPhi(sparms);

	// Create a new level set filter
	if ( destructive)
	{
		DoCreateLevelSetFilter();
	}
}

template<unsigned int VDimension>
void
SegLevelSetDriver<VDimension>::Print()
{
	cout << "LevelSetFilter:" << endl;
#if 0
	if ( m_Parameters->GetSolver() == SegParameters::GEO_ACT_CONT)
	{
		cout << "Advection:          " << filter->GetAdvectionScaling() << endl;
		cout << "Curvature:          " << filter->GetCurvatureScaling() << endl;
		cout << "Propagation:        " << filter->GetPropagationScaling() << endl;
	}
#endif
	cout << "NumberOfIterations: " << m_LevelSetFilter->GetNumberOfIterations() << endl;
	cout << "MaximumRMSError:    " << m_LevelSetFilter->GetMaximumRMSError() << endl;
	cout << "ElapsedIterations:  " << m_LevelSetFilter->GetElapsedIterations() << endl;
	cout << "RMSChange:          " << m_LevelSetFilter->GetRMSChange() << endl << endl;
}

#endif

