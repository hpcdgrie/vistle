#include <array>
#include <limits>
#include <map>
#include <vector>

#include <diy/master.hpp>
#include <diy/link.hpp>

#include <vistle/module/module.h>
#include <vistle/core/coords.h>
#include <vistle/core/vec.h>
#include <vistle/core/unstr.h>

using namespace vistle;

namespace {

using Key = std::array<Scalar, 3>;

struct Block {
    std::vector<Scalar> coords; // interleaved xyz
    std::array<Scalar, 6> bounds{{std::numeric_limits<Scalar>::max(), std::numeric_limits<Scalar>::max(),
                                  std::numeric_limits<Scalar>::max(), std::numeric_limits<Scalar>::lowest(),
                                  std::numeric_limits<Scalar>::lowest(), std::numeric_limits<Scalar>::lowest()}};
    std::map<Key, Index> shared; // local vertex position -> number of other ranks having it
    std::map<Key, int> minRank; // lowest other rank having the vertex

    static void *create() { return new Block; }
    static void destroy(void *b) { delete static_cast<Block *>(b); }
};

} // namespace

class VertexOverlap: public Module {
public:
    VertexOverlap(const std::string &name, int moduleID, mpi::communicator comm);

private:
    Port *m_gridIn = nullptr, *m_gridOut = nullptr, *m_dataOut = nullptr;
    Coords::const_ptr m_grid;
    Object::const_ptr m_input;
    int m_numGrids = 0;

    bool prepare() override;
    bool compute() override;
    bool reduce(int timestep) override;
};

VertexOverlap::VertexOverlap(const std::string &name, int moduleID, mpi::communicator comm)
: Module(name, moduleID, comm)
{
    m_gridIn = createInputPort("grid_in", "grid with coordinates");
    m_gridOut =
        createOutputPort("grid_out", "unstructured grid, cells entirely shared with lower ranks flagged as ghost");
    m_dataOut = createOutputPort("data_out", "number of other ranks sharing each vertex");
    setReducePolicy(message::ReducePolicy::OverAll);
}

bool VertexOverlap::prepare()
{
    m_grid.reset();
    m_input.reset();
    m_numGrids = 0;
    return true;
}

bool VertexOverlap::compute()
{
    auto obj = expect<Object>(m_gridIn);
    if (!obj)
        return true;
    auto coords = Coords::as(obj);
    auto data = DataBase::as(obj);
    if (!coords && data)
        coords = Coords::as(data->grid());
    if (!coords) {
        sendError("input has no coordinates");
        return true;
    }
    if (++m_numGrids == 1) {
        m_grid = coords;
        m_input = obj;
    }
    return true;
}

