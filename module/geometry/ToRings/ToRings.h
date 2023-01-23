#ifndef QUADSTOCIRCLES_H
#define QUADSTOCIRCLES_H

#include <vistle/module/module.h>
#include <array>
#include <vector>
#include <vistle/core/vectortypes.h>
class ToRings: public vistle::Module {
public:
    ToRings(const std::string &name, int moduleID, mpi::communicator comm);
    ~ToRings();

private:
    virtual bool compute();
    std::vector<vistle::Vector3> toTorus(const std::vector<vistle::Vector3> &points, vistle::Index numTorusSegments, vistle::Index numDiameterSegments);
    std::vector<vistle::Vector3> toTorusCircle(const std::vector<vistle::Vector3> &points, const vistle::Vector3& middle, vistle::Index numTorusSegments, vistle::Index numDiameterSegments);
    std::vector<vistle::Vector3> toTorusSpline(const std::vector<vistle::Vector3> &points, const vistle::Vector3& middle, vistle::Index numTorusSegments, vistle::Index numDiameterSegments);

    vistle::IntParameter *m_geoMode;
    vistle::FloatParameter *m_radius;
    vistle::FloatParameter *m_stretch;
    vistle::IntParameter *m_numXSegments;
    vistle::IntParameter *m_numYSegments;
    std::array<vistle::IntParameter *, 5> m_knots;
    float m_radiusValue = -1.0;
};

#endif // QUADSTOCIRCLES_H
