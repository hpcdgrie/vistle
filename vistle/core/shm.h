#ifndef SHM_H
#define SHM_H

#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>

#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>

#include <boost/serialization/access.hpp>
#include <boost/serialization/array.hpp>

#include "exception.h"
#include "vistle.h"

//#define SHMDEBUG
//#define SHMPUBLISH

namespace vistle {

typedef boost::interprocess::managed_shared_memory::handle_t shm_handle_t;
typedef char shm_name_t[32];

namespace message {
   class MessageQueue;
}

class VCEXPORT shm_exception: public exception {
   public:
   shm_exception(const std::string &what = "shared memory error") : exception(what) {}
};

class Object;

#ifdef SHMDEBUG
struct ShmDebugInfo {
   shm_name_t name;
   shm_handle_t handle;
   char deleted;
   char type;

   ShmDebugInfo(char type='\0', const std::string &name = "", shm_handle_t handle = 0)
      : handle(handle)
      , deleted(0)
      , type(type)
   {
      memset(this->name, '\0', sizeof(this->name));
      strncpy(this->name, name.c_str(), sizeof(this->name)-1);
   }
};
#endif

template<typename T>
struct shm {
   typedef boost::interprocess::allocator<T, boost::interprocess::managed_shared_memory::segment_manager> allocator;
   typedef boost::interprocess::basic_string<T, std::char_traits<T>, allocator> string;
   typedef boost::interprocess::vector<T, allocator> vector;
   typedef boost::interprocess::offset_ptr<vector> ptr;
   static typename boost::interprocess::managed_shared_memory::segment_manager::template construct_proxy<T>::type construct(const std::string &name);
   static void destroy(const std::string &name);
};

class VCEXPORT Shm {

 public:
   static Shm & the();
   static Shm & create(const std::string &shmname, const int moduleID, const int rank,
                         message::MessageQueue *messageQueue = NULL);
   static Shm & attach(const std::string &shmname, const int moduleID, const int rank,
                         message::MessageQueue *messageQueue = NULL);
   ~Shm();

   const std::string &name() const;

   typedef boost::interprocess::allocator<void, boost::interprocess::managed_shared_memory::segment_manager> void_allocator;
   const void_allocator &allocator() const;

   boost::interprocess::managed_shared_memory &shm();
   const boost::interprocess::managed_shared_memory &shm() const;
   std::string createObjectID();

   void publish(const shm_handle_t & handle);
   boost::shared_ptr<const Object> getObjectFromHandle(const shm_handle_t & handle);
   shm_handle_t getHandleFromObject(boost::shared_ptr<const Object> object);
   shm_handle_t getHandleFromObject(const Object *object);

   static std::string shmIdFilename();
   static bool cleanAll();

#ifdef SHMDEBUG
   static vistle::shm<ShmDebugInfo>::vector *s_shmdebug;
   void markAsRemoved(const std::string &name);
#endif

 private:
   Shm(const std::string &name, const int moduleID, const int rank, const size_t size,
       message::MessageQueue *messageQueue, bool create);

   void_allocator *m_allocator;
   std::string m_name;
   bool m_created;
   const int m_moduleID;
   const int m_rank;
   int m_objectID;
   static Shm *s_singleton;
   boost::interprocess::managed_shared_memory *m_shm;
   message::MessageQueue *m_messageQueue;
};

template<typename T>
typename boost::interprocess::managed_shared_memory::segment_manager::template construct_proxy<T>::type shm<T>::construct(const std::string &name) {
   return Shm::the().shm().construct<T>(name.c_str());
}

template<typename T>
void shm<T>::destroy(const std::string &name) {
      Shm::the().shm().destroy<T>(name.c_str());
#ifdef SHMDEBUG
      Shm::the().markAsRemoved(name);
#endif
}

template<typename T>
class VCEXPORT ShmVector {
#ifdef SHMDEBUG
   friend void Shm::markAsRemoved(const std::string &name);
#endif

   public:
      class ptr {
         public:
            ptr(ShmVector *p = NULL);
            ptr(const ptr &ptr);
            ~ptr();
            ptr &operator=(ptr &other);
            ptr &operator=(ShmVector *p);

            ShmVector &operator*() {
               return *m_p;
            }
            ShmVector *operator->() {
               return &*m_p;
            }

         private:
            boost::interprocess::offset_ptr<ShmVector> m_p;
      };

      ShmVector(size_t size = 0);
      int refcount() const;
      void* operator new(size_t size);
      void operator delete(void *p);

      T &operator[](size_t i) { return (*m_x)[i]; }
      const T &operator[](size_t i) const { return (*m_x)[i]; }

      size_t size() const { return m_x->size(); }
      void resize(size_t s);

      typename shm<T>::ptr &operator()() { return m_x; }
      typename shm<const T>::ptr &operator()() const { return m_x; }

      void push_back(const T &d) { m_x->push_back(d); }

   private:
      ~ShmVector();
      void ref();
      void unref();

      friend class boost::serialization::access;
      template<class Archive>
         void serialize(Archive &ar, const unsigned int version);
      template<class Archive>
         void save(Archive &ar, const unsigned int version) const;
      template<class Archive>
         void load(Archive &ar, const unsigned int version);

      boost::interprocess::interprocess_mutex m_mutex;
      int m_refcount;
      shm_name_t m_name;
      typename shm<T>::ptr m_x;
};

} // namespace vistle

#endif

#ifdef VISTLE_SHM_IMPL
#include "shm_impl.h"
#endif
