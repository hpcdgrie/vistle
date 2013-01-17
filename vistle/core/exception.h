#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <exception>
#include <string>
#include "vistle.h"

namespace vistle {

class VCEXPORT exception: public std::exception {

   public:
   exception(const std::string &what = "vistle error");
   virtual ~exception() throw();

   virtual const char* what() const throw();

   private:
   std::string m_what;
};

} // namespace vistle
#endif
