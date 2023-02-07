/*=========================================================================

  Program:   ITK-SNAP
  Module:    $RCSfile: Filename.cxx,v $
  Language:  C++
  Date:      $Date: 2010/10/18 11:25:44 $
  Version:   $Revision: 1.12 $
  Copyright (c) 2011 Paul A. Yushkevich

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

=========================================================================*/

#include <GenericSliceModel.h>
#include <GlobalUIModel.h>
#include <IRISException.h>
#include <IRISApplication.h>
#include <GenericImageData.h>
#include <SNAPAppearanceSettings.h>
#include <DisplayLayoutModel.h>
#include "DeformationGridModel.h"

#include <itkImage.h>
#include <itkImageRegionIteratorWithIndex.h>

GenericSliceModel::GenericSliceModel()
{
  // Copy parent pointers
  m_ParentUI = NULL;
  m_Driver = NULL;

  // Set the window ID
  m_Id = -1;

  // Initalize the margin
  m_Margin = 2;

  // Initialize the zoom management
  m_ManagedZoom = false;

  // The slice is not yet initialized
  m_SliceInitialized = false;

  // Viewport size reporter is NULL
  m_SizeReporter = NULL;

  // Create submodels
  m_SliceIndexModel = wrapGetterSetterPairAsProperty(
        this,
        &Self::GetSliceIndexValueAndDomain,
        &Self::SetSlideIndexValue);

  m_CurrentComponentInSelectedLayerModel =
      wrapGetterSetterPairAsProperty(
        this,
        &Self::GetCurrentComponentInSelectedLayerValueAndDomain,
        &Self::SetCurrentComponentInSelectedLayerValue);

  // Nothing is hovered over
  m_HoveredImageLayerIdModel = NewSimpleConcreteProperty(-1ul);
  m_HoveredImageIsThumbnailModel = NewSimpleConcreteProperty(false);

  m_ImageToDisplayTransform = ImageCoordinateTransform::New();
  m_DisplayToImageTransform = ImageCoordinateTransform::New();
  m_DisplayToAnatomyTransform = ImageCoordinateTransform::New();
}

void GenericSliceModel::Initialize(GlobalUIModel *model, int index)
{
  // Copy parent pointers
  m_ParentUI = model;
  m_Driver = model->GetDriver();

  // Set the window ID
  m_Id = index;

  // The slice is not yet initialized
  m_SliceInitialized = false;

  // Listen to events that require action from this object
  Rebroadcast(m_Driver, LayerChangeEvent(), ModelUpdateEvent());

  // Listen to changes in the layout of the slice view into cells. When
  // this change occurs, we have to modify the size of the slice views
  DisplayLayoutModel *dlm = m_ParentUI->GetDisplayLayoutModel();
  Rebroadcast(dlm, DisplayLayoutModel::LayerLayoutChangeEvent(), ModelUpdateEvent());

  // Listen to cursor update events and rebroadcast them for the child model
  m_SliceIndexModel->Rebroadcast(m_Driver, CursorUpdateEvent(), ValueChangedEvent());

  // Also rebroadcast the cursor change events as a model update event
  Rebroadcast(m_Driver, CursorUpdateEvent(), ModelUpdateEvent());

  // Rebroadcast our own zoom/map events as model update events
  Rebroadcast(this, SliceModelGeometryChangeEvent(), ModelUpdateEvent());

  // Also listen for changes in the selected layer
  AbstractSimpleULongProperty *selLayerModel = m_Driver->GetGlobalState()->GetSelectedLayerIdModel();
  Rebroadcast(selLayerModel, ValueChangedEvent(), ModelUpdateEvent());

  AbstractSimpleULongProperty *selSegLayerModel = m_Driver->GetGlobalState()->GetSelectedSegmentationLayerIdModel();
  Rebroadcast(selSegLayerModel, ValueChangedEvent(), ModelUpdateEvent());


  // The current component in selected layer model depends both on the selected model
  // and on the layer metadata changes
  m_CurrentComponentInSelectedLayerModel->Rebroadcast(selLayerModel, ValueChangedEvent(), DomainChangedEvent());
  m_CurrentComponentInSelectedLayerModel->Rebroadcast(m_Driver, WrapperMetadataChangeEvent(), DomainChangedEvent());
  m_CurrentComponentInSelectedLayerModel->Rebroadcast(model, LayerChangeEvent(), DomainChangedEvent());
}

void GenericSliceModel
::SetSizeReporter(ViewportSizeReporter *reporter)
{
  m_SizeReporter = reporter;

  // Rebroadcast the events from the reporter downstream to force an update
  Rebroadcast(m_SizeReporter,
              ViewportSizeReporter::ViewportResizeEvent(),
              ModelUpdateEvent());

  // We also rebroadcast as a special type of event that the slice coordinator
  // is going to listen to
  Rebroadcast(m_SizeReporter,
              ViewportSizeReporter::ViewportResizeEvent(),
              ViewportResizeEvent());
}


