#include "IRISDisplayGeometry.h"

IRISDisplayGeometry::IRISDisplayGeometry()
{
}

IRISDisplayGeometry::IRISDisplayGeometry(
    const std::string &rai0, const std::string &rai1, const std::string &rai2)
{
  DisplayToAnatomyRAI[0] = rai0;
  DisplayToAnatomyRAI[1] = rai1;
  DisplayToAnatomyRAI[2] = rai2;
}

int
IRISDisplayGeometry
::GetDisplayWindowForAnatomicalDirection(AnatomicalDirection iAnat) const
{
  std::string rai1 = "SRA", rai2 = "ILP";
  char c1 = rai1[iAnat], c2 = rai2[iAnat];
  for(int j = 0; j < 3; j++)
    {
    char sd = DisplayToAnatomyRAI[j][2];
    if(sd == c1 || sd == c2)
      return j;
    }

  assert(0);
  return 0;
}

AnatomicalDirection
IRISDisplayGeometry
::GetAnatomicalDirectionForDisplayWindow(int iWin) const
{
  char sd = DisplayToAnatomyRAI[iWin][2];
  if(sd == 'S' || sd == 'I')
    return ANATOMY_AXIAL;
  else if (sd == 'R' || sd == 'L')
    return ANATOMY_SAGITTAL;
  else if (sd == 'A' || sd == 'P')
    return ANATOMY_CORONAL;

  assert(0);
  return(ANATOMY_NONSENSE);
}


