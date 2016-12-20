#include "parrendmgr.h"
#include "renderobject.h"
#include "renderer.h"

namespace mpi = boost::mpi;

namespace vistle {

void toIcet(IceTDouble *imat, const vistle::Matrix4 &vmat) {
   for (int i=0; i<16; ++i) {
      imat[i] = vmat.data()[i];
   }
}

ParallelRemoteRenderManager::ParallelRemoteRenderManager(Renderer *module, IceTDrawCallback drawCallback)
: m_module(module)
, m_drawCallback(drawCallback)
, m_displayRank(0)
, m_rhrControl(module, m_displayRank)
, m_delay(nullptr)
, m_delaySec(0.)
, m_defaultColor(Vector4(255, 255, 255, 255))
, m_updateBounds(1)
, m_doRender(1)
, m_lightsUpdateCount(1000) // start with a value that is different from the one managed by RhrServer
, m_currentView(-1)
, m_frameComplete(true)
{
   m_continuousRendering = m_module->addIntParameter("continuous_rendering", "render even though nothing has changed", 1, Parameter::Boolean);
   m_delay = m_module->addFloatParameter("delay", "artificial delay (s)", m_delaySec);
   m_module->setParameterRange(m_delay, 0., 3.);
   m_colorRank = m_module->addIntParameter("color_rank", "different colors on each rank", 0, Parameter::Boolean);
   m_icetComm = icetCreateMPICommunicator((MPI_Comm)m_module->comm());
}

ParallelRemoteRenderManager::~ParallelRemoteRenderManager() {

   for (auto &icet: m_icet) {
      if (icet.ctxValid)
         icetDestroyContext(icet.ctx);
   }
   icetDestroyMPICommunicator(m_icetComm);
}

bool ParallelRemoteRenderManager::checkIceTError(const char *msg) const {

   const char *err = "No error.";
   switch(icetGetError()) {
      case ICET_INVALID_VALUE:
         err = "An inappropriate value has been passed to a function.";
         break;
      case ICET_INVALID_OPERATION:
         err = "An inappropriate function has been called.";
         break;
      case ICET_OUT_OF_MEMORY:
         err = "IceT has ran out of memory for buffer space.";
         break;
      case ICET_BAD_CAST:
         err = "A function has been passed a value of the wrong type.";
         break;
      case ICET_INVALID_ENUM:
         err = "A function has been passed an invalid constant.";
         break;
      case ICET_SANITY_CHECK_FAIL:
         err = "An internal error (or warning) has occurred.";
         break;
      case ICET_NO_ERROR:
         return false;
         break;
   }

   std::cerr << "IceT error at " << msg << ": " << err << std::endl;
   return true;
}


void ParallelRemoteRenderManager::setModified() {

   m_doRender = 1;
}

void ParallelRemoteRenderManager::setLocalBounds(const Vector3 &min, const Vector3 &max) {

   localBoundMin = min;
   localBoundMax = max;
   m_updateBounds = 1;
}

bool ParallelRemoteRenderManager::handleParam(const Parameter *p) {

    setModified();

    if (p == m_colorRank) {

      if (m_colorRank->getValue()) {

         const int r = m_module->rank()%3;
         const int g = (m_module->rank()/3)%3;
         const int b = (m_module->rank()/9)%3;
         m_defaultColor = Vector4(63+r*64, 63+g*64, 63+b*64, 255);
      } else {

         m_defaultColor = Vector4(255, 255, 255, 255);
      }
      return true;
    } else if (p == m_delay) {
       m_delaySec = m_delay->getValue();
    }

    return m_rhrControl.handleParam(p);
}

bool ParallelRemoteRenderManager::prepareFrame(size_t numTimesteps) {

   m_state.numTimesteps = numTimesteps;
   m_state.numTimesteps = mpi::all_reduce(m_module->comm(), m_state.numTimesteps, mpi::maximum<unsigned>());

   if (m_updateBounds) {
      Vector min, max;
      m_module->getBounds(min, max);
      setLocalBounds(min, max);
   }

   m_updateBounds = mpi::all_reduce(m_module->comm(), m_updateBounds, mpi::maximum<int>());
   if (m_updateBounds) {
      mpi::all_reduce(m_module->comm(), localBoundMin.data(), 3, m_state.bMin.data(), mpi::minimum<Scalar>());
      mpi::all_reduce(m_module->comm(), localBoundMax.data(), 3, m_state.bMax.data(), mpi::maximum<Scalar>());
   }

   auto rhr = m_rhrControl.server();
   if (rhr) {
      if (m_updateBounds) {
         Vector center = 0.5*(m_state.bMin+m_state.bMax);
         Scalar radius = (m_state.bMax-m_state.bMin).norm()*0.5;
         rhr->setBoundingSphere(center, radius);
      }

      rhr->setNumTimesteps(m_state.numTimesteps);

      rhr->preFrame();
      if (m_module->rank() == rootRank()) {

          if (m_state.timestep != rhr->timestep())
              m_doRender = 1;
      }
      m_state.timestep = rhr->timestep();

      if (rhr->lightsUpdateCount != m_lightsUpdateCount) {
          m_doRender = 1;
          m_lightsUpdateCount = rhr->lightsUpdateCount;
      }

      for (size_t i=m_viewData.size(); i<rhr->numViews(); ++i) {
          std::cerr << "new view no. " << i << std::endl;
          m_doRender = 1;
          m_viewData.emplace_back();
      }

      for (size_t i=0; i<rhr->numViews(); ++i) {

          PerViewState &vd = m_viewData[i];

          if (vd.width != rhr->width(i)
                  || vd.height != rhr->height(i)
                  || vd.proj != rhr->projMat(i)
                  || vd.view != rhr->viewMat(i)
                  || vd.model != rhr->modelMat(i)) {
              m_doRender = 1;
          }

          vd.rhrParam = rhr->getViewParameters(i);

          vd.width = rhr->width(i);
          vd.height = rhr->height(i);
          vd.model = rhr->modelMat(i);
          vd.view = rhr->viewMat(i);
          vd.proj = rhr->projMat(i);

          vd.lights = rhr->lights;
      }
   }
   m_updateBounds = 0;

   if (m_continuousRendering->getValue())
      m_doRender = 1;

   bool doRender = mpi::all_reduce(m_module->comm(), m_doRender, mpi::maximum<int>());
   m_doRender = 0;

   if (doRender) {

      mpi::broadcast(m_module->comm(), m_state, rootRank());
      mpi::broadcast(m_module->comm(), m_viewData, rootRank());

      if (m_rgba.size() != m_viewData.size())
          m_rgba.resize(m_viewData.size());
      if (m_depth.size() != m_viewData.size())
          m_depth.resize(m_viewData.size());
      for (size_t i=0; i<m_viewData.size(); ++i) {
          const PerViewState &vd = m_viewData[i];
          m_rgba[i].resize(vd.width*vd.height*4);
          m_depth[i].resize(vd.width*vd.height);
          if (rhr && m_module->rank() != rootRank()) {
              rhr->resize(i, vd.width, vd.height);
          }
      }
   }

   return doRender;
}

size_t ParallelRemoteRenderManager::timestep() const {
   return m_state.timestep;
}

size_t ParallelRemoteRenderManager::numViews() const {
   return m_viewData.size();
}

void ParallelRemoteRenderManager::setCurrentView(size_t i) {

   checkIceTError("setCurrentView");

   vassert(m_currentView == -1);
   auto rhr = m_rhrControl.server();

   int resetTiles = 0;
   while (m_icet.size() <= i) {
      resetTiles = 1;
      m_icet.emplace_back();
      auto &icet = m_icet.back();
      icet.ctx = icetCreateContext(m_icetComm);
      icet.ctxValid = true;
#ifndef NDEBUG
      // that's too much output
      //icetDiagnostics(ICET_DIAG_ALL_NODES | ICET_DIAG_DEBUG);
#endif
   }
   vassert(i < m_icet.size());
   auto &icet = m_icet[i];
   //std::cerr << "setting IceT context: " << icet.ctx << " (valid: " << icet.ctxValid << ")" << std::endl;
   icetSetContext(icet.ctx);
   m_currentView = i;

   if (rhr) {
      if (icet.width != rhr->width(i) || icet.height != rhr->height(i))
         resetTiles = 1;
   }
   resetTiles = mpi::all_reduce(m_module->comm(), resetTiles, mpi::maximum<int>());

   if (resetTiles) {
      std::cerr << "resetting IceT tiles for view " << i << "..." << std::endl;
      icet.width = icet.height = 0;
      if (m_module->rank() == rootRank()) {
         icet.width =  rhr->width(i);
         icet.height = rhr->height(i);
         std::cerr << "IceT dims on rank " << m_module->rank() << ": " << icet.width << "x" << icet.height << std::endl;
      }

      DisplayTile localTile;
      localTile.x = 0;
      localTile.y = 0;
      localTile.width = icet.width;
      localTile.height = icet.height;

      std::vector<DisplayTile> icetTiles;
      mpi::all_gather(m_module->comm(), localTile, icetTiles);
      vassert(icetTiles.size() == (unsigned)m_module->size());

      icetResetTiles();

      for (int i=0; i<m_module->size(); ++i) {
         if (icetTiles[i].width>0 && icetTiles[i].height>0)
            icetAddTile(icetTiles[i].x, icetTiles[i].y, icetTiles[i].width, icetTiles[i].height, i);
      }

      icetSetColorFormat(ICET_IMAGE_COLOR_RGBA_UBYTE);
      icetSetDepthFormat(ICET_IMAGE_DEPTH_FLOAT);
      icetCompositeMode(ICET_COMPOSITE_MODE_Z_BUFFER);
      //icetStrategy(ICET_STRATEGY_REDUCE);
      icetStrategy(ICET_STRATEGY_SEQUENTIAL);
      icetDisable(ICET_COMPOSITE_ONE_BUFFER); // include depth buffer in compositing result

      icetDrawCallback(m_drawCallback);

      checkIceTError("after reset tiles");
   }

   icetBoundingBoxf(localBoundMin[0], localBoundMax[0],
         localBoundMin[1], localBoundMax[1],
         localBoundMin[2], localBoundMax[2]);
}

void ParallelRemoteRenderManager::finishCurrentView(const IceTImage &img, int timestep) {

   const bool lastView = size_t(m_currentView)==m_viewData.size()-1;
   finishCurrentView(img, timestep, lastView);
}

void ParallelRemoteRenderManager::finishCurrentView(const IceTImage &img, int timestep, bool lastView) {

   checkIceTError("before finishCurrentView");

   vassert(m_currentView >= 0);
   const size_t i = m_currentView;
   vassert(i < numViews());
   //std::cerr << "finishCurrentView: " << i << std::endl;
   if (m_module->rank() == rootRank()) {
      auto rhr = m_rhrControl.server();
      vassert(rhr);
      const int bpp = 4;
      const int w = rhr->width(i);
      const int h = rhr->height(i);
      vassert(std::max(0,w) == icetImageGetWidth(img));
      vassert(std::max(0,h) == icetImageGetHeight(img));


      const IceTUByte *color = nullptr;
      switch (icetImageGetColorFormat(img)) {
      case ICET_IMAGE_COLOR_RGBA_UBYTE:
          color = icetImageGetColorcub(img);
          break;
      case ICET_IMAGE_COLOR_RGBA_FLOAT:
          std::cerr << "expected byte color, got float" << std::endl;
          break;
      case ICET_IMAGE_COLOR_NONE:
          std::cerr << "expected byte color, got no color" << std::endl;
          break;
      }
      const IceTFloat *depth = nullptr;
      switch (icetImageGetDepthFormat(img)) {
      case ICET_IMAGE_DEPTH_FLOAT:
          depth = icetImageGetDepthcf(img);
          break;
      case ICET_IMAGE_DEPTH_NONE:
          std::cerr << "expected byte color, got no color" << std::endl;
          break;
      }

      if (color && depth) {
         for (int y=0; y<h; ++y) {
            memcpy(rhr->rgba(i)+w*bpp*y, color+w*(h-1-y)*bpp, bpp*w);
            memcpy(rhr->depth(i)+w*y, depth+w*(h-1-y), sizeof(float)*w);
         }

         m_viewData[i].rhrParam.timestep = timestep;
         rhr->invalidate(i, 0, 0, rhr->width(i), rhr->height(i), m_viewData[i].rhrParam, lastView);
      }
   }
   m_currentView = -1;
   m_frameComplete = lastView;
}

bool ParallelRemoteRenderManager::finishFrame(int timestep) {

   vassert(m_currentView == -1);

   if (m_frameComplete)
      return false;

   if (m_delaySec > 0.) {
      usleep(useconds_t(m_delaySec*1e6));
   }

   if (m_module->rank() == rootRank()) {
      auto rhr = m_rhrControl.server();
      vassert(rhr);
      m_viewData[0].rhrParam.timestep = timestep;
      rhr->invalidate(-1, 0, 0, 0, 0, m_viewData[0].rhrParam, true);
   }
   m_frameComplete = true;
   return true;
}

void ParallelRemoteRenderManager::getModelViewMat(size_t viewIdx, IceTDouble *mat) const {
   const PerViewState &vd = m_viewData[viewIdx];
   Matrix4 mv = vd.view * vd.model;
   toIcet(mat, mv);
}

void ParallelRemoteRenderManager::getProjMat(size_t viewIdx, IceTDouble *mat) const {
   const PerViewState &vd = m_viewData[viewIdx];
   toIcet(mat, vd.proj);
}

const ParallelRemoteRenderManager::PerViewState &ParallelRemoteRenderManager::viewData(size_t viewIdx) const {

    return m_viewData[viewIdx];
}

unsigned char *ParallelRemoteRenderManager::rgba(size_t viewIdx) {

    return m_rgba[viewIdx].data();
}

float *ParallelRemoteRenderManager::depth(size_t viewIdx) {

    return m_depth[viewIdx].data();
}

void ParallelRemoteRenderManager::updateRect(size_t viewIdx, const IceTInt *viewport) {

   auto rhr = m_rhrControl.server();
   if (rhr && m_module->rank() != rootRank()) {
      // observe what slaves are rendering
      const int bpp = 4;
      const int w = rhr->width(m_currentView);
      const int h = rhr->height(m_currentView);

      const unsigned char *color = rgba(viewIdx);

      memset(rhr->rgba(m_currentView), 0, w*h*bpp);

      for (int y=viewport[1]; y<viewport[1]+viewport[3]; ++y) {
         memcpy(rhr->rgba(m_currentView)+(w*y+viewport[0])*bpp, color+(w*(h-1-y)+viewport[0])*bpp, bpp*viewport[2]);
      }

      rhr->invalidate(m_currentView, 0, 0, rhr->width(m_currentView), rhr->height(m_currentView), rhr->getViewParameters(m_currentView), true);
   }
}

void ParallelRemoteRenderManager::addObject(std::shared_ptr<RenderObject> ro) {
   m_updateBounds = 1;
}

void ParallelRemoteRenderManager::removeObject(std::shared_ptr<RenderObject> ro) {
   m_updateBounds = 1;
}

}
