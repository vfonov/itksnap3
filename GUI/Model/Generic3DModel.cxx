#include "Generic3DModel.h"
#include "Generic3DRenderer.h"
#include "GlobalUIModel.h"
#include "IRISException.h"
#include "IRISApplication.h"
#include "GenericImageData.h"
#include "IRISImageData.h"
#include "SNAPImageData.h"
#include "ImageWrapperBase.h"
#include "MeshManager.h"
#include "Window3DPicker.h"
#include "vtkRenderWindow.h"
#include "vtkRendererCollection.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkPointData.h"
#include "MeshOptions.h"
#include "ImageWrapperTraits.h"
#include "SegmentationUpdateIterator.h"
#include "ImageMeshLayers.h"

// All the VTK stuff
#include "vtkPolyData.h"

#include <vnl/vnl_inverse.h>

Generic3DModel::Generic3DModel()
{
  // Initialize the matrix to nil
  m_WorldMatrix.set_identity();

  // Create the spray points
  vtkSmartPointer<vtkPoints> pts = vtkSmartPointer<vtkPoints>::New();
  m_SprayPoints = vtkSmartPointer<vtkPolyData>::New();
  m_SprayPoints->SetPoints(pts);

  // Create the renderer
  m_Renderer = Generic3DRenderer::New();

  // Continuous update model
  m_ContinuousUpdateModel = NewSimpleConcreteProperty(false);

  // Display Color Bar model
  m_DisplayColorBarModel = NewSimpleConcreteProperty(false);

  // Scalpel
  m_ScalpelStatus = SCALPEL_LINE_NULL;

  // Reset clear time
  m_ClearTime = 0;
}

#include "itkImage.h"

void Generic3DModel::Initialize(GlobalUIModel *parent)
{
  // Store the parent
  m_ParentUI = parent;
  m_Driver = parent->GetDriver();

  // Update our geometry model
  OnImageGeometryUpdate();

  // Initialize the renderer
  m_Renderer->SetModel(this);

  // Listen to the layer change events
  Rebroadcast(m_Driver, MainImageDimensionsChangeEvent(), ModelUpdateEvent());

  // Listen to segmentation change events
  Rebroadcast(m_Driver, SegmentationChangeEvent(), StateMachineChangeEvent());
  Rebroadcast(m_Driver, LevelSetImageChangeEvent(), StateMachineChangeEvent());

  // Rebroadcast model change events as state changes
  Rebroadcast(this, ModelUpdateEvent(), StateMachineChangeEvent());
  Rebroadcast(this, SprayPaintEvent(), StateMachineChangeEvent());
  Rebroadcast(this, ScalpelEvent(), StateMachineChangeEvent());
  Rebroadcast(m_ParentUI->GetGlobalState()->GetToolbarMode3DModel(),
              ValueChangedEvent(), StateMachineChangeEvent());
  Rebroadcast(m_ParentUI->GetGlobalState()->GetMeshOptions(),
              ChildPropertyChangedEvent(), StateMachineChangeEvent());

  Rebroadcast(m_Driver->GetIRISImageData()->GetMeshLayers(), LayerChangeEvent(), StateMachineChangeEvent());
}

bool Generic3DModel::CheckState(Generic3DModel::UIState state)
{
  if(!m_ParentUI->GetDriver()->IsMainImageLoaded())
    return false;

  ToolbarMode3DType mode = m_ParentUI->GetGlobalState()->GetToolbarMode3D();
  ImageMeshLayers *layer = m_Driver->IsSnakeModeActive() ?
        m_Driver->GetSNAPImageData()->GetMeshLayers():
        m_Driver->GetIRISImageData()->GetMeshLayers();

  switch(state)
    {
    case UIF_MESH_DIRTY:
      {
      bool ret = layer->IsActiveMeshLayerDirty();

      // clearing time
      if (this->m_ClearTime >= layer->GetActiveMeshMTime())
        ret = true;

      return ret;
      }

    case UIF_MESH_ACTION_PENDING:
      {
      if(mode == SPRAYPAINT_MODE)
        return m_SprayPoints->GetNumberOfPoints() > 0;

      else if (mode == SCALPEL_MODE)
        return m_ScalpelStatus == SCALPEL_LINE_COMPLETED;

      else return false;
      }

    case UIF_CAMERA_STATE_SAVED:
      {
      return m_Renderer->IsSavedCameraStateAvailable();
      }

    case UIF_FLIP_ENABLED:
      {
      return mode == SCALPEL_MODE && m_ScalpelStatus == SCALPEL_LINE_COMPLETED;
      }
    }

  return false;
}

