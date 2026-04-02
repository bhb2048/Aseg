//
//  SegAdvectionFieldImageFilter.h
//  
//  22-dec-11  bhb  From ItkSnap SNAPAdvectionFieldImageFilter.h
//  Modified:
//  22-nov-13  bhb  change itk::ImageToImageFilter<> def, no more GetImageDimension in ITK-4.4
//  
#ifndef __SegAdvectionFieldImageFilter_h_
#define __SegAdvectionFieldImageFilter_h_

#include "itkCovariantVector.h"
#include "itkImage.h"
#include "itkImageToImageFilter.h"

/**
 * \class SegAdvectionFieldImageFilter
 * \brief A filter used to compute the advection field in the Seg level set
 * equation. 
 */
template <class TInputImage, class TOutputValueType=float>
class SegAdvectionFieldImageFilter: public itk::ImageToImageFilter<TInputImage, 
    itk::Image< itk::CovariantVector<TOutputValueType,TInputImage::ImageDimension>,
                                        TInputImage::ImageDimension> >
{
public:
  
  /** Input image types */
  typedef TInputImage                                          InputImageType;
  typedef itk::SmartPointer<InputImageType>                 InputImagePointer;
  
  /** Standard class typedefs. */
  typedef SegAdvectionFieldImageFilter                                  Self;
  typedef itk::SmartPointer<Self>                                     Pointer;
  typedef itk::SmartPointer<const Self>                          ConstPointer;  
  

  /** Image dimension. */
  itkStaticConstMacro(ImageDimension, unsigned int,
                      TInputImage::ImageDimension);    
  

  /** Output image types */
  typedef itk::CovariantVector<
    TOutputValueType, 
    itkGetStaticConstMacro(ImageDimension)>                   VectorType;  
  typedef itk::Image<
    VectorType, 
    itkGetStaticConstMacro(ImageDimension)>              OutputImageType;
  typedef itk::SmartPointer<OutputImageType>               OutputImagePointer;
  
  
  typedef itk::ImageToImageFilter<InputImageType,OutputImageType>  Superclass;
  
  /** Method for creation through the object factory. */
  itkNewMacro(Self);

  /** Set the power of g() by which the gradient is scaled */
  itkSetMacro(Exponent,unsigned int);

  /** Get the power of g() by which the gradient is scaled */
  itkGetMacro(Exponent,unsigned int);
    
protected:

  SegAdvectionFieldImageFilter();
  virtual ~SegAdvectionFieldImageFilter() {};
  void PrintSelf(std::ostream& os, itk::Indent indent) const;
  
  /** Generate Data */
  void GenerateData( void );

private:

  /** The g-scaling exponent */
  unsigned int m_Exponent;

};

#include "SegAdvectionFieldImageFilter.txx"
#endif // __SegAdvectionFieldImageFilter_h_
