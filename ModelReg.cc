//
//	ModelReg.cc -- model to image registration
//	
//	21-apr-14	bhb
//
//	Modified:
//	
#include "ModelReg.h"
#include <ITK/SpeedImageWrapper3D.h>

#include "itkGroupSpatialObject.h"
#include "itkSpatialObjectToImageFilter.h"		// test pixels inside or outside object
#include "itkImageToSpatialObjectRegistrationMethod.h"
#include "itkLinearInterpolateImageFunction.h"	// evaluate image intensity in non-grid positions
#include "itkEuler2DTransform.h"				// map SpatialObject into image space
#include "itkOnePlusOneEvolutionaryOptimizer.h"	// optimizer used to search the parameter space 
												// and identify the best transformation that 
												// will map the shape model to the image
#include "itkDiscreteGaussianImageFilter.h"
#include "itkNormalVariateGenerator.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkCastImageFilter.h"
#include "itkRescaleIntensityImageFilter.h"

#include <itkMinimumMaximumImageCalculator.h>

#define HAVE_IMAGE

using namespace std;

//
//	Track the evolution of the optimizer as it progresses through the parameter space.	
//	This is done by using the Command/Observer paradigm.  The following lines of code 
//	implement the itk::Command observer that monitors the progress of the registration.
//	This command will be invoked at every iteration of the optimizer and will print out 
//	the current combination of transform parameters.
//
#include "itkCommand.h"
template < class TOptimizer>
class IterationCallback : public itk::Command
{
	public:
		typedef IterationCallback				Self;
		typedef itk::Command					Superclass;
		typedef itk::SmartPointer<Self>			Pointer;
		typedef itk::SmartPointer<const Self>	ConstPointer;

		itkTypeMacro( IterationCallback, Superclass);
		itkNewMacro( Self);

		// Type defining the optimizer
		typedef	TOptimizer	 OptimizerType;

		// Method to specify the optimizer
		void SetOptimizer( OptimizerType *optimizer)
		{
			m_Optimizer = optimizer;
			m_Optimizer->AddObserver( itk::IterationEvent(), this);
		}

		// Execute method will print data at each iteration
		void Execute( itk::Object *caller, const itk::EventObject & event)
		{
			Execute( (const itk::Object *)caller, event);
		}

		void Execute( const itk::Object *, const itk::EventObject & event)
		{
			if ( typeid( event) == typeid( itk::StartEvent))
			{
				cout << endl << "Position				Value";
				cout << endl << endl;
			}
			else if ( typeid( event) == typeid( itk::IterationEvent))
			{
				int iter = m_Optimizer->GetCurrentIteration();
				if ( !(iter % 10))
				{
					cout << m_Optimizer->GetCurrentIteration() << "	";
					cout << m_Optimizer->GetValue() << "	";
					cout << m_Optimizer->GetCurrentPosition() << endl;
				}
			}
			else if ( typeid( event) == typeid( itk::EndEvent))
			{
				cout << endl << endl;
				cout << "After " << m_Optimizer->GetCurrentIteration();
				cout << "	iterations " << endl;
				cout << "Solution is	= " << m_Optimizer->GetCurrentPosition();
				cout << endl;
			}
		}

	protected:
		IterationCallback() {};
		itk::WeakPointer<OptimizerType>	m_Optimizer;
};

