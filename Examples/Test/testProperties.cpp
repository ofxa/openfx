/*
  Software License :
  
  Copyright (c) 2004, The Open Effects Association Ltd. All rights reserved.
  
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  
      * Redistributions of source code must retain the above copyright notice,
        this list of conditions and the following disclaimer.
      * Redistributions in binary form must reproduce the above copyright notice,
        this list of conditions and the following disclaimer in the documentation
        and/or other materials provided with the distribution.
      * Neither the name The Open Effects Association Ltd, nor the names of its 
        contributors may be used to endorse or promote products derived from this
        software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


/** @file testProperties.cpp Ofx host testing plug-in which logs all the needed properties
    
the log file is being written with ';' ending lines and execution blocks surrounded by { } pairs, so if you
run it through a c beautifier or emacs auto formatting, automagic indenting will occur.
*/
#include <string> // stl strings
#include <map> // stl maps
#include <stdexcept>
#include <iostream>
#include "ofxImageEffect.h"
#include "ofxMemory.h"
#include "ofxMultiThread.h"
#include "ofxMessage.h"

#include "ofxLog.H"

#if defined __APPLE__ || defined linux || defined __FreeBSD__
#  define EXPORT __attribute__((visibility("default")))
#elif defined _WIN32
#  define EXPORT OfxExport
#else
#  error Not building on your operating system quite yet
#endif

static OfxHost               *gHost;
static OfxImageEffectSuiteV1 *gEffectSuite;
static OfxPropertySuiteV1    *gPropSuite;
static OfxInteractSuiteV1    *gInteractSuite;
static OfxParameterSuiteV1   *gParamSuite;
static OfxMemorySuiteV1      *gMemorySuite;
static OfxMultiThreadSuiteV1 *gThreadSuite;
static OfxMessageSuiteV1     *gMessageSuite;

////////////////////////////////////////////////////////////////////////////////
// fetch a suite
static const void *
fetchSuite(const char *suiteName, int suiteVersion, bool optional = false)
{
  const void *suite = gHost->fetchSuite(gHost->host, suiteName, suiteVersion);
  if(optional)
    OFX::logWarning(suite == 0, "Could not fetch the optional suite '%s' version %d;", suiteName, suiteVersion);
  else
    OFX::logError(suite == 0, "Could not fetch the mandatory suite '%s' version %d;", suiteName, suiteVersion);
  if(!optional && suite == 0) throw kOfxStatErrMissingHostFeature;
  return suite;
}

/** @brief wraps up a set of properties
 */
class PropertySet {
protected :
  int _propLogMessages; // if more than 0, don't log ordinary messages
  
  OfxPropertySetHandle _propHandle;
  
public :
  PropertySet(OfxPropertySetHandle h = 0) 
    : _propLogMessages(0)
    , _propHandle(h)
  {}
  
  virtual ~PropertySet();
  void propSetHandle(OfxPropertySetHandle h) { _propHandle = h;}
  
  // these set
  OfxStatus propSet(const char *property, void *value, int idx = 0);
  OfxStatus propSet(const char *property, const std::string &value, int idx = 0);
  OfxStatus propSet(const char *property, double value, int idx = 0);
  OfxStatus propSet(const char *property, int value, int idx = 0);
  
  // these fetch
  OfxStatus propGet(const char *property, void * &value, int idx = 0) const;
  OfxStatus propGet(const char *property, double &value, int idx = 0) const;
  OfxStatus propGet(const char *property, int &value, int idx = 0) const;
  OfxStatus propGet(const char *property, std::string  &value, int idx = 0) const;
  OfxStatus propGetN(const char *property, int *values, int N) const;
  OfxStatus propGetN(const char *property, double *values, int N) const;
  OfxStatus propGetN(const char *property, std::string *values, int N) const;
  
  // these check for existence and dimensionality
  OfxStatus propGetDimension(const char *property, int &size) const;
  
  // inc/dec the log flag to enable/disable ordinary message logging
  void propEnableLog(void) {--_propLogMessages; }
  void propDisableLog(void) {++_propLogMessages; }
};

////////////////////////////////////////////////////////////////////////////////
// should be a union, but std::string can't be in one
struct PropertyValueOnion {
  std::string  vString;
  int    vInt;
  double vDouble;
  void  *vPointer;
  
  PropertyValueOnion(void) {}
  PropertyValueOnion(const char  *s) : vString(s) {}
  PropertyValueOnion(int    i) : vInt(i) {}
  PropertyValueOnion(double d) : vDouble(d) {}
  PropertyValueOnion(void  *p) : vPointer(p) {}
  
  PropertyValueOnion &operator = (const char *v)  {vString = v; return *this;}
  PropertyValueOnion &operator = (const std::string &v)  {vString = v; return *this;}
  PropertyValueOnion &operator = (void *v)  {vPointer = v; return *this;}
  PropertyValueOnion &operator = (int v)    {vInt = v; return *this;}
  PropertyValueOnion &operator = (double v) {vDouble = v; return *this;}
  
  operator const char * () {return vString.c_str();}
  operator std::string &() {return vString;}
  operator int    &() {return vInt;}
  operator double &() {return vDouble;}
  operator void * &() {return vPointer;}
};

////////////////////////////////////////////////////////////////////////////////
// class to describe properties, check their default and set their values
class PropertyDescription
{
public :
  enum TypeEnum {ePointer,
                 eInt,
                 eString,
                 eDouble};
  const char * _name;
  int      _dimension;        // -1 implies variable dim
  TypeEnum _ilk;
  
  PropertyValueOnion *_defs; // for default values to check against, if not null
  int _nDefs;                // number of defaults, 0 if the default can't be checked against (eg: host name varies)
  
  PropertyValueOnion *_wantedVals; // the value to set it to, if not null
  int _nWantedVals;                // number of values to set to, 0 if we don't set it
  
  PropertyValueOnion *_currentVals; // the value fetched from the host
  int _nCurrentVals;              // the number of values fetched from the host
  
protected :
  
  // set basic beets
  template <class T> void
  initialise( const char *nm, int dim, TypeEnum ilk,
              const T *wantedVals, int nWantedVals, const T *defs, int nDefs)
  {
    _name = nm;
    _dimension = dim;
    _ilk = ilk;
    _defs = _wantedVals = _currentVals = 0;
    _nDefs = _nWantedVals = _nCurrentVals = 0;
    
    // make the default and value arrays and set them
    if(nWantedVals) {
      _nWantedVals = nWantedVals;
      _wantedVals = new PropertyValueOnion[nWantedVals];
      for(int i = 0; i < nWantedVals; i++) 
        _wantedVals[i] = wantedVals[i];
    }
    if(nDefs) {
      _nDefs = nDefs;
      _defs = new PropertyValueOnion[_nDefs];
      for(int i = 0; i < nDefs; i++)
        _defs[i] = defs[i];
    }
  }
  
public :
  virtual ~PropertyDescription(void) 
  {
    delete [ ] _currentVals;
    delete [ ] _wantedVals;
    delete [ ] _defs;
  }
  
