//
//  SegAdvectionFieldImageFilter.cc
//  
//  22-dec-11  bhb  From ItkSnap SNAPAdvectionFieldImageFilter.cxx
//  Modified:
//
#include "itkGradientImageFilter.h"
#include "itkMultiplyImageFilter.h"

template<class TInputImage, class TOutputValueType>
SegAdvectionFieldImageFilter<TInputImage,TOutputValueType>
::SegAdvectionFieldImageFilter()
{
  m_Exponent = 0;
}

template<class TInputImage, class TOutputValueType>
void
SegAdvectionFieldImageFilter<TInputImage,TOutputValueType>
::GenerateData()
{
  // Get the input and output pointers
  typename InputImageType::ConstPointer imgInput = this->GetInput();
  typename OutputImageType::Pointer imgOutput = this->GetOutput();

  // Allocate the output image
  imgOutput->SetBufferedRegion(imgOutput->GetRequestedRegion());
  imgOutput->Allocate();

  // Create a new gradient filter
  typedef itk::GradientImageFilter<
    InputImageType,TOutputValueType,TOutputValueType> GradientFilter;
  typename GradientFilter::Pointer fltGradient = GradientFilter::New();
  fltGradient->SetInput(imgInput);
  fltGradient->ReleaseDataFlagOn();

  // A pointer to the pipeline tail
  typename itk::ImageSource<OutputImageType>::Pointer 
    fltPipeEnd = fltGradient.GetPointer();
  
  // Attach the appropriate number of multiplicative filters
  typedef itk::MultiplyImageFilter<
    OutputImageType,InputImageType,OutputImageType> MultiplyFilter;
  
  for(unsigned int i=0;i<m_Exponent;i++)
    {
    typename MultiplyFilter::Pointer fltMulti = MultiplyFilter::New();
    fltMulti->SetInput1(fltPipeEnd->GetOutput());
    fltMulti->SetInput2(imgInput);
    fltMulti->ReleaseDataFlagOn();
    fltPipeEnd = fltMulti;
    }
  
  // Call the filter's GenerateData()
  fltPipeEnd->GraftOutput(imgOutput);
  fltPipeEnd->Update();

  // graft the mini-pipeline output back onto this filter's output.
  // this is needed to get the appropriate regions passed back.
  this->GraftOutput( fltPipeEnd->GetOutput() );
}

template<class TInputImage, class TOutputValueType>
void
SegAdvectionFieldImageFilter<TInputImage,TOutputValueType>
::PrintSelf(std::ostream& os, itk::Indent indent) const
{
  Superclass::PrintSelf(os,indent);
}