void GenericSliceModel::OnUpdate()
{
  // Has there been a change in the image dimensions?
  if(m_EventBucket->HasEvent(MainImageDimensionsChangeEvent()))
    {
    // Do a complete initialization
    this->InitializeSlice(m_Driver->GetCurrentImageData());
    }

  // TODO: what is the ValueChangeEvent here???
  else if(m_EventBucket->HasEvent(ViewportSizeReporter::ViewportResizeEvent())
          || m_EventBucket->HasEvent(DisplayLayoutModel::LayerLayoutChangeEvent())
          || m_EventBucket->HasEvent(ValueChangedEvent()))
    {
    // Recompute the viewport layout and dimensions
    this->UpdateViewportLayout();

    // We only react to the viewport resize if the zoom is not managed by the
    // coordinator. When zoom is managed, the coordinator will take care of
    // computing the optimal zoom and resetting the view
    if(this->IsSliceInitialized() && !m_ManagedZoom)
      {
      // Check if the zoom should be changed in response to this operation. This
      // is so if the zoom is currently equal to the optimal zoom, and there is
      // no linked zoom
      bool rezoom = (m_ViewZoom == m_OptimalZoom);

      // Just recompute the optimal zoom factor
      this->ComputeOptimalZoom();

      // Keep zoom optimal if before it was optimal
      if(rezoom)
        this->SetViewZoom(m_OptimalZoom);
      }
    }

  if(m_EventBucket->HasEvent(MainImageDimensionsChangeEvent())
     || m_EventBucket->HasEvent(ViewportSizeReporter::ViewportResizeEvent())
     || m_EventBucket->HasEvent(DisplayLayoutModel::LayerLayoutChangeEvent())
     || m_EventBucket->HasEvent(ValueChangedEvent())
     || m_EventBucket->HasEvent(CursorUpdateEvent())
     || m_EventBucket->HasEvent(SliceModelGeometryChangeEvent()))
    {
    // Viewport geometry pretty much   depends on everything!
    if(m_SliceInitialized && m_ViewZoom > 1.e-7)
      this->UpdateUpstreamViewportGeometry();
    }
}

void GenericSliceModel::ComputeOptimalZoom()
{
  // Should be fully initialized
  assert(IsSliceInitialized());

  // Compute slice size in spatial coordinates
  Vector2d worldSize(
    m_SliceSize[0] * m_SliceSpacing[0],
    m_SliceSize[1] * m_SliceSpacing[1]);

  // Set the view position (position of the center of the image?)
  m_OptimalViewPosition = worldSize * 0.5;

  // Reduce the width and height of the slice by the margin
  Vector2ui szCanvas = this->GetCanvasSize();

  // Compute the ratios of window size to slice size
  Vector2d ratios(
    szCanvas(0) / worldSize(0),
    szCanvas(1) / worldSize(1));

  // The zoom factor is the bigger of these ratios, the number of pixels
  // on the screen per millimeter in world space
  m_OptimalZoom = ratios.min_value();
}


GenericSliceModel
::~GenericSliceModel()
{
}



void
GenericSliceModel
::InitializeSlice(GenericImageData *imageData)
{
  // Store the image data pointer
  m_ImageData = imageData;

  // Quit if there is no image loaded
  if (!m_ImageData->IsMainLoaded())
    {
    m_SliceInitialized = false;
    return;
    }

  // Store the transforms between the display and image spaces
  m_ImageToDisplayTransform->SetTransform(
        imageData->GetImageGeometry()->GetImageToDisplayTransform(m_Id));
  m_DisplayToImageTransform->SetTransform(
    imageData->GetImageGeometry()->GetDisplayToImageTransform(m_Id));

  imageData->GetImageGeometry()->GetAnatomyToDisplayTransform(m_Id)->
      ComputeInverse(m_DisplayToAnatomyTransform);

  // Get the volume extents & voxel scale factors
  Vector3ui imageSizeInImageSpace = m_ImageData->GetVolumeExtents();
  Vector3d imageScalingInImageSpace = to_double(m_ImageData->GetImageSpacing());

  // Initialize quantities that depend on the image and its transform
  for(unsigned int i = 0; i < 3; i++)
    {
    // Get the direction in image space that corresponds to the i'th
    // direction in slice space
    m_ImageAxes[i] = m_DisplayToImageTransform->GetCoordinateIndexZeroBased(i);

    // Record the size and scaling of the slice
    m_SliceSize[i] = imageSizeInImageSpace[m_ImageAxes[i]];
    m_SliceSpacing[i] = imageScalingInImageSpace[m_ImageAxes[i]]; // TODO: Reverse sign by orientation?
    }

  // We have been initialized
  m_SliceInitialized = true;

  // Update the viewport dimensions
  UpdateViewportLayout();

  // Compute the optimal zoom for this slice
  ComputeOptimalZoom();

  // Set the zoom to optimal zoom for starters
  m_ViewZoom = m_OptimalZoom;

  // Fire a modified event, forcing a repaint of the window
  InvokeEvent(ModelUpdateEvent());
}

Vector2i
GenericSliceModel
::GetOptimalCanvasSize()
{
  // Compute slice size in spatial coordinates
  Vector2i optSize(
    (int) ceil(m_SliceSize[0] * m_SliceSpacing[0] * m_ViewZoom + 2 * m_Margin),
    (int) ceil(m_SliceSize[1] * m_SliceSpacing[1] * m_ViewZoom + 2 * m_Margin));

  return optSize;
}

