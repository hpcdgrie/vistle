/**########################################################
 *
 *                AIA READER 
 *
 *########################################################
 *
 *
 * This file constitutes the reader which enables the 
 * communication with maia and retrieves the information
 * from maia, i.e., the full access to the cartesian-
 * grid is granted
 * 
 * This function is based in its orginal form on Micheals
 * concept. The reader was cleaned and heavily extended by
 * the multi solver approach
 * 
 * @author: Pascal S. Meysonnat
 * @date:   back in the 90
 * 
 * If you have any questions --> p.meysonnat@aia.rwth-aachen.de
 */


#ifndef MAIAVISREADER_H_
#define MAIAVISREADER_H_

#define MAIA_DEBUG_OUTPUT
#define MAIA_GLOBAL_VARIABLES_
#define DEBUG_H

#include <algorithm>
#include <vector>
#include <string>
#include <bitset>
#include "mpi.h"
#include <iostream>

using namespace maiapv;

//static std::ostream& m_log = std::cerr;

#define MAIA_SWALLOW_EXCEPTIONS(RET_VAL)                         \
  catch (const std::exception& e) {                             \
    vtkErrorMacro("Exception thrown: " << e.what());            \
    std::cerr << "Exception thrown: " << e.what() << std::endl; \
    return RET_VAL;                                             \
  }

#ifndef NDEBUG
#define ASSERT_BOUNDS(i, lower, upper)                        \
  if (i < lower || i >= upper) {                              \
    mTerm(1, FUN_, "index " + to_string(i)   \
                + " out-of-bounds [" + to_string(lower) + ", " \
                + to_string(upper) + ")");                    \
  }
#else
#define ASSERT_BOUNDS(i, lower, upper)
#endif

//include vtk related header
#include "vtkDoubleArray.h"
#include "vtkIntArray.h"
#include "vtkPointData.h"
#include "vtkPoints.h"
#include "vtkPolyVertex.h"
#include "vtkQuad.h"
#include "vtkSmartPointer.h"
#include "vtkStringArray.h"
#include "vtkUnstructuredGrid.h"
#include "vtkMultiBlockDataSet.h"
#include "vtkVoxel.h"
#include "vtkVertex.h"

using namespace std;

//include maia related header
#include "compiler_config.h"
#include "IO/parallelio.h"
#include "DG/dgcartesianinterpolation.h"
#include "DG/sbpcartesianinterpolation.h"
#include "MEMORY/collector.h"
#include "FV/fvcartesiancellproperties.h"
#include "GRID/cartesiangridcellproperties.h"
#include "GRID/cartesiangridio.h"
#include "GRID/cartesiangrid.h"
#include "GRID/cartesiangridtree.h"
#include "GRID/cartesiangridproxy.h"
#include "typetraits.h"
#include "UTIL/debug.h"
#include "INCLUDE/maiamacro.h"

//include reader related header
#include "box.h"
#include "constants.h"
#include "dataset.h"
#include "block.h"
#include "filesystem.h"
#include "point.h"
#include "progress.h"
#include "vtktools.h"
#include "timers.h"
#include "timernames.h"
#include "readerbase.h"

namespace maiapv {

template <MInt nDim>
  struct VisBlock{
    MString solverName;
    MInt solverId;
    MBool isDgOnly;
    MBool isActive;
    MInt noCellsVisualized;
    set<MInt> vertices;
    vector<MInt> vertexIds;
    vector<MInt> visCellIndices;
    MInt visCellHaloOffset;
  };

