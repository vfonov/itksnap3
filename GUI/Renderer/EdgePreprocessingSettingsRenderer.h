#ifndef EDGEPREPROCESSINGSETTINGSRENDERER_H
#define EDGEPREPROCESSINGSETTINGSRENDERER_H

#include "AbstractVTKSceneRenderer.h"
#include "vtkSmartPointer.h"

class vtkActor;
class vtkPropAssembly;
class vtkChartXY;
class vtkFloatArray;
class vtkPlot;
class vtkTable;

class SnakeWizardModel;

class EdgePreprocessingSettingsRenderer : public AbstractVTKSceneRenderer
{
public:
  irisITKObjectMacro(EdgePreprocessingSettingsRenderer, AbstractVTKSceneRenderer)

  void SetModel(SnakeWizardModel *model);

  void OnUpdate() override;

  void UpdatePlotValues();

  virtual void SetRenderWindow(vtkRenderWindow *rwin) override;

protected:

  EdgePreprocessingSettingsRenderer();
  virtual ~EdgePreprocessingSettingsRenderer() {}

  SnakeWizardModel *m_Model;

  // Number of data points
  static const unsigned int NUM_POINTS;

  // Rendering stuff
  vtkSmartPointer<vtkChartXY> m_Chart;
  vtkSmartPointer<vtkTable> m_PlotTable;
  vtkSmartPointer<vtkPlot> m_Plot;
  vtkSmartPointer<vtkFloatArray> m_DataX, m_DataY;

  virtual void OnDevicePixelRatioChange(int old_ratio, int new_ratio) ITK_OVERRIDE;
};

#endif // EDGEPREPROCESSINGSETTINGSRENDERER_H