  // multi dimension string prop
  PropertyDescription(const char *nm, int dim,  const char * const *wantedVals, int nV,  const char * const *defs, int nD)
  {
    initialise(nm, dim, eString, wantedVals, nV, defs, nD);
  }
  
  // single dimension string prop
  PropertyDescription(const char *nm, int dim,  const char *value, bool setValue,  const char *def, bool hasDefault)
  {
    initialise(nm, dim, eString,
               &value, setValue ? 1 : 0,
               &def, hasDefault ? 1 : 0);
  }
  
  // multi dimension int prop
  PropertyDescription(const char *nm, int dim, const int  *wantedVals, int nV, const int *defs, int nD)
  {
    initialise(nm, dim, eInt, wantedVals, nV, defs, nD);
  }
  
  // single dimension int prop
  PropertyDescription(const char *nm, int dim, int value, bool setValue, int def, bool hasDefault)
  {
    initialise(nm, dim, eInt,
               &value, setValue ? 1 : 0,
               &def, hasDefault ? 1 : 0);
  }
  
  // multi dimension double prop
  PropertyDescription(const char *nm, int dim, const double  *wantedVals, int nV, const double *defs, int nD)
  {
    initialise(nm, dim, eDouble, wantedVals, nV, defs, nD);
  }
  
  // single dimension double prop
  PropertyDescription(const char *nm, int dim, double value, bool setValue, double def, bool hasDefault)
  {
    initialise(nm, dim, eDouble,
               &value, setValue ? 1 : 0,
               &def, hasDefault ? 1 : 0);
  }
  
  // single dimension pointer prop
  PropertyDescription(const char *nm, int dim, void *value, bool setValue, void *def, bool hasDefault)
  {
    initialise(nm, dim, ePointer,
               &value, setValue ? 1 : 0,
               &def, hasDefault ? 1 : 0);
  }
  
  // get the dimension of this via the property set handle3
  void checkProperty(PropertySet &propSet); // see if I exist on the property set
  void checkDefault(PropertySet &propSet);  // see if the default values on the property set agree
  void retrieveValue(PropertySet &propSet); // retrieve my values from the property set
  void setValue(PropertySet &propSet);      // set my values on the property set
};

////////////////////////////////////////////////////////////////////////////////
// Describes a set of properties
class PropertySetDescription : PropertySet {
protected :
  const char            *_setName;
  PropertyDescription   *_descriptions;
  int                   _nDescriptions;
  
  std::map<std::string, PropertyDescription *> _descriptionsByName;
  
public :
  PropertySetDescription(const char *setName, OfxPropertySetHandle handle, PropertyDescription *v, int nV);
  void checkProperties(bool logOrdinaryMessages = false); // see if they are there in the first place
  void checkDefaults(bool logOrdinaryMessages = false);   // check default values
  void retrieveValues(bool logOrdinaryMessages = false);  // get current values on the host
  void setValues(bool logOrdinaryMessages = false);       // set values to the requested ones
  PropertyDescription *findDescription(const std::string &name); // find a property with the given name
  
  int intPropValue(const std::string &name, int idx = 0);
  double doublePropValue(const std::string &name, int idx = 0);
  void *pointerPropValue(const std::string &name, int idx = 0);
  const std::string &stringPropValue(const std::string &name, int idx = 0);
};



////////////////////////////////////////////////////////////////////////////////
// maps status to a string
static const char *
mapStatus(OfxStatus stat)
{
  switch(stat) {    
  case kOfxStatOK             : return "kOfxStatOK";
  case kOfxStatFailed         : return "kOfxStatFailed";
  case kOfxStatErrFatal       : return "kOfxStatErrFatal";
  case kOfxStatErrUnknown     : return "kOfxStatErrUnknown";
  case kOfxStatErrMissingHostFeature : return "kOfxStatErrMissingHostFeature";
  case kOfxStatErrUnsupported : return "kOfxStatErrUnsupported";
  case kOfxStatErrExists      : return "kOfxStatErrExists";
  case kOfxStatErrFormat      : return "kOfxStatErrFormat";
  case kOfxStatErrMemory      : return "kOfxStatErrMemory";
  case kOfxStatErrBadHandle   : return "kOfxStatErrBadHandle";
  case kOfxStatErrBadIndex    : return "kOfxStatErrBadIndex";
  case kOfxStatErrValue       : return "kOfxStatErrValue";
  case kOfxStatReplyYes       : return "kOfxStatReplyYes";
  case kOfxStatReplyNo        : return "kOfxStatReplyNo";
  case kOfxStatReplyDefault   : return "kOfxStatReplyDefault";
  case kOfxStatErrImageFormat : return "kOfxStatErrImageFormat";
  }
  return "UNKNOWN STATUS CODE";
}


////////////////////////////////////////////////////////////////////////////////
// property set code
PropertySet::~PropertySet()
{
}

OfxStatus 
PropertySet::propSet(const char *property, void *value, int idx)
{
  OfxStatus stat = gPropSuite->propSetPointer(_propHandle, property, idx, value);
  OFX::logError(stat != kOfxStatOK, "Failed on setting pointer property %s[%d] to %p, host returned status %s;", property, idx, value, mapStatus(stat));
  if(stat == kOfxStatOK && _propLogMessages <= 0) 
    OFX::logPrint("Set pointer property %s[%d] = %p;", property, idx, value);
  return stat;
}

OfxStatus 
PropertySet::propSet(const char *property, const std::string &value, int idx)
{
  OfxStatus stat = gPropSuite->propSetString(_propHandle, property, idx, value.c_str());
  OFX::logError(stat != kOfxStatOK, "Failed on setting string property %s[%d] to '%s', host returned status %s;", property, idx, value.c_str(), mapStatus(stat));
  if(stat == kOfxStatOK && _propLogMessages <= 0) 
    OFX::logPrint("Set string property %s[%d] = '%s';", property, idx, value.c_str());
  return stat;
}

OfxStatus 
PropertySet::propSet(const char *property, double value, int idx)
{
  OfxStatus stat = gPropSuite->propSetDouble(_propHandle, property, idx, value);
  OFX::logError(stat != kOfxStatOK, "Failed on setting double property %s[%d] to %g, host returned status %s;", property, idx, value, mapStatus(stat));
  if(stat == kOfxStatOK && _propLogMessages <= 0) 
    OFX::logPrint("Set double property %s[%d] = %g;", property, idx, value);
  return stat;
}

OfxStatus 
PropertySet::propSet(const char *property, int value, int idx)
{
  OfxStatus stat = gPropSuite->propSetInt(_propHandle, property, idx, value);
  OFX::logError(stat != kOfxStatOK, "Failed on setting int property %s[%d] to %d, host returned status '%s';", property, idx, value, mapStatus(stat));
  if(stat == kOfxStatOK && _propLogMessages <= 0) 
    OFX::logPrint("Set int property %s[%d] = %d;", property, idx, value);
  return stat;
}