  using namespace maiapv;

/**  Main class for building a VTK multi solver grid from Cartesiangrid 
 **  real dependency from maia!!!!!!!! 
 **  based on Mic's test plugin
 **  Improved "features" and complete overhoaled structure
 **
 **  @author: Pascal Meysonnat
 **  @date: back in the 90
 **
 */
template <MInt nDim> class Reader : public ReaderBase {
public:
  //constructor
  Reader(const MString& gridFileName, const MString& dataFileName, const MPI_Comm comm);
  //destructor
  ~Reader();

  //member funcitons
  //function to dermine the filetype and other important information (,e.g., dimensions, number of cells, bounding box)
  void determineFileType(const MString&, const MString&);
  //function setting the parallelization information (,e.g., the rank, the mpi size)
  void getParallelInfo();
  //function to return the MPI communicator (not really necessary but for the sake of similarity to maia it is used)
  MPI_Comm mpiComm() const { return m_mpiComm; }
  //function to return the MPI rank (not really necessary but for the sake of similarity to maia it is used)
  MInt domainId() const { return m_mpiRank; }
  //function to return the MPI size (not really necessary but for the sake of similarity to maia it is used)
  MInt noDomains() const { return m_mpiSize; }
  //function to return if the rank is the root process
  MBool isMpiRoot() const { return domainId() == 0; }
  //function to return the solver Id
  MInt getSolverId();
  //function to set the visualization information
  void setVisualizationInformation(
      vtkMultiBlockDataSet *grid, std::vector<maiapv::SolverPV> &solvers,
      const MInt visType, const MInt noVisNodes, const MInt *visLevels,
      MBool leafCellsOnly, MBool visBox, const MFloat *visBoxPoints,
      const MBool visualizeSbpData);
  //function to request the information of the datasets
   void requestInformation(vector<Dataset>& datasets);
   //function to set the visualization level information
   void setLevelInformation(int *levelRange);
   //function to return the bounding box from file
   void requestBoundingBoxInformation(double (&boundingBox)[6]);
   //function to return the time series information
   MFloat requestTimeInformation(); 
   //determine the levels up to which the visualization should take place
   void determineVisualizationLevelIndex(MInt* levels);
   //function to determine the file type and file information
   MBool requestFiletype();
   //function to return the memory increase proposed due to the average cell distribution
   double getMemoryIncrease();
   //function to set the memory increase for the average cell distribution
   void setMemoryIncrease(double increase);
   //function to return the number of solvers
   MInt requestNumberOfSolvers();
   //function to compute the number degrees of freedom (Dolce & Gabana)
   void computeDofDg();
   //function to compute the vertices
   void calcVertices();
   //function to call the read
   void read();
   //function to read the datasets
   void read(const vector<Dataset>& datasets);
   //function to load the data from the solution file
   void loadSolutionData(ParallelIo& datafile);
   //function to load information of Lagrange particles
   void readParticleData(vtkMultiBlockDataSet*, const vector<Dataset>&);
   //function to load additional data for the grid, e.g., levelId
   void loadGridAuxillaryData(vtkUnstructuredGrid* grid, const MInt b);
   //function to build the VTK multi solver grid
   void buildVtkGrid();
   //function to init the interpolation (Dolce & Gabana)
   void initInterpolation(const MInt maxNoNodes1D);
   //function to return min/max point of the domain extent
   void determineDomainExtent(Point<nDim>& min, Point<nDim>& max);
   //function to add data to the cells
   template <typename T>
   void addCellData(const MString& name, const T* data, vtkUnstructuredGrid* grid, MInt solverId,
                    const MBool solverLocalData = false);

  private:
   long m_noCells =-1;
   vector<long>m_noCellsBySolver{};
   MString m_gridFileName{};
   MString m_dataFileName{};
   MBool m_isParticleFile = false;
   MBool m_isDataFile = false;
   MBool m_isDgFile = false;
   MFloat m_boundingBox[2*nDim];
   MPI_Comm m_mpiComm = MPI_COMM_NULL;
   int m_mpiRank=-1;
   int m_mpiSize=-1;
   MLong m_32BitOffset=0;

   //solver related
   MInt m_noSolvers=-1;
   MInt m_solverId=-1;
   std::vector<maiapv::SolverPV> m_solvers{};
   std::vector<maiapv::VisBlock<nDim>> m_visBlocks{};
   // cell-related
   vector<MInt> m_visCellIndices{};
   const MInt* const m_binaryId = (nDim == 2) ? binaryId2D : binaryId3D;
   MBool m_useHaloCells = false;
   MInt m_noHaloLevels=0;
   //grids
   vtkMultiBlockDataSet* m_vtkGrid = nullptr;
   CartesianGrid<nDim>* m_maiagrid = nullptr;

   // Timer-related
   Timers timers;
   array<MInt, TimerNames::_count> m_timers{};

   //memory related
   MFloat m_memIncrease=-1.0;

   //visualization related
   //MInt m_noDOFsVisualized = -1;
   MInt m_visType = -1;
   MFloat m_visualizationBoxMinMax[2*nDim];
   MBool m_useVisualizationBox = false;
   vector<Dataset> m_datasets{};
   //DG related stuff
   MInt  m_offsetLocalDG = -1;
   MInt  m_dofDg[2]={-1,-1};
   MString m_intMethod{};
   MString m_polyType{};
   MInt* m_polyDeg= nullptr;
   MInt* m_noNodes1D = nullptr;
   vector<DgInterpolation> m_interp{};
   MInt m_noVisNodes = -1;
   MBool m_visualizeSbpData = false;
   MInt m_visLevels[2]={-1,0};
   MBool m_leafCellsOn = true;
   vector<vector<MFloat>> m_vandermonde{};
   vector<maia::grid::Proxy<nDim>*>m_maiaproxy{};
   vector<Point<nDim>> m_vertices;
};

/** \brief Constructor of the reading class
 *
 * @author Pascal S. Meysonnat
 * @date back in the 90
 *  
 */
template <MInt nDim>
  Reader<nDim>::Reader(const MString& gridFileName, const MString& dataFileName, const MPI_Comm comm)
  : m_noCells(0),
  m_mpiComm(comm){
    
    TRACE();
    m_useVisualizationBox =false;
    // get MPI rank and size and encapsulate them in function
    getParallelInfo();
    
    m_useHaloCells = (noDomains() > 1) ? true : false;
    if(m_useHaloCells){m_noHaloLevels=1;}
    determineFileType(gridFileName, dataFileName);
  }

/** \brief Destructor of the reading class
 *
 * @author Pascal S. Meysonnat
 * @date back in the 90
 *  
 */
template <MInt nDim>
  Reader<nDim>::~Reader(){
  if(m_maiagrid != nullptr){
    delete m_maiagrid; //delete the cartesian grid
  }
}

/** \brief Function to determine the filetype and extract basic information
 *
 * @author Pascal S. Meysonnat
 * @date back in the 90
 *  
 */
template <MInt nDim>
void Reader<nDim>::determineFileType(const MString& gridFileName, const MString& dataFileName) {
  TRACE();
  // Extract path and filename from full String
  m_gridFileName = gridFileName;
  m_dataFileName = dataFileName;
  if(!dataFileName.empty()){
    //we have a datafile 
    m_isDataFile=true;
  }else{
    m_isDataFile = false;
  }
  if(gridFileName.empty()){
    if(!dataFileName.empty()) {
      m_isParticleFile = true;
      m_isDataFile = false;
    } else {
      mTerm(1, FUN_, "Error no gridfile is known");
    }
  }


  // Particle files have differnet format. They do not have a grid.
  if ( m_isParticleFile ) {
    m_noSolvers = -1;
    m_solverId = -1;
    for ( MInt d=0; d<2*nDim; d++ ) {
      m_boundingBox[d] = F0;
    }
    return;
  }

  ParallelIo gridfile(m_gridFileName, maia::parallel_io::PIO_READ, mpiComm());
  gridfile.getAttribute(&m_noCells,"noCells");
  //according to the number of cells/core propose a memory increase
  m_memIncrease=1.0;
  MFloat noCellsPerCore=m_noCells/noDomains();
  if(noCellsPerCore > 50000000){
    m_memIncrease=1.1;
  } else if (noCellsPerCore > 10000000 && noCellsPerCore <= 50000000 ){
    m_memIncrease=1.25;
  } else if (noCellsPerCore >2000000 && noCellsPerCore <= 10000000){
    m_memIncrease =1.4;
  } else if (noCellsPerCore <=2000000){
    m_memIncrease =1.6;
  }
  if(noDomains()==1){ m_memIncrease=1.0;}




  if(gridfile.hasAttribute("bitOffset")){gridfile.getAttribute(&m_32BitOffset, "bitOffset");}
  //read the bounding box
  gridfile.getAttribute(m_boundingBox, "boundingBox", 2*nDim);

  // TODO add support for multisolver bounding box
  MFloat centerOfGravity[3];
  gridfile.getAttribute(&centerOfGravity[0], "centerOfGravity", nDim);
  for (MInt i = 0; i < nDim; i++) {
    const MFloat cog = 0.5 * (m_boundingBox[nDim + i] + m_boundingBox[i]);
    const MFloat eps = 1e-8;
    if (centerOfGravity[i] > cog + eps || centerOfGravity[i] < cog - eps) {
      std::cerr << "WARNING: center of gravity mismatch " << centerOfGravity[i] << " " << cog
                << std::endl;
    }
  }

  //get the number of solvers in the file 
  gridfile.getAttribute(&m_noSolvers, "noSolvers");
  if (!gridfile.hasAttribute("noPartitionCells")) {
    mTerm(1, FUN_, "This reader does not support older MAIA file formats");
  }

  if(m_isDataFile){
    ParallelIo datafile(m_dataFileName, maia::parallel_io::PIO_READ, mpiComm());
    if(datafile.hasAttribute("solverId") && m_noSolvers >1){
      datafile.getAttribute(&m_solverId, "solverId");
    }else if (m_noSolvers == 1){
      m_solverId=0;
    }else{
      mTerm(-1, FUN_, "ERROR: We have a multi solver grid but no solver id in solution file");
    }

    // Check if data file is DG file
    m_isDgFile = datafile.hasDataset("polyDegs");
  }

  //check for the cells in each solver (except DG)
  if (m_isDataFile && m_noSolvers > 1 && !m_isDgFile) {
    ParallelIo datafile(m_dataFileName, maia::parallel_io::PIO_READ, mpiComm());
    for (MInt b = 0; b < m_noSolvers; b++) {
      long noCells = 0;

      stringstream solverName;
      solverName << b;
      string noCellName = "noCells_" + solverName.str();
      if (gridfile.hasAttribute(noCellName)) {
        gridfile.getAttribute(&noCells, noCellName);
      } else if (b == m_solverId) {
        // Get the number of cells from the datafile for the selected solver
        // TODO fix this for the visualization of multiple solvers
        datafile.getAttribute(&noCells, "noCells");
      }

      m_noCellsBySolver.push_back(noCells);
    }

    // Note: this assumes the number of cells for each solver is stored in the grid file, which is
    // only true if multisolverGrid is enabled during grid generation
    //for(MInt b=0; b<m_noSolvers; b++){
    //  stringstream solverName;
    //  solverName << b;
    //  string noCellName="noCells_"+solverName.str();
    //  long noCells=0;
    //  gridfile.getAttribute(&noCells,noCellName);
    //  m_noCellsBySolver.push_back(noCells);
    //}
  } else if (m_isDgFile) {
    m_noCellsBySolver.push_back(-1);
  } else {
    m_noCellsBySolver.push_back(m_noCells);
  }
}


/** \brief Function to return the number of solvers (single/multi solver =1/>1)
 *
 * @author Pascal S. Meysonnat
 * @date back in the 90
 *  
 */
template <MInt nDim>
  MInt Reader<nDim>::requestNumberOfSolvers(){
  return m_noSolvers;
}

/** \brief Function to return the dataset names in the file 
 *
 * @author Pascal S. Meysonnat, Michael Schlottke-Lakemper
 * @date back in the 90, 
 *
 * Comment: from old plugin
 *  
 */
template <MInt nDim>
void  Reader<nDim>::requestInformation(vector<Dataset>& datasets){
  MBool datasetsAvailable = (datasets.size()>0) ? 1:0;
  if(m_isDataFile){
    ParallelIo datafile(m_dataFileName, maia::parallel_io::PIO_READ, mpiComm());
    if(!datasetsAvailable){
      datasets.clear();
      MInt datasetSize = 0;
      // Determine the required size of the variables
      if (m_isDgFile) {
        // Use the size of the first dataset as the required size for DG cases
        // to circumvent the use/requirement of the polyDegs array
        if (datafile.hasDataset("variables0")) {
          datasetSize = datafile.getArraySize("variables0");
        } else {
          TERMM(1,"Dataset 'variables0' doesn't exits and therefore the required size cannot be determined.");
        }
      } else {
        datasetSize=m_noCellsBySolver.at(m_solverId);
      }
      for (auto&& variableName : datafile.getDatasetNames()) {
        // Check if the dataset is an array or scalar
        if (datafile.getDatasetNoDims(variableName) > 0) {
          if (datafile.getArraySize(variableName) == datasetSize) {
            // Query the actual variable name and save both names to the
            // respective vectors
            MString realName;
            if (datafile.hasAttribute("name", variableName)) {
              datafile.getAttribute(&realName, "name", variableName);
            } else {
              realName = variableName;
            }
            // Just tick the checkboxes for variablesN
            MBool status = false;
            if (variableName.find("variables") != MString::npos) {
              status = true;
            }
            datasets.push_back(
              {variableName, realName, status, false, false});
          }
        }
      }
    }
  } else if (m_isParticleFile) {
    ParallelIo partFile(m_dataFileName, maia::parallel_io::PIO_READ, mpiComm());
    if(!datasetsAvailable){
      datasets.clear();
      MLong datasetSize = 0;
      if ( partFile.hasDataset("partDia") ) {
        datasetSize = partFile.getArraySize("partDia");
      } else {
        TERMM(1,"Dataset 'partDia' doesn't exits and therefore the required size cannot be determined.");
      }
      for (auto&& variableName : partFile.getDatasetNames()) {
        // Check if the dataset is an array or scalar
        if (partFile.getDatasetNoDims(variableName) == 1) {
          if (partFile.getArraySize(variableName) == datasetSize ||
              partFile.getArraySize(variableName) == nDim*datasetSize) {
            // Just tick the checkboxes for variablesN
            MBool status = true;
            datasets.push_back(
              {variableName, variableName, status, false, false});
          }
        }
      }
    }
    return; // Nothing further to be done for particles
  }

  // Only load grid dataset information if not yet present
  if (!datasetsAvailable) {
    // Add grid datasets
    for (auto&& dataset : gridsets) {
      // Skip DG datasets for non-DG files
      if (!m_isDgFile && dataset.isDgOnly) {
        continue;
      }
      // Otherwise, add grid dataset to list of available datasets
      datasets.push_back(dataset);
      // When loading data file, grid datasets unchecked by default.
      if (m_isDataFile) {
        datasets.back().status = false;
      }
    }
  }
}

/** \brief Function to return the bounding box values
 *
 * @author Pascal S. Meysonnat
 * @date back in the 90
 *  
 */  
template <MInt nDim>
void Reader<nDim>::requestBoundingBoxInformation(double (&boundingBox)[6]){
  for(MInt i=0; i<2*nDim; i++) boundingBox[i]=m_boundingBox[i];
}

/** \brief Function to return the time series information
 *
 * @author Pascal S. Meysonnat, Michael Schlottke-Lakemper
 * @date back in the 90, 
 *
 * Comment: from old plugin
 *  
 */
template <MInt nDim>
MFloat  Reader<nDim>::requestTimeInformation(){
  ParallelIo datafile(m_dataFileName, maia::parallel_io::PIO_READ, mpiComm());
  MFloat time=0.000000;
  if (datafile.hasAttribute("time")) {
    datafile.getAttribute(&time, "time");
  } else if (datafile.hasDataset("time")) {
    datafile.readScalar(&time, "time");
  } else {
    time = -std::numeric_limits<MFloat>::infinity();
  }
  return time;
}

/** \brief Function to return the min/max level in the file
 *
 * @author Pascal S. Meysonnat
 * @date back in the 90
 *  
 */  
template <MInt nDim>
void Reader<nDim>::setLevelInformation(int* levelRange){

  // Particles do not have a gird
  if ( m_isParticleFile ) return;
  
  ParallelIo gridfile(m_gridFileName, maia::parallel_io::PIO_READ, mpiComm());\
  //get the min level
  if (gridfile.hasAttribute("minLevel")) {
    int a=0;
    gridfile.getAttribute(&a, "minLevel");
    levelRange[0]=(double)a;
  }
  //get the max level
  if (gridfile.hasAttribute("maxLevel")) {
    int a=0;
    gridfile.getAttribute(&a, "maxLevel");
    levelRange[1]=(double)a;
  }
}

/** \brief Function to the visualization information
 *
 * @author Pascal S. Meysonnat
 * @date back in the 90
 *  
 */
template <MInt nDim>
void Reader<nDim>::setVisualizationInformation(
    vtkMultiBlockDataSet *grid, vector<maiapv::SolverPV> &solvers, const MInt visType,
    const MInt noVisNodes, const MInt *visLevels, MBool leafCellsOnly,
    MBool visBox, const MFloat *visBoxPoints,
    const MBool visualizeSbpData) {

  if (m_isDgFile) {
    // Set visualization type
    m_visType = visType;

    // Set number of visualization nodes
    m_noVisNodes = noVisNodes;

    // Set if file should be visualized as SBP solution
    m_visualizeSbpData = visualizeSbpData;
    // If SBP visualization is selected, number of visualization nodes is reset
    // to -1, as there is (at the moment) only one supported interpolation
    // method
    if (m_visualizeSbpData) {
      m_noVisNodes = -1;
    }
  } else {
    // For non-DG files, visualization type must be Classic cells
    m_visType = VisType::ClassicCells;
  }
  
  //correct number of cells 
  //average per core * increase which is either user chosen or proposed
  m_noCells=(m_noCells/noDomains())*m_memIncrease;

  m_solvers =solvers;
  //select the solvers which are to be visualized
  for(MInt b=0; b<(MInt)m_solvers.size();b++){
    if(m_solvers[b].status){//yeah is to visualize
      m_visBlocks.push_back({m_solvers[b].solverName, m_solvers[b].solverId, m_solvers[b].isDgOnly, true, -1, {},{},{}, -1});
    }
  }
  //set the grid
  m_vtkGrid=grid;
  //set the visualization levels
  m_visLevels[0]=visLevels[0];//set Min Visualization Level
  m_visLevels[1]=visLevels[1];//set Max Visualization Level
  //set bool if a visualization box is to be used
  m_useVisualizationBox=visBox;
  //set the min and max point of the visualization box
  if(m_useVisualizationBox){
    for(MInt i=0; i<2*nDim; i++) m_visualizationBoxMinMax[i]=visBoxPoints[i];
  }
  //set if leaf cells only is turned on for level range
  m_leafCellsOn=leafCellsOnly;
}

/** \brief Retrunt the proposed memory increase determined from average cells
 *
 * @author Pascal S. Meysonnat
 * @date back in the 90
 *  
 */
template <MInt nDim>
MFloat Reader<nDim>::getMemoryIncrease(){
  return m_memIncrease;
}

/** \brief Set memory increase from average cels
 *
 * @author Pascal S. Meysonnat
 * @date back in the 90
 *  
 */
template <MInt nDim>
void Reader<nDim>::setMemoryIncrease(double increase){
  m_memIncrease=increase;
}




/** \brief Function to the visualization information
 *
 * @author Pascal S. Meysonnat
 * @date back in the 90
 *  
 */  
template <MInt nDim>
MBool Reader<nDim>::requestFiletype(){
  return m_isDgFile;
}




/** \brief Function to return the solver id (e.g., the solver id in multi solver grids)
 *
 * @author Pascal S. Meysonnat
 * @date back in the 90
 *  
 */  
template <MInt nDim>
  MInt Reader<nDim>::getSolverId(){
  return m_solverId;
}


/** \brief Get the rank of the processor and size of the parallel communicator
 **
 ** @author Pascal Meysonnat, Michael Schlottke-Lakemper
 * @date back in the 90
 **
 */
template <MInt nDim> void Reader<nDim>::getParallelInfo() {
  TRACE();

  MPI_Comm_rank(mpiComm(), &m_mpiRank);
  MPI_Comm_size(mpiComm(), &m_mpiSize);
}

/** \brief Function to compute the degrees of freedom (Dolce and Gabana)
 **
 ** @author Pascal S. Meysonnat, Ansgar Niemoeller, Michael Schlottke-Lakemper
 ** @date back in the 90
 **
 */
template <MInt nDim>
  void Reader<nDim>::computeDofDg() {
  if(m_isDgFile){
    // Exchange polyDegs of halo/window cells --> Ansgar Niemoeller
    // Note: this function should only be called by the active ranks for this solver!
    std::cerr << "exchange polynomial degrees of window/halo cells" << std::endl;
    m_maiaproxy[m_solverId]->exchangeHaloCellsForVisualization(&m_polyDeg[0]);

    if(m_visualizeSbpData){
      std::cerr << "exchange number of nodes of window/halo cells" << std::endl;
      m_maiaproxy[m_solverId]->exchangeHaloCellsForVisualization(&m_noNodes1D[0]);
    }

    m_dofDg[0] = 0;
    for (MInt i = 0; i < m_maiaproxy[m_solverId]->noInternalCells(); i++) {
      const MInt noNodes1D = m_visualizeSbpData ? m_noNodes1D[i] : m_polyDeg[i]+1; 
      m_dofDg[0] += ipow(noNodes1D, nDim);
    }
    m_dofDg[1] = m_dofDg[0];
    for (MInt i = m_maiaproxy[m_solverId]->noInternalCells(); i < m_maiaproxy[m_solverId]->noCells();
         i++) {
      const MInt noNodes1D = m_visualizeSbpData ? m_noNodes1D[i] : m_polyDeg[i]+1;
      m_dofDg[1] += ipow(noNodes1D, nDim);
    }
    // Calculate offset for DOFs
    MPI_Exscan(&m_dofDg[0], &m_offsetLocalDG, 1, maia::type_traits<MInt>::mpiType(), MPI_SUM,
               m_maiaproxy[m_solverId]->mpiComm());
    if (m_maiaproxy[m_solverId]->domainId() == 0) {
      m_offsetLocalDG = 0;
    }
    // Calculate DOFs of visualized cells // unused
    //m_noDOFsVisualized = 0;
    //for (size_t i = 0; i < m_visCellIndices.size(); i++) {
    //  const MInt cellId = m_visCellIndices.at(i);
    //  m_noDOFsVisualized += ipow(m_polyDeg[cellId] + 1, nDim);
    //}
  }
}

/** \brief Store datasets and call main read() method.
 **
 ** @author Michael Schlottke, Pascal S. Meysonnat 
 ** @date back in the 90
 **
 ** \param.at(in) datasets List of available datasets with information on whether
 **                     they should be loaded.
 */

template <MInt nDim>
void Reader<nDim>::read(const vector<Dataset>& datasets) {
  TRACE();

  // Create timer for the measurements
  m_timers.at(TimerNames::read) = timers.create("Read");

  m_timers.at(TimerNames::readGridFile)
    = timers.create("ReadGridFile", m_timers.at(TimerNames::read));

  m_timers.at(TimerNames::buildGrid)
    = timers.create("BuildGrid", m_timers.at(TimerNames::read));
  m_timers.at(TimerNames::calcDOFs)
    = timers.create("CalcDOFs", m_timers.at(TimerNames::buildGrid));
  m_timers.at(TimerNames::buildVtkGrid)
    = timers.create("BuildVtkGrid", m_timers.at(TimerNames::buildGrid));
  m_timers.at(TimerNames::loadGridData)
    = timers.create("LoadGridData", m_timers.at(TimerNames::buildGrid));
  m_timers.at(TimerNames::loadSolutionData)
    = timers.create("LoadSolutionData", m_timers.at(TimerNames::read));

  timers.start(m_timers.at(TimerNames::read));

  m_datasets = datasets;

  read();
}

/** \brief Main method reading in the data
 **  based on Michaels test paraview plugin
 **  now real dependency on MAIA 
 **  and new features
 **
 **  @author Pascal Meysonnat
 **  @date back in the 90
 **
 */
template <MInt nDim> void Reader<nDim>::read() {
  TRACE();

  // Set first status of progress bar to low value
  setProgressUpdate(0.01, "Start reading MAIA file");

  // initialize cell collector:
  
  // Read grid file
  timers.start(m_timers.at(TimerNames::readGridFile));

  //we need to create a cartesian grid
  (m_noSolvers > 1) ? g_multiSolverGrid = 1 : g_multiSolverGrid = 0;

  //create a cartesian grid instance
  m_maiagrid = new CartesianGrid<nDim>((MInt)m_noCells, m_boundingBox, mpiComm(), m_gridFileName);

  m_maiaproxy.clear();
  for(MInt b=0; b<m_noSolvers; b++){
    maia::grid::Proxy<nDim>* a = new maia::grid::Proxy<nDim>(b,*m_maiagrid);
    m_maiaproxy.push_back(a);
  }

  for (MInt b = 0; b < (MInt)m_visBlocks.size(); b++) {
    const MInt solverId = m_visBlocks[b].solverId;
    m_visBlocks[b].isActive = m_maiaproxy[solverId]->isActive();
  }

  //m_cells_ = m_maiagrid->tree(); //original tree should be replaced by proxy
  timers.stop(m_timers.at(TimerNames::readGridFile));
  setProgressUpdate(0.1, "Finished reading MAIA file");

  // Build up grid structures with VTK
  timers.start(m_timers.at(TimerNames::buildGrid));

  // If this is a data file with DG data, we first need to read the polynomial degree info
  if (m_isDgFile) {
    ParallelIo datafile(m_dataFileName, maia::parallel_io::PIO_READ, mpiComm());
    datafile.getAttribute(&m_intMethod, "dgIntegrationMethod", "polyDegs");
    datafile.getAttribute(&m_polyType, "dgPolynomialType", "polyDegs");

    const MInt isActive = m_maiaproxy[m_solverId]->isActive();
    const MInt noCells = (isActive) ? m_maiaproxy[m_solverId]->noCells() : 0;
    const MInt noInternalCells = (isActive) ? m_maiaproxy[m_solverId]->noInternalCells() : 0;
    const MInt solverDomainId = (isActive) ? m_maiaproxy[m_solverId]->domainId() : -1;
    const MInt offset = (isActive) ? m_maiaproxy[m_solverId]->domainOffset(solverDomainId) : 0;

    mAlloc(m_polyDeg, std::max(noCells, 1), "dgPolynomialDeg", -1, AT_);
    datafile.setOffset(noInternalCells, offset);
    MUcharScratchSpace polyDegs(std::max(noInternalCells, 1), AT_, "polyDegs");
    datafile.readArray(polyDegs.data(), "polyDegs");
    std::copy_n(polyDegs.data(), noInternalCells, m_polyDeg);

    // Additioanlly read noNodes in sbpMode
    if(m_visualizeSbpData){
      mAlloc(m_noNodes1D, std::max(noCells, 1), "dgNoNodes1D", -1, AT_);
      datafile.setOffset(noInternalCells, offset);
      MUcharScratchSpace noNodes1D(std::max(noInternalCells, 1), AT_, "noNodes1D");
      datafile.readArray(noNodes1D.data(), "noNodes1D");
      std::copy_n(noNodes1D.data(), noInternalCells, m_noNodes1D);
    }
  }

  //build the grid 
  buildVtkGrid();
  timers.stop(m_timers.at(TimerNames::buildGrid));

  // If this is a data file, also load data, e.g., the solution data
  if (m_isDataFile) {
    //check if the number of solvers is the same as in the whole vector
    //for the moment the solution can only be read if every solver is present
    if(m_vtkGrid->GetNumberOfBlocks()!=(MInt)m_visBlocks.size()) {
      mTerm(-1, FUN_, "number of visualization solvers != number of solvers in vtkGrid");
    }  

    ParallelIo datafile(m_dataFileName, maia::parallel_io::PIO_READ, mpiComm());
    timers.start(m_timers.at(TimerNames::loadSolutionData));
    //load the solution data
    loadSolutionData(datafile);
    timers.stop(m_timers.at(TimerNames::loadSolutionData));
  }
  timers.print(mpiComm());
}

/** \brief Main method to build VTK grid
 **
 **  @author Pascal Meysonnat
 **  @date back in the 90
 **
 */
template <MInt nDim> void Reader<nDim>::buildVtkGrid() {
  TRACE();

  //reset the visualization flag in the solver if only one solver is present
  //just safety
  if(m_solvers.size() == 1){m_solvers[0].status=true;}
  
  //MOVE THIS FUNCTION TO other part 

  // Determine which cells are to be visualized (,e.g., cells without child cells)
  // that is for every solver you get cellIds to be visualized
  determineVisualizationLevelIndex(m_visLevels); 

  // Initialize interpolation for DG files
  if (m_isDgFile) {
    const MInt b = 0; // TODO fixme for visualization of multiple DG solvers
    if (m_visBlocks[b].isActive) {
      // Exchange the halo-cell polynomial degrees and compute the degrees of freedom
      computeDofDg();

      // Determine the maximum (local) polynomial degree (including halo cells) for interpolation
      const MInt maxPolyDeg = *max_element(
          &m_polyDeg[0], &m_polyDeg[0] + m_maiaproxy[m_solverId]->noCells());
      // Set for DG case
      MInt maxNoNodes1D = maxPolyDeg + 1;
      // Correct for SBP mode
      if(m_visualizeSbpData){
        maxNoNodes1D = *max_element(
          &m_noNodes1D[0], &m_noNodes1D[0] + m_maiaproxy[m_solverId]->noCells());
      }
      initInterpolation(maxNoNodes1D);
    } else {
      m_dofDg[0] = -1;
      m_dofDg[1] = -1;
      m_offsetLocalDG = -1;
    }
  }

  timers.start(m_timers.at(TimerNames::buildVtkGrid));
  // building the vtkGrid
  // Calculate the vertices (cell corner points) and vertex ids
  calcVertices();//vertices, vertexIds);

  //============== Build the vtkGrid ===================
  // 1) set the number of solvers
  m_vtkGrid->SetNumberOfBlocks(m_visBlocks.size());
  // 2) for each solver build a vtk unstructured grid
  for(MInt b=0; b<(MInt)m_visBlocks.size(); b++){
    const MInt solverId = m_visBlocks[b].solverId;
    const MBool isActive = m_visBlocks[b].isActive;

    //get the number of vertices
    const MInt noVertices = m_visBlocks[b].vertices.size();
    //build the unstructured grid
    vtkUnstructuredGrid* localUnstructuredGrid = vtkUnstructuredGrid::New();
    localUnstructuredGrid->Initialize();
    localUnstructuredGrid->Allocate(0,0);
    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    localUnstructuredGrid->SetPoints(points);
    localUnstructuredGrid->GetPoints()->SetNumberOfPoints(noVertices);
    //set the points of the grid
    MInt cnt = 0;
    map<MInt, MInt> vertexMapping;
    for (auto it=m_visBlocks[b].vertices.begin(); it != m_visBlocks[b].vertices.end(); ++it) {
      Point<3> p = {0.0, 0.0, 0.0};
      copy(m_vertices.at(*it).data(), m_vertices.at(*it).data() + nDim, &p.at(0));
      localUnstructuredGrid->GetPoints()->SetPoint(cnt, &p.at(0));
      vertexMapping.insert({*it, cnt});
      cnt++;
    }
    
    // Loop over all cells that are visualized
    MInt vertexId = 0;
    for (MInt i = 0; i < m_visBlocks[b].noCellsVisualized; i++) {
      // Create cell pointer and appropriate cell type, calculate number of
      // vertices per cell
      vtkCell* cell = nullptr;
      MInt noVerticesPerCell = 0;
      cell = Cell<nDim>::New();
      noVerticesPerCell = IPOW2(nDim);

      // Assign points to cell
      for (MInt j = 0; j < noVerticesPerCell; j++) {
        vertexId = vertexMapping[m_visBlocks[b].vertexIds[i*noVerticesPerCell+j]];
        cell->GetPointIds()->SetId(j, vertexId);
      }

      // Insert cell into grid
      localUnstructuredGrid->InsertNextCell(cell->GetCellType(), cell->GetPointIds());

      // Finally delete cell
      cell->Delete();
    }

    // TODO inactive ranks fixme
    //if (!isActive) {
    //  vector<unsigned char> ghostLevels(1);
    //  ghostLevels.at(0) = vtkDataSetAttributes::HIDDENCELL;
    //  addCellData("vtkGhostType", &ghostLevels.at(0), localUnstructuredGrid, 0, true);
    //}

    //add ghost cell data necessary for proper visualization of iso contours
    if (m_useHaloCells && isActive) {
      vector<unsigned char> ghostLevels(m_maiaproxy[solverId]->noCells(), 0);
      MInt noHalos = 0;
      for (MInt i = 0; i < (MInt)m_visBlocks[b].visCellIndices.size(); i++) {
        const MInt cellId = m_visBlocks[b].visCellIndices.at(i);
        if (m_maiaproxy[solverId]->tree().hasProperty(cellId, GridCell::IsHalo)) {
          ghostLevels.at(cellId)=vtkDataSetAttributes::DUPLICATECELL; // no distinction between different halo layers necessary anymore
          noHalos++;
        }
      }
      //std::cerr << domainId() << " marked halos: " << noHalos << std::endl;
      addCellData("vtkGhostType", &ghostLevels.at(0), localUnstructuredGrid, b, true);
      // Add for testing
      //addCellData("GhostType", &ghostLevels.at(0), localUnstructuredGrid, b, true);
    }
    timers.stop(m_timers.at(TimerNames::buildVtkGrid));
    
    timers.start(m_timers.at(TimerNames::loadGridData));
    // Add all grid-specific data to VTK grid
    loadGridAuxillaryData(localUnstructuredGrid, b);
    timers.stop(m_timers.at(TimerNames::loadGridData));
    
    //now set the solver in the multisolver dataset algorithm
    m_vtkGrid->SetBlock(b, localUnstructuredGrid);
    m_vtkGrid->GetMetaData(b)->Set(vtkCompositeDataSet::NAME(), m_visBlocks[b].solverName);
    //delete the local solver instance
    localUnstructuredGrid->Delete();
  }
}


/** /brief Determine the cellIndices of the cells to visualize
 **
 ** @author Pascal Meysonnat
 ** @date back in the 90
 **
 **
 */
template <MInt nDim> void Reader<nDim>::determineVisualizationLevelIndex(MInt* visLevels /*=-1*/) {
  TRACE();

  std::vector<MBool> isActive(m_visBlocks.size());
  for(MInt b=0; b<(MInt)m_visBlocks.size();b++){
    const MInt solverId=m_visBlocks[b].solverId;
    isActive[b] = m_visBlocks[b].isActive;

    if (isActive[b]) {
      m_visBlocks[b].visCellIndices.reserve(m_maiaproxy[solverId]->noCells());
    } else {
      std::cerr << "WARNING: domain " << domainId()
                << " is inactive and has no cells to visualize, parallel "
                   "processing of the data might fail."
                << std::endl;
      m_visBlocks[b].visCellIndices.clear();
      // TODO add one dummy cell to domains without cells since it is assumed that every process has
      // the datasets when processing in parallel. However, tagging this cell as HIDDENCELL seems to
      // not work at the moment
      //m_visBlocks[b].visCellIndices.push_back(0);
    }
  }

  //set the min and max visualization levels
  const MInt minVisLevel = visLevels[0];
  const MInt maxVisLevel = visLevels[1];

  // Visualization box
  Point<nDim> min;
  Point<nDim> max;
  if (m_useVisualizationBox) {
    for (MInt d = 0; d < nDim; d++) {
      min.at(d) = m_visualizationBoxMinMax[d];
      max.at(d) = m_visualizationBoxMinMax[nDim + d];
    }
  } else {
    min.fill(std::numeric_limits<MFloat>::infinity());
    max.fill(std::numeric_limits<MFloat>::infinity());
  }
  Box<nDim> visualizationBox(min, max);

  //std::cerr << "determine visualization cells: useBox=" << m_useVisualizationBox
  //          << ", levelRange=" << (minVisLevel != -1)
  //          << ", leafCells=" << m_leafCellsOn << std::endl;

  MBool onlyVisGridLeafCells = false;
  if ( !m_isDataFile ) onlyVisGridLeafCells = true;

  for(MInt b=0; b<(MInt)m_visBlocks.size();b++){
    if (!isActive[b]) continue;

    MInt solverId = m_visBlocks[b].solverId;

    std::vector<MInt> isVisCell(m_maiaproxy[solverId]->noCells(), 0);

    for (MInt i = 0; i < m_maiaproxy[solverId]->noInternalCells(); i++) {
      MBool vis = true;

      // Check for visualization box
      if (m_useVisualizationBox) {
        Point<nDim> p;
        for (MInt d = 0; d < nDim; d++) {
          p.at(d) = m_maiaproxy[solverId]->tree().coordinate(i, d);
        }

        // Cell is outside the visualization box
        if (!visualizationBox.isPointInBox(p)) {
          vis = false;
        }
      }

      // Check for visualization level range
      if (minVisLevel != -1) {
        const MInt level = m_maiaproxy[solverId]->tree().level(i);

        // Check if cell is outside the level range
        if (level < minVisLevel || level > maxVisLevel) {
          vis = false;
        }

        // Only show non-leaf cells on the maximum visualization level
        if (m_leafCellsOn && m_maiaproxy[solverId]->tree().hasChildren(i) && level != maxVisLevel) {
          vis = false;
        }
      } else {
        // Default: visualize all leaf cells
        if (m_leafCellsOn && m_maiaproxy[solverId]->tree().hasChildren(i)) {
          vis = false;
        }
      }

      // Check if cell is not leaf cell on other solver
      if ( onlyVisGridLeafCells && vis == true ) {
        MInt gridId = m_maiaproxy[solverId]->tree().solver2grid(i);

        for(MInt b2=0; b2<m_visBlocks.size();b2++){
          MInt solverId_2 = m_visBlocks[b2].solverId;

          if (solverId_2 == solverId) continue;
          if (!isActive[b2]) continue;
          if (!m_maiaproxy[solverId_2]->solverFlag(gridId, solverId_2)) continue;

          MInt solverCellId = m_maiaproxy[solverId_2]->tree().grid2solver(gridId);
          // Only show non-leaf cells on the maximum visualization level
          if (m_leafCellsOn && (m_maiaproxy[solverId_2]->tree().noChildren(solverCellId) == m_maiaproxy[solverId_2]->m_maxNoChilds)) {

            vis = false;
          }
        }
      }

      // Add cell if it needs to be visualized
      if (vis){
        m_visBlocks[b].visCellIndices.push_back(i);
        isVisCell.at(i) = 1;
      }
    }

    const MInt visCellHaloOffset = m_visBlocks[b].visCellIndices.size();
    m_visBlocks[b].visCellHaloOffset = visCellHaloOffset;

    // TODO fix this for 'visualizing' inactive ranks
    const MInt tmpSolverId = solverId;

    // Exchange information which halo cells need to be 'visualized' (i.e. the corresponding
    // internal cell is visualized on another domain)
    m_maiaproxy[tmpSolverId]->exchangeHaloCellsForVisualization(&isVisCell[0]);

    // Add halo cells to be 'visualized'
    for (MInt i = m_maiaproxy[tmpSolverId]->noInternalCells(); i < m_maiaproxy[tmpSolverId]->noCells(); i++) {
      if (isVisCell.at(i)) {
        m_visBlocks[b].visCellIndices.push_back(i);
      }
    }
  }
}

/** \brief Function to compute the vertices depending on visualization level and visualization box
 *
 * @author Pascal S. Meysonnat, Michael Schlottke-Lakemper
 * @date back in the 90
 *
 */
template <MInt nDim>
void Reader<nDim>::calcVertices() {
  TRACE();

  // Calculate the bounding box for the grid by determining the maximum
  // coordinate extent, and determine
  // the maximum number of cell/integration nodes/visualization nodes
  Point<nDim> min, max;
  determineDomainExtent(min, max);
  Box<nDim> box(min, max);

  MLong totalNoCellsVisualized = 0;
  for(MInt b=0; b<(MInt)m_visBlocks.size(); b++){
    MInt solverId=m_visBlocks[b].solverId;

    switch (m_visType) {
    case VisType::ClassicCells: {
      // Set size to match the number of cells without children (i.e. all
      // classic cells that are to be visualized)
      m_visBlocks[b].noCellsVisualized = m_visBlocks[b].visCellIndices.size();
      totalNoCellsVisualized += m_visBlocks[b].noCellsVisualized;
      
      // Clear containers and reserve memory for efficient adding of elements
      m_visBlocks[b].vertexIds.clear();
      m_visBlocks[b].vertexIds.reserve(m_visBlocks[b].noCellsVisualized * IPOW2(nDim));
    } break;

    case VisType::VisualizationNodes: {
      // Determine number of cells to be visualized by iterating over all cells
      m_visBlocks[b].noCellsVisualized = 0;
      for (size_t i = 0; i < m_visBlocks[b].visCellIndices.size(); i++) {
        const MInt cellId = m_visBlocks[b].visCellIndices.at(i);
        const MInt noNodes1D = (m_visualizeSbpData ? m_noNodes1D[cellId] : (m_polyDeg[cellId]+1)) + m_noVisNodes;
        const MInt noNodesXD = ipow(noNodes1D, nDim);
        m_visBlocks[b].noCellsVisualized += noNodesXD;
      }
      totalNoCellsVisualized += m_visBlocks[b].noCellsVisualized;

      // Resize containers and drop all previous contents
      m_visBlocks[b].vertexIds.clear();
      m_visBlocks[b].vertexIds.reserve(m_visBlocks[b].noCellsVisualized * IPOW2(nDim));
    } break;

    default:
      mTerm(1, AT_, "Bad value");
    }
  }
  
  m_vertices.clear();
  m_vertices.reserve(totalNoCellsVisualized * IPOW2(nDim));

  for(MInt b=0; b<m_visBlocks.size(); b++){
    MInt solverId=m_visBlocks[b].solverId;

    // TODO fixme
    // Add one dummy cell for an inactive rank
    //if (!m_visBlocks[b].isActive) {
    //  m_visBlocks[b].noCellsVisualized = 1;
    //  std::cerr << "noCellsVisualized " << domainId() << " " << m_visBlocks[b].noCellsVisualized << std::endl;

    //  // Clear containers and reserve memory for efficient adding of elements
    //  m_visBlocks[b].vertices.clear();
    //  m_visBlocks[b].vertices.reserve(m_visBlocks[b].noCellsVisualized * IPOW2(nDim));
    //  m_visBlocks[b].vertexIds.clear();
    //  m_visBlocks[b].vertexIds.reserve(m_visBlocks[b].noCellsVisualized * IPOW2(nDim));

    //  const MFloat cellLength = m_maiaproxy[solverId]->cellLengthAtLevel(10);

    //  // Calculate the location of the 4 (2D) or 8 (3D) corner points of the
    //  // cell, and save them to the box
    //  for (MInt i = 0; i < IPOW2(nDim); i++) {
    //    Point<nDim> vertex;
    //    for (MInt d = 0; d < nDim; d++) {
    //      vertex.at(d) = 0.0
    //        + 0.5 * m_binaryId[(3 * i) + d] * cellLength;
    //    }
    //    m_visBlocks[b].vertexIds.push_back(box.insert(vertex, m_visBlocks[b].vertices));
    //  }
    //  continue;
    //}

    switch (m_visType) {
    case VisType::ClassicCells: {
      // Loop over all cells that should be visualized
      for (const MInt cellId : m_visBlocks[b].visCellIndices) {
        const MFloat cellLength = m_maiaproxy[solverId]->cellLengthAtLevel(m_maiaproxy[solverId]->tree().level(cellId));//m_maiagrid->cellLengthAtLevel(m_cells_.level(cellId));

        // Calculate the location of the 4 (2D) or 8 (3D) corner points of the
        // cell, and save them to the box
        for (MInt i = 0; i < IPOW2(nDim); i++) {
          Point<nDim> vertex;
          for (MInt d = 0; d < nDim; d++) {
            vertex.at(d) = m_maiaproxy[solverId]->tree().coordinate(cellId, d)
              + 0.5 * m_binaryId[(3 * i) + d] * cellLength;
          }

          MInt vertexId = box.insert(vertex, m_vertices);
          m_visBlocks[b].vertices.insert(vertexId);
          m_visBlocks[b].vertexIds.push_back(vertexId);
        }
      }
    } break;

    case VisType::VisualizationNodes: {
      // Loop over all actual cells and create all visualization node elements
      for (const MInt cellId : m_visBlocks[b].visCellIndices) {
        const MInt noNodes1D = (m_visualizeSbpData ? m_noNodes1D[cellId] : (m_polyDeg[cellId]+1)) + m_noVisNodes;
        const MInt noNodes1D3 = (nDim == 3) ? noNodes1D : 1;
        const MFloat cellLength = m_maiaproxy[solverId]->cellLengthAtLevel(m_maiaproxy[solverId]->tree().level(cellId));     
        const MFloat nodeLength = cellLength / noNodes1D;
        // Calculate the node center at the -ve corner (i.e. most negative node
        // in x,y,z-directions)
        Point<nDim> node0;
        for (MInt d = 0; d < nDim; d++) {
          node0.at(d) = m_maiaproxy[solverId]->tree().coordinate(cellId, d)
                     + 0.5 * (-cellLength + nodeLength);
        }

        // Calculate all vertices and add them to the Box
        array<MInt, 3> index{};
        for (index.at(0) = 0; index.at(0) < noNodes1D; index.at(0)++) {
          for (index.at(1) = 0; index.at(1) < noNodes1D; index.at(1)++) {
            for (index.at(2) = 0; index.at(2) < noNodes1D3; index.at(2)++) {
              Point<nDim> node;
              for (MInt d = 0; d < nDim; d++) {
                node.at(d) = node0.at(d) + nodeLength * index.at(d);
              }

              for (MInt l = 0; l < IPOW2(nDim); l++) {
                Point<nDim> vertex;
                for (MInt d = 0; d < nDim; d++) {
                  vertex.at(d)
                      = node.at(d) + (0.5 * m_binaryId[(3 * l) + d] * nodeLength);
                }
                MInt vertexId = box.insert(vertex, m_vertices);
                m_visBlocks[b].vertices.insert(vertexId);
                m_visBlocks[b].vertexIds.push_back(vertexId);
              }
            }
          }
        }
      }
    } break;

    default:
      mTerm(1, AT_, "Bad value");
    }
  }
}


/** \brief Function to init the interpolation (DG)
 *
 * @author Michael Schlottke-Lakemper
 * @date back in the 90
 *  
 */
template <MInt nDim>
void Reader<nDim>::initInterpolation(const MInt maxNoNodes1D) {
  TRACE();

  // Do not initialize for noNodes1D = 1 with Gauss-Lobatto, as this is not defined
  const MInt start = (string2enum(m_intMethod) == DG_INTEGRATE_GAUSS_LOBATTO) ? 2 : 1;

  // Init interpolation objects
  vector<DgInterpolation>(maxNoNodes1D+1).swap(m_interp);
  for (MInt noNodes1D = start; noNodes1D <= maxNoNodes1D; noNodes1D++) {
    const MInt polyDeg = noNodes1D-1; // Irrelevant for SBP
    const MString sbpOperator = ""; // Irrelevant for interpolation 
    m_interp.at(noNodes1D).initInterpolation(
        polyDeg,
        static_cast<DgPolynomialType>(string2enum(m_polyType)),
        noNodes1D,
        static_cast<DgIntegrationMethod>(string2enum(m_intMethod)),
        m_visualizeSbpData,
        sbpOperator);
  }

  // Calculate polynomial interpolation matrices
  vector<vector<MFloat>>(maxNoNodes1D+1).swap(m_vandermonde);
  for (MInt noNodesIn = start; noNodesIn <= maxNoNodes1D; noNodesIn++) {
    const MInt noNodesOut = noNodesIn + m_noVisNodes;
    
    const MFloat nodeLength = F2 / noNodesOut;
    vector<MFloat> nodesOut(noNodesOut);
    for (MInt j = 0; j < noNodesOut; j++) {
      nodesOut.at(j) = -F1 + F1B2 * nodeLength + j * nodeLength;
    }

    m_vandermonde.at(noNodesIn).resize(noNodesIn * noNodesOut);
   
    if(m_visualizeSbpData){
      maia::dg::interpolation::calcLinearInterpolationMatrix(
          noNodesIn, &m_interp.at(noNodesIn).m_nodes[0], noNodesOut, &nodesOut.at(0),
          &m_vandermonde.at(noNodesIn).at(0));
    }else{
      maia::dg::interpolation::calcPolynomialInterpolationMatrix(
          noNodesIn, &m_interp.at(noNodesIn).m_nodes[0], noNodesOut, &nodesOut.at(0),
          &m_interp.at(noNodesIn).m_wBary[0], &m_vandermonde.at(noNodesIn).at(0));
    }
  }
}


/** \brief Function to determine the domain extents
 *
 * @author Michael Schlottke-Lakemper
 * @date back in the 90
 *  
 */
template <MInt nDim>
void Reader<nDim>::determineDomainExtent(Point<nDim>& min, Point<nDim>& max) {
  TRACE();

  // Determine minimum and maximum extent (on a global scale the orginial grid
  // is taken to evaluate the maximumn box extent) 
  for (MInt d = 0; d < nDim; d++) {
    min.at(d) = m_maiaproxy[0]->raw().treeb().coordinate(0, d);
    max.at(d) = m_maiaproxy[0]->raw().treeb().coordinate(0, d);  
  }
  
  for (MInt i = 0; i < (MInt)m_maiaproxy[0]->raw().treeb().size(); i++) {
    const MFloat cellLength = m_maiaproxy[0]->raw().cellLengthAtLevel(
                                m_maiaproxy[0]->raw().a_level(i));
    const MFloat cellLengthB2 = cellLength/2.0;
    for (MInt d = 0; d < nDim; d++) {
      min.at(d) = std::min(min.at(d), 
                  m_maiaproxy[0]->raw().treeb().coordinate(i, d)-cellLengthB2);
      max.at(d) = std::max(max.at(d), 
                  m_maiaproxy[0]->raw().treeb().coordinate(i, d)+cellLengthB2);
    }
  }

  // Increase the min/max range by 5% of the extent in each dimension
  // to avoid numerical errors in the box algorithm for boundary nodes (DG/SBP)
  for (MInt d = 0; d < nDim; d++) {
    const MFloat extension = 0.05 * (max.at(d) - min.at(d));
    min.at(d) -= extension;
    max.at(d) += extension;
  }
}


/** \brief Function to add cell data to the grid
 *
 * @author Pascal S. Meysonnat, Michael Schlottke-Lakemper
 * @date back in the 90
 *  
 */
template <MInt nDim>
template <typename T>
  void Reader<nDim>::addCellData(const MString& name, const T* data, vtkUnstructuredGrid* grid, const MInt b, const MBool solverLocalData) {
  TRACE();

  // If data is local for the solver it is already in the correct order in memory, i.e., we do not
  // convert local cell ids into grid cell ids to access the data
  const MInt solverId = (solverLocalData) ? -1 : m_visBlocks[b].solverId;
  const MBool isActive = m_visBlocks[b].isActive;

  //std::cerr << domainId() << " " << isActive << " addCellData " << name
  //          << " b=" << b << " solverId=" << solverId << " "
  //          << m_visBlocks[b].noCellsVisualized << std::endl;

  // Create smart pointer to vtk array
  typename type_traits<T>::smart_array_type vtkArrayPtr
      = type_traits<T>::smart_array_type::New();

  // Set name and allocate space for the number of visualized cells
  vtkArrayPtr->SetName(name.c_str());
  vtkArrayPtr->SetNumberOfValues(m_visBlocks[b].noCellsVisualized);

  // Copy data from member variables to VTK array
  switch (m_visType) {
  case VisType::ClassicCells: {
    if (solverId > -1) {
      for (size_t i = 0; i < m_visBlocks[b].visCellIndices.size(); i++) {
        // Visualization of global data on a solver, convert solver cell ids to grid ids
        const MInt cellId = m_visBlocks[b].visCellIndices.at(i);
        const MInt gridCellId = m_maiaproxy[solverId]->tree().solver2grid(cellId);

        vtkArrayPtr->SetValue(i, data[gridCellId]);
      }
    } else {
      // Visualization of data that is local to a solver, i.e., sorted by the local cells
      for (size_t i = 0; i < m_visBlocks[b].visCellIndices.size(); i++) {
        const MInt cellId = m_visBlocks[b].visCellIndices.at(i);
        vtkArrayPtr->SetValue(i, data[cellId]);
      }
    }
    break;
  }  
  case VisType::VisualizationNodes: {
    MInt visNodeId = 0;
    if (solverId > -1) {
      for (size_t i = 0; i < m_visBlocks[b].visCellIndices.size(); i++) {
        // Visualization of global data on a solver, convert solver cell ids to grid ids
        const MInt cellId = m_visBlocks[b].visCellIndices.at(i);
        const MInt gridCellId = m_maiaproxy[solverId]->tree().solver2grid(cellId);
        const MInt noNodes1D = (m_visualizeSbpData ? m_noNodes1D[cellId] : (m_polyDeg[cellId]+1)) + m_noVisNodes;
        const MInt noNodesXD = ipow(noNodes1D, nDim);
        for (MInt j = 0; j < noNodesXD; j++) {
          vtkArrayPtr->SetValue(visNodeId, data[gridCellId]);
          visNodeId++;
        }
      }
    } else {
      // Visualization of data that is local to a solver, i.e., sorted by the local cells
      for (size_t i = 0; i < m_visBlocks[b].visCellIndices.size(); i++) {
        const MInt cellId = m_visBlocks[b].visCellIndices.at(i);
        const MInt noNodes1D = (m_visualizeSbpData ? m_noNodes1D[cellId] : (m_polyDeg[cellId]+1)) + m_noVisNodes;
        const MInt noNodesXD = ipow(noNodes1D, nDim);
        for (MInt j = 0; j < noNodesXD; j++) {
          vtkArrayPtr->SetValue(visNodeId, data[cellId]);
          visNodeId++;
        }
      }
    }
    break;
  }
  default:
    mTerm(1, AT_, "Bad value");
  }

  // Add VTK array to grid cell data
  grid->GetCellData()->AddArray(vtkArrayPtr);
}


/** \brief Function to add auxiallary data to the grid
 *
 * @author Pascal S. Meysonnat, Michael Schlottke-Lakemper
 * @date back in the 90
 *  
 */
template <MInt nDim>
void Reader<nDim>::loadGridAuxillaryData(vtkUnstructuredGrid* grid, const MInt b) {
  TRACE();

  // Container for min cell offsets
  vector<MInt> partitionCellIdOffsets;

  const MInt solverId = m_visBlocks[b].solverId;

  for (auto&& dataset : m_datasets) {
    if (!dataset.isGrid) {
      continue;
    }

    // Skip datasets for which the status is zero (i.e. the user chose to not
    // load them)
    if (!dataset.status) {
      continue;
    }

    if (dataset.variableName == "globalId") {
      addCellData(dataset.realName, &m_maiaproxy[solverId]->raw().treeb().globalId(0), grid, b);
      //m_vtkGrid->GetCellData()->SetActiveScalars(dataset.realName.c_str());
      continue;
    }

    if (dataset.variableName == "level") {
      addCellData(dataset.realName, &m_maiaproxy[solverId]->raw().treeb().level(0), grid, b);
      continue;
    }

    //DG related stuff
    if (m_isDgFile){
      // Data only for visualization nodes
      if (m_visType == VisType::VisualizationNodes && dataset.variableName == "nodeId") {
        vtkSmartPointer<vtkIntArray> nodeIdData
          = vtkSmartPointer<vtkIntArray>::New();
        nodeIdData->SetName(dataset.realName.c_str());
        nodeIdData->SetNumberOfComponents(nDim);
        nodeIdData->SetNumberOfTuples(m_visBlocks[b].noCellsVisualized);

        MInt visNodeId = 0;
        for (size_t i = 0; i < m_visBlocks[b].visCellIndices.size(); i++) {
          const MInt cellId = m_visBlocks[b].visCellIndices.at(i);
          const MInt noNodes1D = (m_visualizeSbpData ? m_noNodes1D[cellId] : (m_polyDeg[cellId]+1)) + m_noVisNodes;
          const MInt noNodes1D3 = (nDim == 3) ? noNodes1D : 1;
          for (MInt ii = 0; ii < noNodes1D; ii++) {
          for (MInt jj = 0; jj < noNodes1D; jj++) {
            for (MInt kk = 0; kk < noNodes1D3; kk++) {
              const MInt nodeId[] = {ii, jj, kk};
              nodeIdData->SetTypedTuple(visNodeId, nodeId);
              visNodeId++;
            }
          }
          }
        }
        grid->GetCellData()->AddArray(nodeIdData);
        continue;
      } else if (dataset.variableName == "polyDeg") {
        // Polynomial degree (data is local for solver)
        addCellData(dataset.realName, &m_polyDeg[0], grid, b, true);
        continue;
      } else if (m_visualizeSbpData && dataset.variableName == "noNodes1D"){
        addCellData(dataset.realName, &m_noNodes1D[0], grid, b, true);
        continue;
      }
    }
  }

  // DG reletaed stuff
  if (m_isDgFile) {
    // Integration method
    vtkSmartPointer<vtkStringArray> intMethod
      = vtkSmartPointer<vtkStringArray>::New();
    intMethod->SetNumberOfComponents(1);
    intMethod->SetName("integration method (grid)");
    intMethod->InsertNextValue(m_intMethod.c_str());
    grid->GetFieldData()->AddArray(intMethod);

    // Polynomial type
    vtkSmartPointer<vtkStringArray> polyType
      = vtkSmartPointer<vtkStringArray>::New();
    polyType->SetNumberOfComponents(1);
    polyType->SetName("polynomial type (grid)");
    polyType->InsertNextValue(m_polyType.c_str());
    grid->GetFieldData()->AddArray(polyType);
  }

  // Add info on filename and path for later use in filters
  // Path of grid file
  vtkSmartPointer<vtkStringArray> gridFile
    = vtkSmartPointer<vtkStringArray>::New();
  gridFile->SetNumberOfComponents(1);
  gridFile->SetName("gridFileName");
  gridFile->InsertNextValue(m_gridFileName.c_str());
  grid->GetFieldData()->AddArray(gridFile);
  
  if(m_isDataFile){
    // Path of data file
    vtkSmartPointer<vtkStringArray> dataFile
      = vtkSmartPointer<vtkStringArray>::New();
    dataFile->SetNumberOfComponents(1);
    dataFile->SetName("dataFileName");
    dataFile->InsertNextValue(m_dataFileName.c_str());
    grid->GetFieldData()->AddArray(dataFile);
  }
}

/** \brief Function to load the solution data
 *
 * @author Pascal S. Meysonnat, Michael Schlottke-Lakemper
 * @date back in the 90
 *  
 */
template <MInt nDim> void Reader<nDim>::loadSolutionData(ParallelIo& datafile) {
  TRACE();

  const MBool isActive = m_maiaproxy[m_solverId]->isActive();
  const MInt solverDomainId = m_maiaproxy[m_solverId]->domainId();

  //if (!isActive) {
  //  std::cerr << "inactive rank " << domainId() << " "
  //            << m_visBlocks[0].visCellIndices.size() << " "
  //            << m_visBlocks[0].noCellsVisualized << std::endl;
  //} else {
  //  std::cerr << "active rank " << domainId() << " "
  //            << m_visBlocks[0].visCellIndices.size() << " "
  //            << m_visBlocks[0].noCellsVisualized << std::endl;
  //}

  //copy the offsets so that we can use one call independent of dg/fv/lb
  MInt cellOffsets[2]={m_maiaproxy[m_solverId]->noInternalCells(), m_maiaproxy[m_solverId]->noCells()};
  if(m_isDgFile){
    cellOffsets[0]=m_dofDg[0];
    cellOffsets[1]=m_dofDg[1];
  }

  //set the offset correctly
  if (!isActive) {
    cellOffsets[0] = 0;
    cellOffsets[1] = 0;
    datafile.setOffset(0, 0);
  } else if (m_isDgFile) {
    datafile.setOffset(m_dofDg[0], m_offsetLocalDG);
  } else {
    datafile.setOffset(m_maiaproxy[m_solverId]->noInternalCells(),
                       m_maiaproxy[m_solverId]->domainOffset(solverDomainId));
    //datafile.setOffset(m_maiagrid->noInternalCells(), m_maiagrid->domainOffset(domainId())-m_32BitOffset);
  }

  // Calculate the offsets for the cell data based on the polynomial degree (DG)
  // or trivially based on the number of cells (FV/LB)
  const MInt noOffsets = (isActive) ? m_maiaproxy[m_solverId]->noCells() + 1 : 1;
  vector<MInt> dataOffsets(noOffsets);

  if (m_isDgFile) {
    dataOffsets.at(0) = 0;
    for (MInt i = 1; i < noOffsets; i++) {
      const MInt noNodes1D = (m_visualizeSbpData ? m_noNodes1D[i-1] : (m_polyDeg[i-1]+1));
      const MInt noNodesXD = ipow(noNodes1D, nDim);
      dataOffsets.at(i)
          = dataOffsets.at(i - 1) + noNodesXD;
    }
  }

  // The last 20% are set depending on the number of processed datasets
  MFloat curShare = 0.8;
  MFloat lastShare = curShare;
  const MFloat stepShare = 0.2 * (1.0 / m_datasets.size());
    // Load all valid datasets into the grid
  // MBool activeScalarsSet = false;
  for (auto&& dataset : m_datasets) {
    curShare += stepShare;
    if (curShare > (lastShare + 0.05)) {
      setProgressUpdate(curShare, "Load solution data");
      lastShare = curShare;
    }
    
    // Skip grid datasets
    if (dataset.isGrid) {continue;}
    
    // Skip datasets for which the status is zero (i.e. the user chose to not load them)
    if (!dataset.status) {continue;}

    // Read in data
    std::vector<MFloat> rawdata(std::max(cellOffsets[1], 1), 0.0);

    datafile.readArray(&rawdata[0], dataset.variableName);

    // MPI communication
    if (noDomains() > 1 && isActive) {
      if(!m_isDgFile) { //needs to be implemented for DG style
        m_maiaproxy[m_solverId]->exchangeHaloCellsForVisualization(&rawdata[0]);
        // exchangeHaloDataset(scalars, dataOffsets);
      } else {
        if(m_visualizeSbpData){
          std::cerr << "exchange SBP window/halo cell data" << std::endl;
          m_maiaproxy[m_solverId]->exchangeHaloCellsForVisualizationSBP(&rawdata[0], &m_noNodes1D[0],
                                                                     &dataOffsets[0]);
        } else {
          std::cerr << "exchange DG window/halo cell data" << std::endl;
          m_maiaproxy[m_solverId]->exchangeHaloCellsForVisualizationDG(&rawdata[0], &m_polyDeg[0],
                                                                     &dataOffsets[0]);
        }
      }
    }

    //std::cerr << "exchangeHaloDataset: OK" << std::endl;
    for(MInt b=0; b<(MInt)m_visBlocks.size(); b++){
      // Create VTK array to hold data values
      vtkSmartPointer<vtkDoubleArray> data
        = vtkSmartPointer<vtkDoubleArray>::New();
      data->SetName(dataset.realName.c_str());
      data->SetNumberOfValues(m_visBlocks[b].noCellsVisualized);
      
      // Add solution data to VTK array
      std::cerr << "add solution data to VTK..." << std::endl;
      if (m_visType == VisType::VisualizationNodes && isActive) {
        // Normal DG visualization (non-SBP data)
        // Interpolate data to cells
        std::cerr << "interpolate data to cells..." << std::endl;
        MInt nodeId = 0;
        for (size_t i = 0; i < m_visBlocks[b].visCellIndices.size(); i++) {
          const MInt cellId = m_visBlocks[b].visCellIndices.at(i);
          const MInt noNodesIn = (m_visualizeSbpData ? m_noNodes1D[cellId] : (m_polyDeg[cellId]+1));
          const MInt noNodesOut = noNodesIn + m_noVisNodes;
          const MInt noNodesXDOut = ipow(noNodesOut, nDim);
          const MInt noVariables = 1;
          vector<MFloat> interpolated(noNodesXDOut);
          using namespace maia::dg::interpolation;
          (nDim < 3)
            ? interpolateNodes<2>(&rawdata[dataOffsets.at(cellId)],
                                  &m_vandermonde.at(noNodesIn).at(0), noNodesIn,
                                  noNodesOut, noVariables, &interpolated.at(0))
            : interpolateNodes<3>(&rawdata[dataOffsets.at(cellId)],
                                  &m_vandermonde.at(noNodesIn).at(0), noNodesIn,
                                  noNodesOut, noVariables, &interpolated.at(0));
          
          for (MInt j = 0; j < noNodesXDOut; j++) {
            data->SetValue(nodeId, interpolated.at(j));
            nodeId++;
          }
        }
        std::cerr << "interpolate data to cells: OK" << std::endl;
      } /*else if (m_visType == VisType::VisualizationNodes && m_visualizeSbpData && isActive) {
        // Visualization of SBP data generated by DG solver
        std::cerr << "interpolate SBP data to cells..." << std::endl;
        MInt nodeId = 0;
        for (size_t idx = 0; idx < m_visBlocks[b].visCellIndices.size(); idx++) {
          const MInt cellId = m_visBlocks[b].visCellIndices.at(idx);
          const MInt noNodesIn = (m_visualizeSbpData ? m_noNodes1D[cellId] : (m_polyDeg[cellId]+1));
          const MInt noNodesOut = noNodesIn;
          const MInt noNodesXDOut = ipow(noNodesOut, nDim);
          vector<MFloat> interpolated(noNodesXDOut);

          if (nDim == 2) {
            MFloatTensor in(&rawdata[dataOffsets.at(cellId)], noNodesIn, noNodesIn);
            MFloatTensor out(&interpolated.at(0), noNodesOut, noNodesOut);
            for (MInt i = 0; i < noNodesOut; i++) {
              for (MInt j = 0; j < noNodesOut; j++) {
                out(i, j) = (in(i, j) + in(i, j + 1) + in(i + 1, j) + in(i + 1, j + 1)) / 4;
              }
            }
          } else {
            MFloatTensor in(&rawdata[dataOffsets.at(cellId)], noNodesIn, noNodesIn, noNodesIn);
            MFloatTensor out(&interpolated.at(0), noNodesOut, noNodesOut, noNodesOut);
            for (MInt i = 0; i < noNodesOut; i++) {
              for (MInt j = 0; j < noNodesOut; j++) {
                for (MInt k = 0; k < noNodesOut; k++) {
                  out(i, j, k) = (in(i, j, k) + in(i, j, k + 1) +
                                  in(i, j + 1, k) + in(i, j + 1, k + 1) +
                                  in(i + 1, j, k) + in(i + 1, j, k + 1) +
                                  in(i + 1, j + 1, k) + in(i + 1, j + 1, k + 1)) / 8;
                }
              }
            }
          }

          for (MInt j = 0; j < noNodesXDOut; j++) {
            data->SetValue(nodeId, interpolated.at(j));
            nodeId++;
          }
        }
        std::cerr << "interpolate SBP data to cells: OK" << std::endl;
      }*/ else {
        std::cerr << "copying raw data..." << std::endl;
        // For classic cells, just copy values from raw data
        for (size_t i = 0; i < m_visBlocks[b].visCellIndices.size(); i++) {
          const MInt cellId = m_visBlocks[b].visCellIndices.at(i);
          data->SetValue(i, rawdata[cellId]);
        }
        std::cerr << "copying raw data: OK" << std::endl;
      }
      vtkUnstructuredGrid* localGrid = vtkUnstructuredGrid::SafeDownCast(m_vtkGrid->GetBlock(b));
      localGrid->GetCellData()->AddArray(data);
    }

    /*
    // Set the first data array to be the "active" one
    if (!activeScalarsSet) {
      m_vtkGrid->GetCellData()->SetActiveScalars(dataset.realName.c_str());
      activeScalarsSet = true;
    }
    */
  }
  
  std::cerr << "loadSolutionData: OK" << std::endl;
}

/** \brief Function to load information of Lagrange particles
 *
 * @author Thomas Hoesgen
 * @date 28.05.2020
 *  
 */
template <MInt nDim>
  void Reader<nDim>::readParticleData(vtkMultiBlockDataSet *grid, const vector<Dataset>& datasets) {
  TRACE();

  
  m_datasets = datasets;


  cerr << "Build particle vtkgrid...";
  
  //Build the grid
  m_vtkGrid=grid;
  m_vtkGrid->SetNumberOfBlocks(1);
  
  vtkUnstructuredGrid* localGrid = vtkUnstructuredGrid::New();
  vtkPoints* partPoints = vtkPoints::New();
  localGrid->Initialize();
  localGrid->Allocate(0,0);
  localGrid->SetPoints(partPoints);
  
  ParallelIo partFile(m_dataFileName, maia::parallel_io::PIO_READ, mpiComm());
  MLong noGlobalParticles = (partFile.getArrayDims("partDia")).at(0);

  MInt avgNoParticles = noGlobalParticles / noDomains();
 
  MInt localOffset = domainId() * avgNoParticles;
  MInt noParticles = avgNoParticles;

  if (domainId() == noDomains() - 1) {
    noParticles = noGlobalParticles - domainId() * avgNoParticles;
  }

  // Set offsets
  partFile.setOffset(nDim*noParticles, localOffset);

  // Read in coordinates
  std::vector<MFloat> rawdata(std::max(nDim*noParticles, 1), 0.0);
  partFile.readArray(&rawdata[0], "partPos");
  for (MInt i = 0; i < noParticles; i++) {
    MInt index = localOffset + i*nDim;
    MFloat coord[3] = {rawdata[index], rawdata[index+1], F0};
    if (nDim == 3) coord[2] = rawdata[index+2];

    partPoints->InsertNextPoint(coord[0], coord[1], coord[2]);
  }
  for (MInt i = 0; i < noParticles; i++) {
    vtkVertex * cell = vtkVertex::New();
    cell->GetPointIds()->SetId(0, i);
    localGrid->InsertNextCell(cell->GetCellType(), cell->GetPointIds());
    cell->Delete();
  }

  cerr << "   done." << endl;
  // Build grid done
  
  // Load data
  cerr << "Load paricle data";
  for (auto&& dataset : m_datasets) {
    if (!dataset.status) {continue;}

    // Read data
    MLong dataSize = partFile.getArraySize(dataset.variableName);
    if ( dataSize / noGlobalParticles == nDim ) {

      // Set offsets
      partFile.setOffset(nDim*noParticles, localOffset);

      partFile.readArray(&rawdata[0], dataset.variableName);

      MFloat* values_0 = new MFloat [noParticles];
      MFloat* values_1 = new MFloat [noParticles];
      MFloat* values_2 = new MFloat [noParticles];
  
      for (MInt i = 0; i < noParticles; i++) {
        values_0[i] = rawdata[nDim*i];
        values_1[i] = rawdata[nDim*i + 1];
        if ( nDim == 3 )
          values_2[i] = rawdata[nDim*i + 2];
        else
          values_2[i] = F0;
      }

      for (MInt d = 0; d < nDim; d++) {
        vtkSmartPointer<vtkDoubleArray> data = vtkSmartPointer<vtkDoubleArray>::New();
        data->SetNumberOfValues(noParticles);
        
        stringstream dim;
        dim << d;
        data->SetName( (dataset.variableName + dim.str()).c_str());
        for (MInt i = 0; i < noParticles; i++) {
          data->SetValue(i, rawdata[i*nDim+d]);
        }
        localGrid->GetCellData()->AddArray(data);

      }
      delete[] values_0;
      delete[] values_1;
      delete[] values_2;

    } else {

      // Set offsets
      partFile.setOffset(noParticles, localOffset);

      partFile.readArray(&rawdata[0], dataset.variableName);

      vtkSmartPointer<vtkDoubleArray> data = vtkSmartPointer<vtkDoubleArray>::New();
      data->SetNumberOfValues(noParticles);

      data->SetName( dataset.variableName.c_str() );
      for (MInt i = 0; i < noParticles; ++i) {
        data->SetValue(i, rawdata[i]);
      }
      localGrid->GetCellData()->AddArray(data);
    }
  }
  cerr << "   done." << endl;

  // Add to grid
  m_vtkGrid->SetBlock(0, localGrid);
  m_vtkGrid->GetMetaData((MInt) 0)->Set(vtkCompositeDataSet::NAME(), "particles");

  return;
}


}//namespace maiapv
#endif
