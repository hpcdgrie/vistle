#ifndef MAIAVISPROGRESS_H_
#define MAIAVISPROGRESS_H_

#include "vtkAlgorithm.h"
#include "vtkMultiBlockDataSetAlgorithm.h"

namespace maiapv{

// Pointer to plugin object, which is needed to set progress bar from anywhere
extern vtkMultiBlockDataSetAlgorithm* mainPlugin;

// Set pointer to the main class
void setProgressMain(vtkMultiBlockDataSetAlgorithm* const main) {
  mainPlugin = main;
}

// Update the progress with a new value and text
void setProgressUpdate(const MFloat progress, const MString message) {
  if (mainPlugin != nullptr) {
    mainPlugin->UpdateProgress(std::min(progress, 1.0));
    mainPlugin->SetProgressText(message.c_str());
  } else {
    cout << "Progress bar could not be updated due to uninitialized pointer"
         << endl;
  }
}

}
#endif