void
GenericSliceModel
::ResetViewToFit()
{
  // Should be fully initialized
  assert(IsSliceInitialized());

  // The zoom factor is the bigger of these ratios, the number of pixels
  // on the screen per millimeter in world space
  SetViewZoom(m_OptimalZoom);
  SetViewPosition(m_OptimalViewPosition);
}

Vector3d GenericSliceModel::MapSliceToImage(const Vector3d &xSlice)
{
  assert(IsSliceInitialized());

  // Get corresponding position in display space
  return m_DisplayToImageTransform->TransformPoint(xSlice);
}

Vector3d GenericSliceModel::MapSliceToImagePhysical(const Vector3d &xSlice)
{
  Vector3d xImage = this->MapSliceToImage(xSlice);
  ImageWrapperBase *main = this->GetDriver()->GetCurrentImageData()->GetMain();
  return main->TransformVoxelCIndexToPosition(xImage);
}

/**
 * Map a point in image coordinates to slice coordinates
 */
Vector3d GenericSliceModel::MapImageToSlice(const Vector3d &xImage)
{
  assert(IsSliceInitialized());

  // Get corresponding position in display space
  return  m_ImageToDisplayTransform->TransformPoint(xImage);
}

Vector2d GenericSliceModel::MapSliceToWindow(const Vector3d &xSlice)
{
  assert(IsSliceInitialized());

  // Adjust the slice coordinates by the scaling amounts
  Vector2d uvScaled(
    xSlice(0) * m_SliceSpacing(0), xSlice(1) * m_SliceSpacing(1));

  // Compute the window coordinates
  Vector2ui size = this->GetCanvasSize();
  Vector2d uvWindow =
    m_ViewZoom * (uvScaled - m_ViewPosition) +
      Vector2d(0.5 * size[0], 0.5 * size[1]);

  // That's it, the projection matrix is set up in the scaled-slice coordinates
  return uvWindow;
}

std::pair<Vector2d, Vector2d>
GenericSliceModel::GetSliceCornersInWindowCoordinates() const
{
  std::pair<Vector2d, Vector2d> corners;

  Vector2d uv0(0, 0);
  Vector2d uv1(m_SliceSize[0] * m_SliceSpacing[0], m_SliceSize[1] * m_SliceSpacing[1]);

  Vector2ui size = this->GetCanvasSize();
  Vector2d ctr(0.5 * size[0], 0.5 * size[1]);
  corners.first = m_ViewZoom * (uv0 - m_ViewPosition) + ctr;
  corners.second = m_ViewZoom * (uv1 - m_ViewPosition) + ctr;
  return corners;
}

Vector3d GenericSliceModel::MapWindowToSlice(const Vector2d &uvWindow)
{
  assert(IsSliceInitialized() && m_ViewZoom > 0);

  // Compute the scaled slice coordinates
  Vector2ui size = this->GetCanvasSize();
  Vector2d winCenter(0.5 * size[0],0.5 * size[1]);
  Vector2d uvScaled =
    m_ViewPosition + (uvWindow - winCenter) / m_ViewZoom;

  // The window coordinates are already in the scaled-slice units
  Vector3d uvSlice(
    uvScaled(0) / m_SliceSpacing(0),
    uvScaled(1) / m_SliceSpacing(1),
    this->GetCursorPositionInSliceCoordinates()[2]);

  // Return this vector
  return uvSlice;
}

Vector3d GenericSliceModel::MapWindowOffsetToSliceOffset(const Vector2d &uvWindowOffset)
{
  assert(IsSliceInitialized() && m_ViewZoom > 0);

  Vector2d uvScaled = uvWindowOffset / m_ViewZoom;

  // The window coordinates are already in the scaled-slice units
  Vector3d uvSlice(
    uvScaled(0) / m_SliceSpacing(0),
    uvScaled(1) / m_SliceSpacing(1),
    0);

  // Return this vector
  return uvSlice;
}

Vector2d GenericSliceModel::MapSliceToPhysicalWindow(const Vector3d &xSlice)
{
  assert(IsSliceInitialized());

  // Compute the physical window coordinates
  Vector2d uvPhysical;
  uvPhysical[0] = xSlice[0] * m_SliceSpacing[0];
  uvPhysical[1] = xSlice[1] * m_SliceSpacing[1];

  return uvPhysical;
}

Vector3d GenericSliceModel::MapPhysicalWindowToSlice(const Vector2d &uvPhysical)
{
  assert(IsSliceInitialized());

  Vector3d xSlice;
  xSlice[0] = uvPhysical[0] / m_SliceSpacing[0];
  xSlice[1] = uvPhysical[1] / m_SliceSpacing[1];
  xSlice[2] = this->GetCursorPositionInSliceCoordinates()[2];

  return xSlice;
}

void
GenericSliceModel
::ResetViewPosition()
{
  // Compute slice size in spatial coordinates
  Vector2d worldSize(
    m_SliceSize[0] * m_SliceSpacing[0],
    m_SliceSize[1] * m_SliceSpacing[1]);

  // Set the view position (position of the center of the image?)
  m_ViewPosition = worldSize * 0.5;

  // Update view
  InvokeEvent(SliceModelGeometryChangeEvent());
}

