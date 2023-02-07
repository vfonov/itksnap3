/*=========================================================================

  Program:   ITK-SNAP
  Module:    $RCSfile: GenericImageData.cxx,v $
  Language:  C++
  Date:      $Date: 2010/06/28 18:45:08 $
  Version:   $Revision: 1.14 $
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
// ITK Includes
#include "itkImage.h"
#include "itkImageIterator.h"
#include "RLEImageRegionIterator.h"
#include "itkImageRegionConstIterator.h"
#include "itkImageRegionIteratorWithIndex.h"
#include "itkMinimumMaximumImageCalculator.h"
#include "itkUnaryFunctorImageFilter.h"
#include "itkRGBAPixel.h"
#include "IRISSlicer.h"
#include "IRISException.h"
#include "IRISApplication.h"
#include <algorithm>
#include <list>
#include <map>
#include <iostream>
#include "SNAPEventListenerCallbacks.h"
#include "GenericImageData.h"
#include "Rebroadcaster.h"
#include "LayerIterator.h"
#include "GuidedNativeImageIO.h"
#include "ImageAnnotationData.h"
#include "RLERegionOfInterestImageFilter.h"
#include "TimePointProperties.h"
#include "ImageMeshLayers.h"

// System includes
#include <fstream>
#include <iostream>
#include <iomanip>


GenericImageData
::GenericImageData()
{
  // Make main image wrapper point to grey wrapper initially
  m_MainImageWrapper = NULL;

  // Add to the relevant lists
  m_Wrappers[MAIN_ROLE].push_back(m_MainImageWrapper);

  // Create empty annotations
  m_Annotations = ImageAnnotationData::New();

  // Create TimePointProperties
  m_TimePointProperties = TimePointProperties::New();
  m_TimePointProperties->SetParent(this);
	Rebroadcaster::Rebroadcast(m_TimePointProperties, WrapperGlobalMetadataChangeEvent(),
														 this, WrapperGlobalMetadataChangeEvent());

  // Initialize the display viewport geometry objects
  m_DisplayViewportGeometry[0] = ImageBaseType::New();
  m_DisplayViewportGeometry[1] = ImageBaseType::New();
  m_DisplayViewportGeometry[2] = ImageBaseType::New();
}

GenericImageData
::~GenericImageData()
{
  UnloadMainImage();
}

Vector3d 
GenericImageData
::GetImageSpacing() 
{
  assert(m_MainImageWrapper->IsInitialized());
  return m_MainImageWrapper->GetImageBase()->GetSpacing().GetVnlVector();
}

Vector3d 
GenericImageData
::GetImageOrigin() 
{
  assert(m_MainImageWrapper->IsInitialized());
  return m_MainImageWrapper->GetImageBase()->GetOrigin().GetVnlVector();
}


void
GenericImageData
::SetMainImageInternal(ImageWrapperBase *wrapper)
{
  // Clear the nickname counters and assign a the default nickname
  m_NicknameCounter.clear();
  wrapper->SetDefaultNickname(this->GenerateNickname(MAIN_ROLE));

  // Make the wrapper the main image
  SetSingleImageWrapper(MAIN_ROLE, wrapper);
  m_MainImageWrapper = wrapper;

  // Reset the segmentation state to a single empty image
  this->ResetSegmentations();

  // Set opaque
  m_MainImageWrapper->SetAlpha(255);

}

#include "itkIdentityTransform.h"

SmartPtr<ImageWrapperBase>
GenericImageData::CreateAnatomicWrapper(GuidedNativeImageIO *io, ITKTransformType *transform)
{
  // The output wrapper
  SmartPtr<ImageWrapperBase> out_wrapper;

  // If the transform is not NULL, the reference space must be specified
  ImageBaseType *refSpace = (transform) ? this->GetMain()->GetImageBase() : NULL;

  // Split depending on whether the image is scalar or vector
  if(io->GetNumberOfComponentsInNativeImage() > 1)
    {
    // The image will be cast to a vector anatomic image
    typedef AnatomicImageWrapper::Image4DType AnatomicImage4DType;

    // Rescale the image to desired number of bits
    RescaleNativeImageToIntegralType<AnatomicImage4DType> rescaler;
    AnatomicImage4DType::Pointer image = rescaler(io);

    // Create a mapper to native intensity
    LinearInternalToNativeIntensityMapping mapper(
          rescaler.GetNativeScale(), rescaler.GetNativeShift());

    // Create a main wrapper of fixed type.
    SmartPtr<AnatomicImageWrapper> wrapper = AnatomicImageWrapper::New();

    // Set properties
    wrapper->SetDisplayGeometry(m_DisplayGeometry);
    wrapper->SetImage4D(image, refSpace, transform);
    wrapper->SetNativeMapping(mapper);
    for(int i = 0; i < 3; i++)
      wrapper->SetDisplayViewportGeometry(i, m_DisplayViewportGeometry[i]);

    out_wrapper = wrapper.GetPointer();
    }

  else
    {
    // Rescale the image to desired number of bits
    typedef AnatomicScalarImageWrapper::Image4DType AnatomicImage4DType;

    // Rescale the image to desired number of bits
    RescaleNativeImageToIntegralType<AnatomicImage4DType> rescaler;
    AnatomicImage4DType::Pointer image = rescaler(io);

    // Create a mapper to native intensity
    LinearInternalToNativeIntensityMapping mapper(
          rescaler.GetNativeScale(), rescaler.GetNativeShift());

    // Create a main wrapper of fixed type.
    SmartPtr<AnatomicScalarImageWrapper> wrapper = AnatomicScalarImageWrapper::New();

    // Set properties
    wrapper->SetDisplayGeometry(m_DisplayGeometry);
    wrapper->SetImage4D(image, refSpace, transform);
    wrapper->SetNativeMapping(mapper);

    for(int i = 0; i < 3; i++)
      wrapper->SetDisplayViewportGeometry(i, m_DisplayViewportGeometry[i]);

    out_wrapper = wrapper.GetPointer();
    }

  // Create an image coordinate geometry object
  return out_wrapper;
}

void GenericImageData::SetMainImage(GuidedNativeImageIO *io)
{
  // Create the wrapper from the Native IO (the wrapper will either be a scalar
  // or a vector-valued image, depending on the number of components)
  SmartPtr<ImageWrapperBase> wrapper = this->CreateAnatomicWrapper(io, NULL);

  // Assign this wrapper to the main image
  this->SetMainImageInternal(wrapper);
}

void
GenericImageData
::UnloadMainImage()
{
  // First unload the overlays if exist
  UnloadOverlays();

  // Clear the main image wrappers
  RemoveSingleImageWrapper(MAIN_ROLE);
  m_MainImageWrapper = NULL;

  // Unload all the segmentations
  this->RemoveAllWrappers(LABEL_ROLE);

  // Unload all the meshes
  m_MeshLayers->Unload();

  // Clear the annotations
  m_Annotations->Reset();

  // Clear timepoint properties
  m_TimePointProperties->Reset();
}



void
GenericImageData
::AddOverlay(GuidedNativeImageIO *io)
{
  // Create the wrapper from the Native IO (the wrapper will either be a scalar
  // or a vector-valued image, depending on the number of components)
  SmartPtr<ImageWrapperBase> wrapper = this->CreateAnatomicWrapper(io, NULL);

  // Assign this wrapper to the main image
  this->AddOverlayInternal(wrapper);
}

void
GenericImageData
::AddCoregOverlay(GuidedNativeImageIO *io, ITKTransformType *transform)
{
  // Create the wrapper from the Native IO (the wrapper will either be a scalar
  // or a vector-valued image, depending on the number of components)
  SmartPtr<ImageWrapperBase> wrapper = this->CreateAnatomicWrapper(io, transform);

  // Assign this wrapper to the main image
  this->AddOverlayInternal(wrapper, false);
}

void
GenericImageData
::AddOverlayInternal(ImageWrapperBase *overlay, bool checkSpace)
{
  // Check that the image matches the size of the main image
  if(checkSpace && m_MainImageWrapper->GetBufferedRegion() != overlay->GetBufferedRegion())
    {
    throw IRISException("Main and overlay data sizes are different");
    }

  // Pass the image to a Grey image wrapper
  overlay->SetAlpha(0.5);
  overlay->SetDefaultNickname(this->GenerateNickname(OVERLAY_ROLE));

  // Sync up spacing between the main and overlay image
  if(checkSpace)
    overlay->CopyImageCoordinateTransform(m_MainImageWrapper);

  // Add to the overlay wrapper list
  PushBackImageWrapper(OVERLAY_ROLE, overlay);
}

void
GenericImageData
::UnloadOverlays()
{
  while (m_Wrappers[OVERLAY_ROLE].size() > 0)
    UnloadOverlayLast();
}

void
GenericImageData
::UnloadOverlayLast()
{
  // Make sure at least one grey overlay is loaded
  if (!AreOverlaysLoaded())
    return;

  // Release the data associated with the last overlay
  PopBackImageWrapper(OVERLAY_ROLE);
}

void GenericImageData
::UnloadOverlay(ImageWrapperBase *overlay)
{
  this->RemoveImageWrapper(OVERLAY_ROLE, overlay);
}

void
GenericImageData
::UnloadMeshLayer(unsigned long id)
{
  m_MeshLayers->RemoveLayer(id);
}

SmartPtr<GenericImageData::LabelImage4DType>
GenericImageData
::CompressSegmentation(GuidedNativeImageIO *io)
{
  // Check that the image matches the size of the grey image
  itkAssertOrThrowMacro(
        m_MainImageWrapper->IsInitialized(),
        "Main image not initialized in GenericImageData::CompressSegmentation");

  // This is the uncompressed representation of the segmentation
  typedef itk::Image<LabelType, 4> UncompressedImage4DType;

  // Cast the native to label type
  CastNativeImage<UncompressedImage4DType> caster;
  UncompressedImage4DType::Pointer imgUncompressed = caster(io);

  //use specialized RoI filter to convert to RLEImage
  typedef LabelImageWrapper::Image4DType LabelImage4DType;
  typedef itk::RegionOfInterestImageFilter<UncompressedImage4DType, LabelImage4DType> inConverterType;
  inConverterType::Pointer inConv = inConverterType::New();
  inConv->SetInput(imgUncompressed);
  inConv->SetRegionOfInterest(imgUncompressed->GetLargestPossibleRegion());
  inConv->Update();
  LabelImage4DType::Pointer imgLabel = inConv->GetOutput();
  imgUncompressed = NULL; //deallocate intermediate image to save memory

  // Disconnect from the pipeline right away
  imgLabel->DisconnectPipeline();

  // The header of the label image is made to match that of the grey image
  imgLabel->SetOrigin(this->GetMain()->GetImage4DBase()->GetOrigin());
  imgLabel->SetSpacing(this->GetMain()->GetImage4DBase()->GetSpacing());
  imgLabel->SetDirection(this->GetMain()->GetImage4DBase()->GetDirection());

  // Return the image
  return imgLabel;
}

void
GenericImageData
::ConfigureSegmentationFromMainImage(LabelImageWrapper *wrapper)
{
  // Main Image should exist
  assert(m_MainImageWrapper->IsInitialized());

  // Segmentation and Main Image should have same slice index
  wrapper->SetSliceIndex(m_MainImageWrapper->GetSliceIndex());

  // Set default nickname
  wrapper->SetDefaultNickname(this->GenerateNickname(LABEL_ROLE));

  // Send the color table to the new wrapper
  wrapper->GetDisplayMapping()->SetLabelColorTable(m_Parent->GetColorLabelTable());

  // Sync up spacing between the main and label image
  wrapper->CopyImageCoordinateTransform(m_MainImageWrapper);
}

LabelImageWrapper *
GenericImageData
::SetSegmentationImage(GuidedNativeImageIO *io, bool add_to_existing)
{
  // Check that the image matches the size of the grey image
  itkAssertOrThrowMacro(
        m_MainImageWrapper->IsInitialized(),
        "Main image not initialized in GenericImageData::AddSegmentationImage");

  // Create a compressed image from the IO
  SmartPtr<LabelImage4DType> imgLabel = CompressSegmentation(io);

  // Create a new wrapper of label type
  SmartPtr<LabelImageWrapper> seg_wrapper = LabelImageWrapper::New();
  seg_wrapper->InitializeToWrapper(m_MainImageWrapper, (LabelType) 0);
  seg_wrapper->SetImage4D(imgLabel);

  // Configure the new wrapper
  this->ConfigureSegmentationFromMainImage(seg_wrapper);

  // Update filenames
  seg_wrapper->SetFileName(io->GetFileNameOfNativeImage());

  // Remove loaded segmentation unless adding to existing
  if(!add_to_existing)
    this->RemoveAllWrappers(LABEL_ROLE);

  // Add the segmentation label to the list of segmentation wrappers
  PushBackImageWrapper(LABEL_ROLE, seg_wrapper);

  // Intensity changes in the image wrapper are broadcast as segmentation events
  Rebroadcaster::Rebroadcast(seg_wrapper, WrapperImageChangeEvent(),
                             this, SegmentationChangeEvent());

  // Return the newly added wrapper
  return seg_wrapper;
}

void GenericImageData
::UpdateSegmentationTimePoint(LabelImageWrapper *wrapper, GuidedNativeImageIO *io)
{
  // Check that the image matches the size of the grey image
  itkAssertOrThrowMacro(
        m_MainImageWrapper->IsInitialized(),
        "Main image not initialized in GenericImageData::UpdateSegmentationTimePoint");

  // Create a compressed image from the IO
  SmartPtr<LabelImage4DType> imgLabel = CompressSegmentation(io);

  // Drop the dimension of the 4D image to 3D. We can reuse existing code in ImageWrapper
  // for doing this
  SmartPtr<LabelImageWrapper> temp_wrapper = LabelImageWrapper::New();
  temp_wrapper->SetImage4D(imgLabel);

  // Update the target wrapper
  // TODO: make this operation undo-able!
  wrapper->UpdateTimePoint(temp_wrapper->GetModifiableImage());
}

LabelImageWrapper *GenericImageData::AddBlankSegmentation()
{
  assert(m_MainImageWrapper->IsInitialized());

  // Initialize the segmentation data to zeros
  LabelImageWrapper::Pointer seg = LabelImageWrapper::New();
  seg->InitializeToWrapper(m_MainImageWrapper, (LabelType) 0);

  // Configure the new wrapper
  this->ConfigureSegmentationFromMainImage(seg);

  this->PushBackImageWrapper(LABEL_ROLE, seg.GetPointer());

  // Intensity changes in the image wrapper are broadcast as segmentation events
  Rebroadcaster::Rebroadcast(seg, WrapperImageChangeEvent(),
                             this, SegmentationChangeEvent());

  // Return the added wrapper
  return seg;
}

void GenericImageData
::UnloadSegmentation(ImageWrapperBase *seg)
{
  auto id = seg->GetUniqueId();

  // Erase the segmentation image
  this->RemoveImageWrapper(LABEL_ROLE, seg);

  // If it has mesh, also remove it
  if (m_MeshLayers->HasMeshForImage(id))
    {
    m_MeshLayers->RemoveLayer(m_MeshLayers->GetMeshForImage(id)->GetUniqueId());
    }

  // If main is loaded and this is the only segmentation, reset so that
  // there is a blank segmentation left
  if(this->IsMainLoaded() && this->GetNumberOfLayers(LABEL_ROLE) == 0)
    ResetSegmentations();
}

void
GenericImageData
::ResetSegmentations()
{
  // The main image must be loaded
  assert(this->IsMainLoaded());

  // Unload all segmentations
  this->RemoveAllWrappers(LABEL_ROLE);

  // Add a new blank segmentation
  this->AddBlankSegmentation();
}

bool
GenericImageData
::AreOverlaysLoaded()
{
  return (m_Wrappers[OVERLAY_ROLE].size() > 0);
}

void
GenericImageData
::SetCrosshairs(const Vector3ui &crosshairs)
{
  // Set crosshairs in all wrappers
  for(LayerIterator lit(this); !lit.IsAtEnd(); ++lit)
    if(lit.GetLayer() && lit.GetLayer()->IsInitialized())
      lit.GetLayer()->SetSliceIndex(to_itkIndex(crosshairs));
}

void
GenericImageData
::SetTimePoint(unsigned int time_point)
{
  // Set the time point in all wrappers
  for(LayerIterator lit(this); !lit.IsAtEnd(); ++lit)
    {
    if(lit.GetLayer() && lit.GetLayer()->IsInitialized())
      {
      // Time point is handled a little differently from the cursor. The
      // main image and overlays may have different number of timepoints.
      // The idea is only to allow N-N, N-1 or 1-N combinations, i.e., the
      // main image has N time points, and then overlays have either 1 or N.
      // We can simply use modulo to make this work
      unsigned int tp = time_point % lit.GetLayer()->GetNumberOfTimePoints();
      lit.GetLayer()->SetTimePointIndex(tp);
      }
    }
}

void GenericImageData::SetDisplayGeometry(const IRISDisplayGeometry &dispGeom)
{
  m_DisplayGeometry = dispGeom;
  for(LayerIterator lit(this); !lit.IsAtEnd(); ++lit)
    if(lit.GetLayer())
      {
      // Set the direction matrix in the image
      lit.GetLayer()->SetDisplayGeometry(m_DisplayGeometry);
      }
}

GenericImageData::ImageBaseType *GenericImageData::GetDisplayViewportGeometry(int index)
{
  return m_DisplayViewportGeometry[index];
}

void GenericImageData::SetDirectionMatrix(const vnl_matrix<double> &direction)
{
  for(LayerIterator lit(this); !lit.IsAtEnd(); ++lit)
    if(lit.GetLayer())
      {
      // Set the direction matrix in the image
      lit.GetLayer()->SetDirectionMatrix(direction);
      }
}

const ImageCoordinateGeometry *GenericImageData::GetImageGeometry() const
{
  assert(m_MainImageWrapper->IsInitialized());
  return m_MainImageWrapper->GetImageGeometry();
}

void GenericImageData::ClearUndoPoints()
{
  for(LayerIterator lit(this, LABEL_ROLE); !lit.IsAtEnd(); ++lit)
    {
    LabelImageWrapper *liw = dynamic_cast<LabelImageWrapper *>(lit.GetLayer());
    if(liw)
      liw->ClearUndoPointsForAllTimePoints();
    }
}

GenericImageData::RegionType
GenericImageData
::GetImageRegion() const
{
  assert(m_MainImageWrapper->IsInitialized());
  return m_MainImageWrapper->GetBufferedRegion();
}


unsigned int GenericImageData::GetNumberOfLayers(int role_filter)
{
  unsigned int n = 0;

  LayerIterator it = this->GetLayers(role_filter);
  while(!it.IsAtEnd())
    {
    n++; ++it;
    }

  return n;
}

ImageWrapperBase *
GenericImageData
::FindLayer(unsigned long unique_id, bool search_derived, int role_filter)
{
  for(LayerIterator it = this->GetLayers(role_filter); !it.IsAtEnd(); ++it)
    {
    if(it.GetLayer()->GetUniqueId() == unique_id)
      {
      return it.GetLayer();
      }
    else if(search_derived)
      {
      VectorImageWrapperBase *vec = it.GetLayerAsVector();
      if(vec)
        {
        for(int j = SCALAR_REP_COMPONENT; j < NUMBER_OF_SCALAR_REPS; j++)
          {
          int n = (j == SCALAR_REP_COMPONENT) ? vec->GetNumberOfComponents() : 1;
          for(int k = 0; k < n; k++)
            {
            ImageWrapperBase *w = vec->GetScalarRepresentation((ScalarRepresentation) j, k);
            if(w && w->GetUniqueId() == unique_id)
              return w;
            }
          }
        }
      }
    }

  return NULL;
}

std::list<ImageWrapperBase *>
GenericImageData
::FindLayersByTag(std::string tag, int role_filter)
{
  std::list<ImageWrapperBase *> retval;
  for(LayerIterator it = this->GetLayers(role_filter); !it.IsAtEnd(); ++it)
    {
    if(it.GetLayer()->GetTags().Contains(tag))
      {
      retval.push_back(it.GetLayer());
      }
    }
  return retval;
}

std::list<ImageWrapperBase *> GenericImageData::FindLayersByRole(int role_filter)
{
  std::list<ImageWrapperBase *> retval;
  for(LayerIterator it = this->GetLayers(role_filter); !it.IsAtEnd(); ++it)
    retval.push_back(it.GetLayer());
  return retval;
}


int GenericImageData::GetNumberOfOverlays()
{
  return m_Wrappers[OVERLAY_ROLE].size();
}

ImageWrapperBase *GenericImageData::GetLastOverlay()
{
  return m_Wrappers[OVERLAY_ROLE].back();
}

LabelImageWrapper *GenericImageData::GetFirstSegmentationLayer()
{
  assert(m_Wrappers[LABEL_ROLE].size() > 0);
  return dynamic_cast<LabelImageWrapper *>(m_Wrappers[LABEL_ROLE].front().GetPointer());
}

void GenericImageData::PushBackImageWrapper(LayerRole role,
                                            ImageWrapperBase *wrapper)
{
  // Append the wrapper
  m_Wrappers[role].push_back(wrapper);

  // Rebroadcast the wrapper-related events as our own events
  Rebroadcaster::RebroadcastAsSourceEvent(wrapper, WrapperChangeEvent(), this);
  
  // Fire the layer change event
  this->InvokeEvent(LayerChangeEvent());
}

void GenericImageData::PopBackImageWrapper(LayerRole role)
{
  m_Wrappers[role].pop_back();

  // Fire the layer change event
  this->InvokeEvent(LayerChangeEvent());
}

void GenericImageData::MoveLayer(ImageWrapperBase *layer, int direction)
{
  // Find the layer
  LayerIterator it(this);
  it.Find(layer);
  if(!it.IsAtEnd())
    {
    WrapperList &wl = m_Wrappers[it.GetRole()];
    int k = it.GetPositionInRole();

    // Make sure the operation is legal!
    assert(k + direction >= 0 && k + direction < wl.size());

    // Do the swap
    std::swap(wl[k], wl[k+direction]);
    }

  // Fire the layer change event
  this->InvokeEvent(LayerChangeEvent());
}

void GenericImageData::RemoveImageWrapper(LayerRole role,
                                          ImageWrapperBase *wrapper)
{
  // Erase the segmentation image
  WrapperList &wrappers = m_Wrappers[role];
  WrapperIterator it =
      std::find(wrappers.begin(), wrappers.end(), wrapper);
  if(it != wrappers.end())
    {
    auto *volume = it->GetPointer()->GetUserData("volume");
    if (volume)
      it->GetPointer()->RemoveUserData("volume");

    wrappers.erase(it);
    }

  // Fire the layer change event
  this->InvokeEvent(LayerChangeEvent());
}

void GenericImageData::SetSingleImageWrapper(LayerRole role,
                                             ImageWrapperBase *wrapper)
{
  assert(m_Wrappers[role].size() == 1);
  m_Wrappers[role].front() = wrapper;

  // Rebroadcast the wrapper-related events as our own events
  Rebroadcaster::RebroadcastAsSourceEvent(wrapper, WrapperChangeEvent(), this);

  // Fire the layer change event
  this->InvokeEvent(LayerChangeEvent());
}

void GenericImageData::RemoveSingleImageWrapper(LayerRole role)
{
  assert(m_Wrappers[role].size() == 1);
  m_Wrappers[role].front() = NULL;

  // Fire the layer change event
  this->InvokeEvent(LayerChangeEvent());
}

void GenericImageData::RemoveAllWrappers(LayerRole role)
{
  while(m_Wrappers[role].size() > 0)
    this->PopBackImageWrapper(role);
}

std::string GenericImageData::GenerateNickname(LayerRole role)
{
  // Have we generated a nickname for this role already?
  int count = 0;
  if(m_NicknameCounter.find(role) != m_NicknameCounter.end())
    count = m_NicknameCounter[role];

  // Get the basename for the nickname
  std::string name;
  switch(role)
    {
    case MAIN_ROLE:
      name = "Main Image";
      break;
    case OVERLAY_ROLE:
      name = "Additional Image";
      break;
    case LABEL_ROLE:
      name = "Segmentation Image";
      break;
    default:
      name = "Undefined";
      break;
    }

  // If the count is zero, nickname has never been generated
  if(count > 0)
    {
    std::ostringstream oss;
    oss << name << " " << count;
    name = oss.str();
    }

  // Update the counter for this kind of image
  m_NicknameCounter[role] = count + 1;

  return name;
}


void GenericImageData::AddOverlay(ImageWrapperBase *new_layer)
{
  this->AddOverlayInternal(new_layer, true);
}

ImageMeshLayers *
GenericImageData::GetMeshLayers()
{
  return m_MeshLayers;
}

