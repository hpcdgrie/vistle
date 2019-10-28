#include <sstream>
#include <iomanip>

#include <core/object.h>
#include <core/coords.h>
#include <core/points.h>
#include <core/spheres.h>
#include <core/lines.h>
#include <core/tubes.h>
#include <util/math.h>

#include "Thicken.h"

DEFINE_ENUM_WITH_STRING_CONVERSIONS(MapMode,
      (DoNothing)
      (Fixed)
      (Radius)
      (InvRadius)
      (Surface)
      (InvSurface)
      (Volume)
      (InvVolume)
)

MODULE_MAIN(Thicken)

using namespace vistle;

Thicken::Thicken(const std::string &name, int moduleID, mpi::communicator comm)
   : Module("transform lines to tubes", name, moduleID, comm) {

   createInputPort("grid_in");
   createInputPort("data_in");
   createOutputPort("grid_out");
   createOutputPort("data_out");

   m_radius = addFloatParameter("radius", "radius or radius scale factor of tube/sphere", 1.);
   setParameterMinimum(m_radius, (Float)0.);
   m_sphereScale = addFloatParameter("sphere_scale", "extra scale factor for spheres", 2.5);
   setParameterMinimum(m_sphereScale, (Float)0.);
   m_mapMode = addIntParameter("map_mode", "mapping of data to tube diameter", (Integer)Fixed, Parameter::Choice);
   V_ENUM_SET_CHOICES(m_mapMode, MapMode);
   m_range = addVectorParameter("range", "allowed radius range", ParamVector(0., 1.));
   setParameterMinimum(m_range, ParamVector(0., 0.));

   m_startStyle = addIntParameter("start_style", "cap style for initial tube segments", (Integer)Tubes::Open, Parameter::Choice);
   V_ENUM_SET_CHOICES_SCOPE(m_startStyle, CapStyle, Tubes);
   m_jointStyle = addIntParameter("connection_style", "cap style for tube segment connections", (Integer)Tubes::Round, Parameter::Choice);
   V_ENUM_SET_CHOICES_SCOPE(m_jointStyle, CapStyle, Tubes);
   m_endStyle = addIntParameter("end_style", "cap style for final tube segments", (Integer)Tubes::Open, Parameter::Choice);
   V_ENUM_SET_CHOICES_SCOPE(m_endStyle, CapStyle, Tubes);
}

Thicken::~Thicken() {

}

bool Thicken::compute() {

   auto obj = expect<Object>("grid_in");
   if (!obj) {
       sendError("no input data");
       return true;
   }

   MapMode mode = (MapMode)m_mapMode->getValue();
   if (mode == DoNothing) {
       passThroughObject("grid_out", obj);

       auto data = accept<Object>("data_in");
       if (data) {
           passThroughObject("data_out", data);
       }
       return true;
   }

   auto lines = Lines::as(obj);
   auto points = Points::as(obj);
   auto radius = Vec<Scalar, 1>::as(obj);
   auto radius3 = Vec<Scalar, 3>::as(obj);

   DataBase::ptr basedata;
   if (!points && !lines && radius) {
      lines = Lines::as(radius->grid());
      points = Points::as(radius->grid());
      if (isConnected("grid_out"))
          basedata = radius->clone();
   }
   if (!points && !lines && radius3) {
      lines = Lines::as(radius3->grid());
      points = Points::as(radius3->grid());
      if (isConnected("grid_out"))
          basedata = radius3->clone();
   }
   if (!lines && !points) {
      sendError("no Lines and no Points object");
      return true;
   }

   if (mode != Fixed && !radius && !radius3) {
      sendError("data input required for varying radius");
      return true;
   }

   if (lines && mode == InvVolume) {
       sendInfo("setting inverse tube cross section for Lines input");
       mode = InvSurface;
   }

   if (lines && mode == Volume) {
       sendInfo("setting tube cross section for Lines input");
       mode = Surface;
   }

   Tubes::ptr tubes;
   Spheres::ptr spheres;
   CoordsWithRadius::ptr cwr;
   const Index *cl = nullptr;
   if (lines && lines->getNumCorners()>0) {
       cl = &lines->cl()[0];
   }

   if (lines) {
       // set coordinates
       if (lines->getNumCorners() == 0) {
           tubes = Tubes::clone<Vec<Scalar, 3>>(lines);
           tubes->components().resize(lines->getNumElements()+1);
       } else {
           tubes.reset(new Tubes(lines->getNumElements(), lines->getNumCorners()));
           auto lx = &lines->x()[0];
           auto ly = &lines->y()[0];
           auto lz = &lines->z()[0];
           auto tx = tubes->x().data();
           auto ty = tubes->y().data();
           auto tz = tubes->z().data();
           for (Index i=0; i<lines->getNumCorners(); ++i) {
               Index l = i;
               if (cl)
                   l = cl[l];
               tx[i] = lx[l];
               ty[i] = ly[l];
               tz[i] = lz[l];
           }
       }
       cwr = tubes;

       // set tube lengths
       tubes->d()->components = lines->d()->el;

       tubes->setCapStyles((Tubes::CapStyle)m_startStyle->getValue(), (Tubes::CapStyle)m_jointStyle->getValue(), (Tubes::CapStyle)m_endStyle->getValue());
       tubes->setMeta(lines->meta());
       tubes->copyAttributes(lines);
   } else if (points) {
       spheres = Spheres::clone<Vec<Scalar, 3>>(points);
       cwr = spheres;
   }

   assert(cwr);

   // set radii
   auto r = cwr->r().data();
   auto radx = radius3 ? &radius3->x()[0] : radius ? &radius->x()[0] : nullptr;
   auto rady = radius3 ? &radius3->y()[0] : nullptr;
   auto radz = radius3 ? &radius3->z()[0] : nullptr;
   const Scalar scale = spheres ? m_radius->getValue()*m_sphereScale->getValue() : m_radius->getValue();
   const Scalar rmin = m_range->getValue()[0];
   const Scalar rmax = m_range->getValue()[1];
   Index nv = lines ? lines->getNumCorners() : points->getNumPoints();
   for (Index i=0; i<nv; ++i) {
      Index l = i;
      if (cl)
          l = cl[l];
      const Scalar rad = (radx && rady && radz) ? Vector3(radx[l], rady[l], radz[l]).norm() : radx ? radx[l] : Scalar(1.);
      switch (mode) {
         case DoNothing:
            break;
         case Fixed:
            r[i] = scale;
            break;
         case Radius:
            r[i] = scale * rad;
            break;
         case Surface:
            r[i] = scale * sqrt(rad);
            break;
         case InvRadius:
            if (fabs(rad) >= 1e-9)
               r[i] = scale / rad;
            else
               r[i] = 1e9;
            break;
         case InvSurface:
            if (fabs(rad) >= 1e-9)
               r[i] = scale / sqrt(rad);
            else
               r[i] = 1e9;
            break;
         case Volume:
            r[i] = scale * powf(rad, 0.333333f);
            break;
         case InvVolume:
            if (fabs(rad) >= 1e-9)
               r[i] = scale / powf(rad, 0.333333f);
            else
               r[i] = 1e9;
            break;
      }

      r[i] = clamp(r[i], rmin, rmax);
   }

   if (basedata) {
       basedata->setGrid(cwr);
       addObject("grid_out", basedata);
   } else {
       addObject("grid_out", cwr);
   }

   auto data = accept<DataBase>("data_in");
   if (data && isConnected("data_out")) {
       auto ndata = data->clone();
       ndata->setGrid(cwr);
       addObject("data_out", ndata);
   }

   return true;
}
