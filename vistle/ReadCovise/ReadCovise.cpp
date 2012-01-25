#include <sstream>
#include <iomanip>
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "object.h"

#include "ReadCovise.h"

MODULE_MAIN(ReadCovise)

ReadCovise::ReadCovise(int rank, int size, int moduleID)
   : Module("ReadCovise", rank, size, moduleID) {

   createOutputPort("grid_out");
   addFileParameter("filename", "");
}

void swap_int(unsigned int * data, const unsigned int num) {

   for (unsigned int i = 0; i < num; i++) {
      *data = (((*data) & 0xff000000) >> 24)
         | (((*data) & 0x00ff0000) >>  8)
         | (((*data) & 0x0000ff00) <<  8)
         | (((*data) & 0x000000ff) << 24);
      data++;
   }
}

void swap_float(float * d, const unsigned int num) {

   unsigned int *data = (unsigned int *) d;
   for (unsigned int i = 0; i < num; i++) {
      *data = (((*data) & 0xff000000) >> 24)
         | (((*data) & 0x00ff0000) >>  8)
         | (((*data) & 0x0000ff00) <<  8)
         | (((*data) & 0x000000ff) << 24);
      data++;
   }
}

size_t read_type(const int fd, char * data) {

   size_t r = 0, num = 6;
   while (r != num) {
      size_t n = read(fd, data + r, sizeof(char) * (num - r));
      if (n <= 0)
         break;
      r += n;
   }

   if (r != num) {
      std::cout << "ERROR ReadCovise::read_type read " << r
                << " elements instead of " << num << std::endl;
   }
   return r;
}


size_t read_int(const int fd, unsigned int * data, const size_t & num,
                const bool byte_swap) {

   size_t r = 0;
   while (r != num) {
      size_t n = read(fd, data + r, sizeof(unsigned int) * (num - r));
      if (n <= 0)
         break;
      r += n;
   }

   if (r != num) {
      std::cout << "ERROR ReadCovise::read_int read " << r
                << " elements instead of " << num << std::endl;
   }

   if (byte_swap)
      swap_int(data, r);

   return r;
}


size_t read_float(const int fd, float * data,
                  const size_t & num, const bool byte_swap) {

   size_t r = 0;
   while (r != num) {
      size_t n = read(fd, data + r, sizeof(float) * (num - r));
      if (n <= 0)
         break;
      r += n;
   }

   if (r != num) {
      std::cout << "ERROR ReadCovise::read_float read " << r
                << " elements instead of " << num << std::endl;
   }

   if (byte_swap)
      swap_float(data, r);

   return r;
}

ReadCovise::~ReadCovise() {

}

void ReadCovise::readAttributes(const int fd, const bool byteswap) {

   unsigned int size, num;
   read_int(fd, &size, 1, byteswap);
   size -= sizeof(unsigned int);
   read_int(fd, &num, 1, byteswap);
   /*
   std::cout << "ReadCovise::readAttributes: " << num << " attributes\n"
             << std::endl;
   */
   lseek(fd, size, SEEK_CUR);
}

vistle::Object * ReadCovise::readSETELE(const int fd, const bool byteswap) {

   vistle::Set *set = vistle::Set::create();

   unsigned int num;
   read_int(fd, &num, 1, byteswap);
   printf("set %d elems\n", num);
   char buf[7];
   buf[6] = 0;

   for (unsigned int index = 0; index < num; index ++) {
      read_type(fd, buf);

      vistle::Object *object = NULL;

      if (!strncmp(buf, "SETELE", 6))
         object = readSETELE(fd, byteswap);

      else if (!strncmp(buf, "UNSGRD", 6))
         object = readUNSGRD(fd, byteswap);

      else if (!strncmp(buf, "USTSDT", 6))
         object = readUSTSDT(fd, byteswap);

      else if (!strncmp(buf, "POLYGN", 6))
         object = readPOLYGN(fd, byteswap);

      else {
         std:: cout << "ReadCovise: object type [" << buf << "] unsupported"
                    << std::endl;
         exit(1);
      }

      set->elements->push_back(object);
   }
   readAttributes(fd, byteswap);

   return set;
}

vistle::Object *  ReadCovise::readUNSGRD(const int fd,
                                         const bool byteswap) {

   vistle::UnstructuredGrid *usg = NULL;

   unsigned int numElements;
   unsigned int numCorners;
   unsigned int numVertices;

   read_int(fd, &numElements, 1, byteswap);
   read_int(fd, &numCorners, 1, byteswap);
   read_int(fd, &numVertices, 1, byteswap);

   usg = vistle::UnstructuredGrid::create(numElements, numCorners, numVertices);

   unsigned int *el = new unsigned int[numElements];
   unsigned int *tl = new unsigned int[numElements];
   unsigned int *cl = new unsigned int[numCorners];

   read_int(fd, el, numElements, byteswap);
   read_int(fd, tl, numElements, byteswap);
   read_int(fd, cl, numCorners, byteswap);

   for (unsigned int index = 0; index < numElements; index ++) {
      (*usg->el)[index] = el[index];
      (*usg->tl)[index] = (vistle::UnstructuredGrid::Type) tl[index];
   }

   for (unsigned int index = 0; index < numCorners; index ++) {
      (*usg->cl)[index] = cl[index];
   }

   read_float(fd, &((*usg->x)[0]), numVertices, byteswap);
   read_float(fd, &((*usg->y)[0]), numVertices, byteswap);
   read_float(fd, &((*usg->z)[0]), numVertices, byteswap);

   readAttributes(fd, byteswap);

   delete[] el;
   delete[] tl;
   delete[] cl;

   /*
      unsigned int numElements;
      unsigned int numCorners;
      unsigned int numVertices;

      read_int(fd, &numElements, 1, byteswap);
      read_int(fd, &numCorners, 1, byteswap);
      read_int(fd, &numVertices, 1, byteswap);

      lseek(fd, numElements * sizeof(int), SEEK_CUR);
      lseek(fd, numElements * sizeof(int), SEEK_CUR);
      lseek(fd, numCorners * sizeof(int), SEEK_CUR);

      lseek(fd, numVertices * sizeof(float), SEEK_CUR);
      lseek(fd, numVertices * sizeof(float), SEEK_CUR);
      lseek(fd, numVertices * sizeof(float), SEEK_CUR);

   */
   readAttributes(fd, byteswap);

   return usg;
}

