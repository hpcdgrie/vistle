//-------------------------------------------------------------------------
// PRINT METADATA H
// * Prints Meta-Data  about the input object to the Vistle console
// *
// * Sever Topan, 2016
//-------------------------------------------------------------------------

#ifndef PRINTMETADATA_H
#define PRINTMETADATA_H

#include <vector>

#include <module/module.h>
#include <core/object.h>
#include <core/index.h>
#include <core/unstr.h>
#include <core/vec.h>

//-------------------------------------------------------------------------
// PRINT METADATA CLASS DECLARATION
//-------------------------------------------------------------------------
class PrintMetaData : public vistle::Module {

 public:
   PrintMetaData(const std::string &shmname, const std::string &name, int moduleID);
   ~PrintMetaData();

 private:
   // overriden functions
   virtual bool prepare();
   virtual bool compute();
   virtual bool reduce(int timestep);

   // helper functions
   void compute_acquireGenericData(vistle::DataBase::const_ptr data);
   void compute_acquireGridData(vistle::Indexed::const_ptr dataGrid);
   void reduce_printData();

   // private member variables
   vistle::Index m_numCurrElements;
   vistle::Index m_numCurrVertices;
   vistle::Index m_numTotalElements;
   vistle::Index m_numTotalVertices;
   unsigned m_numCurrGhostCells;
   unsigned m_numTotalGhostCells;
   std::vector<unsigned> m_elCurrTypeVector;
   std::vector<unsigned> m_elTotalTypeVector;
   std::vector<std::string> m_attributesVector;
   std::string m_dataType;
   int m_executionCounter;
   int m_iterationCounter;
   int m_creator;
   int m_numBlocks;
   int m_numTimesteps;
   int m_numAnimationSteps;
   double m_realTime;
   bool m_isGridAttatched;
   bool m_isGhostCellsPresent;
   bool m_isFirstComputeCall;

   // private constants
   const int M_ROOT_NODE = 0;
   const std::string M_HORIZONTAL_RULER = "\n-----------------------------------------------------";

};

#endif /* PRINTMETADATA_H */
