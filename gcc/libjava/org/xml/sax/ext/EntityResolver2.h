
// DO NOT EDIT THIS FILE - it is machine generated -*- c++ -*-

#ifndef __org_xml_sax_ext_EntityResolver2__
#define __org_xml_sax_ext_EntityResolver2__

#pragma interface

#include <java/lang/Object.h>
extern "Java"
{
  namespace org
  {
    namespace xml
    {
      namespace sax
      {
          class InputSource;
        namespace ext
        {
            class EntityResolver2;
        }
      }
    }
  }
}

class org::xml::sax::ext::EntityResolver2 : public ::java::lang::Object
{

public:
  virtual ::org::xml::sax::InputSource * getExternalSubset(::java::lang::String *, ::java::lang::String *) = 0;
  virtual ::org::xml::sax::InputSource * resolveEntity(::java::lang::String *, ::java::lang::String *, ::java::lang::String *, ::java::lang::String *) = 0;
  virtual ::org::xml::sax::InputSource * resolveEntity(::java::lang::String *, ::java::lang::String *) = 0;
  static ::java::lang::Class class$;
} __attribute__ ((java_interface));

#endif // __org_xml_sax_ext_EntityResolver2__
