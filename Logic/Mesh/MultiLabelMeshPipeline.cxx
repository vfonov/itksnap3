/*=========================================================================

  Program:   ITK-SNAP
  Module:    $RCSfile: MultiLabelMeshPipeline.cxx,v $
  Language:  C++
  Date:      $Date: 2010/06/28 18:45:08 $
  Version:   $Revision: 1.4 $
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
// Borland compiler is very lazy so we need to instantiate the template
//  by hand 
#if defined(__BORLANDC__)
#include "SNAPBorlandDummyTypes.h"
#endif

#include "MultiLabelMeshPipeline.h"

// SNAP includes
#include "IRISVectorTypesToITKConversion.h"
#include "VTKMeshPipeline.h"
#include "MeshOptions.h"
#include "vtkUnsignedShortArray.h"

// ITK includes
#include "itkBinaryThresholdImageFilter.h"

using namespace std;

MultiLabelMeshPipeline
::MultiLabelMeshPipeline()
{
  // Initialize the region of interest filter
  m_ROIFilter = ROIFilter::New();
  m_ROIFilter->ReleaseDataFlagOn();

  // Define the binary thresholding filter that will map the image onto the 
  // range -1 to 1
  m_ThrehsoldFilter = ThresholdFilter::New();
  m_ThrehsoldFilter->SetInput(m_ROIFilter->GetOutput());
  m_ThrehsoldFilter->ReleaseDataFlagOn();
  m_ThrehsoldFilter->SetInsideValue(1.0f);
  m_ThrehsoldFilter->SetOutsideValue(-1.0f);

  // Initialize the VTK Processing Pipeline
  m_VTKPipeline = new VTKMeshPipeline();
  m_VTKPipeline->SetImage(m_ThrehsoldFilter->GetOutput());

  // Set the initial mesh options
  m_MeshOptions = MeshOptions::New();
  m_VTKPipeline->SetMeshOptions(m_MeshOptions);
}

MultiLabelMeshPipeline
::~MultiLabelMeshPipeline()
{
  delete m_VTKPipeline;
}

void
MultiLabelMeshPipeline
::SetMeshOptions(const MeshOptions *options)
{
  // Store the options
  if(*m_MeshOptions != *options)
    {
    // Save the options
    m_MeshOptions->DeepCopy(options);

    // Apply the options to the internal pipeline
    m_VTKPipeline->SetMeshOptions(m_MeshOptions);

    // Clear the cached stuff
    m_MeshInfo.clear();
    }
}

unsigned long
MultiLabelMeshPipeline
::GetVoxelsInBoundingBox(LabelType label) const
{
  return m_BoundingBox[label].GetNumberOfPixels();
}

AllPurposeProgressAccumulator *
MultiLabelMeshPipeline
::GetProgressAccumulator()
{
  return m_VTKPipeline->GetProgressAccumulator();
}
  

#include <ctime>

bool
MultiLabelMeshPipeline
::ComputeMesh(LabelType label, vtkPolyData *outMesh)
{
  // The label must be present in the image
  if(m_Histogram[label] == 0)
    return false;

  // TODO: make this more elegant
  InputImageType::RegionType bbWiderRegion = m_BoundingBox[label];
  bbWiderRegion.PadByRadius(5);
  bbWiderRegion.Crop(m_InputImage->GetLargestPossibleRegion()); 

  // Pass the region to the ROI filter and propagate the filter
  m_ROIFilter->SetInput(m_InputImage);
  m_ROIFilter->SetRegionOfInterest(bbWiderRegion);
  m_ROIFilter->Update();
  
  // Set the parameters for the thresholding filter
  m_ThrehsoldFilter->SetLowerThreshold(label);
  m_ThrehsoldFilter->SetUpperThreshold(label);
  m_ThrehsoldFilter->UpdateLargestPossibleRegion();

  // Graft the polydata to the last filter in the pipeline
  m_VTKPipeline->SetImage(m_ThrehsoldFilter->GetOutput());
  m_VTKPipeline->ComputeMesh(outMesh);

  // Done
  return true;
}

#include "itkImageLinearConstIteratorWithIndex.h"
#include "itk_zlib.h"

inline unsigned long rotl(unsigned long value, int shift)
{
  return (value << shift) | (value >> (32 - shift));
}

void MultiLabelMeshPipeline::UpdateMeshInfoHelper(
    MultiLabelMeshPipeline::MeshInfo *current_meshinfo,
    const itk::Index<3> &run_start,
    itk::ImageRegionConstIteratorWithIndex<MultiLabelMeshPipeline::InputImageType> &it,
    unsigned long pos)
{
  // The end of the run, i.e., the last voxel that matched the label of run_start
  itk::Index<3> run_end = run_start; run_end[0] = pos - 1;

  // Append the start index to the checksum
  for(int d = 0; d < 3; d++)
    {
    long p_start = run_start[d], p_end = run_end[d];
    current_meshinfo->CheckSum =
        adler32(current_meshinfo->CheckSum,
                (unsigned char *) &p_start,
                sizeof(itk::Index<3>::IndexValueType));

    current_meshinfo->CheckSum =
        adler32(current_meshinfo->CheckSum,
                (unsigned char *) &p_end,
                sizeof(itk::Index<3>::IndexValueType));
    }

  // Update the extents
  unsigned long run_length = pos - run_start[0];
  if(current_meshinfo->Count == 0)
    {
    current_meshinfo->BoundingBox[0] = run_start;
    current_meshinfo->BoundingBox[1] = run_end;
    }
  else
    {
    for(int d = 0; d < 3; d++)
      {
      if(run_start[d] < current_meshinfo->BoundingBox[0][d])
        current_meshinfo->BoundingBox[0][d] = run_start[d];
      if(run_end[d] > current_meshinfo->BoundingBox[1][d])
        current_meshinfo->BoundingBox[1][d] = run_end[d];
      }
    }

  // Add the number of pixels traversed to the index
  current_meshinfo->Count += run_length;
}

void MultiLabelMeshPipeline::UpdateMeshes(itk::Command *progressCommand)
{
  // Create a temporary table of mesh info
  MeshInfoMap meshmap;

  // Current label and current mesh map
  LabelType current_label = 0;
  MeshInfo *current_meshinfo = NULL;
  itk::Index<3> run_start;

  // The length of a line
  //unsigned long line_length = m_InputImage->GetLargestPossibleRegion().GetSize()[0];

  // Iterate through the image updating the mesh map. This code takes advantage
  // of the organization of label data. Rather than updating the extents after
  // each pixel read, the code collects runs of pixels of the same label and
  // updates once the run ends (a pixel of another label is found or the end
  // of a line of pixels is reached). This makes for much more efficient code.
  typedef itk::ImageRegionConstIteratorWithIndex<InputImageType> InputIterator;
  InputIterator it(m_InputImage, m_InputImage->GetLargestPossibleRegion());
  while( !it.IsAtEnd() )
    {
    run_start = it.GetIndex();
    const InputIterator::RLLine &line=*(it.rlLine);
    int t = 0;
    // Iterate through the line
    for (size_t x = 0; x < line.size(); x++)
      {
      run_start[0] = t;
      current_label = line[x].second;
      t += line[x].first;
      if (current_label != 0)
        {
        current_meshinfo = &meshmap[current_label];
        // Update the current mesh info
        UpdateMeshInfoHelper(current_meshinfo, run_start, it, t);
        current_meshinfo = &meshmap[current_label];
        }
      }
    ++(it.bi);
    it.rlLine = &it.bi.Value();
    }

  // At this point, meshmap has the number of voxels for every label, as well
  // as the checksum for every label and the extent for every label. Now we
  // can determine which meshes actually need to be updated

  // First we go through the stored mesh map and delete all meshes that are no
  // longer present in the image
  for(MeshInfoMap::iterator it = m_MeshInfo.begin(); it != m_MeshInfo.end();)
    {
    if(meshmap.find(it->first) == meshmap.end())
      m_MeshInfo.erase(it++);
    else
      it++;
    }

  // Deal with progress accumulation
  SmartPtr<AllPurposeProgressAccumulator> progress = AllPurposeProgressAccumulator::New();
  progress->AddObserver(itk::ProgressEvent(), progressCommand);

  // Next we check which meshes are new or updated and mark them as needing to
  // be recomputed
  for(MeshInfoMap::const_iterator it = meshmap.begin(); it != meshmap.end(); ++it)
    {
    // Get the cached mesh info for this label
    MeshInfo &info = m_MeshInfo[it->first];

    // Compare the values
    if(info.Count != it->second.Count || info.CheckSum != it->second.CheckSum)
      {
      // Cache the current information
      info.CheckSum = it->second.CheckSum;
      info.Count = it->second.Count;
      info.BoundingBox[0] = it->second.BoundingBox[0];
      info.BoundingBox[1] = it->second.BoundingBox[1];
      info.Mesh = NULL;

      //auto src = m_VTKPipeline->GetProgressAccumulator();

      // Capture progress from this mesh
      progress->RegisterSource(m_VTKPipeline->GetProgressAccumulator(), info.Count);
      }
    }

  // Now compute the meshes
  for(MeshInfoMap::iterator it = m_MeshInfo.begin(); it != m_MeshInfo.end(); it++)
    {
    if(it->second.Mesh == NULL)
      {
      // Create the mesh
      MeshInfo &mi = it->second;
      mi.Mesh = vtkSmartPointer<vtkPolyData>::New();

      // TODO: make this more elegant
      InputImageType::RegionType bbWiderRegion;
      for(int d = 0; d < 3; d++)
        {
        unsigned long len =
            (unsigned long) (1 + mi.BoundingBox[1][d] - mi.BoundingBox[0][d]);
        bbWiderRegion.SetIndex(d, mi.BoundingBox[0][d]);
        bbWiderRegion.SetSize(d, len);
        }
      bbWiderRegion.PadByRadius(5);
      bbWiderRegion.Crop(m_InputImage->GetLargestPossibleRegion());

      // Pass the region to the ROI filter and propagate the filter
      m_ROIFilter->SetInput(m_InputImage);
      m_ROIFilter->SetRegionOfInterest(bbWiderRegion);
      m_ROIFilter->Update();

      // Set the parameters for the thresholding filter
      m_ThrehsoldFilter->SetLowerThreshold(it->first);
      m_ThrehsoldFilter->SetUpperThreshold(it->first);
      m_ThrehsoldFilter->UpdateLargestPossibleRegion();

      // Graft the polydata to the last filter in the pipeline
      m_VTKPipeline->SetImage(m_ThrehsoldFilter->GetOutput());
      m_VTKPipeline->ComputeMesh(it->second.Mesh);

      // Update progress
      progress->StartNextRun(m_VTKPipeline->GetProgressAccumulator());
      }
    }

  // Clean up the progress
  progress->UnregisterAllSources();

  // Set the modified flag, so we can use the pipeline's MTime
  this->Modified();
}

void 
MultiLabelMeshPipeline
::SetImage(const InputImageType *image)
{
  if(m_InputImage != image)
    {
    m_InputImage = image;
    m_MeshInfo.clear();
    }
}


MultiLabelMeshPipeline::MeshInfo::MeshInfo()
{
  this->Mesh = NULL;
  this->Count = 0;
  this->CheckSum = adler32(0L, NULL, 0);
}

MultiLabelMeshPipeline::MeshInfo::~MeshInfo()
{
}


std::map<LabelType, vtkSmartPointer<vtkPolyData> > MultiLabelMeshPipeline::GetMeshCollection()
{
  std::map<LabelType, vtkSmartPointer<vtkPolyData> > meshes;
  for(MeshInfoMap::iterator it = m_MeshInfo.begin(); it != m_MeshInfo.end(); ++it)
    meshes[it->first] = it->second.Mesh;
  return meshes;
}

SmartPtr<MultiLabelMeshPipeline>
MultiLabelMeshPipelineTable::GetPipeline(unsigned int timepoint)
{
  return m_table[timepoint];
}

void
MultiLabelMeshPipelineTable::SetPipeline(unsigned int timepoint, SmartPtr<MultiLabelMeshPipeline> pipeline)
{
  m_table[timepoint] = pipeline;
  uint32_t size = GetPipelineMemorySize(pipeline);
  m_MemoryUsage += size;

  // Remove item from the front of the table if memory usage exceeds threshold
  // -- but will keep item if it's the only item in the table
  // -- because we still want to render single huge mesh
  if (m_MemoryUsage > m_MemoryLimit && m_table.size() > 1)
    {
      //printf("[table trimming] usage = %u MB\n", m_MemoryUsage);
      m_MemoryUsage -= GetPipelineMemorySize(m_table.begin()->second);
      m_table.erase(m_table.begin());
    }
}

uint32_t
MultiLabelMeshPipelineTable::GetPipelineMemorySize(SmartPtr<MultiLabelMeshPipeline> pipeline)
{
  std::map<LabelType, vtkSmartPointer<vtkPolyData>> collection = pipeline->GetMeshCollection();
  uint32_t sum = 0;
  for (auto pair : collection)
    {
      sum += (pair.second->GetActualMemorySize())/1024;
    }
  return sum;
}

void
MultiLabelMeshPipelineTable::printMeshSize()
{
  for (auto pit = m_table.cbegin(); pit != m_table.cend(); ++pit)
  {
    unsigned int timepoint = pit->first;
    cout << "Timepoint: " << timepoint << "------------------------" << endl;
    double totalSize = 0;
    SmartPtr<MultiLabelMeshPipeline> pipeline = pit->second;
    auto meshTable = pipeline->GetMeshCollection();
    for (auto mit = meshTable.cbegin(); mit != meshTable.cend(); ++mit)
      {
        LabelType lbl = mit->first;
        auto pMesh = mit->second;
        unsigned long size = pMesh->GetActualMemorySize();
        printf("Label %d: %d KB\n", lbl, (int) size);
        totalSize += size / 1024.0;
      }
    cout << "Total Size: " << totalSize << " MB" << endl;
  }
}

















