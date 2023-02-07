/*=========================================================================

  Program:   ITK-SNAP
  Module:    $RCSfile: SNAPLevelSetFunction.txx,v $
  Language:  C++
  Date:      $Date: 2009/09/19 14:00:16 $
  Version:   $Revision: 1.5 $
  Copyright (c) 2007 Paul A. Yushkevich
  
  This file is part of ITK-SNAP 

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  -----

  Copyright (c) 2003 Insight Software Consortium. All rights reserved.
  See ITKCopyright.txt or http://www.itk.org/HTML/Copyright.htm for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notices for more information. 

=========================================================================*/
#include "itkGradientImageFilter.h"
#include "itkUnaryFunctorImageFilter.h"
#include "itkMultiplyImageFilter.h"
#include "itkNumericTraits.h"

#include <map>

template <class TSpeedImageType, class TImageType>
SNAPLevelSetFunction<TSpeedImageType,TImageType>
::SNAPLevelSetFunction()
: Superclass()
{
  m_TimeStepFactor = 1.0f;
  
  m_CurvatureSpeedExponent = 0;
  m_AdvectionSpeedExponent = 0;
  m_PropagationSpeedExponent = 0;
  m_LaplacianSmoothingSpeedExponent = 0;
  m_UseExternalAdvectionField = false;
  m_SpeedScaleFactor = 1.0;

  m_SpeedInterpolator = SpeedImageInterpolatorType::New();
  m_AdvectionFieldInterpolator = VectorInterpolatorType::New();
  
  m_AdvectionFilter = AdvectionFilterType::New();
}

template <class TSpeedImageType, class TImageType>
SNAPLevelSetFunction<TSpeedImageType,TImageType>
::~SNAPLevelSetFunction()
{

}

template <class TSpeedImageType, class TImageType>
void 
SNAPLevelSetFunction<TSpeedImageType,TImageType>
::SetSpeedImage(SpeedImageType *pointer)
{
  m_SpeedImage = pointer;
  m_SpeedInterpolator->SetInputImage(m_SpeedImage);
  m_AdvectionFilter->SetInput(m_SpeedImage);
}

template <class TSpeedImageType, class TImageType>
void
SNAPLevelSetFunction<TSpeedImageType,TImageType>
::CalculateInternalImages()
{
  
  // There is still the business of the advection image to attend to
  // Compute \f$ \nabla g() \f$ (will be cached from run to run)
  if(!m_UseExternalAdvectionField)
    {
    assert(m_AdvectionSpeedExponent >= 0);
    m_AdvectionFilter->SetExponent((unsigned int)m_AdvectionSpeedExponent);
    m_AdvectionFilter->Update();
    m_AdvectionField = 
      reinterpret_cast< VectorImageType* > (
        m_AdvectionFilter->GetOutput());
    }

  // Set up the advection interpolator
  // if(m_AdvectionSpeedExponent != 0)
  m_AdvectionFieldInterpolator->SetInputImage(m_AdvectionField);
}

template <class TSpeedImageType, class TImageType>
typename SNAPLevelSetFunction<TSpeedImageType,TImageType>::VectorType
SNAPLevelSetFunction<TSpeedImageType,TImageType>
::AdvectionField(const NeighborhoodType &neighborhood,
                 const FloatOffsetType &offset,
                 GlobalDataStruct *) const
{
  IndexType idx = neighborhood.GetIndex();
  typename VectorInterpolatorType::ContinuousIndexType cdx;
  VectorType avec;

  for (unsigned i = 0; i < ImageDimension; ++i)
    cdx[i] = static_cast<double>(idx[i]) - offset[i];

  if ( m_AdvectionFieldInterpolator->IsInsideBuffer(cdx) )
    {
    avec = m_AdvectionFieldInterpolator->EvaluateAtContinuousIndex(cdx);
    }
  else
    {
    avec = m_AdvectionField->GetPixel(idx);
    }

  for(unsigned i = 0; i < ImageDimension; i++)
    avec[i] *= m_SpeedScaleFactor;

  return avec;
}

template <class TSpeedImageType, class TImageType>
void
SNAPLevelSetFunction<TSpeedImageType, TImageType>
::PrintSelf(std::ostream &os, itk::Indent indent) const
{
  Superclass::PrintSelf(os, indent);
}

template <class TSpeedImageType, class TImageType>
typename SNAPLevelSetFunction<TSpeedImageType, TImageType>::PixelType
SNAPLevelSetFunction<TSpeedImageType, TImageType>
::ComputeUpdate(
    const NeighborhoodType &neighborhood,
    void *globalData, const FloatOffsetType &offset)
{
  // Call the parent method
  return Superclass::ComputeUpdate(neighborhood, globalData, offset);
}

template <class TSpeedImageType, class TImageType>
typename SNAPLevelSetFunction<TSpeedImageType, TImageType>::ScalarValueType
SNAPLevelSetFunction<TSpeedImageType, TImageType>
::GetSpeedWithExponent(int exponent,
                       const NeighborhoodType &neighbourhood,
                       const FloatOffsetType &offset,
                       GlobalDataStruct *) const
{
  thread_local ScalarValueType cached_speed = 0;
  thread_local IndexType cached_speed_index;
  thread_local SpeedImageType *cached_speed_ptr = nullptr;

  IndexType idx = neighbourhood.GetIndex();

  if(cached_speed_ptr != m_SpeedImage || cached_speed_index != neighbourhood.GetIndex())
    {
    // Interpolate the speed value at this location. This way, we don't need to
    // perform interpolation each time the GetXXXSpeed() function is called.
    ContinuousIndexType cdx;
    for (unsigned i = 0; i < ImageDimension; ++i)
      cdx[i] = static_cast<double>(idx[i]) - offset[i];

    // Store the speed in thread-specific memory
    cached_speed =
        m_SpeedScaleFactor * static_cast<ScalarValueType>(
          m_SpeedInterpolator->IsInsideBuffer(cdx)
          ? m_SpeedInterpolator->EvaluateAtContinuousIndex(cdx)
          : m_SpeedImage->GetPixel(idx));

    cached_speed_index = idx;
    cached_speed_ptr = m_SpeedImage;
    }

  // Use the cached speed value
  ScalarValueType speed = cached_speed;

  // Get the speed value from thread-specific cache
  switch(exponent)
    {
    case 0 : return itk::NumericTraits<ScalarValueType>::One;
    case 1 : return speed;
    case 2 : return speed * speed;
    case 3 : return speed * speed * speed;
    default : return pow(speed, exponent);
    }
}