void
GenericSliceModel
::SetViewPositionRelativeToCursor(Vector2d offset)
{
  // Get the crosshair position
  Vector3ui xCursorInteger = m_Driver->GetCursorPosition();

  // Shift the cursor position by by 0.5 in order to have it appear
  // between voxels
  Vector3d xCursorImage = to_double(xCursorInteger) + Vector3d(0.5);

  // Get the cursor position on the slice
  Vector3d xCursorSlice = MapImageToSlice(xCursorImage);

  // Subtract from the view position
  Vector2d vp;
  vp[0] = offset[0] + xCursorSlice[0] * m_SliceSpacing[0];
  vp[1] = offset[1] + xCursorSlice[1] * m_SliceSpacing[1];
  SetViewPosition(vp);
}

Vector2d GenericSliceModel::GetViewPositionRelativeToCursor()
{
  // Get the crosshair position
  Vector3ui xCursorInteger = m_Driver->GetCursorPosition();

  // Shift the cursor position by by 0.5 in order to have it appear
  // between voxels
  Vector3d xCursorImage = to_double(xCursorInteger) + Vector3d(0.5);

  // Get the cursor position on the slice
  Vector3d xCursorSlice = MapImageToSlice(xCursorImage);

  // Subtract from the view position
  Vector2d offset;
  offset[0] = m_ViewPosition[0] - xCursorSlice[0] * m_SliceSpacing[0];
  offset[1] = m_ViewPosition[1] - xCursorSlice[1] * m_SliceSpacing[1];

  return offset;
}

void GenericSliceModel::CenterViewOnCursor()
{
  Vector2d offset; offset.fill(0.0);
  this->SetViewPositionRelativeToCursor(offset);
}

void GenericSliceModel::SetViewZoom(double zoom)
{
  assert(zoom > 0);
  m_ViewZoom = zoom;
  this->Modified();
  this->InvokeEvent(SliceModelGeometryChangeEvent());
}

void GenericSliceModel::SetViewZoomInLogicalPixels(double zoom)
{
  this->SetViewZoom(zoom * m_SizeReporter->GetViewportPixelRatio());
}

void GenericSliceModel::ZoomInOrOut(double factor)
{
  double oldzoom = m_ViewZoom;
  double newzoom = oldzoom * factor;

  if( (oldzoom < m_OptimalZoom && newzoom > m_OptimalZoom) ||
      (oldzoom > m_OptimalZoom && newzoom < m_OptimalZoom) )
    {
    newzoom = m_OptimalZoom;
    }

  SetViewZoom(newzoom);
}

double GenericSliceModel::GetViewZoomInLogicalPixels() const
{
  double zoom_physical = this->GetViewZoom();
  return zoom_physical / m_SizeReporter->GetViewportPixelRatio();
}

/*
GenericSliceModel *
GenericSliceModel
::GenericSliceModel()
{
  SliceWindowCoordinator *swc = m_ParentUI->GetSliceCoordinator();
  return swc->GetWindow( (m_Id+1) % 3);
}
*/

bool
GenericSliceModel
::IsThumbnailOn()
{
  const GlobalDisplaySettings *gds = m_ParentUI->GetGlobalDisplaySettings();
  return gds->GetFlagDisplayZoomThumbnail() && (m_ViewZoom > m_OptimalZoom);
}

const SliceViewportLayout::SubViewport *GenericSliceModel::GetHoveredViewport()
{
  for(int i = 0; i < m_ViewportLayout.vpList.size(); i++)
    {
    const SliceViewportLayout::SubViewport *vpcand = &m_ViewportLayout.vpList[i];
    if(vpcand->layer_id == this->GetHoveredImageLayerId()
       && (vpcand->isThumbnail == this->GetHoveredImageIsThumbnail()))
      return vpcand;
    }

  return NULL;
}

Vector3d GenericSliceModel::GetCursorPositionInSliceCoordinates()
{
  Vector3ui cursorImageSpace = m_Driver->GetCursorPosition();
  Vector3d cursorDisplaySpace =
    m_ImageToDisplayTransform->TransformPoint(
      to_double(cursorImageSpace) + Vector3d(0.5));
  return cursorDisplaySpace;
}

unsigned int GenericSliceModel::GetSliceIndex()
{
  Vector3ui cursorImageSpace = m_Driver->GetCursorPosition();
  return cursorImageSpace[m_ImageAxes[2]];
}


void GenericSliceModel::UpdateSliceIndex(unsigned int newIndex)
{
  Vector3ui cursorImageSpace = m_Driver->GetCursorPosition();
  cursorImageSpace[m_ImageAxes[2]] = newIndex;
  m_Driver->SetCursorPosition(cursorImageSpace);
}

void GenericSliceModel::ComputeThumbnailProperties()
{
  // Get the global display settings
  const GlobalDisplaySettings *gds = m_ParentUI->GetGlobalDisplaySettings();

  // The thumbnail will occupy a specified fraction of the target canvas
  double xFraction = 0.01 * gds->GetZoomThumbnailSizeInPercent();

  // But it must not exceed a predefined size in pixels in either dimension
  double xThumbMax = gds->GetZoomThumbnailMaximumSize();

  // Recompute the fraction based on maximum size restriction
  Vector2ui size = this->GetCanvasSize();
  double xNewFraction = xFraction;
  if( size[0] * xNewFraction > xThumbMax )
    xNewFraction = xThumbMax * 1.0 / size[0];
  if( size[1] * xNewFraction > xThumbMax )
    xNewFraction = xThumbMax * 1.0 / size[1];

  // Set the position and size of the thumbnail, in pixels
  m_ThumbnailZoom = xNewFraction * m_OptimalZoom;
  m_ZoomThumbnailPosition.fill(5);
  m_ZoomThumbnailSize[0] =
      (int)(m_SliceSize[0] * m_SliceSpacing[0] * m_ThumbnailZoom);
  m_ZoomThumbnailSize[1] =
      (int)(m_SliceSize[1] * m_SliceSpacing[1] * m_ThumbnailZoom);
}