Generic3DModel::Mat4d &Generic3DModel::GetWorldMatrix()
{
  return m_WorldMatrix;
}


Vector3d Generic3DModel::GetCenterOfRotation()
{
  return affine_transform_point(m_WorldMatrix, m_Driver->GetCursorPosition());
}

void Generic3DModel::ResetView()
{
  m_Renderer->ResetView();
}

void Generic3DModel::SaveCameraState()
{
  m_Renderer->SaveCameraState();
  InvokeEvent(StateMachineChangeEvent());
}

void Generic3DModel::RestoreCameraState()
{
  m_Renderer->RestoreSavedCameraState();
}

#include "MeshExportSettings.h"
#include "vtkVRMLExporter.h"
void Generic3DModel::ExportMesh(const MeshExportSettings &settings)
{
  // Update the mesh
  this->UpdateSegmentationMesh(m_ParentUI->GetProgressCommand());

  // Prevent concurrent access to this method and mesh update
  std::lock_guard<std::mutex> guard(m_Mutex);

  // Certain formats require a VTK exporter and use a render window. They
  // are handled directly in this code, rather than in the Guided code.
  // TODO: it would make sense to unify this functionality in GuidedMeshIO
  GuidedMeshIO mesh_io;
  Registry reg_format = settings.GetMeshFormat();
  if(mesh_io.GetFileFormat(reg_format) == GuidedMeshIO::FORMAT_VRML)
    {
    // Create the exporter
    // TODO: temporarily commented, needs to be fixed!
    vtkSmartPointer<vtkVRMLExporter> exporter = vtkSmartPointer<vtkVRMLExporter>::New();
    exporter->SetFileName(settings.GetMeshFileName().c_str());
    exporter->SetInput(m_Renderer->GetRenderWindow());
    exporter->Update();
    return;
    }

  // Export the mesh
  m_ParentUI->GetDriver()->ExportSegmentationMesh(
        settings, m_ParentUI->GetProgressCommand());
}

ImageMeshLayers *
Generic3DModel
::GetMeshLayers()
{
  return m_Driver->IsSnakeModeActive() ?
        m_Driver->GetSNAPImageData()->GetMeshLayers() :
        m_Driver->GetIRISImageData()->GetMeshLayers();
}

vtkPolyData *Generic3DModel::GetSprayPoints() const
{
  return m_SprayPoints.GetPointer();
}

void Generic3DModel::OnUpdate()
{
  // If we experienced a change in main image, we have to respond!
  if(m_EventBucket->HasEvent(MainImageDimensionsChangeEvent()))
    {
    // There is no more mesh to render - until the user does something!
    // m_Mesh->DiscardVTKMeshes();

    // Clear the spray points
    m_SprayPoints->GetPoints()->Reset();
    m_SprayPoints->Modified();

    // The geometry has changed
    this->OnImageGeometryUpdate();
    }
}

void Generic3DModel::OnImageGeometryUpdate()
{
  // Update the world matrix and other stored variables
  if(m_Driver->IsMainImageLoaded())
    {
    ImageWrapperBase *main = m_Driver->GetCurrentImageData()->GetMain();
    m_WorldMatrix = main->GetNiftiSform();
    m_WorldMatrixInverse = main->GetNiftiInvSform();
    }
  else
    {
    m_WorldMatrix.set_identity();
    m_WorldMatrixInverse.set_identity();
    }
}

void Generic3DModel::UpdateSegmentationMesh(itk::Command *progressCmd)
{
  // Prevent concurrent access to this method
  std::lock_guard<std::mutex> guard(m_Mutex);

  try
  {
    // Generate all the mesh objects
    m_MeshUpdating = true;

    // Check if snake mode is active and get mode specific image data
    GenericImageData *imgData = m_Driver->IsSnakeModeLevelSetActive() ?
          (GenericImageData*) m_Driver->GetSNAPImageData() : m_Driver->GetIRISImageData();

    // Update Mesh Layer
    imgData->GetMeshLayers()->UpdateActiveMeshLayer(progressCmd);

    m_MeshUpdating = false;

    InvokeEvent(ModelUpdateEvent());
  }
  catch(std::bad_alloc &)
  {
    throw IRISException("Out of memory during mesh computation");
  }
  catch(IRISException & IRISexc)
  {
    throw IRISexc;
  }
}

bool Generic3DModel::IsMeshUpdating()
{
  return m_MeshUpdating;
}

