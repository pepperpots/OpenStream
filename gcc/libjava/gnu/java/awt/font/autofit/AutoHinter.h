
// DO NOT EDIT THIS FILE - it is machine generated -*- c++ -*-

#ifndef __gnu_java_awt_font_autofit_AutoHinter__
#define __gnu_java_awt_font_autofit_AutoHinter__

#pragma interface

#include <java/lang/Object.h>
extern "Java"
{
  namespace gnu
  {
    namespace java
    {
      namespace awt
      {
        namespace font
        {
          namespace autofit
          {
              class AutoHinter;
              class GlyphHints;
              class HintScaler;
              class Latin;
              class LatinMetrics;
          }
          namespace opentype
          {
              class OpenTypeFont;
            namespace truetype
            {
                class Zone;
            }
          }
        }
      }
    }
  }
}

class gnu::java::awt::font::autofit::AutoHinter : public ::java::lang::Object
{

public:
  AutoHinter();
  virtual void init(::gnu::java::awt::font::opentype::OpenTypeFont *);
  virtual void applyHints(::gnu::java::awt::font::opentype::truetype::Zone *);
  virtual void setFlags(jint);
public: // actually package-private
  ::gnu::java::awt::font::autofit::Latin * __attribute__((aligned(__alignof__( ::java::lang::Object)))) latinScript;
  ::gnu::java::awt::font::autofit::LatinMetrics * metrics;
  ::gnu::java::awt::font::autofit::GlyphHints * hints;
  ::gnu::java::awt::font::autofit::HintScaler * scaler;
public:
  static ::java::lang::Class class$;
};

#endif // __gnu_java_awt_font_autofit_AutoHinter__