OfxStatus 
PropertySet::propGet(const char *property, void * &value, int idx) const
{
  OfxStatus stat = gPropSuite->propGetPointer(_propHandle, property, idx, &value);
  OFX::logError(stat != kOfxStatOK, "Failed on fetching pointer property %s[%d], host returned status %s;", property, idx, mapStatus(stat));
  if(stat == kOfxStatOK && _propLogMessages <= 0) 
    OFX::logPrint("Fetched pointer property %s[%d] = %p;", property, idx, value);
  return stat;
}

OfxStatus 
PropertySet::propGet(const char *property, double &value, int idx) const
{
  OfxStatus stat = gPropSuite->propGetDouble(_propHandle, property, idx, &value);
  OFX::logError(stat != kOfxStatOK, "Failed on fetching double property %s[%d], host returned status %s;", property, idx, mapStatus(stat));
  if(stat == kOfxStatOK && _propLogMessages <= 0) 
    OFX::logPrint("Fetched double property %s[%d] = %g;", property, idx, value);
  return stat;
}

OfxStatus 
PropertySet::propGetN(const char *property, double *values, int N) const
{
  OfxStatus stat = gPropSuite->propGetDoubleN(_propHandle, property, N, values);
  OFX::logError(stat != kOfxStatOK, "Failed on fetching multiple double property %s X %d, host returned status %s;", property, N, mapStatus(stat));
  if(stat == kOfxStatOK && _propLogMessages <= 0) {
    OFX::logPrint("Fetched multiple double property %s X %d;", property, N);
    for(int i = 0; i < N; i++) {      
      OFX::logPrint("  %s[%d] = %g;", property, i, values[i]);
    }
  }
  return stat;
}

OfxStatus 
PropertySet::propGetN(const char *property, int *values, int N) const
{
  OfxStatus stat = gPropSuite->propGetIntN(_propHandle, property, N, values);
  OFX::logError(stat != kOfxStatOK, "Failed on fetching multiple int property %s X %d, host returned status %s;", property, N, mapStatus(stat));
  if(stat == kOfxStatOK && _propLogMessages <= 0) {
    OFX::logPrint("Fetched multiple int property %s X %d;", property, N);
    for(int i = 0; i < N; i++) {      
      OFX::logPrint("  %s[%d] = %d;", property, i, values[i]);
    }
  }
  return stat;
}


OfxStatus 
PropertySet::propGet(const char *property, int &value, int idx) const
{
  OfxStatus stat = gPropSuite->propGetInt(_propHandle, property, idx, &value);
  OFX::logError(stat != kOfxStatOK, "Failed on fetching int property %s[%d], host returned status %s;", property, idx, mapStatus(stat));
  if(stat == kOfxStatOK && _propLogMessages <= 0) 
    OFX::logPrint("Fetched int property %s[%d] = %d;", property, idx, value);
  return stat;
}

OfxStatus 
PropertySet::propGet(const char *property, std::string  &value, int idx) const
{
  char *str;
  OfxStatus stat = gPropSuite->propGetString(_propHandle, property, idx, &str);
  OFX::logError(stat != kOfxStatOK, "Failed on fetching string property %s[%d], host returned status %s;", property, idx, mapStatus(stat));
  if(kOfxStatOK == stat) {
    value = str;
    if(_propLogMessages <= 0)
      OFX::logPrint("Fetched string property %s[%d] = '%s';", property, idx, value.c_str());
  }
  else {
    value = "";
  }
  return stat;
}

OfxStatus 
PropertySet::propGetN(const char *property, std::string *values, int N) const
{
  char **strs = new char * [N];
  
  OfxStatus stat = gPropSuite->propGetStringN(_propHandle, property, N, strs);
  
  OFX::logError(stat != kOfxStatOK, "Failed on fetching multiple string property %s X %d, host returned status %s;", property, N, mapStatus(stat));
  
  if(kOfxStatOK == stat) {
    if(_propLogMessages <= 0) OFX::logPrint("Fetched multiple string property %s X %d;", property, N);
    for(int i = 0; i < N; i++) {      
      values[i] = strs[i];
      if(_propLogMessages <= 0) OFX::logPrint("  %s[%d] = '%s';", property, i, strs[i]);
    }
  }
  else {
    for(int i = 0; i < N; i++) {      
      values[i] = "";
    }
  }
  
  delete [] strs;
  
  return stat;
}


OfxStatus 
PropertySet::propGetDimension(const char *property, int &size) const
{
  OfxStatus stat = gPropSuite->propGetDimension(_propHandle, property, &size);
  OFX::logError(stat != kOfxStatOK, "Failed on fetching dimension for property %s, host returned status %s;", property, mapStatus(stat));
  return stat;
}

////////////////////////////////////////////////////////////////////////////////
// PropertyDescription code

// check to see if this property exists on the host
void
PropertyDescription::checkProperty(PropertySet &propSet)
{
  // see if it exists by fetching the dimension, 
  int dimension;
  OfxStatus stat = propSet.propGetDimension(_name, dimension);
  if(stat == kOfxStatOK) {
    if(_dimension != -1)
      OFX::logError(dimension != _dimension, "Host reports property '%s' has dimension %d, it should be %d;", _name, dimension, _dimension); 
    
    // check type by getting the first element, the property getting will print failure messages to the log
    if(dimension > 0) {
      void       *vP;
      int         vI;
      double      vD;
      std::string vS;
      
      switch(_ilk) {
      case PropertyDescription::ePointer : propSet.propGet(_name, vP); break;
      case PropertyDescription::eInt :     propSet.propGet(_name, vI); break;
      case PropertyDescription::eString  : propSet.propGet(_name, vS); break;
      case PropertyDescription::eDouble  : propSet.propGet(_name, vD); break;
      }
    }
  }
}

// see if the default values on the property set agree
void PropertyDescription::checkDefault(PropertySet &propSet)
{
  if(_nDefs > 0) {
    // fetch the dimension on the host
    int hostDimension;
    OfxStatus stat = propSet.propGetDimension(_name, hostDimension);
    (void)stat;

    OFX::logError(hostDimension != _nDefs, "Host reports default dimension of '%s' is %d, which is different to the default value %d;", _name, hostDimension, _nDefs);
    
    int N = hostDimension < _nDefs ? hostDimension : _nDefs;
    
    for(int i = 0; i < N; i++) {
      void       *vP;
      int         vI;
      double      vD;
      std::string vS;
      
      switch(_ilk) {
      case PropertyDescription::ePointer : 
        propSet.propGet(_name, vP, i); 
        OFX::logError(vP != (void *) _defs[i], "Default value of %s[%d] = %p, it should be %p;", _name, i, vP, (void *) _defs[i]);
        break;
      case PropertyDescription::eInt : 
        propSet.propGet(_name, vI, i);
        OFX::logError(vI != (int) _defs[i], "Default value of %s[%d] = %d, it should be %d;", _name, i, vI, (int) _defs[i]);
        break;
      case PropertyDescription::eString  :
        propSet.propGet(_name, vS, i); 
        OFX::logError(vS != _defs[i].vString, "Default value of %s[%d] = '%s', it should be '%s';", _name, i, vS.c_str(), _defs[i].vString.c_str());
        break;
      case PropertyDescription::eDouble  :
        propSet.propGet(_name, vD, i); 
        OFX::logError(vD != (double) _defs[i], "Default value of %s[%d] = %g, it should be %g;", _name, i, vD, (double) _defs[i]);
        break;
      }
    }
  }
}

