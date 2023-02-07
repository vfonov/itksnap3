/*=========================================================================

  Program:   ITK-SNAP
  Module:    $RCSfile: IRISSlicer.h,v $
  Language:  C++
  Date:      $Date: 2007/12/30 04:05:15 $
  Version:   $Revision: 1.3 $
  Copyright (c) 2007 Paul A. Yushkevich
  
  This file is part of ITK-SNAP 

  ITK-SNAP is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

  -----

  Copyright (c) 2003 Insight Software Consortium. All rights reserved.
  See ITKCopyright.txt or http://www.itk.org/HTML/Copyright.htm for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notices for more information. 

=========================================================================*/
#ifndef __IRISSlicer_h_
#define __IRISSlicer_h_

#include <ImageCoordinateTransform.h>

#include "RLEImageRegionConstIterator.h"
#include <itkImageToImageFilter.h>
#include <itkImageSliceConstIteratorWithIndex.h>
#include <itkImageRegionIteratorWithIndex.h>
#include <itkImageLinearIteratorWithIndex.h>

/**
 * \class IRISSlicer
 * \brief A slice extraction filter for 3D images.  
 *
 * This filter takes a transform
 * from image space (x=pixel, y=line, z=slice) to display space.  A slice 
 * shows either x-y, y-z, or x-z in the display space.  This filter is necessary
 * because the origin in the slice can correspond to any corner of the image, not
 * just to the origin of the image.  This filter can traverse the image in different
 * directions, accomodating different positions of the display space origin in the
 * image space.
 */
template <class TInputImage, class TOutputImage, class TPreviewImage>
class ITK_EXPORT IRISSlicer 
: public itk::ImageToImageFilter<TInputImage, TOutputImage>
{
public:
  /** Standard class typedefs. */
  typedef IRISSlicer                                                     Self;
  typedef itk::ImageToImageFilter<TInputImage, TOutputImage>       Superclass;
  typedef itk::SmartPointer<Self>                                     Pointer;
  typedef itk::SmartPointer<const Self>                          ConstPointer;

  typedef TInputImage                                          InputImageType;
  typedef typename InputImageType::ConstPointer             InputImagePointer;
  typedef typename InputImageType::PixelType                   InputPixelType;
  typedef typename InputImageType::InternalPixelType       InputComponentType;

  typedef TPreviewImage                                      PreviewImageType;
  typedef typename InputImageType::ConstPointer           PreviewImagePointer;
  typedef typename InputImageType::PixelType                 PreviewPixelType;
  typedef typename InputImageType::InternalPixelType     PreviewComponentType;

  typedef TOutputImage                                        OutputImageType;
  typedef typename OutputImageType::Pointer                OutputImagePointer;
  typedef typename OutputImageType::PixelType                 OutputPixelType;
  typedef typename OutputImageType::InternalPixelType     OutputComponentType;


  /** Method for creation through the object factory. */
  itkNewMacro(Self)
  
  /** Run-time type information (and related methods). */
  itkTypeMacro(IRISSlicer, ImageToImageFilter)

  /** Some more typedefs. */
  typedef typename InputImageType::RegionType             InputImageRegionType;
  typedef typename OutputImageType::RegionType           OutputImageRegionType;
  typedef itk::ImageSliceConstIteratorWithIndex<InputImageType>  InputIteratorType;
  typedef itk::ImageRegionIteratorWithIndex<OutputImageType>  SimpleOutputIteratorType;
  typedef itk::ImageLinearIteratorWithIndex<OutputImageType> OutputIteratorType;

  /** Set the current slice index */
  itkSetMacro(SliceIndex,unsigned int);
  itkGetMacro(SliceIndex,unsigned int);

  /** Set the image axis along which the subsequent slices lie */
  itkSetMacro(SliceDirectionImageAxis,unsigned int);
  itkGetMacro(SliceDirectionImageAxis,unsigned int);

  /** Set the image axis along which the subsequent lines in a slice lie */
  itkSetMacro(LineDirectionImageAxis,unsigned int);
  itkGetMacro(LineDirectionImageAxis,unsigned int);

  /** Set the image axis along which the subsequent pixels in a line lie */
  itkSetMacro(PixelDirectionImageAxis,unsigned int);
  itkGetMacro(PixelDirectionImageAxis,unsigned int);

  /** Set the direction of line traversal */
  itkSetMacro(LineTraverseForward,bool);
  itkGetMacro(LineTraverseForward,bool);

  /** Set the direction of pixel traversal */
  itkSetMacro(PixelTraverseForward,bool);
  itkGetMacro(PixelTraverseForward,bool);

  /** Add a second `preview' input to the slicer. The slicer will check if
    the preview input is newer than the main input, and if so, will obtain
    the data from the preview input. This is used in the speed preview
    framework, but could also be adapted for other features. Setting the
    preview input to NULL disables this feature. */
  void SetPreviewInput(PreviewImageType *input);

  /**
    Get the preview input.
    */
  PreviewImageType *GetPreviewInput();

  /**
   * Indicate whether the main input should always be bypassed when the preview
   * input is present. If not, the slicer will use whichever input is newer.
   */
  itkGetMacro(BypassMainInput, bool)
  itkSetMacro(BypassMainInput, bool)

protected:
  IRISSlicer();
  virtual ~IRISSlicer() {};
  void PrintSelf(std::ostream &s, itk::Indent indent) const ITK_OVERRIDE;

  /** 
   * IRISSlicer can produce an image which is a different
   * resolution than its input image.  As such, IRISSlicer
   * needs to provide an implementation for
   * GenerateOutputInformation() in order to inform the pipeline
   * execution model.  The original documentation of this method is
   * below.
   *
   * \sa ProcessObject::GenerateOutputInformaton()  */
  virtual void GenerateOutputInformation() ITK_OVERRIDE;

  void GenerateInputRequestedRegion() ITK_OVERRIDE;

  /**
   * This method maps an input region to an output region
   */
  virtual void CallCopyOutputRegionToInputRegion(InputImageRegionType &destRegion,
                              const OutputImageRegionType &srcRegion) ITK_OVERRIDE;

  /** 
   * IRISSlicer is not implemented as a multithreaded filter.
   * \sa ImageToImageFilter::GenerateData()  
   */
  virtual void GenerateData() ITK_OVERRIDE;

  template <class TSourceImage> void DoGenerateData(const TSourceImage *source);

private:
  IRISSlicer(const Self&); //purposely not implemented
  void operator=(const Self&); //purposely not implemented

  // Current slice in each of the dimensions
  unsigned int m_SliceIndex;

  // Image axis corresponding to the slice direction
  unsigned int m_SliceDirectionImageAxis;

  // Image axis corresponding to the line direction
  unsigned int m_LineDirectionImageAxis;
  
  // Image axis corresponding to the pixel direction  
  unsigned int m_PixelDirectionImageAxis;

  // Whether the line direction is reversed
  bool m_LineTraverseForward;

  // Whether the pixel direction is reversed
  bool m_PixelTraverseForward;

  // Whether the main input should always be bypassed
  bool m_BypassMainInput;
  
  // The worker methods in this filter
  // void CopySliceLineForwardPixelForward(InputIteratorType, OutputImageType *);
  // void CopySliceLineForwardPixelBackward(InputIteratorType, OutputImageType *);
  // void CopySliceLineBackwardPixelForward(InputIteratorType, OutputImageType *);
  // void CopySliceLineBackwardPixelBackward(InputIteratorType, OutputImageType *);
};

