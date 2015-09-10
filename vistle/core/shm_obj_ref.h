#ifndef SHM_OBJ_REF_H
#define SHM_OBJ_REF_H

#include <string>

namespace vistle {

template<class T>
class shm_obj_ref {

    typedef T ObjType;
    typedef typename ObjType::Data ObjData;

 public:
    shm_obj_ref()
    : m_name(Shm::the().createObjectId())
    , m_d(nullptr)
    {
        ref();
    }

    shm_obj_ref(const std::string &name, ObjType *p)
    : m_name(name)
    , m_d(p->d())
    {
        ref();
    }

    shm_obj_ref(const shm_obj_ref &other)
    : m_name(other.m_name)
    , m_d(other.m_d)
    {
        ref();
    }

    shm_obj_ref(const shm_name_t name)
    : m_name(name)
    , m_d(shm<T>::find(name))
    {
        ref();
    }

    ~shm_obj_ref() {
        unref();
    }

    template<typename... Args>
    static shm_obj_ref create(const Args&... args) {
       shm_obj_ref result;
       result.construct(args...);
       return result;
    }

    bool find() {
        auto mem = vistle::shm<ObjData>::find(m_name);
        m_d = mem;

        return valid();
    }

    template<typename... Args>
    void construct(const Args&... args)
    {
        unref();
        m_d = shm<T>::construct(m_name)(args..., Shm::the().allocator());
        ref();
    }

    const shm_obj_ref &operator=(const shm_obj_ref &rhs) {
        unref();
        m_d = rhs.m_d;
        ref();
        return *this;
    }

    const shm_obj_ref &operator=(typename ObjType::const_ptr &rhs) {
        unref();
        m_d = rhs->d();
        ref();
        return *this;
    }

   bool valid() const {
       return m_d;
   }

   typename ObjType::const_ptr getObject() const {
       if (!valid())
           return nullptr;
       return ObjType::as(Object::create(m_d.get()));
   }

   typename ObjType::Data *getData() const {
       if (!valid())
           return nullptr;
       return m_d.get();
   }

   operator bool() const {
       return valid();
   }

#if 0
   T &operator*();
   const T &operator*() const;

   T *operator->();
   const T *operator->() const;
#endif

   const shm_name_t &name() const;

 private:
   shm_name_t m_name;
   boost::interprocess::offset_ptr<ObjData> m_d;

   void ref() {
       if (m_d) {
           assert(m_d->refcount >= 0);
           m_d->ref();
       }
   }

   void unref() {
       if (m_d) {
          assert(m_d->refcount > 0);
          m_d->unref();
       }
       m_d = nullptr;
   }

   friend class boost::serialization::access;
   template<class Archive>
   void serialize(Archive &ar, const unsigned int version) {
       boost::serialization::split_member(ar, *this, version);
   }
   template<class Archive>
   void save(Archive &ar, const unsigned int version) const {
      ar & boost::serialization::make_nvp("obj_name", m_name);
   }
   template<class Archive>
   void load(Archive &ar, const unsigned int version) {
      ar & boost::serialization::make_nvp("obj_name", m_name);

      //assert(shmname == m_name);
      std::string name = m_name;

      unref();
      m_d = nullptr;
      auto obj = ar.currentObject();
      auto handler = ar.objectCompletionHandler();
      auto ref =  ar.template getObject(name, [this, name, obj, handler]() -> void {
         std::cerr << "object completion handler: " << name << std::endl;
         auto ref = Shm::the().getObjectFromName(name);
         assert(ref);
         *this = ref;
         if (obj) {
            obj->referenceResolved(handler);
         }
      });
      if (ref) {
         *this = ref;
      } else {
         std::cerr << "waiting for completion of object " << name << std::endl;
         auto obj = ar.currentObject();
         if (obj)
            obj->objectValid(getData());
      }
   }
};

} // namespace vistle

#endif