vistle::Object * ReadCovise::readUSTSDT(const int fd, const bool byteswap) {

   vistle::Vec<float> *array = NULL;
   unsigned int numElements;

   read_int(fd, &numElements, 1, byteswap);

   array = vistle::Vec<float>::create(numElements);
   read_float(fd, &((*array->x)[0]), numElements, byteswap);

   // lseek(fd, numElements * sizeof(float), SEEK_CUR);

   readAttributes(fd, byteswap);
   return array;
}

vistle::Object * ReadCovise::readPOLYGN(const int fd, const bool byteswap) {

   vistle::Polygons * polygons = NULL;
   unsigned int numElements, numCorners, numVertices;

   read_int(fd, &numElements, 1, byteswap);
   read_int(fd, &numCorners, 1, byteswap);
   read_int(fd, &numVertices, 1, byteswap);

   polygons = vistle::Polygons::create(numElements, numCorners, numVertices);

   unsigned int *el = new unsigned int[numElements];
   unsigned int *cl = new unsigned int[numCorners];

   read_int(fd, el, numElements, byteswap);
   read_int(fd, cl, numCorners, byteswap);

   for (unsigned int index = 0; index < numElements; index ++)
      (*polygons->el)[index] = el[index];

   for (unsigned int index = 0; index < numCorners; index ++)
      (*polygons->cl)[index] = cl[index];

   read_float(fd, &((*polygons->x)[0]), numVertices, byteswap);
   read_float(fd, &((*polygons->y)[0]), numVertices, byteswap);
   read_float(fd, &((*polygons->z)[0]), numVertices, byteswap);

   /*
      lseek(fd, numElements * sizeof(int), SEEK_CUR);
      lseek(fd, numCorners * sizeof(int), SEEK_CUR);

      lseek(fd, numVertices * sizeof(float), SEEK_CUR);
      lseek(fd, numVertices * sizeof(float), SEEK_CUR);
      lseek(fd, numVertices * sizeof(float), SEEK_CUR);
   */
   readAttributes(fd, byteswap);

   return polygons;
}

vistle::Object * ReadCovise::load(const std::string & name) {

   vistle::Object *object = NULL;

   bool byteswap = true;

   int fd = open(name.c_str(), O_RDONLY);
   if (fd == -1) {
      std::cout << "ERROR ReadCovise::load could not open file " << name
                << std::endl;
      return NULL;
   }

   char buf[7];
   buf[6] = 0;

   int r = read_type(fd, buf);
   if (r != 6)
      return NULL;

   if (!strncmp(buf, "COV_BE", 6))
      byteswap = false;
   else if (!strncmp(buf, "COV_LE", 6))
      byteswap = true;
   else
      lseek(fd, 0, SEEK_SET);

   printf("byteswap: %d\n", byteswap);

   if (read_type(fd, buf) == 6) {

      if (!strncmp(buf, "SETELE", 6))
         object = readSETELE(fd, byteswap);

      else if (!strncmp(buf, "UNSGRD", 6))
         object = readUNSGRD(fd, byteswap);

      else if (!strncmp(buf, "USTSDT", 6))
         object = readUSTSDT(fd, byteswap);

      else if (!strncmp(buf, "POLYGN", 6))
         object = readPOLYGN(fd, byteswap);

      else {
         std:: cout << "ReadCovise: object type [" << buf << "] unsupported"
                    << std::endl;
         exit(1);
      }
   }

   close(fd);

   return object;
}

bool ReadCovise::compute() {

   const std::string * name = getFileParameter("filename");

   struct timeval t0, t1;
   if (name) {
      gettimeofday(&t0, NULL);
      vistle::Object *object = load(*name);
      gettimeofday(&t1, NULL);

      long long usec = t1.tv_sec - t0.tv_sec;
      usec *= 1000000;
      usec += (t1.tv_usec - t0.tv_usec);

      printf("++++++++++ ReadCovise: %ld\n", usec);

      if (object)
         addObject("grid_out", object);
      /*
      std::vector<vistle::Object *>::iterator i;
      for (i = objects.begin(); i != objects.end(); i++)
         addObject("grid_out", *i);
      */
   }
   return true;
}
