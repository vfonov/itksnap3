#ifndef SNAKEROIMODEL_H
#define SNAKEROIMODEL_H

#include <SNAPCommon.h>
#include "AbstractModel.h"
#include <IRISException.h>
#include "PropertyModel.h"

class GenericSliceModel;

struct SnakeROISideSelectionState
{
  /**
   * The four edges in the rectangle, ordered first by orientation
   * (0 = horizontal), (1 = vertical) and then by adjacency to the
   * first or second vertex of the cardinal cube defining the region
   * of interest (0 = closest to 0,0,0), (1 = closest to sx,sy,sz)
   */
  bool Highlighted[2][2];

  bool AnyHighlighted()
  {
    return (Highlighted[0][0] ||
            Highlighted[0][1] ||
            Highlighted[1][0] ||
            Highlighted[1][1]);

  }

  SnakeROISideSelectionState()
  {
    Highlighted[0][0] = Highlighted[0][1] =
        Highlighted[1][0] = Highlighted[1][1] = false;
  }

  SnakeROISideSelectionState(const SnakeROISideSelectionState &ref)
  {
    Highlighted[0][0] = ref.Highlighted[0][0];
    Highlighted[0][1] = ref.Highlighted[0][1];
    Highlighted[1][0] = ref.Highlighted[1][0];
    Highlighted[1][1] = ref.Highlighted[1][1];
  }

  bool operator == (const SnakeROISideSelectionState &ref)
  {
    return (Highlighted[0][0] == ref.Highlighted[0][0] &&
            Highlighted[0][1] == ref.Highlighted[0][1] &&
            Highlighted[1][0] == ref.Highlighted[1][0] &&
            Highlighted[1][1] == ref.Highlighted[1][1]);
  }

  bool operator != (const SnakeROISideSelectionState &ref)
  {
    return !(*this == ref);
  }
};

/**
  This class handles the interaction with the selection box.

  TODO: The interaction could be improved to do things like highlight
  the edge under the mouse on mouse motion, to change the cursor when
  the mouse is over the draggable region, etc. Also it would be nice
  to always show the box (even when the cursor is outside of the selection)
  and to use a fill to indicate that a region is outside of the ROI. But
  all this is easy to change.

  The model also provides the property models for the dialog used to resample
  the snake ROI.
*/

class SnakeROIModel : public AbstractModel
{
public:
  irisITKObjectMacro(SnakeROIModel, AbstractModel)

  irisGetMacro(Parent, GenericSliceModel *)

  void SetParent(GenericSliceModel *);

  bool ProcessPushEvent(double x, double y);

  bool ProcessDragEvent(double x, double y, double xStart, double yStart, bool itkNotUsed(release));

  bool ProcessMoveEvent(double x, double y);

  void ProcessLeaveEvent();
  void ProcessEnterEvent();

  void ResetROI();

  /** Map from system's ROI in image coordinates to 2D slice coords */
  void GetSystemROICorners(Vector3d corner[2]);

  /** Compute the slice-space vertices corresponding to an edge */
  void GetEdgeVertices(unsigned int direction,
    unsigned int index,Vector2d &x0,Vector2d &x1,const Vector3d corner[2]);

  /** Compute a distance to an edge */
  double GetEdgeDistance(unsigned int direction,
    unsigned int index,const Vector2d &point,const Vector3d corner[2]);


  irisGetMacro(Highlight, const SnakeROISideSelectionState &)

  friend class SnakeROIRenderer;


protected:

  // The click detection radius (delta)
  static const unsigned int m_PixelDelta;

  // Four vertices in the region box (correspond to the two corners
  // of the 3D region of interest
  Vector3d m_CornerDragStart[2];

  /**
   * The four edges in the rectangle, ordered first by orientation
   * (0 = horizontal), (1 = vertical) and then by adjacency to the
   * first or second vertex of the cardinal cube defining the region
   * of interest (0 = closest to 0,0,0), (1 = closest to sx,sy,sz)
   */
  SnakeROISideSelectionState m_Highlight;

  /**
   * Update the region of interest in response to the dragging or release
   * operations.
   */
  void UpdateCorners(const Vector2d &xPress, const Vector2d &xDrag);

  SnakeROISideSelectionState ComputeSelection(Vector2d &uvSlice, Vector3d corners[]);

  // Parent model
  GenericSliceModel *m_Parent;

  // Private constructor stuff
  SnakeROIModel();
  virtual ~SnakeROIModel() {}

};

#endif // SNAKEROIMODEL_H