//	A metric is defined to evaluate the fitness between the SpatialObject and the Image.
//	The base class for this type of metric is the itk::ImageToSpatialObjectMetric.
//	
//	This component evaluates the match between the SpatialObject and the Image. The
//	smoothness and regularity of the metric determine the difficulty of the task assigned 
//	to the optimizer. In this case, we use a very robust optimizer that should be able to 
//	find its way even in the most discontinuous cost functions. The metric to be implemented 
//	should derive from the ImageToSpatialObjectMetric class.
//
//	The following code implements a simple metric that computes the sum of the pixels that 
//	are inside the spatial object. In fact, the metric maximum is obtained when the model 
//	and the image are aligned. The metric	is templated over the type of the SpatialObject 
//	and the type of the Image.
//	
template <typename TFixedImage, typename TMovingSpatialObject>
class SimpleImageToSpatialObjectMetric :
	public itk::ImageToSpatialObjectMetric<TFixedImage,TMovingSpatialObject>
{

	public:
		// Standard class typedefs.
		typedef SimpleImageToSpatialObjectMetric	Self;
		typedef itk::ImageToSpatialObjectMetric<TFixedImage,TMovingSpatialObject>
													Superclass;
		typedef itk::SmartPointer<Self>				Pointer;
		typedef itk::SmartPointer<const Self>		ConstPointer;

		typedef itk::Point<double,2>				PointType;
		typedef list<PointType>						PointListType;
		typedef TMovingSpatialObject				MovingSpatialObjectType;
		typedef typename Superclass::ParametersType ParametersType;
		typedef typename Superclass::DerivativeType DerivativeType;
		typedef typename Superclass::MeasureType	MeasureType;
		
		// Method for creation through the object factory.
		itkNewMacro( Self);

		// Run-time type information (and related methods).
		itkTypeMacro( SimpleImageToSpatialObjectMetric, ImageToSpatialObjectMetric);

		itkStaticConstMacro( ParametricSpaceDimension, unsigned int, 3);

		// Specify the moving spatial object.
		void SetMovingSpatialObject( const MovingSpatialObjectType * object)
		{
			if ( !this->m_FixedImage)
			{
				cout << "Please set the image before the moving spatial object" << endl;
				return;
			}
			this->m_MovingSpatialObject = object;
			m_PointList.clear();
			typedef itk::ImageRegionConstIteratorWithIndex<TFixedImage> myIteratorType;

			myIteratorType it(this->m_FixedImage,this->m_FixedImage->GetBufferedRegion());

			itk::Point<double,2> point;

			while( !it.IsAtEnd())
			{
				this->m_FixedImage->TransformIndexToPhysicalPoint( it.GetIndex(), point);
				if ( this->m_MovingSpatialObject->IsInside(point,99999))
				{
					m_PointList.push_back( point);
				}
				++it;
			}

			cout << "Number of points in the metric = " 
				<< static_cast<unsigned long>( m_PointList.size()) << endl;
		}

		unsigned int GetNumberOfParameters(void) const	{return ParametricSpaceDimension;}

		// Get the Derivatives of the Match Measure
		void GetDerivative( const ParametersType &, DerivativeType &) const
		{
			return;
		}

		//
		//	The fundamental operation of the metric is its GetValue() method.
		//	It is in this method that the fitness value is computed. In our current
		//	example, the fitness is computed over the points of the
		//	SpatialObject. For each point, its coordinates are mapped
		//	through the transform into image space. The resulting point is used
		//	to evaluate the image and the resulting value is accumulated in a sum.
		//	Since we are not allowing scale changes, the optimal value of the sum
		//	will result when all the SpatialObject points are mapped on
		//	the white regions of the image. Note that the argument for the
		//	GetValue() method is the array of parameters of the transform.
		//

		// Get the value for SingleValue optimizers.
		MeasureType	GetValue( const ParametersType & parameters) const
		{
			double value;
			this->m_Transform->SetParameters( parameters);
			double minX = 1000.0, maxX = -1000.0;
			double minY = 1000.0, maxY = -1000.0;
			
			value = 0;
			for (PointListType::const_iterator it = m_PointList.begin();
														it != m_PointList.end(); ++it)
			{
				PointType transformedPoint = this->m_Transform->TransformPoint(*it);
				if ( transformedPoint[0] < minX)
					minX = transformedPoint[0];
				if ( transformedPoint[0] > maxX)
					maxX = transformedPoint[0];
				if ( transformedPoint[1] < minY)
					minY = transformedPoint[0];
				if ( transformedPoint[1] > maxY)
					maxY = transformedPoint[0];
				
				if ( this->m_Interpolator->IsInsideBuffer( transformedPoint))
				{
					value += this->m_Interpolator->Evaluate( transformedPoint);
				}
			}
			cout << "GetValue: min = " << minX << ", " << minY << " max = " << maxX << ", "
				<< maxY << endl;
			return value;
		}

		// Get Value and Derivatives for MultipleValuedOptimizers
		void GetValueAndDerivative( const ParametersType & parameters,
			 MeasureType & Value, DerivativeType	& Derivative) const
		{
			Value = this->GetValue(parameters);
			this->GetDerivative(parameters,Derivative);
		}

	private:
		PointListType m_PointList;
};

ModelReg::ModelReg() : _image(0)
{
	
}

ModelReg::~ModelReg()
{
	
}

void
ModelReg::initEllipse( double radius, double x, double y)
{
	cout << "initEllipse: radius = " << radius << ", x = " << x << ", y = " << y << endl;
	_radius = radius;
	_x = x;
	_y = y;
}

