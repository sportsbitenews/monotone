/*************************************************
* DataSink Header File                           *
* (C) 1999-2007 Jack Lloyd                       *
*************************************************/

#ifndef BOTAN_DATA_SINK_H__
#define BOTAN_DATA_SINK_H__

#include <botan/filter.h>
#include <iosfwd>

namespace Botan {

/*************************************************
* Generic DataSink Interface                     *
*************************************************/
class BOTAN_DLL DataSink : public Filter
   {
   public:
      bool attachable() { return false; }
      DataSink() {}
      virtual ~DataSink() {}
   private:
      DataSink& operator=(const DataSink&) { return (*this); }
      DataSink(const DataSink&);
   };

/*************************************************
* Stream-Based DataSink                          *
*************************************************/
class BOTAN_DLL DataSink_Stream : public DataSink
   {
   public:
      void write(const byte[], u32bit);

      DataSink_Stream(std::ostream&, const std::string& = "");
      DataSink_Stream(const std::string&, bool = false);
      ~DataSink_Stream();
   private:
      const std::string identifier;
      const bool owner;

      std::ostream* sink;
   };

}

#endif