// get the current value from the property set into me
void
PropertyDescription::retrieveValue(PropertySet &propSet)
{
  if(_currentVals) delete [] _currentVals;
  _currentVals = 0;
  _nCurrentVals = 0;
  
  // fetch the dimension on the host
  int hostDimension;
  OfxStatus stat = propSet.propGetDimension(_name, hostDimension);
  if(stat == kOfxStatOK && hostDimension > 0) {
    _nCurrentVals = hostDimension;
    _currentVals = new PropertyValueOnion[hostDimension];
    
    for(int i = 0; i < hostDimension; i++) {
      void       *vP;
      int         vI;
      double      vD;
      std::string vS;
      
      switch(_ilk) {
      case PropertyDescription::ePointer : 
        stat = propSet.propGet(_name, vP, i); 
        _currentVals[i] = (stat == kOfxStatOK)  ? vP : (void *)(0);
        break;
      case PropertyDescription::eInt : 
        stat = propSet.propGet(_name, vI, i);
        _currentVals[i] = (stat == kOfxStatOK)  ? vI : 0;
        break;
      case PropertyDescription::eString  :
        stat = propSet.propGet(_name, vS, i); 
          if(stat == kOfxStatOK)
                                  _currentVals[i] = vS;
                                  else
                                        _currentVals[i] = std::string("");
	  break;
      case PropertyDescription::eDouble  :
        stat = propSet.propGet(_name, vD, i); 
        _currentVals[i] = (stat == kOfxStatOK)  ? vD : 0.0;
        break;
      }
    }
  }
}

// set the property from my 'set' property
void
PropertyDescription::setValue(PropertySet &propSet)
{
  // fetch the dimension on the host
  if(_nWantedVals > 0) {
    int i;
    for(i = 0; i < _nWantedVals; i++) {	
      switch(_ilk) {
      case PropertyDescription::ePointer : 
        propSet.propSet(_name, _wantedVals[i].vPointer, i); 
        break;
      case PropertyDescription::eInt : 
        propSet.propSet(_name, _wantedVals[i].vInt, i); 
        break;
      case PropertyDescription::eString  :
        propSet.propSet(_name, _wantedVals[i].vString, i); 
        break;
      case PropertyDescription::eDouble  :
        propSet.propSet(_name, _wantedVals[i].vDouble, i); 
        break;
      }
    }
    
    // Now fetch the current values back into current. Don't be verbose about it.
    propSet.propDisableLog();
    retrieveValue(propSet);
    propSet.propEnableLog();
    
    // and see if they are the same
    OFX::logError(_nWantedVals != _nCurrentVals, "After setting property %s, the dimension %d is not the same as what was set %d", _name, _nCurrentVals, _nWantedVals);
    int N = _nWantedVals < _nCurrentVals ? _nWantedVals : _nCurrentVals;
    for(i = 0; i < N; i++) {
      switch(_ilk) {
      case PropertyDescription::ePointer : 
        OFX::logError(_wantedVals[i].vPointer != _currentVals[i].vPointer, 
                      "After setting pointer value %s[%d] value fetched back %p not same as value set %p", 
                      _name, i, _wantedVals[i].vPointer, _currentVals[i].vPointer);
        break;
      case PropertyDescription::eInt : 
        OFX::logError(_wantedVals[i].vInt != _currentVals[i].vInt, 
                      "After setting int value %s[%d] value fetched back %d not same as value set %d", 
                      _name, i, _wantedVals[i].vInt, _currentVals[i].vInt);
        break;
      case PropertyDescription::eString  :
        OFX::logError(_wantedVals[i].vString != _currentVals[i].vString, 
                      "After setting string value %s[%d] value fetched back '%s' not same as value set '%s'", 
                      _name, i, _wantedVals[i].vString.c_str(), _currentVals[i].vString.c_str());
        break;
      case PropertyDescription::eDouble  :
        OFX::logError(_wantedVals[i].vDouble != _currentVals[i].vDouble, 
                      "After setting double value %s[%d] value fetched back %g not same as value set %g", 
                      _name, i, _wantedVals[i].vDouble, _currentVals[i].vDouble);
        break;
      }
      
    }
    
  }
}

////////////////////////////////////////////////////////////////////////////////
// a set of property descriptions
PropertySetDescription::PropertySetDescription(const char *setName, OfxPropertySetHandle handle, PropertyDescription *v, int nV)
  : PropertySet(handle)
  , _setName(setName)
  , _descriptions(v)
  , _nDescriptions(nV)
{
  // add the properties to my map so I can look them up by name
  for(int i = 0; i < nV; i++) {
    std::pair<std::string, PropertyDescription *> mePair(v[i]._name, &v[i]);
    _descriptionsByName.insert(mePair);
  }
}

void
PropertySetDescription::checkProperties(bool logOrdinaryMessages)
{
  OFX::logPrint("PropertySetDescription::checkProperties - start(checking properties on %s);\n{", _setName);
  
  // don't print ordinary messages whilst we are checking them
  if(!logOrdinaryMessages) propDisableLog();
  
  // check each property in the description
  for(int i = 0; i < _nDescriptions; i++) {
    _descriptions[i].checkProperty(*this);
  }
  if(!logOrdinaryMessages) propEnableLog();
  
  OFX::logPrint("}PropertySetDescription::checkProperties - stop;");
}

void
PropertySetDescription::checkDefaults(bool logOrdinaryMessages)
{
  OFX::logPrint("PropertySetDescription::checkDefaults - start(checking default value of properties on %s);\n{", _setName);
  
  // don't print ordinary messages whilst we are checking them
  if(!logOrdinaryMessages) propDisableLog();
  
  // check each property in the description
  for(int i = 0; i < _nDescriptions; i++) {
    _descriptions[i].checkDefault(*this);
  }
  if(!logOrdinaryMessages) propEnableLog();
  
  OFX::logPrint("}PropertySetDescription::checkDefaults - stop;");
}