bool VertexOverlap::reduce(int timestep)
{
    if (timestep != -1)
        return true;

    // all ranks must take part in the collective operations below
    if (m_numGrids > 1)
        sendWarning("%d grids received, only the first one is used", m_numGrids);

    diy::mpi::communicator world{static_cast<MPI_Comm>(comm())};
    const int nranks = world.size();
    const int rank = world.rank();

    diy::Master master(world, 1, -1, &Block::create, &Block::destroy);
    auto *block = new Block;
    if (m_grid) {
        const Index n = m_grid->getNumCoords();
        const Scalar *x[3] = {m_grid->x().data(), m_grid->y().data(), m_grid->z().data()};
        block->coords.resize(3 * size_t(n));
        for (Index i = 0; i < n; ++i) {
            for (int c = 0; c < 3; ++c) {
                const Scalar v = x[c][i];
                block->coords[3 * i + c] = v;
                block->bounds[c] = std::min(block->bounds[c], v);
                block->bounds[3 + c] = std::max(block->bounds[3 + c], v);
            }
            block->shared[Key{x[0][i], x[1][i], x[2][i]}] = 0;
        }
    }

    // one block per rank, linked to all other ranks
    auto *link = new diy::Link;
    for (int r = 0; r < nranks; ++r) {
        if (r != rank)
            link->add_neighbor(diy::BlockID{r, r});
    }
    master.add(rank, block, link);

    // exchange bounding boxes
    master.foreach ([](Block *b, const diy::Master::ProxyWithLink &cp) {
        std::vector<Scalar> bounds(b->bounds.begin(), b->bounds.end());
        for (int i = 0; i < cp.link()->size(); ++i)
            cp.enqueue(cp.link()->target(i), bounds);
    });
    master.exchange();

    // send vertices lying in the overlap of bounding boxes
    master.foreach ([](Block *b, const diy::Master::ProxyWithLink &cp) {
        for (int i = 0; i < cp.link()->size(); ++i) {
            const auto target = cp.link()->target(i);
            std::vector<Scalar> nb;
            cp.dequeue(target.gid, nb);
            std::vector<Scalar> out;
            if (nb.size() == 6) {
                Scalar lo[3], hi[3];
                bool overlap = true;
                for (int c = 0; c < 3; ++c) {
                    lo[c] = std::max(b->bounds[c], nb[c]);
                    hi[c] = std::min(b->bounds[3 + c], nb[3 + c]);
                    overlap = overlap && lo[c] <= hi[c];
                }
                if (overlap) {
                    for (size_t v = 0; v + 2 < b->coords.size(); v += 3) {
                        const Scalar *p = &b->coords[v];
                        if (p[0] >= lo[0] && p[0] <= hi[0] && p[1] >= lo[1] && p[1] <= hi[1] && p[2] >= lo[2] &&
                            p[2] <= hi[2]) {
                            out.insert(out.end(), p, p + 3);
                        }
                    }
                }
            }
            cp.enqueue(target, out);
        }
    });
    master.exchange();

    // count matches with vertices received from other ranks
    master.foreach ([](Block *b, const diy::Master::ProxyWithLink &cp) {
        for (int i = 0; i < cp.link()->size(); ++i) {
            std::vector<Scalar> in;
            const int nbr = cp.link()->target(i).gid;
            cp.dequeue(nbr, in);
            for (size_t v = 0; v + 2 < in.size(); v += 3) {
                const Key key{in[v], in[v + 1], in[v + 2]};
                auto it = b->shared.find(key);
                if (it == b->shared.end())
                    continue;
                ++it->second;
                auto m = b->minRank.emplace(key, nbr);
                m.first->second = std::min(m.first->second, nbr);
            }
        }
    });

    if (!m_grid)
        return true;

    const Index n = m_grid->getNumCoords();
    auto out = std::make_shared<Vec<Index>>(n);
    Index *o = out->x().data();
    const Scalar *x = m_grid->x().data();
    const Scalar *y = m_grid->y().data();
    const Scalar *z = m_grid->z().data();
    for (Index i = 0; i < n; ++i)
        o[i] = block->shared.at(Key{x[i], y[i], z[i]});

    out->setGrid(m_grid);
    out->setMapping(DataBase::Vertex);
    out->setMeta(m_input->meta());
    out->copyAttributes(m_input);
    updateMeta(out);
    addObject(m_dataOut, out);

    if (auto unstr = UnstructuredGrid::as(m_grid)) {
        auto ghosted = unstr->clone();
        const Index *el = unstr->el().data();
        const Index *cl = unstr->cl().data();
        for (Index e = 0; e < unstr->getNumElements(); ++e) {
            bool ghost = el[e + 1] > el[e];
            for (Index k = el[e]; k < el[e + 1] && ghost; ++k) {
                const Index v = cl[k];
                auto it = block->minRank.find(Key{x[v], y[v], z[v]});
                ghost = it != block->minRank.end() && it->second < rank;
            }
            ghosted->setGhost(e, ghost);
        }
        updateMeta(ghosted);
        addObject(m_gridOut, ghosted);
    }
    return true;
}

MODULE_MAIN(VertexOverlap)
