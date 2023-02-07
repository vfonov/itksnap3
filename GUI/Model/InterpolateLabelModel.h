#ifndef INTERPOLATELABELMODEL_H
#define INTERPOLATELABELMODEL_H

#include <AbstractModel.h>
#include <PropertyModel.h>

#include "Registry.h"
#include "AbstractPropertyContainerModel.h"
#include "ColorLabelPropertyModel.h"

#include "SNAPImageData.h"
#include "itkImage.h"

class GlobalUIModel;
class GenericImageData; // DO I need this?

class InterpolateLabelModel : public AbstractPropertyContainerModel
{
public:

  enum InterpolationType
  {
      DISTANCE_MAP = 0, LEVEL_SET, MORPHOLOGY, BINARY_WEIGHTED_AVERAGE
  };

  // Standard ITK stuff
  irisITKObjectMacro(InterpolateLabelModel, AbstractModel)

  /** Initialize with the parent model */
  void SetParentModel(GlobalUIModel *parent);

  /** Whether all the labels should be interpolated */
  irisSimplePropertyAccessMacro(InterpolateAll, bool)

  /** Model for the label that will be interpolated */
  irisGenericPropertyAccessMacro(InterpolateLabel, LabelType, ColorLabelItemSetDomain)

  /** Model for the label that will be drawn with */
  irisGenericPropertyAccessMacro(DrawingLabel, LabelType, ColorLabelItemSetDomain)

  /** Model for the label that will be drawn over */
  irisGenericPropertyAccessMacro(DrawOverFilter, DrawOverFilter, DrawOverLabelItemSetDomain)

  /** Whether the scaffold should be kept */
  irisSimplePropertyAccessMacro(RetainScaffold, bool)

  /** Smoothing factor */
  irisRangedPropertyAccessMacro(DefaultSmoothing, double)
  irisRangedPropertyAccessMacro(LevelSetSmoothing, double)

  /** Curvature parameter for level set method */
  irisRangedPropertyAccessMacro(LevelSetCurvature, double)

  /** Whether to use signed distance transform for morphological interpolation */
  irisSimplePropertyAccessMacro(MorphologyUseDistance, bool)
  /** Whether to use optimal slice alignment for morphological interpolation */
  irisSimplePropertyAccessMacro(MorphologyUseOptimalAlignment, bool)

  /** Which interpolation method to use */
  irisSimplePropertyAccessMacro(InterpolationMethod, InterpolationType)

  /** Which axis to interpolate along */
  irisSimplePropertyAccessMacro(MorphologyInterpolateOneAxis, bool)
  irisSimplePropertyAccessMacro(MorphologyInterpolationAxis, AnatomicalDirection)

  /** Update the state of the widget whenever it is shown */
  void UpdateOnShow();

  /** Perform the actual interpolation */
  void Interpolate();

  // Added by SR

  /** Whether to interpolate intermediate slices only */
  irisSimplePropertyAccessMacro(BWAInterpolateIntermediateOnly, bool)

  /** Whether to use contour interpolation only */
  irisSimplePropertyAccessMacro(BWAUseContourOnly, bool)

  /** Whether to overwrite an existing segmentation or not */
  irisSimplePropertyAccessMacro(BWAOverwriteSegmentation, bool)

  /** Manually select slicing direction */
  irisSimplePropertyAccessMacro(SliceDirection, bool)
  irisSimplePropertyAccessMacro(SliceDirectionAxis, AnatomicalDirection)

  typedef SNAPImageData                                          InputDataType;
  using ShortType =  itk::Image<short,3>;
  using GreyScalarType =  AnatomicScalarImageWrapper::ImageType;
  using GreyVectorType =  AnatomicImageWrapper::ImageType;

protected:

  // Constructor
  InterpolateLabelModel();
  virtual ~InterpolateLabelModel() {}

  // Models for the main things settable by the user
  SmartPtr<ConcreteSimpleBooleanProperty> m_InterpolateAllModel;
  SmartPtr<ConcreteColorLabelPropertyModel> m_InterpolateLabelModel;
  SmartPtr<ConcreteColorLabelPropertyModel> m_DrawingLabelModel;
  SmartPtr<ConcreteDrawOverFilterPropertyModel> m_DrawOverFilterModel;

  SmartPtr<ConcreteSimpleBooleanProperty> m_RetainScaffoldModel;

  SmartPtr<ConcreteRangedDoubleProperty> m_DefaultSmoothingModel;

  SmartPtr<ConcreteRangedDoubleProperty> m_LevelSetSmoothingModel;
  SmartPtr<ConcreteRangedDoubleProperty> m_LevelSetCurvatureModel;

  SmartPtr<ConcreteSimpleBooleanProperty> m_MorphologyUseDistanceModel;
  SmartPtr<ConcreteSimpleBooleanProperty> m_MorphologyUseOptimalAlignmentModel;
  SmartPtr<ConcreteSimpleBooleanProperty> m_MorphologyInterpolateOneAxisModel;
  typedef ConcretePropertyModel<AnatomicalDirection, TrivialDomain> ConcreteInterpolationAxisType;
  SmartPtr<ConcreteInterpolationAxisType> m_MorphologyInterpolationAxisModel;

  typedef ConcretePropertyModel<InterpolationType, TrivialDomain> ConcreteInterpolationType;
  SmartPtr<ConcreteInterpolationType> m_InterpolationMethodModel;

  //Options for BWA
  SmartPtr<ConcreteSimpleBooleanProperty> m_BWAInterpolateIntermediateOnlyModel;
  SmartPtr<ConcreteSimpleBooleanProperty> m_BWAUseContourOnlyModel;
  SmartPtr<ConcreteSimpleBooleanProperty> m_BWAOverwriteSegmentationModel;
  SmartPtr<ConcreteSimpleBooleanProperty> m_SliceDirectionModel;
  SmartPtr<ConcreteInterpolationAxisType> m_SliceDirectionAxisModel;

  // Templated code to interpolate an image
  template <class TImage> void DoInterpolate(TImage *image);

  // The parent model
  GlobalUIModel *m_Parent;

  GenericImageData *m_CurrentImageData;

};

#endif // INTERPOLATELABELMODEL_H