void
PropertySetDescription::retrieveValues(bool logOrdinaryMessages)
{
  OFX::logPrint("PropertySetDescription::retrieveValues - start(retrieving values of properties on %s);\n{", _setName);
  
  if(!logOrdinaryMessages) propDisableLog();

  // check each property in the description
  for(int i = 0; i < _nDescriptions; i++) {
    _descriptions[i].retrieveValue(*this);
  }

  if(!logOrdinaryMessages) propEnableLog();
  
  OFX::logPrint("}PropertySetDescription::retrieveValues - stop;");
}

void
PropertySetDescription::setValues(bool logOrdinaryMessages)
{
  OFX::logPrint("PropertySetDescription::setValues - start(retrieving values of properties on %s);\n{", _setName);
  
  if(!logOrdinaryMessages) propDisableLog();

  // check each property in the description
  for(int i = 0; i < _nDescriptions; i++) {
    _descriptions[i].setValue(*this);
  }
  
  if(!logOrdinaryMessages) propEnableLog();

  OFX::logPrint("}PropertySetDescription::setValues - stop;");
}

// find a property with the given name out of our set of properties
PropertyDescription *
PropertySetDescription::findDescription(const std::string &name)
{
  std::map<std::string, PropertyDescription *>::iterator iter;
  iter = _descriptionsByName.find(name);
  if (iter != _descriptionsByName.end()) {
    return iter->second;
  }
  return 0;
}