unsigned int GenericSliceModel::GetNumberOfSlices() const
{
  return m_SliceSize[2];
}

/*
void GenericSliceModel::OnSourceDataUpdate()
{
  this->InitializeSlice(m_Driver->GetCurrentImageData());
}
*/

void GenericSliceModel::SetViewPosition(Vector2d pos)
{
  if(m_ViewPosition != pos)
    {
    m_ViewPosition = pos;
    InvokeEvent(SliceModelGeometryChangeEvent());
    }
}

std::pair<Vector2d, Vector2d> GenericSliceModel::GetSliceCorners() const
{
  Vector2d c0(0.0, 0.0);
  Vector2d c1(m_SliceSize[0] * m_SliceSpacing[0], m_SliceSize[1] * m_SliceSpacing[1]);
  return std::make_pair(c0, c1);
}


/*
GenericSliceWindow::EventHandler
::EventHandler(GenericSliceWindow *parent)
: InteractionMode(parent->GetCanvas())
{
  m_Parent = parent;
}

void
GenericSliceWindow::EventHandler
::Register()
{
  m_Driver = m_Parent->m_Driver;
  m_ParentUI = m_Parent->m_ParentUI;
  m_GlobalState = m_Parent->m_GlobalState;
}

#include <itksys/SystemTools.hxx>

int
GenericSliceWindow::OnDragAndDrop(const FLTKEvent &event)
{
  // Check if it is a real file
  if(event.Id == FL_PASTE)
    {
    if(itksys::SystemTools::FileExists(Fl::event_text(), true))
      {
      m_ParentUI->OpenDraggedContent(Fl::event_text(), true);
      return 1;
      }
    return 0;
    }
  else
    return 1;
}
*/


unsigned int
GenericSliceModel
::MergeSliceSegmentation(itk::Image<unsigned char, 2> *drawing)
{
  // Z position of slice
  double zpos = this->GetCursorPositionInSliceCoordinates()[2];
  return m_Driver->UpdateSegmentationWithSliceDrawing(
        drawing, m_DisplayToImageTransform, zpos, "Polygon Drawing");
}

Vector2ui GenericSliceModel::GetSize()
{
  Vector2ui viewport = m_SizeReporter->GetViewportSize();
  DisplayLayoutModel *dlm = m_ParentUI->GetDisplayLayoutModel();
  Vector2ui layout = dlm->GetSliceViewLayerTilingModel()->GetValue();
  unsigned int rows = layout[0], cols = layout[1];
  return Vector2ui(viewport[0] / cols, viewport[1] / rows);
}

Vector2ui GenericSliceModel::GetSizeInLogicalPixels()
{
  Vector2ui size = this->GetSize();
  size[0] = (unsigned int) (size[0] / m_SizeReporter->GetViewportPixelRatio());
  size[1] = (unsigned int) (size[1] / m_SizeReporter->GetViewportPixelRatio());
  return size;
}

Vector2ui GenericSliceModel::GetCanvasSize() const
{
  assert(m_ViewportLayout.vpList.size() > 0);
  assert(!m_ViewportLayout.vpList.front().isThumbnail);
  return m_ViewportLayout.vpList.front().size;
}

bool GenericSliceModel::IsTiling() const
{
  return m_Driver->GetGlobalState()->GetSliceViewLayerLayout() == LAYOUT_TILED;
}

void GenericSliceModel::GetNonThumbnailViewport(Vector2ui &pos, Vector2ui &size)
{
  // Initialize to the entire view
  pos.fill(0);
  size = m_SizeReporter->GetViewportSize();

  DisplayLayoutModel *dlm = this->GetParentUI()->GetDisplayLayoutModel();
  LayerLayout tiling = dlm->GetSliceViewLayerLayoutModel()->GetValue();

  // Are thumbnails being used?
  // TODO: this should be done through a state variable
  if(tiling == LAYOUT_STACKED && dlm->GetNumberOfGroundLevelLayersModel()->GetValue() > 1)
    {
    for(int i = 0; i < m_ViewportLayout.vpList.size(); i++)
      {
      const SliceViewportLayout::SubViewport &sv = m_ViewportLayout.vpList[i];
      if(!sv.isThumbnail)
        {
        pos = sv.pos;
        size = sv.size;
        break;
        }
      }
    }
}


ImageWrapperBase *GenericSliceModel::GetThumbnailedLayerAtPosition(int x, int y)
{
  bool isThumb;
  ImageWrapperBase *layer = this->GetContextLayerAtPosition(x, y, isThumb);
  return isThumb ? layer : NULL;
}

