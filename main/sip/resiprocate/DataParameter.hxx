#ifndef DataParameter_hxx
#define DataParameter_hxx

#include <sipstack/Parameter.hxx>
#include <util/Data.hxx>
#include <iostream>

namespace Vocal2
{

class DataParameter : public Parameter
{
   public:
      typedef Data Type;
      
      DataParameter(ParameterTypes::Type, const char* startData, unsigned int dataSize);
      DataParameter(ParameterTypes::Type, const Data& data);
      
      Data& value();
      virtual Parameter* clone() const;
      virtual std::ostream& encode(std::ostream& stream) const;
      
   private:
      Data mData;
};
 
}

#endif
