#include <core/polygons.h>
#include <core/triangles.h>
#include <core/quads.h>
#include <core/lines.h>
#include <core/tubes.h>
#include <core/spheres.h>
#include <core/points.h>
#include <core/vec.h>

#include <core/assert.h>

#include <embree3/rtcore.h>
#include <embree3/rtcore_device.h>

#include "rayrenderobject.h"
#include "spheres_ispc.h"
#include "tubes_ispc.h"

using namespace vistle;

using ispc::Vertex;
using ispc::Quad;

float RayRenderObject::pointSize = 0.001f;

RayRenderObject::RayRenderObject(RTCDevice device, int senderId, const std::string &senderPort,
      Object::const_ptr container,
      Object::const_ptr geometry,
      Object::const_ptr normals,
      Object::const_ptr texture)
: vistle::RenderObject(senderId, senderPort, container, geometry, normals, texture)
, data(new ispc::RenderObjectData)
{
   updateBounds();

   data->device = device;
   data->scene = nullptr;
   data->geomID = RTC_INVALID_GEOMETRY_ID;
   data->instID = RTC_INVALID_GEOMETRY_ID;
   data->spheres = nullptr;
   data->primitiveFlags = nullptr;
   data->indexBuffer = nullptr;
   data->triangles = 1;
   data->texCoords = nullptr;
   data->lighted = 1;
   data->hasSolidColor = hasSolidColor;
   data->perPrimitiveMapping = 0;
   data->normalsPerPrimitiveMapping = 0;
   data->cmap = nullptr;
   for (int c=0; c<3; ++c) {
       data->normalTransform[c].x = c==0 ? 1 : 0;
       data->normalTransform[c].y = c==1 ? 1 : 0;
       data->normalTransform[c].z = c==2 ? 1 : 0;
   }
   for (int c=0; c<3; ++c) {
       data->normals[c] = nullptr;
   }
   for (int c=0; c<4; ++c) {
      data->solidColor[c] = solidColor[c];
   }
   if (this->scalars) {
       if (this->scalars->guessMapping(geometry) == DataBase::Element)
         data->perPrimitiveMapping = 1;

       data->texCoords = &this->scalars->x()[0];

       std::cerr << "texcoords from scalar field" << std::endl;

   } else if (this->texture) {
      if (this->texture->guessMapping(geometry) == DataBase::Element)
         data->perPrimitiveMapping = 1;

      data->texCoords = &this->texture->coords()[0];

      cmap.reset(new ispc::ColorMapData);
      data->cmap = cmap.get();
      data->cmap->texData = this->texture->pixels().data();
      data->cmap->texWidth = this->texture->getWidth();
      // texcoords as computed by Color module are between 0 and 1
      data->cmap->min = 0.;
      data->cmap->max = 1.;
      data->cmap->blendWithMaterial = 0;

       std::cerr << "texcoords from texture" << std::endl;
   } else if (auto vec = Vec<Scalar,3>::as(texture)) {
       if (vec->guessMapping(geometry) == DataBase::Element)
         data->perPrimitiveMapping = 1;

       tcoord.resize(vec->getSize());
       data->texCoords = tcoord.data();
       const Scalar *x = &vec->x()[0];
       const Scalar *y = &vec->y()[0];
       const Scalar *z = &vec->z()[0];
       for (auto it = tcoord.begin(); it != tcoord.end(); ++it) {
           *it = sqrtf(*x * *x + *y * *y + *z * *z);
           ++x;
           ++y;
           ++z;
       }
   } else if (auto iscal = Vec<Index>::as(texture)) {
       if (iscal->guessMapping(geometry) == DataBase::Element)
         data->perPrimitiveMapping = 1;

       tcoord.resize(iscal->getSize());
       data->texCoords = tcoord.data();
       float *d = tcoord.data();
       for (const Index *i = &iscal->x()[0], *end = i+iscal->getSize(); i<end; ++i) {
           *d++ = *i;
       }
   }

   if (geometry->isEmpty()) {
      return;
   }

   data->scene = rtcNewScene(data->device);
   rtcSetSceneFlags(data->scene, RTC_SCENE_FLAG_NONE);
   rtcSetSceneBuildQuality(data->scene, RTC_BUILD_QUALITY_MEDIUM);

   RTCGeometry geom = 0;
   bool useNormals = true;
   if (auto quads = Quads::as(geometry)) {

      Index numElem = quads->getNumElements();
      geom = rtcNewGeometry (data->device, RTC_GEOMETRY_TYPE_QUAD);
      rtcSetGeometryBuildQuality(geom,RTC_BUILD_QUALITY_MEDIUM);
      rtcSetGeometryTimeStepCount(geom,1);
      std::cerr << "Quad: #: " << quads->getNumElements() << ", #corners: " << quads->getNumCorners() << ", #coord: " << quads->getNumCoords() << std::endl;

      Vertex* vertices = (Vertex*) rtcSetNewGeometryBuffer(geom,RTC_BUFFER_TYPE_VERTEX,0,RTC_FORMAT_FLOAT3,4*sizeof(float),quads->getNumCoords());
      for (Index i=0; i<quads->getNumCoords(); ++i) {
         vertices[i].x = quads->x()[i];
         vertices[i].y = quads->y()[i];
         vertices[i].z = quads->z()[i];
      }


      //data->indexBuffer = new Triangle[numElem];
      //rtcSetSharedGeometryBuffer(geom_0,RTC_BUFFER_TYPE_INDEX,0,RTC_FORMAT_UINT3,data->indexBuffer,0,sizeof(Triangle),numElem);
      Quad* quad = (Quad*) rtcSetNewGeometryBuffer(geom,RTC_BUFFER_TYPE_INDEX,0,RTC_FORMAT_UINT4,sizeof(Quad),numElem);
      data->indexBuffer = quad;
      data->triangles = 0;
      if (quads->getNumCorners() == 0) {
         for (Index i=0; i<numElem; ++i) {
            quad[i].v0 = i*4;
            quad[i].v1 = i*4+1;
            quad[i].v2 = i*4+2;
            quad[i].v3 = i*4+3;
         }
      } else {
         const auto *cl = &quads->cl()[0];
         for (Index i=0; i<numElem; ++i) {
            quad[i].v0 = cl[i*4];
            quad[i].v1 = cl[i*4+1];
            quad[i].v2 = cl[i*4+2];
            quad[i].v3 = cl[i*4+3];
         }
      }

   } else if (auto tri = Triangles::as(geometry)) {

      Index numElem = tri->getNumElements();
      geom = rtcNewGeometry (data->device, RTC_GEOMETRY_TYPE_TRIANGLE);
      rtcSetGeometryBuildQuality(geom,RTC_BUILD_QUALITY_MEDIUM);
      rtcSetGeometryTimeStepCount(geom,1);
      std::cerr << "Tri: #: " << tri->getNumElements() << ", #corners: " << tri->getNumCorners() << ", #coord: " << tri->getNumCoords() << std::endl;

      Vertex* vertices = (Vertex*) rtcSetNewGeometryBuffer(geom,RTC_BUFFER_TYPE_VERTEX,0,RTC_FORMAT_FLOAT3,4*sizeof(float),tri->getNumCoords());
      for (Index i=0; i<tri->getNumCoords(); ++i) {
         vertices[i].x = tri->x()[i];
         vertices[i].y = tri->y()[i];
         vertices[i].z = tri->z()[i];
      }
      

      //data->indexBuffer = new Triangle[numElem];
      //rtcSetSharedGeometryBuffer(geom_0,RTC_BUFFER_TYPE_INDEX,0,RTC_FORMAT_UINT3,data->indexBuffer,0,sizeof(Triangle),numElem);
      Quad* quad = (Quad*) rtcSetNewGeometryBuffer(geom,RTC_BUFFER_TYPE_INDEX,0,RTC_FORMAT_UINT3,sizeof(Quad),numElem);
      data->indexBuffer = quad;
      if (tri->getNumCorners() == 0) {
         for (Index i=0; i<numElem; ++i) {
            quad[i].v0 = i*3;
            quad[i].v1 = i*3+1;
            quad[i].v2 = i*3+2;
            quad[i].v3 = i; // element id
         }
      } else {
         for (Index i=0; i<numElem; ++i) {
            quad[i].v0 = tri->cl()[i*3];
            quad[i].v1 = tri->cl()[i*3+1];
            quad[i].v2 = tri->cl()[i*3+2];
            quad[i].v3 = i; // element id
         }
      }
      
   } else if (auto poly = Polygons::as(geometry)) {

      Index ntri = poly->getNumCorners()-2*poly->getNumElements();
      vassert(ntri >= 0);

      geom = rtcNewGeometry (data->device, RTC_GEOMETRY_TYPE_TRIANGLE);
      rtcSetGeometryBuildQuality(geom,RTC_BUILD_QUALITY_MEDIUM);
      rtcSetGeometryTimeStepCount(geom,1);
      //std::cerr << "Poly: #tri: " << poly->getNumCorners()-2*poly->getNumElements() << ", #coord: " << poly->getNumCoords() << std::endl;

      Vertex* vertices = (Vertex*) rtcSetNewGeometryBuffer(geom,RTC_BUFFER_TYPE_VERTEX,0,RTC_FORMAT_FLOAT3,4*sizeof(float),poly->getNumCoords());
      for (Index i=0; i<poly->getNumCoords(); ++i) {
         vertices[i].x = poly->x()[i];
         vertices[i].y = poly->y()[i];
         vertices[i].z = poly->z()[i];
      }
      

      //data->indexBuffer = new Triangle[ntri];
      //rtcSetSharedGeometryBuffer(geom,RTC_BUFFER_TYPE_INDEX,0,RTC_FORMAT_UINT3,data->indexBuffer,0,sizeof(Triangle),ntri);
      Quad* triangles = (Quad*) rtcSetNewGeometryBuffer(geom,RTC_BUFFER_TYPE_INDEX,0,RTC_FORMAT_UINT3,sizeof(Quad),ntri);
      data->indexBuffer = triangles;
      Index t = 0;
      for (Index i=0; i<poly->getNumElements(); ++i) {
         const Index start = poly->el()[i];
         const Index end = poly->el()[i+1];
         const Index nvert = end-start;
         const Index last = end-1;
         for (Index v=0; v<nvert-2; ++v) {
            const Index v2 = v/2;
            if (v%2) {
               triangles[t].v0 = poly->cl()[last-v2];
               triangles[t].v1 = poly->cl()[start+v2+1];
               triangles[t].v2 = poly->cl()[last-v2-1];
            } else {
               triangles[t].v0 = poly->cl()[start+v2];
               triangles[t].v1 = poly->cl()[start+v2+1];
               triangles[t].v2 = poly->cl()[last-v2];
            }
            triangles[t].v3 = t;
            ++t;
         }
      }
      vassert(t == ntri);
      
   } else if (auto sph = Spheres::as(geometry)) {
      useNormals = false;

      Index nsph = sph->getNumSpheres();
      //std::cerr << "Spheres: #sph: " << nsph << std::endl;
      data->spheres = new ispc::Sphere[nsph];
      auto x = &sph->x()[0];
      auto y = &sph->y()[0];
      auto z = &sph->z()[0];
      auto r = &sph->r()[0];
      auto s = data->spheres;
      for (Index i=0; i<nsph; ++i) {
         s[i].p.x = x[i];
         s[i].p.y = y[i];
         s[i].p.z = z[i];
         s[i].r = r[i];
      }
      geom = newSpheres(data.get(), nsph);
   } else if (auto point = Points::as(geometry)) {

      Index np = point->getNumPoints();
      //std::cerr << "Points: #sph: " << np << std::endl;
      data->spheres = new ispc::Sphere[np];
      auto x = &point->x()[0];
      auto y = &point->y()[0];
      auto z = &point->z()[0];
      auto s = data->spheres;
      for (Index i=0; i<np; ++i) {
         s[i].p.x = x[i];
         s[i].p.y = y[i];
         s[i].p.z = z[i];
         s[i].r = pointSize;
      }
      data->lighted = 0;
      geom = newSpheres(data.get(), np);
   } else if (auto line = Lines::as(geometry)) {

      Index nStrips = line->getNumElements();
      Index nPoints = line->getNumCorners();
      std::cerr << "Lines: #strips: " << nStrips << ", #corners: " << nPoints << std::endl;
      data->primitiveFlags = new unsigned int[nPoints];
      data->spheres = new ispc::Sphere[nPoints];

      auto el = &line->el()[0];
      auto cl = &line->cl()[0];
      auto x = &line->x()[0];
      auto y = &line->y()[0];
      auto z = &line->z()[0];
      auto s = data->spheres;
      auto p = data->primitiveFlags;
      Index idx=0;
      for (Index strip=0; strip<nStrips; ++strip) {
         const Index begin = el[strip], end = el[strip+1];
         for (Index c=begin; c<end; ++c) {
            
            Index i = cl[c];
            s[idx].p.x = x[i];
            s[idx].p.y = y[i];
            s[idx].p.z = z[i];
            s[idx].r = pointSize;
            p[idx] = ispc::PFNone;
            if (c+1 != end)
               p[idx] |= ispc::PFCone;
            p[idx] |= ispc::PFStartSphere;
            ++idx;
         }
      }
      vassert(idx == nPoints);
      data->lighted = 0;
      geom = newTubes(data.get(), nPoints-1);
   } else if (auto tube = Tubes::as(geometry)) {
      useNormals= false;

      Index nStrips = tube->getNumTubes();
      Index nPoints = tube->getNumCoords();
      std::cerr << "Tubes: #strips: " << nStrips << ", #corners: " << nPoints << std::endl;
      data->primitiveFlags = new unsigned int[nPoints];
      data->spheres = new ispc::Sphere[nPoints];
      const Tubes::CapStyle startStyle = tube->startStyle(), jointStyle = tube->jointStyle(), endStyle = tube->endStyle();

      auto el = tube->components().data();
      auto x = &tube->x()[0];
      auto y = &tube->y()[0];
      auto z = &tube->z()[0];
      auto r = &tube->r()[0];
      auto s = data->spheres;
      auto p = data->primitiveFlags;
      Index idx=0;
      for (Index strip=0; strip<nStrips; ++strip) {
         const Index begin = el[strip], end = el[strip+1];
         vassert(idx == begin);
         for (Index i=begin; i<end; ++i) {
            
            s[idx].p.x = x[i];
            s[idx].p.y = y[i];
            s[idx].p.z = z[i];
            s[idx].r = r[i];

            p[idx] = ispc::PFNone;
            if (i == begin) {
               switch(startStyle) {
                  case Tubes::Flat:
                     p[idx] |= ispc::PFStartDisc;
                     break;
                  case Tubes::Round:
                     p[idx] |= ispc::PFStartSphere;
                     break;
                  default:
                     break;
               }
            } else if (i+1 != end) {
               if (jointStyle == Tubes::Flat) {
                  p[idx] |= ispc::PFStartDisc;
               }
            }

            if (i+1 != end) {
               p[idx] |= ispc::PFCone;

               switch((i+2==end) ? endStyle : jointStyle) {
                  case Tubes::Open:
                     break;
                  case Tubes::Flat:
                     p[idx] |= ispc::PFEndDisc;
                     break;
                  case Tubes::Round:
                     p[idx] |= i+2==end ? ispc::PFEndSphere : ispc::PFEndSphereSect;
                     break;
                  case Tubes::Arrow:
                     p[idx] |= ispc::PFArrow;
                     break;
               }
            }

            ++idx;
         }
         vassert(idx == end);
      }
      vassert(idx == nPoints);
      geom = newTubes(data.get(), nPoints > 0 ? nPoints-1 : 0);
   }

   if (geom) {
       if (this->normals && useNormals) {

           if (this->normals->guessMapping(geometry) == DataBase::Element)
               data->normalsPerPrimitiveMapping = 1;

           for (int c=0; c<3; ++c) {
               data->normals[c] = &this->normals->x(c)[0];
           }
       }

       data->geomID = rtcAttachGeometry(data->scene, geom);
       rtcReleaseGeometry(geom);
       rtcCommitGeometry(geom);

       std::cerr << "added geom " << (data->indexBuffer ? "with" : "without") << " indexbuffer" << std::endl;
   }

   rtcCommitScene(data->scene);
}

RayRenderObject::~RayRenderObject() {

   delete[] data->spheres;
   delete[] data->primitiveFlags;
   //rtcDeleteGeometry(data->scene, data->geomID); // not possible for static geometry
   if (data->scene)
      rtcReleaseScene(data->scene);
   //delete[] data->indexBuffer;
}

void RayColorMap::deinit() {

    if (cmap) {
        cmap->min = 0.f;
        cmap->max = 1.f;
        cmap->texWidth = 0;
        cmap->blendWithMaterial = 0;
        cmap->texData = nullptr;
    }
}