ImageWrapperBase *GenericSliceModel::GetContextLayerAtPosition(int x, int y, bool &outIsThumbnail)
{
  x *= m_SizeReporter->GetViewportPixelRatio();
  y *= m_SizeReporter->GetViewportPixelRatio();
  for(int i = 0; i < m_ViewportLayout.vpList.size(); i++)
    {
    const SliceViewportLayout::SubViewport &sv = m_ViewportLayout.vpList[i];
    if(x >= sv.pos[0] && y >= sv.pos[1]
       && x < sv.pos[0] + sv.size[0] && y < sv.pos[1] + sv.size[1])
      {
      outIsThumbnail = sv.isThumbnail;
      return m_Driver->GetCurrentImageData()->FindLayer(sv.layer_id, false);
      }
    }
  return NULL;
}


bool GenericSliceModel
::GetSliceIndexValueAndDomain(int &value, NumericValueRange<int> *domain)
{
  if(!m_Driver->IsMainImageLoaded())
    return false;

  value = this->GetSliceIndex();
  if(domain)
    {
    domain->Set(0, this->GetNumberOfSlices()-1, 1);
    }
  return true;
}

void GenericSliceModel::SetSlideIndexValue(int value)
{
  this->UpdateSliceIndex(value);
}

bool
GenericSliceModel
::GetCurrentComponentInSelectedLayerValueAndDomain(
    unsigned int &value, NumericValueRange<unsigned int> *domain)
{
  // Make sure there is a layer selected and it's a multi-component layer
  if(!m_Driver->IsMainImageLoaded())
    return false;

  ImageWrapperBase *layer =
      m_Driver->GetCurrentImageData()->FindLayer(
        m_Driver->GetGlobalState()->GetSelectedLayerId(), false);

  if(!layer || layer->GetNumberOfComponents() <= 1)
    return false;

  // Make sure the display mode is to scroll through components
  AbstractMultiChannelDisplayMappingPolicy *dpolicy =
      static_cast<AbstractMultiChannelDisplayMappingPolicy *>(layer->GetDisplayMapping());

  // Get the current display mode
  MultiChannelDisplayMode mode = dpolicy->GetDisplayMode();

  // Mode must be single component
  if(!mode.IsSingleComponent())
    return false;

  // Finally we can return a value and range
  value = mode.SelectedComponent;
  if(domain)
    domain->Set(0, layer->GetNumberOfComponents()-1, 1);

  return true;
}

void GenericSliceModel::SetCurrentComponentInSelectedLayerValue(unsigned int value)
{
  // Get the target layer
  ImageWrapperBase *layer =
      m_Driver->GetCurrentImageData()->FindLayer(
        m_Driver->GetGlobalState()->GetSelectedLayerId(), false);

  assert(layer);

  // Get the target policy
  AbstractMultiChannelDisplayMappingPolicy *dpolicy =
      static_cast<AbstractMultiChannelDisplayMappingPolicy *>(layer->GetDisplayMapping());
  MultiChannelDisplayMode mode = dpolicy->GetDisplayMode();

  assert(mode.IsSingleComponent());

  // Update the mode
  mode.SelectedComponent = value;
  dpolicy->SetDisplayMode(mode);
}