bool
ModelReg::Register()
{
	//
	//	First we instantiate the GroupSpatialObject and EllipseSpatialObject. 
	//	These two objects are parameterized by the dimension of the space. 
	//	In our current example a 2D instantiation is created.
	//
	typedef itk::GroupSpatialObject<2>	 	GroupType;

	//
	//	The image is instantiated in the following lines using the pixel
	//	type and the space dimension. This image uses a float pixel
	//	type since we plan to blur it in order to increase the capture radius of
	//	the optimizer. Images of real pixel type behave better under blurring
	//	than those of integer pixel type.
	//
	typedef itk::Image<float, 2>		ImageType;

	//
	//	Here is where the fun begins! In the following lines we create the
	//	EllipseSpatialObjects using their New() methods, and
	//	assigning the results to SmartPointers. These lines will create
	//	three ellipses.
	//
	
#ifdef HAVE_IMAGE
	EllipseType::Pointer ellipse1 = EllipseType::New();
	ellipse1->SetRadius( _radius);
	EllipseType::TransformType::OffsetType offset;
	offset[ 0] = _x;
	offset[ 1] = _y;

	ellipse1->GetObjectToParentTransform()->SetOffset(offset);
	ellipse1->ComputeObjectToWorldTransform();
//	cout << "ellipse IndexToObject Matrix:" << endl;
//	cout << ellipse1->GetIndexToObjectTransform()->GetMatrix() << endl;
//	cout << "ellipse IndexToWorld Matrix:" << endl;
//	cout << ellipse1->GetIndexToWorldTransform()->GetMatrix() << endl;
//	cout << "ellipse ObjectToParent Matrix:" << endl;
//	cout << ellipse1->GetObjectToParentTransform()->GetMatrix() << endl;
//	All were 2d identity.	
#else
	EllipseType::Pointer ellipse1 = EllipseType::New();
	EllipseType::Pointer ellipse2 = EllipseType::New();
	EllipseType::Pointer ellipse3 = EllipseType::New();
	//
	//	Every class deriving from SpatialObject has particular
	//	parameters enabling the user to tailor its shape. In the case of the
	//	EllipseSpatialObject, SetRadius() is used to
	//	define the ellipse size. An additional SetRadius(Array) method
	//	allows the user to define the ellipse axes independently.
	//
	ellipse1->SetRadius( 10.0);
	ellipse2->SetRadius( 10.0);
	ellipse3->SetRadius( 10.0);
#endif
	//
	//	The ellipses are created centered in space by default. We use the
	//	following lines of code to arrange the ellipses in a triangle.
	//	The spatial transform intrinsically associated with the object is
	//	accessed by the GetTransform() method. This transform can define
	//	a translation in space with the SetOffset() method.	We take
	//	advantage of this feature to place the ellipses at particular
	//	points in space.
	//

	// Place each ellipse at the right position to form a triangle
#ifndef HAVE_IMAGE
	EllipseType::TransformType::OffsetType offset;
	offset[ 0 ] = 100.0;
	offset[ 1 ] =  40.0;

	ellipse1->GetObjectToParentTransform()->SetOffset(offset);
	ellipse1->ComputeObjectToWorldTransform();

	offset[ 0] = 40.0;
	offset[ 1] = 150.0;
	ellipse2->GetObjectToParentTransform()->SetOffset(offset);
	ellipse2->ComputeObjectToWorldTransform();

	offset[ 0] = 150.0;
	offset[ 1] = 150.0;
	ellipse3->GetObjectToParentTransform()->SetOffset(offset);
	ellipse3->ComputeObjectToWorldTransform();
#endif
	//
	//	Note that after a change has been made in the transform, the
	//	SpatialObject invokes the method
	//	ComputeGlobalTransform() in order to update its global
	//	transform.	The reason for doing this is that SpatialObjects
	//	can be arranged in hierarchies. It is then possible to change the
	//	position of a set of spatial objects by moving the parent of the group.
	//

	//	Now we add the three EllipseSpatialObjects to a
	//	GroupSpatialObject that will be subsequently passed on to the
	//	registration method. The GroupSpatialObject facilitates the
	//	management of the three ellipses as a higher level structure
	//	representing a complex shape. Groups can be nested any number of levels
	//	in order to represent shapes with higher detail.
	//
	GroupType::Pointer group = GroupType::New();
	group->AddSpatialObject( ellipse1 );
#ifndef HAVE_IMAGE
	group->AddSpatialObject( ellipse2);
	group->AddSpatialObject( ellipse3);

	//
	//	Having the geometric model ready, we proceed to generate the binary
	//	image representing the imprint of the space occupied by the ellipses.
	//	The SpatialObjectToImageFilter is used to that end. Note that
	//	this filter is instantiated over the spatial object used and the image
	//	type to be generated.
	//
	typedef itk::SpatialObjectToImageFilter< GroupType, ImageType>
		SpatialObjectToImageFilterType;

	//
	//	With the defined type, we construct a filter using the New()
	//	method. The newly created filter is assigned to a SmartPointer.
	//
	SpatialObjectToImageFilterType::Pointer imageFilter =
		SpatialObjectToImageFilterType::New();

	//
	//	The GroupSpatialObject is passed as input to the filter.
	//
	imageFilter->SetInput( group);

	//
	//	The itk::SpatialObjectToImageFilter acts as a resampling filter.
	//	Therefore it requires the user to define the size of the desired output
	//	image. This is specified with the SetSize() method.
	//
	ImageType::SizeType size;
	size[ 0] = 200;
	size[ 1] = 200;
	imageFilter->SetSize( size);

	//
	//	Finally we trigger the execution of the filter by calling the
	//	Update() method.
	//
	imageFilter->Update();
#endif
	//
	//	In order to obtain a smoother metric, we blur the image using a
	//	itk::DiscreteGaussianImageFilter. This extends the capture radius
	//	of the metric and produce a more continuous cost function to
	//	optimize. The following lines instantiate the Gaussian filter and
	//	create one object of this type using the New() method.
	//
	typedef itk::DiscreteGaussianImageFilter< ImageType, ImageType> GaussianFilterType;
	GaussianFilterType::Pointer	gaussianFilter = GaussianFilterType::New();

	//
	//	The output of the SpatialObjectToImageFilter is connected as
	//	input to the DiscreteGaussianImageFilter.
	//
#ifdef HAVE_IMAGE
	gaussianFilter->SetInput( image()->GetSlice(0));
	cout << "Min = " << image()->GetImageMin() << ", Max = " <<  image()->GetImageMax()
		<< endl;
#else
	gaussianFilter->SetInput( imageFilter->GetOutput());
	typedef itk::MinimumMaximumImageCalculator<ImageType> MinMaxCalculatorType;
	typedef itk::SmartPointer<MinMaxCalculatorType> MinMaxCalculatorPointer;
	MinMaxCalculatorPointer MinMaxCalc = MinMaxCalculatorType::New();
	MinMaxCalc->SetImage( imageFilter->GetOutput());
	MinMaxCalc->Compute();
	cout << "Min = " << MinMaxCalc->GetMinimum() << ", Max = " <<  MinMaxCalc->GetMaximum()
		<< endl;
#endif

	
	//
	//	The variance of the filter is defined as a large value in order to
	//	increase the capture radius. Finally the execution of the filter is
	//	triggered using the Update() method.
	//
	const double variance = 20;
	gaussianFilter->SetVariance(variance);
	gaussianFilter->Update();
	
	//
	//	Below we instantiate the type of the
	//	itk::ImageToSpatialObjectRegistrationMethod method and instantiate a
	//	registration object with the New() method. Note that the
	//	registration type is templated over the Image and the
	//	SpatialObject types. The spatial object in this case is the
	//	group of spatial objects.
	//
	typedef itk::ImageToSpatialObjectRegistrationMethod< ImageType, GroupType>
		RegistrationType;
	RegistrationType::Pointer registration = RegistrationType::New();

	//
	//	Now we instantiate the metric that is templated over
	//	the image type and the spatial object type. As usual, the New()
	//	method is used to create an object.
	//
	typedef SimpleImageToSpatialObjectMetric< ImageType, GroupType> MetricType;
	MetricType::Pointer metric = MetricType::New();

	//
	//	An interpolator will be needed to evaluate the image at non-grid
	//	positions. Here we instantiate a linear interpolator type.
	//
	typedef itk::LinearInterpolateImageFunction< ImageType, double> InterpolatorType;
	InterpolatorType::Pointer interpolator = InterpolatorType::New();

	//
	//	The following lines instantiate the evolutionary optimizer.
	//
	typedef itk::OnePlusOneEvolutionaryOptimizer OptimizerType;
	OptimizerType::Pointer optimizer = OptimizerType::New();

	//
	//	Next, we instantiate the transform class. In this case we use the
	//	Euler2DTransform that implements a rigid transform in 2D space.
	//
	typedef itk::Euler2DTransform<> TransformType;
	TransformType::Pointer transform = TransformType::New();

	//
	//	Evolutionary algorithms are based on testing random variations
	//	of parameters. In order to support the computation of random values,
	//	ITK provides a family of random number generators. In this example, we
	//	use the itk::NormalVariateGenerator which generates values with a
	//	normal distribution.
	//
	itk::Statistics::NormalVariateGenerator::Pointer generator = 
		itk::Statistics::NormalVariateGenerator::New();

	//
	//	The random number generator must be initialized with a seed.
	//
	generator->Initialize(12345);

	//
	//	The OnePlusOneEvolutionaryOptimizer is initialized by
	//	specifying the random number generator, the number of samples for the
	//	initial population and the maximum number of iterations.
	//
	optimizer->SetNormalVariateGenerator( generator);
	optimizer->Initialize( 10);
	optimizer->SetMaximumIteration( 400);

	//
	//	As in previous registration examples, we take care to normalize the dynamic 
	//	range of the different transform parameters. In particular, we must compensate 
	//	for the ranges of the angle and translations of the Euler2DTransform.
	//	In order to achieve this goal, we provide an array of scales to the optimizer.
	//
	TransformType::ParametersType parametersScale;
	parametersScale.set_size(3);
	parametersScale[0] = 1000;		// angle scale

	for ( unsigned int i=1; i<3; i++)
	{
		parametersScale[i] = 2;		// offset scale
	}
	optimizer->SetScales( parametersScale);

	//
	//	Here we instantiate the Command object that will act as an
	//	observer of the registration method and print out parameters at each
	//	iteration. Earlier, we defined this command as a class templated over the
	//	optimizer type. Once it is created with the New() method, we
	//	connect the optimizer to the command.
	//
	typedef IterationCallback< OptimizerType>	IterationCallbackType;
	IterationCallbackType::Pointer callback = IterationCallbackType::New();
	callback->SetOptimizer( optimizer);

	//
	//	All the components are plugged into the
	//	ImageToSpatialObjectRegistrationMethod object. The typical
	//	Set() methods are used here. Note the use of the
	//	SetMovingSpatialObject() method for connecting the spatial object.
	//	We provide the blurred version of the original synthetic binary
	//	image as the input image.
	//
	registration->SetFixedImage( gaussianFilter->GetOutput());
	registration->SetMovingSpatialObject( group);
	registration->SetTransform( transform);
	registration->SetInterpolator( interpolator);
	registration->SetOptimizer( optimizer);
	registration->SetMetric( metric);

	//
	//	The initial set of transform parameters is passed to the registration
	//	method using the SetInitialTransformParameters() method. Note that
	//	since our original model is already registered with the synthetic image,
	//	we introduce an artificial mis-registration in order to initialize
	//	the optimization at some point away from the optimal value.
	//
	TransformType::ParametersType initialParameters( transform->GetNumberOfParameters());

#ifdef HAVE_IMAGE
	initialParameters[0] = 0.0;	 // Angle
	initialParameters[1] = 0.0;	 // Offset X
	initialParameters[2] = 0.0;	 // Offset Y
#else
	initialParameters[0] = 0.2;	 // Angle
	initialParameters[1] = 7.0;	 // Offset X
	initialParameters[2] = 6.0;	 // Offset Y
#endif
	registration->SetInitialTransformParameters( initialParameters);

	cout << "Initial Parameters	: " << initialParameters << endl;

	//
	//	Due to the character of the metric used to evaluate the fitness
	//	between the spatial object and the image, we must tell the optimizer that
	//	we are interested in finding the maximum value of the metric. Some
	//	metrics associate low numeric values with good matching, while others associate
	//	high numeric values with good matching. The MaximizeOn() and
	//	MaximizeOff() methods allow the user to deal with both types of
	//	metrics.
	//
//	optimizer->MaximizeOn();
	optimizer->MaximizeOff();

	//
	//	Finally, we trigger the execution of the registration process with the
	//	Update() method. We place this call in a
	//	try/catch block in case any exception is thrown during the
	//	process.
	//
	try
	{
		registration->Update();
		cout << "Optimizer stop condition: "
			<< registration->GetOptimizer()->GetStopConditionDescription() << endl;
	}
	catch( itk::ExceptionObject & exp)
	{
		cerr << "Exception caught ! " << endl;
		cerr << exp << endl;
	}

	//
	//	The set of transform parameters resulting from the registration can be
	//	recovered with the GetLastTransformParameters() method. This
	//	method returns the array of transform parameters that should be
	//	interpreted according to the implementation of each transform. In our
	//	current example, the Euler2DTransform has three parameters:
	//	the rotation angle, the translation in x and the translation in y.
	//
	RegistrationType::ParametersType finalParameters
		= registration->GetLastTransformParameters();

	cout << "Final Solution is : " << finalParameters << endl;
	return true;	
}