// find value of the named property from the _currentVals array
int 
PropertySetDescription::intPropValue(const std::string &name, int idx)
{
  PropertyDescription *desc = 0;
  desc = findDescription(name);
  if(desc) {
    if(idx < desc->_nCurrentVals) {
      return int(desc->_currentVals[idx]);
    }
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// host description stuff

// list of the properties on the host. We can't set any of these, and most don't have defaults
static PropertyDescription gHostPropDescription[ ] =
  {
    PropertyDescription(kOfxPropType,                                  1, "", false, kOfxTypeImageEffectHost, true), 
    PropertyDescription(kOfxPropName,                                  1, "", false, "", false),
    PropertyDescription(kOfxPropLabel,                                 1, "", false, "", false),
    PropertyDescription(kOfxImageEffectHostPropIsBackground,           1, 0, false, 0, false),
    PropertyDescription(kOfxImageEffectPropSupportsOverlays,           1, 0, false, 0, false ),
    PropertyDescription(kOfxImageEffectPropSupportsMultiResolution,    1, 0, false, 0, false ),
    PropertyDescription(kOfxImageEffectPropSupportsTiles,              1, 0, false, 0, false ),
    PropertyDescription(kOfxImageEffectPropTemporalClipAccess,         1, 0, false, 0, false ),
    PropertyDescription(kOfxImageEffectPropSupportsMultipleClipDepths, 1, 0, false, 0, false ),
    PropertyDescription(kOfxImageEffectPropSupportsMultipleClipPARs,   1, 0, false, 0, false ),
    PropertyDescription(kOfxImageEffectPropSetableFrameRate,           1, 0, false, 0, false ),
    PropertyDescription(kOfxImageEffectPropSetableFielding,            1, 0, false, 0, false ),
    PropertyDescription(kOfxImageEffectPropSupportedComponents,       -1, "", false, "", false ),
    PropertyDescription(kOfxImageEffectPropSupportedContexts,         -1, "", false, "", false ),
    PropertyDescription(kOfxParamHostPropSupportsStringAnimation,      1, 0, false, 0, false ),
    PropertyDescription(kOfxParamHostPropSupportsCustomInteract,       1, 0, false, 0, false ),
    PropertyDescription(kOfxParamHostPropSupportsChoiceAnimation,      1, 0, false, 0, false ),
    PropertyDescription(kOfxParamHostPropSupportsBooleanAnimation,     1, 0, false, 0, false ),
    PropertyDescription(kOfxParamHostPropSupportsCustomAnimation,      1, 0, false, 0, false ),
    PropertyDescription(kOfxParamHostPropMaxParameters,                1, 0, false, 0, false ),
    PropertyDescription(kOfxParamHostPropMaxPages,                     1, 0, false, 0, false ),
    PropertyDescription(kOfxParamHostPropPageRowColumnCount,           2, 0, false, 0, false )
  };

// some host property descriptions we may be interested int
class HostDescription : public PropertySet {
public :
  int hostIsBackground;
  int supportsOverlays;
  int supportsMultiResolution;
  int supportsTiles;
  int temporalClipAccess;
  int supportsMultipleClipDepths;
  int supportsMultipleClipPARs;
  int supportsSetableFrameRate;
  int supportsSetableFielding;
  int supportsCustomAnimation;
  int supportsStringAnimation;
  int supportsCustomInteract;
  int supportsChoiceAnimation;
  int supportsBooleanAnimation;
  int maxParameters;
  int maxPages;
  int pageRowCount;
  int pageColumnCount;
  
  HostDescription(OfxPropertySetHandle handle);
};
HostDescription *gHostDescription = 0;

// create a host description, checking properties on the way
HostDescription::HostDescription(OfxPropertySetHandle handle) :
  PropertySet(handle) ,
  hostIsBackground(false),
  supportsOverlays(false),
  supportsMultiResolution(false),
  supportsTiles(false),
  temporalClipAccess(false),
  supportsMultipleClipDepths(false),
  supportsMultipleClipPARs(false),
  supportsSetableFrameRate(false),
  supportsSetableFielding(false),
  supportsCustomAnimation(false),
  supportsStringAnimation(false),
  supportsCustomInteract(false),
  supportsChoiceAnimation(false),
  supportsBooleanAnimation(false),
  maxParameters(-1),
  maxPages(-1),
  pageRowCount(-1),
  pageColumnCount(-1)
{
  OFX::logPrint("HostDescription::HostDescription - start ( fetching host description);\n{");
  
  // do basic existence checking with a PropertySetDescription
  PropertySetDescription hostPropSet("Host", handle, gHostPropDescription, sizeof(gHostPropDescription)/sizeof(PropertyDescription));
  hostPropSet.checkProperties();
  hostPropSet.checkDefaults();
  hostPropSet.retrieveValues(true);
  
  // now go through and fill in the host description
  hostIsBackground = hostPropSet.intPropValue(kOfxImageEffectHostPropIsBackground);
  supportsOverlays = hostPropSet.intPropValue(kOfxImageEffectPropSupportsOverlays);
  supportsMultiResolution = hostPropSet.intPropValue(kOfxImageEffectPropSupportsMultiResolution);
  supportsTiles = hostPropSet.intPropValue(kOfxImageEffectPropSupportsTiles);
  temporalClipAccess = hostPropSet.intPropValue(kOfxImageEffectPropTemporalClipAccess);
  supportsMultipleClipDepths    = hostPropSet.intPropValue(kOfxImageEffectPropSupportsMultipleClipDepths);
  supportsMultipleClipPARs = hostPropSet.intPropValue(kOfxImageEffectPropSupportsMultipleClipPARs);
  supportsSetableFrameRate = hostPropSet.intPropValue(kOfxImageEffectPropSetableFrameRate);
  supportsSetableFielding = hostPropSet.intPropValue(kOfxImageEffectPropSetableFielding);
  
  supportsStringAnimation = hostPropSet.intPropValue(kOfxParamHostPropSupportsStringAnimation);
  supportsCustomInteract = hostPropSet.intPropValue(kOfxParamHostPropSupportsCustomInteract);
  supportsChoiceAnimation = hostPropSet.intPropValue(kOfxParamHostPropSupportsChoiceAnimation);
  supportsBooleanAnimation = hostPropSet.intPropValue(kOfxParamHostPropSupportsBooleanAnimation);
  supportsCustomAnimation  = hostPropSet.intPropValue(kOfxParamHostPropSupportsCustomAnimation);
  maxParameters = hostPropSet.intPropValue(kOfxParamHostPropMaxParameters);
  maxPages = hostPropSet.intPropValue(kOfxParamHostPropMaxPages);
  pageRowCount = hostPropSet.intPropValue(kOfxParamHostPropPageRowColumnCount, 0);
  pageColumnCount = hostPropSet.intPropValue(kOfxParamHostPropPageRowColumnCount, 1);
  
  OFX::logPrint("}HostDescription::HostDescription - stop;");
}

////////////////////////////////////////////////////////////////////////////////
//test the memory suite
static void
testMemorySuite(void)
{
  OFX::logPrint("testMemorySuite - start();\n{");
  void *oneMeg;
  
  OfxStatus stat = gMemorySuite->memoryAlloc(NULL, 1024 * 1024, &oneMeg);
  OFX::logError(stat != kOfxStatOK, "OfxMemorySuiteV1::memoryAlloc failed to alloc 1MB, returned %s", mapStatus(stat));
  
  if(stat == kOfxStatOK) {
    // touch 'em all to see if it crashes
    char *lotsOfChars = (char *) oneMeg;
    for(int i = 0; i < 1024 * 1024; i++) {
      *lotsOfChars++ = 0;
    }

    stat = gMemorySuite->memoryFree(oneMeg);
    OFX::logError(stat != kOfxStatOK, "OfxMemorySuiteV1::memoryFree failed to free 1MB, returned %s", mapStatus(stat));
  }
  
  OFX::logPrint("}HostDescription::HostDescription - stop;");
}


////////////////////////////////////////////////////////////////////////////////
// how many times has actionLoad been called
static int gLoadCount = 0;

/** @brief Called at load */
static OfxStatus
actionLoad(void)
{
  OFX::logPrint("loadAction - start();\n{");
  OFX::logError(gLoadCount != 0, "Load action called more than once without unload being called;");
  gLoadCount++;
  
  
  OfxStatus status = kOfxStatOK;
  
  try {
    // fetch the suites
    OFX::logError(gHost == 0, "Host pointer has not been set;");
    if(!gHost) throw kOfxStatErrBadHandle;
    
    if(gLoadCount == 1) {
      gEffectSuite    = (OfxImageEffectSuiteV1 *) fetchSuite(kOfxImageEffectSuite, 1);
      gPropSuite      = (OfxPropertySuiteV1 *)    fetchSuite(kOfxPropertySuite, 1);
      gParamSuite     = (OfxParameterSuiteV1 *)   fetchSuite(kOfxParameterSuite, 1);
      gMemorySuite    = (OfxMemorySuiteV1 *)      fetchSuite(kOfxMemorySuite, 1);
      gThreadSuite    = (OfxMultiThreadSuiteV1 *) fetchSuite(kOfxMultiThreadSuite, 1);
      gMessageSuite  = (OfxMessageSuiteV1 *)     fetchSuite(kOfxMessageSuite, 1);
      
      // OK check and fetch host information
      gHostDescription = new HostDescription(gHost->host);
      
      // fetch the interact suite if the host supports interaction
      if(gHostDescription->supportsOverlays || gHostDescription->supportsCustomInteract)
        gInteractSuite  = (OfxInteractSuiteV1 *)    fetchSuite(kOfxInteractSuite, 1);
      
      // test the memory suite
      testMemorySuite();
    }
  }
  
  catch(int err) {
    status = err;
  }
  
  OFX::logPrint("}loadAction - stop;");
  return status;
}

/** @brief Called before unload */
static OfxStatus
unLoadAction(void)
{
  OFX::logError(gLoadCount <= 0, "UnLoad action called without a corresponding load action having been called;");
  gLoadCount--;
  
  // force these to null
  gEffectSuite = 0;
  gPropSuite = 0;
  gParamSuite = 0;
  gMemorySuite = 0;
  gThreadSuite = 0;
  gMessageSuite = 0;
  gInteractSuite = 0;
  return kOfxStatOK;
}

//  instance construction
static OfxStatus
createInstance( OfxImageEffectHandle /*effect*/)
{
  return kOfxStatOK;
}

// instance destruction
static OfxStatus
destroyInstance( OfxImageEffectHandle  /*effect*/)
{
  return kOfxStatOK;
}

// tells the host what region we are capable of filling
OfxStatus 
getSpatialRoD( OfxImageEffectHandle  /*effect*/,  OfxPropertySetHandle /*inArgs*/,  OfxPropertySetHandle /*outArgs*/)
{
  return kOfxStatOK;
}

// tells the host how much of the input we need to fill the given window
OfxStatus 
getSpatialRoI( OfxImageEffectHandle  /*effect*/,  OfxPropertySetHandle /*inArgs*/,  OfxPropertySetHandle /*outArgs*/)
{
  return kOfxStatOK;
}

// Tells the host how many frames we can fill, only called in the general context.
// This is actually redundant as this is the default behaviour, but for illustrative
// purposes.
OfxStatus 
getTemporalDomain( OfxImageEffectHandle  /*effect*/,  OfxPropertySetHandle /*inArgs*/,  OfxPropertySetHandle /*outArgs*/)
{
  return kOfxStatOK;
}


// Set our clip preferences 
static OfxStatus 
getClipPreferences( OfxImageEffectHandle  /*effect*/,  OfxPropertySetHandle /*inArgs*/,  OfxPropertySetHandle /*outArgs*/)
{
  return kOfxStatOK;
}

// are the settings of the effect performing an identity operation
static OfxStatus
isIdentity( OfxImageEffectHandle /*effect*/,
            OfxPropertySetHandle /*inArgs*/,
            OfxPropertySetHandle /*outArgs*/)
{
  // In this case do the default, which in this case is to render 
  return kOfxStatReplyDefault;
}

////////////////////////////////////////////////////////////////////////////////
// function called when the instance has been changed by anything
static OfxStatus
instanceChanged( OfxImageEffectHandle  /*effect*/,
                 OfxPropertySetHandle /*inArgs*/,
                 OfxPropertySetHandle /*outArgs*/)
{
  // don't trap any others
  return kOfxStatReplyDefault;
}

// the process code  that the host sees
static OfxStatus render(OfxImageEffectHandle  /*instance*/,
                        OfxPropertySetHandle /*inArgs*/,
                        OfxPropertySetHandle /*outArgs*/)
{  
  return kOfxStatOK;
}

//  describe the plugin in context
static OfxStatus
describeInContext( OfxImageEffectHandle  /*effect*/,  OfxPropertySetHandle /*inArgs*/)
{
  if (!OFX::logOpenFile()) {
    std::cout << "Error: OFX Test Properties plugin cannot open log file " << OFX::logGetFileName() << std::endl;
  }
  return kOfxStatOK;
}

////////////////////////////////////////////////////////////////////////////////
// code for the plugin's description routine

// contexts we can be 
static const char *gSupportedContexts[] =
  {
    kOfxImageEffectContextGenerator,
    kOfxImageEffectContextFilter,
    kOfxImageEffectContextTransition, 
    kOfxImageEffectContextPaint,
    kOfxImageEffectContextGeneral, 
    kOfxImageEffectContextRetimer 
  };

// pixel depths we can be
static const char *gSupportedPixelDepths[] =
  {
    kOfxBitDepthByte,
    kOfxBitDepthShort,
    kOfxBitDepthFloat
  };

// the values to set and the defaults to check on the various properties
static PropertyDescription gPluginPropertyDescriptions[] =
  {
    PropertyDescription(kOfxPropType,                                    1, "",                             false, kOfxTypeImageEffect,            true),
    PropertyDescription(kOfxPropLabel,                                   1, "OFX Test Properties",          true,  "",                             false),
    PropertyDescription(kOfxPropShortLabel,                              1, "OFX Test Props",               true,  "",                             false),
    PropertyDescription(kOfxPropLongLabel,                               1, "OFX Test Properties",          true,  "",                             false),
    PropertyDescription(kOfxPluginPropFilePath,                          1, "",                             false, "",                             false),
    PropertyDescription(kOfxImageEffectPluginPropGrouping,               1, "OFX Example",                  true,  "",                             false),
    PropertyDescription(kOfxImageEffectPluginPropSingleInstance,         1, 0,                              true,  0,                              true),
    PropertyDescription(kOfxImageEffectPluginRenderThreadSafety,         1, kOfxImageEffectRenderFullySafe, true,  kOfxImageEffectRenderFullySafe, true),
    PropertyDescription(kOfxImageEffectPluginPropHostFrameThreading,     1, 0,                              true,  0,                              true),
    PropertyDescription(kOfxImageEffectPluginPropOverlayInteractV1,      1, (void *)(0),                    true,  (void *)(0),                    true),
    PropertyDescription(kOfxImageEffectPropSupportsMultiResolution,      1, 1,                              true,  1,                              true),
    PropertyDescription(kOfxImageEffectPropSupportsTiles,                1, 1,                              true,  1,                              true),
    PropertyDescription(kOfxImageEffectPropTemporalClipAccess,           1, 0,                              true,  0,                              true),
    PropertyDescription(kOfxImageEffectPluginPropFieldRenderTwiceAlways, 1, 1,                              true,  1,                              true),
    PropertyDescription(kOfxImageEffectPropSupportsMultipleClipDepths,   1, 0,                              true,  0,                              true),
    PropertyDescription(kOfxImageEffectPropSupportsMultipleClipPARs,     1, 0,                              true,  0,                              true),
    PropertyDescription(kOfxImageEffectPropSupportedContexts,           -1, gSupportedContexts,    sizeof(gSupportedContexts)/sizeof(char *),      gSupportedContexts, 0),
    PropertyDescription(kOfxImageEffectPropSupportedPixelDepths,        -1, gSupportedPixelDepths, sizeof(gSupportedPixelDepths)/sizeof(char *),   gSupportedPixelDepths, 0)
  };

static OfxStatus
actionDescribe(OfxImageEffectHandle effect)
{  
  // get the property handle for the plugin
  OfxPropertySetHandle effectProps;
  gEffectSuite->getPropertySet(effect, &effectProps);
  
  // check the defaults
  PropertySetDescription pluginPropSet("Plugin", effectProps, gPluginPropertyDescriptions, sizeof(gPluginPropertyDescriptions)/sizeof(PropertyDescription));
  pluginPropSet.checkProperties();
  pluginPropSet.checkDefaults();
  pluginPropSet.retrieveValues(true);
  pluginPropSet.setValues();
  
  return kOfxStatOK;
}

////////////////////////////////////////////////////////////////////////////////.
// check handles to the main function
static void
checkMainHandles(const char *action,  const void *handle, OfxPropertySetHandle inArgsHandle,  OfxPropertySetHandle outArgsHandle,
                 bool handleCanBeNull, bool inArgsCanBeNull, bool outArgsCanBeNull)
{
  if(handleCanBeNull)
    OFX::logWarning(handle != 0, "Handle passed to '%s' is not null;", action);
  else
    OFX::logError(handle == 0, "'Handle passed to '%s' is null;", action);
  
  if(inArgsCanBeNull)
    OFX::logWarning(inArgsHandle != 0, "'inArgs' Handle passed to '%s' is not null;", action);
  else
    OFX::logError(inArgsHandle == 0, "'inArgs' handle passed to '%s' is null;", action);
  
  if(outArgsCanBeNull)
    OFX::logWarning(outArgsHandle != 0, "'outArgs' Handle passed to '%s' is not null;", action);
  else
    OFX::logError(outArgsHandle == 0, "'outArgs' handle passed to '%s' is null;", action);
  
  if(!handleCanBeNull && !handle) throw kOfxStatErrBadHandle;
  if(!inArgsCanBeNull && !inArgsHandle) throw kOfxStatErrBadHandle;
  if(!outArgsCanBeNull && !outArgsHandle) throw kOfxStatErrBadHandle;
}

////////////////////////////////////////////////////////////////////////////////
// The main function
static OfxStatus
pluginMain(const char *action,  const void *handle, OfxPropertySetHandle inArgsHandle,  OfxPropertySetHandle outArgsHandle)
{
  OFX::logPrint("pluginMain - start();\n{");
  OFX::logPrint("  action is '%s';", action);
  OfxStatus stat = kOfxStatReplyDefault;
  
  try {
    // cast to handle appropriate type
    OfxImageEffectHandle effectHandle = (OfxImageEffectHandle) handle;
    
    // construct two property set wrappers
    PropertySet inArgs(inArgsHandle);
    PropertySet outArgs(outArgsHandle);
    
    if(OFX::strEquals(action, kOfxActionLoad)) {    
      checkMainHandles(action, handle, inArgsHandle, outArgsHandle, true, true, true);
      stat = actionLoad();
    }
    else if(OFX::strEquals(action, kOfxActionUnload)) {
      checkMainHandles(action, handle, inArgsHandle, outArgsHandle, true, true, true);
      stat = unLoadAction();
    }
    else if(OFX::strEquals(action, kOfxActionDescribe)) {
      checkMainHandles(action, handle, inArgsHandle, outArgsHandle, false, true, true);
      stat = actionDescribe(effectHandle);
    }
    else if(OFX::strEquals(action, kOfxActionPurgeCaches)) {
      checkMainHandles(action, handle, inArgsHandle, outArgsHandle, false, true, true);
    }
    else if(OFX::strEquals(action, kOfxActionSyncPrivateData)) {
      checkMainHandles(action, handle, inArgsHandle, outArgsHandle, false, true, true);
    }
    else if(OFX::strEquals(action, kOfxActionCreateInstance)) {
      checkMainHandles(action, handle, inArgsHandle, outArgsHandle, false, true, true);
      stat = createInstance(effectHandle);
    }
    else if(OFX::strEquals(action, kOfxActionDestroyInstance)) {
      checkMainHandles(action, handle, inArgsHandle, outArgsHandle, false, true, true);
      stat = destroyInstance(effectHandle);
    }
    else if(OFX::strEquals(action, kOfxActionInstanceChanged)) {
      checkMainHandles(action, handle, inArgsHandle, outArgsHandle, false, false, true);
    }
    else if(OFX::strEquals(action, kOfxActionBeginInstanceChanged)) {
      checkMainHandles(action, handle, inArgsHandle, outArgsHandle, false, false, true);
    }
    else if(OFX::strEquals(action, kOfxActionEndInstanceChanged)) {
      checkMainHandles(action, handle, inArgsHandle, outArgsHandle, false, false, true);
      stat = instanceChanged(effectHandle, inArgsHandle, outArgsHandle);
    }
    else if(OFX::strEquals(action, kOfxActionBeginInstanceEdit)) {
      checkMainHandles(action, handle, inArgsHandle, outArgsHandle, false, true, true);
    }
    else if(OFX::strEquals(action, kOfxActionEndInstanceEdit)) {
      checkMainHandles(action, handle, inArgsHandle, outArgsHandle, false, true, true);
    }
    else if(OFX::strEquals(action, kOfxImageEffectActionGetRegionOfDefinition)) {
      checkMainHandles(action, handle, inArgsHandle, outArgsHandle, false, false, false);
    }
    else if(OFX::strEquals(action, kOfxImageEffectActionGetRegionsOfInterest)) {
      checkMainHandles(action, handle, inArgsHandle, outArgsHandle, false, false, false);
    }
    else if(OFX::strEquals(action, kOfxImageEffectActionGetTimeDomain)) {
      checkMainHandles(action, handle, inArgsHandle, outArgsHandle, false, true, false);
    }
    else if(OFX::strEquals(action, kOfxImageEffectActionGetFramesNeeded)) {
      checkMainHandles(action, handle, inArgsHandle, outArgsHandle, false, false, false);
    }
    else if(OFX::strEquals(action, kOfxImageEffectActionGetClipPreferences)) {
      checkMainHandles(action, handle, inArgsHandle, outArgsHandle, false, false, false);
      stat = getClipPreferences(effectHandle, inArgsHandle, outArgsHandle);
    }
    else if(OFX::strEquals(action, kOfxImageEffectActionIsIdentity)) {
      checkMainHandles(action, handle, inArgsHandle, outArgsHandle, false, false, false);
      stat = isIdentity(effectHandle, inArgsHandle, outArgsHandle);
    }
    else if(OFX::strEquals(action, kOfxImageEffectActionRender)) {
      checkMainHandles(action, handle, inArgsHandle, outArgsHandle, false, false, true);
      stat = render(effectHandle, inArgsHandle, outArgsHandle);
    }
    else if(OFX::strEquals(action, kOfxImageEffectActionBeginSequenceRender)) {
      checkMainHandles(action, handle, inArgsHandle, outArgsHandle, false, false, true);
    }
    else if(OFX::strEquals(action, kOfxImageEffectActionEndSequenceRender)) {
      checkMainHandles(action, handle, inArgsHandle, outArgsHandle, false, false, true);
    }
    else if(OFX::strEquals(action, kOfxImageEffectActionDescribeInContext)) {
      checkMainHandles(action, handle, inArgsHandle, outArgsHandle, false, false, true);
      stat = describeInContext(effectHandle, inArgsHandle);
    }
    else {
      OFX::logError(true, "Unknown action '%s';", action);
    }
  } catch (std::bad_alloc) {
    // catch memory
    OFX::logError(true, "OFX Plugin Memory error;");
    stat = kOfxStatErrMemory;
  } catch ( const std::exception& e ) {
    // standard exceptions
    OFX::logError(true, "OFX Plugin error: '%s';", e.what());
    stat = kOfxStatErrUnknown;
  } catch (int err) {
    // ho hum, gone wrong somehow
    OFX::logError(true, "OFX Plugin error: '%s';", mapStatus(err));
    stat = err;
  } catch ( ... ) {
    // everything else
    OFX::logError(true, "OFX Plugin error;");
    stat = kOfxStatErrUnknown;
  }
  
  OFX::logPrint("}pluginMain - stop;");
  
  // other actions to take the default value
  return stat;
}

// function to set the host structure
static void
setHostFunc(OfxHost *hostStruct)
{
  OFX::logPrint("setHostFunc - start();\n{");
  OFX::logError(hostStruct == 0, "host is a null pointer;");
  gHost = hostStruct;
  OFX::logPrint("}setHostFunc - stop;");
}

////////////////////////////////////////////////////////////////////////////////
// the plugin struct 
static OfxPlugin basicPlugin = 
  {       
    kOfxImageEffectPluginApi,
    1,
    "net.sf.openfx:PropertyTestPlugin",
    1,
    0,
    setHostFunc,
    pluginMain
  };

// the two mandated functions
EXPORT OfxPlugin *
OfxGetPlugin(int nth)
{
  OFX::logPrint("OfxGetPlugin - start();\n{");
  OFX::logPrint("  asking for %dth plugin;", nth);
  OFX::logError(nth != 0, "requested plugin %d is more than the number of plugins in the file;", nth);
  OFX::logPrint("}OfxGetPlugin - stop;");
  
  if(nth == 0) return &basicPlugin;
  return 0;
}

EXPORT int
OfxGetNumberOfPlugins(void)
{       
  OFX::logPrint("OfxGetNumberOfPlugins - start();\n{");
  OFX::logPrint("}OfxGetNumberOfPlugins - stop;");
  return 1;
}

////////////////////////////////////////////////////////////////////////////////
// globals destructor, the destructor is called when the plugin is unloaded
class GlobalDestructor {
public :
  ~GlobalDestructor();
};

GlobalDestructor::~GlobalDestructor()
{
  if(gHostDescription) 
    delete gHostDescription;
}

static GlobalDestructor globalDestructor;
