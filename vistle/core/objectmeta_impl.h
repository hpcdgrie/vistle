#ifndef OBJECTMETA_IMPL_H
#define OBJECTMETA_IMPL_H

#include "archives.h"
#include "serialize.h"

namespace vistle {

template<class Archive>
void Meta::serialize(Archive &ar, const unsigned int version) {
#ifdef DEBUG_SERIALIZATION
   const unsigned int check = 0xdeadbeef;
   unsigned int check1 = check;
   ar & V_NAME("check1", check1);
   assert(check1 == check);
#endif
   ar & V_NAME("block", m_block);
   ar & V_NAME("numblocks", m_numBlocks);
   ar & V_NAME("timestep", m_timestep);
   ar & V_NAME("numtimesteps", m_numTimesteps);
   ar & V_NAME("animationstep", m_animationstep);
   ar & V_NAME("numanimationsteps", m_numAnimationsteps);
   ar & V_NAME("iteration", m_iteration);
   ar & V_NAME("creator", m_creator);
   ar & V_NAME("executioncount", m_executionCount);
#ifdef DEBUG_SERIALIZATION
   unsigned int check2 = check;
   ar & V_NAME("check2", check2);
   assert(check2 == check);
#endif

}

// update this function to provide Write/Read HDF5 support for metadata object
// only members that can be typecasted to a double are currently supported
template<class Functor>
void Meta::doAllMembers(Functor &f) const {
    f(m_block);
    f(m_numBlocks);
    f(m_timestep);
    f(m_numTimesteps);
    f(m_animationstep);
    f(m_numAnimationsteps);
    f(m_iteration);
    f(m_executionCount);
    f(m_creator);
    f(m_realtime);
}

} // namespace vistle

#endif
