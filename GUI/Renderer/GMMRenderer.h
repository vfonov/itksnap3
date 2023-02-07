#ifndef GMMRENDERER_H
#define GMMRENDERER_H

#include "AbstractVTKSceneRenderer.h"
#include "vtkSmartPointer.h"

class vtkActor;
class vtkPropAssembly;
class vtkChartXY;
class vtkFloatArray;
class vtkPlot;
class vtkTable;

class SnakeWizardModel;
class LayerHistogramPlotAssembly;

class GMMRenderer : public AbstractVTKSceneRenderer
{
public:

  irisITKObjectMacro(GMMRenderer, AbstractVTKSceneRenderer)

  void SetModel(SnakeWizardModel *model);

  void SetRenderWindow(vtkRenderWindow *rwin) override;

  void OnUpdate() ITK_OVERRIDE;

  void UpdatePlotValues();

protected:

  GMMRenderer();
  virtual ~GMMRenderer() {}


  SnakeWizardModel *m_Model;

  // Rendering stuff
  vtkSmartPointer<vtkChartXY> m_Chart;

  // Histogram rendering
  LayerHistogramPlotAssembly *m_HistogramAssembly;

  static unsigned int NUM_POINTS;

  void OnDevicePixelRatioChange(int old_ratio, int new_ratio) ITK_OVERRIDE;
};

#endif // GMMRENDERER_H