bool Generic3DModel::AcceptAction()
{
  ToolbarMode3DType mode = m_ParentUI->GetGlobalState()->GetToolbarMode3D();
  IRISApplication *app = m_ParentUI->GetDriver();

  // Get the segmentation image
  LabelImageWrapper *seg = app->GetSelectedSegmentationLayer();

  // Accept the current action
  if(mode == SPRAYPAINT_MODE)
    {
    // Anything to update?
    bool update = false;

    // Merge all the spray points into the segmentation
    for(int i = 0; i < m_SprayPoints->GetNumberOfPoints(); i++)
      {
      // Find the point in image coordinates
      double *x = m_SprayPoints->GetPoint(i);

      // Create a region around this one voxel (in the future could use other shapes)
      SegmentationUpdateIterator::RegionType region;
      region.SetIndex(0, static_cast<unsigned int>(x[0])); region.SetSize(0, 1);
      region.SetIndex(1, static_cast<unsigned int>(x[1])); region.SetSize(1, 1);
      region.SetIndex(2, static_cast<unsigned int>(x[2])); region.SetSize(2, 1);

      // Treat each point as a region update
      SegmentationUpdateIterator it(seg, region,
                                    app->GetGlobalState()->GetDrawingColorLabel(),
                                    app->GetGlobalState()->GetDrawOverFilter());

      for(; !it.IsAtEnd(); ++it)
        {
        it.PaintAsForeground();
        }

      // Store the delta for this update
      if(it.Finalize())
        {
        update = true;
        seg->StoreIntermediateUndoDelta(it.RelinquishDelta());
        }
      }

    // Store the undo point
    if(update)
      {
      seg->StoreUndoPoint("3D spray paint");
      app->RecordCurrentLabelUse();

      // Clear the spray points
      m_SprayPoints->GetPoints()->Reset();
      m_SprayPoints->Modified();
      InvokeEvent(SprayPaintEvent());
      }

    // Return true if anything changed
    return update;
    }
  else if(mode == SCALPEL_MODE && m_ScalpelStatus == SCALPEL_LINE_COMPLETED)
    {
    // Get the plane origin and normal in world coordinates
    Vector3d xw = m_Renderer->GetScalpelPlaneOrigin();
    Vector3d nw = m_Renderer->GetScalpelPlaneNormal();

    // Map these properties into the image coordinates
    Vector3d xi = affine_transform_point(m_WorldMatrixInverse, xw);

    // Normal is a covariant tensor and has to be transformed by (M^-1)'
    vnl_matrix<double> Madj = m_WorldMatrix.extract(3,3).transpose();
    Vector3d ni = Madj * nw;

    // Use the driver to relabel the plane
    int nMod = app->RelabelSegmentationWithCutPlane(ni, dot_product(xi, ni));

    // Reset the scalpel state, but only if the operation was successful
    if(nMod > 0)
      {
      m_ScalpelStatus = SCALPEL_LINE_NULL;
      InvokeEvent(ScalpelEvent());
      return true;
      }
    else return false;
    }
  return true;
}

void Generic3DModel::CancelAction()
{
  ToolbarMode3DType mode = m_ParentUI->GetGlobalState()->GetToolbarMode3D();
  if(mode == SPRAYPAINT_MODE)
    {
    // Clear the spray points
    m_SprayPoints->GetPoints()->Reset();
    m_SprayPoints->Modified();
    InvokeEvent(SprayPaintEvent());
    }
  else if(mode == SCALPEL_MODE && m_ScalpelStatus == SCALPEL_LINE_COMPLETED)
    {
    // Reset the scalpel state
    m_ScalpelStatus = SCALPEL_LINE_NULL;
    InvokeEvent(ScalpelEvent());
    }
}

void Generic3DModel::FlipAction()
{
  ToolbarMode3DType mode = m_ParentUI->GetGlobalState()->GetToolbarMode3D();
  if(mode == SCALPEL_MODE && m_ScalpelStatus == SCALPEL_LINE_COMPLETED)
    {
    m_Renderer->FlipScalpelPlaneNormal();
    }
}

void Generic3DModel::ClearRenderingAction()
{
  m_Renderer->ClearRendering();

  ImageMeshLayers *layer = m_Driver->IsSnakeModeActive() ?
        m_Driver->GetSNAPImageData()->GetMeshLayers():
        m_Driver->GetIRISImageData()->GetMeshLayers();

  m_ClearTime = layer->GetActiveMeshMTime();
  InvokeEvent(ModelUpdateEvent());
}

#include "ImageRayIntersectionFinder.h"
#include "SNAPImageData.h"

/** These classes are used internally for m_Ray intersection testing */
class LabelImageHitTester
{
public:
  LabelImageHitTester(const ColorLabelTable *table = NULL)
  {
    m_LabelTable = table;
  }

