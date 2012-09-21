#include <sstream>
#include <iomanip>

#include "object.h"

#include "Collect.h"

MODULE_MAIN(Collect)


Collect::Collect(int rank, int size, int moduleID)
   : Module("Collect", rank, size, moduleID) {

   createInputPort("grid_in");
   createInputPort("texture_in");
   createInputPort("normal_in");

   createOutputPort("grid_out");
}

Collect::~Collect() {

}


bool Collect::compute() {

   ObjectList gridObjects = getObjects("grid_in");
   ObjectList textureObjects = getObjects("texture_in");

   while (gridObjects.size() > 0 && textureObjects.size()) {

      vistle::Geometry::ptr geom(new vistle::Geometry);
      geom->setGeometry(gridObjects.front());
      geom->setTexture(textureObjects.front());

      geom->setBlock(gridObjects.front()->getBlock());
      geom->setTimestep(gridObjects.front()->getTimestep());

      removeObject("grid_in", gridObjects.front());
      removeObject("texture_in", textureObjects.front());

      addObject("grid_out", geom);

      gridObjects = getObjects("grid_in");
      textureObjects = getObjects("texture_in");
   }

   return true;
}
