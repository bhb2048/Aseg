//
//  SegLevelSetDriver.h
//  
//  22-dec-11  bhb  From ItkSnap SNAPLevelSetDriver.h
//  Modified:
//  06-jan-12  bhb  change m_Parameters to be a pointer
//  22-sep-15  bhb  add Print()
//  
#ifndef __SegLevelSetDriver_h_
#define __SegLevelSetDriver_h_

#include <ITK/Common.h>
#include "SegLevelSetFunction.h"
#include "itkCommand.h"
// #include "SegLevelSetStopAndGoFilter.h"

template <class TFilter> class LevelSetExtensionFilter;
class SegParameters;
 
namespace itk {
	template <class TInputImage, class TOutputImage> class ImageToImageFilter;
	template <class TInputImage, class TOutputImage> class FiniteDifferenceImageFilter;
	template <class TOwner> class SimpleMemberCommand;
	template <class TOwner> class MemberCommand;
	class Command;
	template <class TType, unsigned int VDimension, class TOutputImage> class PCAShapeSignedDistanceFunction;
};

/** 
 * class SegLevelSetDriverBase
 * An abstract interface that allows code to be written independently of
 * the dimensionality of the level set filter. For documentation of the methods,
 * see SegLevelSetDriver.
 */
class SegLevelSetDriverBase
{
	public:
		  virtual ~SegLevelSetDriverBase() { /*To avoid compiler warning.*/ }
		/** Set snake parameters */
		virtual void SetSegParameters(const SegParameters *parms) = 0;

		/** Run the snake for a number of iterations */
		virtual void Run(unsigned int nIterations) = 0;

		/** Restart the snake */
		virtual void Restart() = 0;

		/** Clean up the snake's state */
		virtual void CleanUp() = 0;
};

/**
 * class SegLevelSetDriver
 * A generic interface between the Aseg application and ITK level set
 * framework.
 *
 * This interface allows the Aseg code to exist independently of the way 
 * stop-and-go level set evolution is implemented in ITK.  This gives the software 
 * a bit of modularity.  As far as Aseg cares, the public methods declared in this 
 * class are the only ways to control level set evolution.
 */
template <unsigned int VDimension> 
class SegLevelSetDriver : public SegLevelSetDriverBase
{
	public:

		typedef SegLevelSetDriver Self;

		// A callback type
		typedef itk::SmartPointer<itk::Command> CommandPointer;
		typedef itk::SimpleMemberCommand<Self> SelfCommandType;
		typedef itk::SmartPointer<SelfCommandType> SelfCommandPointer;

		/** Floating point image type used internally */
		typedef itk::Image<float, VDimension> FloatImageType;
		typedef typename itk::SmartPointer<FloatImageType> FloatImagePointer;

		/** Type definition for the level set filter */
		typedef itk::FiniteDifferenceImageFilter< FloatImageType,FloatImageType> FilterType;

		/** Type definition for the level set function */
		typedef SegLevelSetFunction<FloatImageType> LevelSetFunctionType;
		typedef typename LevelSetFunctionType::VectorImageType VectorImageType;

		// Type definition for the shape function
		typedef itk::PCAShapeSignedDistanceFunction< double, VDimension, FloatImageType>
															ShapeFunctionType;
		
		/** Initialize the level set driver.  Note that the type of snake (in/out
		 * or edge) is determined entirely by the speed image and by the values
		 * of the parameters.  Moreover, the type of solver used is specified in
		 * the parameters as well. The last parameter is the optional external 
		 * advection field, that can be used instead of the default advection
		 * field that is based on the image gradient */
		SegLevelSetDriver( FloatImageType *initialLevelSet, FloatImageType *speed,
					const SegParameters *parms, VectorImageType *externalAdvection = NULL, 
					itk::Command *progressCB=NULL);

		/** Virtual destructor */
		virtual ~SegLevelSetDriver() {}

		/** Set snake parameters */
		void SetSegParameters( const SegParameters *parms);

		/** Run the filter */
		void Run(unsigned int nIterations);

		/** Restart the snake */
		void Restart();

		/** Get the level set filter */
		GetMacro( LevelSetFilter, FilterType *);

		/** Get the level set function */
		GetMacro( LevelSetFunction, LevelSetFunctionType *);

		// Get the shape function
		GetMacro( Shape, ShapeFunctionType *);
		
		/** Get the current state of the snake (level set and narrow band) */
		FloatImageType *GetCurrentState();

		/** Get the number of elapsed iterations */
		unsigned int GetElapsedIterations() const;

		/** Clean up the snake's state */
		void CleanUp();

		void Print();

	private:

		/** Level set filter wrapped by this object */
		typename FilterType::Pointer m_LevelSetFilter;

		/** Level set function used by the level set filter */
		typename LevelSetFunctionType::Pointer m_LevelSetFunction;

		typename ShapeFunctionType::Pointer m_Shape;

		/** An initialization image */
		FloatImagePointer m_InitializationImage;

		/** Last accepted snake parameters */
		const SegParameters * m_Parameters;

		/** Assign the values of snake parameters to a snake function */
		void AssignParametersToPhi(const SegParameters *parms);

		/** Internal routines */
		void DoCreateLevelSetFilter(itk::Command *progressCB=NULL);
};

// Type definitions
typedef SegLevelSetDriver<3> SegLevelSetDriver3d;
typedef SegLevelSetDriver<2> SegLevelSetDriver2d;

#include "SegLevelSetDriver.txx"

#endif // __SegLevelSetDriver_h_