//specialization for run-length encoded image
template< typename TPixel, typename CounterType, class TOutputImage, class TPreviewImage>
class ITK_EXPORT IRISSlicer<RLEImage<TPixel, 3, CounterType>, TOutputImage, TPreviewImage >
    : public itk::ImageToImageFilter<RLEImage<TPixel, 3, CounterType>, TOutputImage>
{
public:
  /** Standard class typedefs. */
  typedef IRISSlicer                                                     Self;
  typedef RLEImage<TPixel, 3, CounterType>                     InputImageType;
  typedef itk::ImageToImageFilter<InputImageType, TOutputImage>    Superclass;
  typedef itk::SmartPointer<Self>                                     Pointer;
  typedef itk::SmartPointer<const Self>                          ConstPointer;

  typedef typename InputImageType::ConstPointer             InputImagePointer;
  typedef typename InputImageType::PixelType                   InputPixelType;
  typedef typename InputImageType::InternalPixelType       InputComponentType;

  typedef TPreviewImage                                      PreviewImageType;
  typedef typename PreviewImageType::ConstPointer         PreviewImagePointer;
  typedef typename PreviewImageType::PixelType               PreviewPixelType;
  typedef typename PreviewImageType::InternalPixelType   PreviewComponentType;

  typedef TOutputImage                                        OutputImageType;
  typedef typename OutputImageType::Pointer                OutputImagePointer;
  typedef typename OutputImageType::PixelType                 OutputPixelType;
  typedef typename OutputImageType::InternalPixelType     OutputComponentType;

  /** Method for creation through the object factory. */
  itkNewMacro(Self)

  /** Run-time type information (and related methods). */
  itkTypeMacro(IRISSlicer, ImageToImageFilter)

  /** Some more typedefs. */
  typedef typename InputImageType::RegionType             InputImageRegionType;
  typedef typename OutputImageType::RegionType           OutputImageRegionType;
  typedef itk::ImageSliceConstIteratorWithIndex<InputImageType>  InputIteratorType;
  typedef itk::ImageRegionIteratorWithIndex<OutputImageType>  SimpleOutputIteratorType;
  typedef itk::ImageLinearIteratorWithIndex<OutputImageType> OutputIteratorType;

  /** Set the current slice index */
  itkSetMacro(SliceIndex, unsigned int);
  itkGetMacro(SliceIndex, unsigned int);

  /** Set the image axis along which the subsequent slices lie */
  itkSetMacro(SliceDirectionImageAxis, unsigned int);
  itkGetMacro(SliceDirectionImageAxis, unsigned int);

  /** Set the image axis along which the subsequent lines in a slice lie */
  itkSetMacro(LineDirectionImageAxis, unsigned int);
  itkGetMacro(LineDirectionImageAxis, unsigned int);

  /** Set the image axis along which the subsequent pixels in a line lie */
  itkSetMacro(PixelDirectionImageAxis, unsigned int);
  itkGetMacro(PixelDirectionImageAxis, unsigned int);

  /** Set the direction of line traversal */
  itkSetMacro(LineTraverseForward, bool);
  itkGetMacro(LineTraverseForward, bool);

  /** Set the direction of pixel traversal */
  itkSetMacro(PixelTraverseForward, bool);
  itkGetMacro(PixelTraverseForward, bool);

  /** Add a second `preview' input to the slicer. The slicer will check if
    the preview input is newer than the main input, and if so, will obtain
    the data from the preview input. This is used in the speed preview
    framework, but could also be adapted for other features. Setting the
    preview input to NULL disables this feature. */
  void SetPreviewInput(PreviewImageType *input);

  /**
    Get the preview input.
    */
  PreviewImageType *GetPreviewInput();

  /**
     * Indicate whether the main input should always be bypassed when the preview
     * input is present. If not, the slicer will use whichever input is newer.
     */
  itkGetMacro(BypassMainInput, bool)
  itkSetMacro(BypassMainInput, bool)

protected:

  IRISSlicer();
  virtual ~IRISSlicer() {};
  void PrintSelf(std::ostream &s, itk::Indent indent) const ITK_OVERRIDE;

  /**
    * IRISSlicer can produce an image which is a different
    * resolution than its input image.  As such, IRISSlicer
    * needs to provide an implementation for
    * GenerateOutputInformation() in order to inform the pipeline
    * execution model.  The original documentation of this method is
    * below.
    *
    * \sa ProcessObject::GenerateOutputInformaton()  */
  virtual void GenerateOutputInformation() ITK_OVERRIDE;

  void GenerateInputRequestedRegion() ITK_OVERRIDE;

  /**
    * This method maps an input region to an output region
    */
  virtual void CallCopyOutputRegionToInputRegion(InputImageRegionType &destRegion,
                                                 const OutputImageRegionType &srcRegion) ITK_OVERRIDE;

  void GenerateData() ITK_OVERRIDE;

  /** Uncompresses a RLE line into a buffer pointed by out.
    * After each pixel is written, adds stride to the pointer.
    * The buffer needs to have enough room.
    * No error checking is conducted. */
  inline void uncompressLine(const typename InputImageType::RLLine & line, TPixel *out, long stride)
  {
    //complete Run-Length Lines have to be buffered
    itkAssertOrThrowMacro(this->GetInput()->GetBufferedRegion().GetSize(0)
                          == this->GetInput()->GetLargestPossibleRegion().GetSize(0),
                          "BufferedRegion must contain complete run-length lines!");
#ifdef _DEBUG
    int debugCount = 0;
#endif
    for (int x = 0; x < line.size(); x++)
      for (CounterType r = 0; r < line[x].first; r++)
        {
#ifdef _DEBUG
        debugCount++;
        assert(debugCount <= this->GetInput()->GetLargestPossibleRegion().GetSize(0));
#endif
        *out = line[x].second;
        out += stride;
        }
  }

private:
  IRISSlicer(const Self&); //purposely not implemented
  void operator=(const Self&); //purposely not implemented

  // Current slice in each of the dimensions
  unsigned int m_SliceIndex;

  // Image axis corresponding to the slice direction
  unsigned int m_SliceDirectionImageAxis;

  // Image axis corresponding to the line direction
  unsigned int m_LineDirectionImageAxis;

  // Image axis corresponding to the pixel direction
  unsigned int m_PixelDirectionImageAxis;

  // Whether the line direction is reversed
  bool m_LineTraverseForward;

  // Whether the pixel direction is reversed
  bool m_PixelTraverseForward;

  // Whether the main input should always be bypassed
  bool m_BypassMainInput;

};

#ifndef ITK_MANUAL_INSTANTIATION
#include "IRISSlicer.txx"
#include "IRISSlicer_RLE.txx"
#endif

#endif //__IRISSlicer_h_
