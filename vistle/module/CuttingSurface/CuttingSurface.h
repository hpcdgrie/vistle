#ifndef CUTTINGSURFACE_H
#define CUTTINGSURFACE_H

#include "module.h"
#include "vector.h"

class CuttingSurface: public vistle::Module {

 public:
   CuttingSurface(int rank, int size, int moduleID);
   ~CuttingSurface();

 private:
   std::pair<vistle::Object::ptr, vistle::Object::ptr>
      generateCuttingSurface(vistle::Object::const_ptr grid,
                             vistle::Object::const_ptr data,
                             const vistle::Vector & normal,
                             const vistle::Scalar distance);

   virtual bool compute();
};

#endif