void GenericSliceModel::UpdateViewportLayout()
{
  // Get the information about how the viewport is split into sub-viewports
  DisplayLayoutModel *dlm = this->GetParentUI()->GetDisplayLayoutModel();
  Vector2ui layout = dlm->GetSliceViewLayerTilingModel()->GetValue();
  int nrows = (int) layout[0];
  int ncols = (int) layout[1];

  // Number of ground-level layers - together with the tiling, this determines
  // the behavior of the display
  int n_base_layers = dlm->GetNumberOfGroundLevelLayersModel()->GetValue();

  // Get the dimensions of the main viewport (in real pixels, not logical pixels)
  unsigned int w = m_SizeReporter->GetViewportSize()[0];
  unsigned int h = m_SizeReporter->GetViewportSize()[1];

  // Get the current image data
  GenericImageData *id = this->GetDriver()->GetCurrentImageData();

  // Clear the viewport array
  m_ViewportLayout.vpList.clear();

  // Is there anything to do?
  if(!this->GetDriver()->IsMainImageLoaded())
    return;

  // Is tiling being used
  if(nrows == 1 && ncols == 1)
    {
    // There is no tiling. One base layer is emphasized
    if(n_base_layers == 1)
      {
      // There is only one base layer (main). It's viewport occupies the whole screen
      SliceViewportLayout::SubViewport vp;
      vp.pos = Vector2ui(0, 0);
      vp.size = Vector2ui(w, h);
      vp.isThumbnail = false;
      vp.layer_id = id->GetMain()->GetUniqueId();
      m_ViewportLayout.vpList.push_back(vp);
      }
    else
      {
      // We are in thumbnail mode. Draw the selected layer big and all the ground-level
      // layers small, as thumbnails.
      unsigned int margin = 4;

      // The preferred width of the thumbnails (without margin)
      int k = n_base_layers;

      // This is a complicated calculation to make sure it all fits
      double max_thumb_size = dlm->GetThumbnailRelativeSize() / 100.0;
      double thumb_wd =
          std::min(max_thumb_size * w - 2 * margin,
                   (h - (1.0 + k) * margin) * (w - 2.0 * margin) / ((h - margin) * (1.0 + k)));

      double thumb_hd = h * thumb_wd / (w - thumb_wd - 2.0 * margin);

      // Round down the thumb sizes
      unsigned int thumb_w = (unsigned int) thumb_wd;
      unsigned int thumb_h = (unsigned int) thumb_hd;

      // Set the bottom of the first thumbnail
      unsigned int thumb_y = h - thumb_h - margin + 1;

      // Go through eligible layers
      for(LayerIterator it = id->GetLayers(); !it.IsAtEnd(); ++it)
        {
        if(it.GetRole() == MAIN_ROLE || !it.GetLayer()->IsSticky())
          {
          // Is this the visible layer?
          if(this->GetDriver()->GetGlobalState()->GetSelectedLayerId()
             == it.GetLayer()->GetUniqueId())
            {
            SliceViewportLayout::SubViewport vp;
            vp.layer_id = it.GetLayer()->GetUniqueId();
            vp.pos = Vector2ui(0, 0);
            vp.size = Vector2ui(w - thumb_w - 2 * margin, h);
            vp.isThumbnail = false;

            // Notice we are sticking this viewport in the beginning! It's primary.
            m_ViewportLayout.vpList.insert(m_ViewportLayout.vpList.begin(), vp);
            }

          // Either way, add the layer to the thumbnail region
          SliceViewportLayout::SubViewport vp;
          vp.layer_id = it.GetLayer()->GetUniqueId();
          vp.pos = Vector2ui(w - thumb_w - margin, thumb_y);
          vp.size = Vector2ui(thumb_w, thumb_h);
          vp.isThumbnail = true;
          m_ViewportLayout.vpList.push_back(vp);

          thumb_y -= thumb_h + margin;
          }
        }
      }
    }
  else
    {
    double cell_w = w / ncols;
    double cell_h = h / nrows;
    for(int irow = 0; irow < nrows; irow++)
      for(int icol = 0; icol < ncols; icol++)
        if(m_ViewportLayout.vpList.size() < n_base_layers)
          {
          SliceViewportLayout::SubViewport vp;
          vp.pos = Vector2ui(icol * cell_w, (nrows - 1 - irow) * cell_h);
          vp.size = Vector2ui(cell_w, cell_h);
          vp.isThumbnail = false;
          vp.layer_id = this->GetLayerForNthTile(irow, icol)->GetUniqueId();
          m_ViewportLayout.vpList.push_back(vp);
          }
    }
}

void GenericSliceModel::UpdateUpstreamViewportGeometry()
{
  // In this function, we have to figure out where the active viewport
  // is located in the physical image space of ITK-SNAP.
  GenericImageData *gid = this->GetImageData();

  // Get the display image spec corresponding to the current viewport
  GenericImageData::ImageBaseType *dispimg = gid->GetDisplayViewportGeometry(this->GetId());

  // Get the primary viewport - this is what affects everything else
  SliceViewportLayout::SubViewport &vp = m_ViewportLayout.vpList.front();

  // The size of the viewport is fairly easy
  GenericImageData::RegionType region;
  region.SetSize(0, vp.size[0]); region.SetSize(1, vp.size[1]); region.SetSize(2, 1);

  // The spacing of the viewport in physical units. This refers to the size of each
  // pixel. The easiest way to determine this is to map the edges of the viewport to
  // physical image coordinates
  Vector2d u[3];
  Vector3d s[4];
  Vector3d x[4];

  // Define the corners of the viewport in screen pixel units
  u[0][0] = 0;
  u[0][1] = 0;
  u[1][0] = u[0][0] + vp.size[0]; u[1][1] = u[0][1];
  u[2][0] = u[0][0]; u[2][1] = u[0][1] + vp.size[1];

  // Map into slice coordinates, adding the third dimension
  s[0] = this->MapWindowToSlice(u[0]);
  s[1] = this->MapWindowToSlice(u[1]);
  s[2] = this->MapWindowToSlice(u[2]);
  s[3] = this->MapWindowToSlice(u[0]);
  s[3][2] += m_DisplayToImageTransform->GetCoordinateOrientation(2);

  // Map these four points into the physical image space
  for(int i = 0; i < 4; i++)
    {
    // Shift by half-voxel in the in-plane dimensions
    s[i][0] -= this->GetDisplayToImageTransform()->GetCoordinateOrientation(0) * 0.5;
    s[i][1] -= this->GetDisplayToImageTransform()->GetCoordinateOrientation(1) * 0.5;

    itk::ContinuousIndex<double, 3> j = to_itkContinuousIndex(this->MapSliceToImage(s[i]));
    itk::Point<double, 3> px;
    gid->GetMain()->GetImageBase()->TransformContinuousIndexToPhysicalPoint(j, px);
    x[i] = Vector3d(px);
    }

  // Spacing - divide the length of each edge by the size in voxels
  GenericImageData::ImageBaseType::SpacingType spacing;
  spacing[0] = (x[1] - x[0]).magnitude() / vp.size[0];
  spacing[1] = (x[2] - x[0]).magnitude() / vp.size[1];
  spacing[2] = (x[3] - x[0]).magnitude();

  // Origin - the coordinates of the first point, plus a half-voxel
  Vector3d origin = x[0];
  origin += (x[1] - x[0]) / (2.0 * vp.size[0]);
  origin += (x[2] - x[0]) / (2.0 * vp.size[1]);
  origin -= (x[3] - x[0]) / 2.0;

  // Direction cosines - these are the normalized directions
  GenericImageData::ImageBaseType::DirectionType dir;
  for(int col = 0; col < 3; col++)
    {
    Vector3d dirvec = (x[col+1] - x[0]).normalize();
    for(int row = 0; row < 3; row++)
      dir(row, col) = dirvec[row];
    }

  // Set all of the parameters for the reference image
  dispimg->SetSpacing(spacing);
  dispimg->SetOrigin(to_itkPoint(origin));
  dispimg->SetDirection(dir);
  dispimg->SetRegions(region);
}

