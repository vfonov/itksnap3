#ifndef UIREPORTERDELEGATES_H
#define UIREPORTERDELEGATES_H

#include "SNAPCommon.h"
#include "SNAPEvents.h"
#include "itkObject.h"
#include "itkObjectFactory.h"

namespace itk
{
template <class TPixel, unsigned int VDim> class Image;
template <class TPixel> class RGBAPixel;
class Command;
}

class Registry;

/**
  An interface used by models to request the viewport size from the GUI and
  for the GUI to notify models of changes in the viewport size.
  */
class ViewportSizeReporter : public itk::Object
{
public:
  itkEventMacro(ViewportResizeEvent,IRISEvent)

  /** Child classes are expected to fire this event when viewport resizes */
  FIRES(ViewportResizeEvent)

  virtual bool CanReportSize() = 0;
  virtual Vector2ui GetViewportSize() = 0;

  /** For retina displays, report the ratio of actual GL pixels to logical pixels */
  virtual float GetViewportPixelRatio() = 0;

  /** Get the viewport size in logical units (for retina-like displays) */
  virtual Vector2ui GetLogicalViewportSize() = 0;


protected:
  virtual ~ViewportSizeReporter() {}
};


/**
  This is a progress reporter delegate that allows the SNAP model classes
  to report progress without knowing what GUI toolkit actually implements
  the progress dialog.
  */
class ProgressReporterDelegate
{
public:

  /** Set the progress value between 0 and 1 */
  virtual void SetProgressValue(double) = 0;

  /** For convenience, the delegate can be hooked up to an ITK command */
  void ProgressCallback(itk::Object *source, const itk::EventObject &event);

  /** Create a command that will call this delegate */
  SmartPtr<itk::Command> CreateCommand();


};


/**
 This delegate is used to obtain system-specific information
 */
class SystemInfoDelegate
{
public:
  virtual std::string GetApplicationDirectory() = 0;
  virtual std::string GetApplicationFile() = 0;
  virtual std::string GetApplicationPermanentDataLocation() = 0;
  virtual std::string GetUserDocumentsLocation() = 0;

  virtual std::string EncodeServerURL(const std::string &url) = 0;

  typedef itk::Image<unsigned char, 2> GrayscaleImage;
  typedef itk::RGBAPixel<unsigned char> RGBAPixelType;
  typedef itk::Image<RGBAPixelType, 2> RGBAImageType;

  virtual void LoadResourceAsImage2D(std::string tag, GrayscaleImage *image) = 0;
  virtual void LoadResourceAsRegistry(std::string tag, Registry &reg) = 0;

  virtual void WriteRGBAImage2D(std::string file, RGBAImageType *image) = 0;
};

#endif // UIREPORTERDELEGATES_H