  int operator()(LabelType label) const
  {
    const ColorLabel &cl = m_LabelTable->GetColorLabel(label);
    return (cl.IsVisible() && cl.IsVisibleIn3D()) ? 1 : 0;
  }

private:
  const ColorLabelTable *m_LabelTable;
};

class SnakeImageHitTester
{
public:
  int operator()(float levelSetValue) const
    { return levelSetValue <= 0 ? 1 : 0; }
};

bool Generic3DModel::IntersectSegmentation(int vx, int vy, Vector3i &hit)
{
  // World coordinate of the click position and direction
  Vector3d x_world, ray_world, dx_world, dy_world;
  m_Renderer->ComputeRayFromClick(vx, vy, x_world, ray_world, dx_world, dy_world);

  // Convert these to image coordinates
  Vector3d x_image = affine_transform_point(m_WorldMatrixInverse, x_world);
  Vector3d d_image = affine_transform_vector(m_WorldMatrixInverse, ray_world);

  int result = 0;
  if(m_Driver->IsSnakeModeLevelSetActive())
    {
    typedef ImageRayIntersectionFinder<itk::Image<float, 3>, SnakeImageHitTester> RayCasterType;
    RayCasterType caster;
    result = caster.FindIntersection(
          m_ParentUI->GetDriver()->GetSNAPImageData()->GetSnake()->GetImage(),
          x_image, d_image, hit);
    }
  else
    {
    typedef ImageRayIntersectionFinder<LabelImageWrapperTraits::ImageType, LabelImageHitTester> RayCasterType;
    RayCasterType caster;
    LabelImageHitTester tester(m_ParentUI->GetDriver()->GetColorLabelTable());
    caster.SetHitTester(tester);
    result = caster.FindIntersection(
          m_ParentUI->GetDriver()->GetSelectedSegmentationLayer()->GetImage(),
          x_image, d_image, hit);
    }

  return (result == 1);
}

bool Generic3DModel::IntersectSegmentation(int vx, int vy, double v_radius, int n_samples, std::set<Vector3i> &hits)
{
  // World coordinate of the click position and direction
  Vector3d x_world, ray_world, dx_world, dy_world;
  m_Renderer->ComputeRayFromClick(vx, vy, x_world, ray_world, dx_world, dy_world);

  // Convert these to image coordinates
  Vector3d x_image = affine_transform_point(m_WorldMatrixInverse, x_world);
  Vector3d ray_image = affine_transform_vector(m_WorldMatrixInverse, ray_world);
  Vector3d dx_image = affine_transform_vector(m_WorldMatrixInverse, dx_world);
  Vector3d dy_image = affine_transform_vector(m_WorldMatrixInverse, dy_world);

  // TODO figure our sampling code here

  return false;


}


bool Generic3DModel::PickSegmentationVoxelUnderMouse(int px, int py)
{
  // Find the voxel under the cursor
  Vector3i hit;
  if(this->IntersectSegmentation(px, py, hit))
    {
    Vector3ui cursor = to_unsigned_int(hit);

    itk::ImageRegion<3> region = m_Driver->GetCurrentImageData()->GetImageRegion();
    if(region.IsInside(to_itkIndex(cursor)))
      {
      m_Driver->SetCursorPosition(cursor);
      return true;
      }
    }

  return false;
}

bool Generic3DModel::SpraySegmentationVoxelUnderMouse(int px, int py)
{
  // Find the voxel under the cursor
  Vector3i hit;
  if(this->IntersectSegmentation(px, py, hit))
    {
    itk::ImageRegion<3> region = m_Driver->GetCurrentImageData()->GetImageRegion();
    if(region.IsInside(to_itkIndex(hit)))
      {
      m_SprayPoints->GetPoints()->InsertNextPoint(hit[0], hit[1], hit[2]);
      m_SprayPoints->Modified();
      this->InvokeEvent(SprayPaintEvent());
      return true;
      }
    }

  return false;
}

void Generic3DModel::SetScalpelStartPoint(int px, int py)
{
  m_ScalpelEnd[0] = m_ScalpelStart[0] = px;
  m_ScalpelEnd[1] = m_ScalpelStart[1] = py;
  m_ScalpelStatus = SCALPEL_LINE_STARTED;
  this->InvokeEvent(ScalpelEvent());
}

void Generic3DModel::SetScalpelEndPoint(int px, int py, bool complete)
{
  m_ScalpelEnd[0] = px;
  m_ScalpelEnd[1] = py;
  if(complete)
    m_ScalpelStatus = SCALPEL_LINE_COMPLETED;
  this->InvokeEvent(ScalpelEvent());
}