ImageWrapperBase *GenericSliceModel::GetLayerForNthTile(int row, int col)
{
  // Number of divisions
  DisplayLayoutModel *dlm = this->GetParentUI()->GetDisplayLayoutModel();
  Vector2ui layout = dlm->GetSliceViewLayerTilingModel()->GetValue();
  int nrows = (int) layout[0], ncols = (int) layout[1];

  // This code is used if the layout is actually tiled
  if(ncols > 1 || nrows > 1)
    {
    // How many layers to go until we get to the one we want to paint?
    int togo = row * ncols + col;

    // Skip all layers until we get to the sticky layer we want to paint
    for(LayerIterator it(this->GetImageData()); !it.IsAtEnd(); ++it)
      {
      if(it.GetRole() == MAIN_ROLE || !it.GetLayer()->IsSticky())
        {
        if(togo == 0)
          return it.GetLayer();
        togo--;
        }
      }
    }
  else
    {
    for(LayerIterator it(this->GetImageData()); !it.IsAtEnd(); ++it)
      {
      if(it.GetLayer() && it.GetLayer()->GetUniqueId() ==
         this->GetDriver()->GetGlobalState()->GetSelectedLayerId())
        {
        return it.GetLayer();
        }
      }
    }

  return NULL;
}

Vector3d GenericSliceModel::ComputeGridPosition(
    const Vector3d &disp_pix,
    const itk::Index<2> &slice_index,
    ImageWrapperBase *vecimg)
{

  // The pixel must be mapped to native
  Vector3d disp;
  disp[0] = vecimg->GetNativeIntensityMapping()->MapInternalToNative(disp_pix[0]);
  disp[1] = vecimg->GetNativeIntensityMapping()->MapInternalToNative(disp_pix[1]);
  disp[2] = vecimg->GetNativeIntensityMapping()->MapInternalToNative(disp_pix[2]);

  // This is the physical coordinate of the current pixel - in LPS
  Vector3d xPhys;
  if(vecimg->IsSlicingOrthogonal())
    {
    // The pixel gives the displacement in LPS coordinates (by ANTS/Greedy convention)
    // We need to map it back into the slice domain. First, we need to know the 3D index
    // of the current pixel in the image space
    Vector3d xSlice;
    xSlice[0] = slice_index[0] + 0.5;
    xSlice[1] = slice_index[1] + 0.5;
    xSlice[2] = this->GetSliceIndex();

    // For orthogonal slicing, the input coordinates are in units of image voxels
    xPhys = this->MapSliceToImagePhysical(xSlice);
    }
  else
    {
    // Otherwise, the slice coordinates are relative to the rendered slice
    GenericImageData *gid = this->GetImageData();
    GenericImageData::ImageBaseType *dispimg =
        gid->GetDisplayViewportGeometry(this->GetId());

    // Use that image to transform coordinates
    itk::Point<double, 3> pPhys;
    itk::Index<3> index;
    index[0] = slice_index[0]; index[1] = slice_index[1]; index[2] = 0;
    dispimg->TransformIndexToPhysicalPoint(index, pPhys);
    xPhys = pPhys;
    }

  // Add displacement and map back to slice space
  itk::ContinuousIndex<double, 3> cix;
  itk::Point<double, 3> pt = to_itkPoint(xPhys + disp);

  this->GetDriver()->GetCurrentImageData()->GetMain()->GetImageBase()
      ->TransformPhysicalPointToContinuousIndex(pt, cix);

  // The displaced location in slice coordinates
  Vector3d disp_slice = this->MapImageToSlice(Vector3d(cix));

  // What we return also depends on whether slicing is ortho or not. For ortho
  // slicing, the renderer is configured in the "Slice" coordinate system (1 unit =
  // 1 image voxel) while for oblique slicing, the renderer uses the window coordinate
  // system (1 unit = 1 screen pixel). Whatever we return needs to be in those units.
  if(vecimg->IsSlicingOrthogonal())
    {
    return disp_slice;
    }
  else
    {
    Vector3d win3d;
    Vector2d win2d = this->MapSliceToWindow(disp_slice);
    win3d[0] = win2d[0]; win3d[1] = win2d[1]; win3d[2] = disp_slice[2];
    return win3d;
    }
}

DeformationGridModel *
GenericSliceModel
::GetDeformationGridModel()
{
  if (!m_DeformationGridModel)
    {
    m_DeformationGridModel = DeformationGridModel::New();
    m_DeformationGridModel->SetParent(this);
    }

  return m_DeformationGridModel;
}
