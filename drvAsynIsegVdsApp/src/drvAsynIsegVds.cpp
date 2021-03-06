//******************************************************************************
// Copyright (C) 2014 Florian Feldbauer <florian@ep1.ruhr-uni-bochum.de>
//                    - Helmholtz-Institut/University Mainz, Institute for nuclear physics
//
// This file is part of drvAsynIsegVds
//
// drvAsynIsegVds is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// drvAsynIsegVds is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
// version 1.0.0; Aug. 15, 2014
//******************************************************************************

//_____ I N C L U D E S _______________________________________________________

// ANSI C/C++ includes
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

// EPICS includes
#include <epicsEvent.h>
#include <epicsExport.h>
#include <epicsMutex.h>
#include <epicsString.h>
#include <epicsThread.h>
#include <epicsTime.h>
#include <epicsTimer.h>
#include <epicsTypes.h>
#include <iocsh.h>

// ASYN includes
#include "asynDriver.h"
#include "asynStandardInterfaces.h"

// local includes
#include "drvAsynIsegVds.h"
#include "VmeMaster.h"

//_____ D E F I N I T I O N S __________________________________________________
typedef  std::map<int, epicsUInt32>::const_iterator  cmdIter;

typedef union{
  epicsFloat32 fval;
  epicsUInt32  ival;
} float_t;

//_____ G L O B A L S __________________________________________________________

//_____ L O C A L S ____________________________________________________________
static const char *driverName = "drvAsynIsegVdsDriver";
static const epicsUInt32 chanAddr[8] = { 0x0100, 0x0140, 0x0180, 0x01c0,
                                         0x0200, 0x0240, 0x0280, 0x02c0 };

//_____ F U N C T I O N S ______________________________________________________

//------------------------------------------------------------------------------
//! @brief   Called when asyn clients call pasynUInt32Digital->read().
//!
//! @param   [in]  pasynUser  pasynUser structure that encodes the reason and address
//! @param   [out] value      Address of the value to read
//! @param   [in]  mask       Mask value to use when reading the value.
//!
//! @return  in case of no error occured asynSuccess is returned. Otherwise
//!          asynError or asynTimeout is returned. A error message is stored
//!          in pasynUser->errorMessage.
//------------------------------------------------------------------------------
asynStatus drvAsynIsegVds::readUInt32Digital( asynUser *pasynUser, epicsUInt32 *value, epicsUInt32 mask ) {
  static const char *functionName = "readUInt32Digital";
  int function = pasynUser->reason;
  int addr = 0;
  epicsTimeStamp timeStamp; getTimeStamp(&timeStamp);
  asynStatus status = asynSuccess;
  epicsUInt32 vmeData = 0;
  epicsUInt32 vmeAddr = 0;
    
  status = getAddress( pasynUser, &addr ); if( status ) return status;

  cmdIter it = _modcmds.find( function );
  if( _modcmds.end() == it ) {
    it = _chancmds.find( function );
    if( _chancmds.end() == it ) return asynError;
    vmeAddr += chanAddr[addr];
  }
  vmeAddr += it->second;

  try {
    vmeData = _vme->readRegisterA16D32( _base, vmeAddr );
  } catch( VmeException &e ){
    epicsSnprintf( pasynUser->errorMessage, pasynUser->errorMessageSize, 
                   "%s:%s: function=%d %s",
                   driverName, functionName, function, e.what() );
    return asynError;
  }

  status = (asynStatus) setUIntDigitalParam( addr, function, vmeData, mask );
  status = (asynStatus) getUIntDigitalParam( addr, function, value, mask );
  pasynUser->timestamp = timeStamp;
  if( status ) 
    epicsSnprintf( pasynUser->errorMessage, pasynUser->errorMessageSize, 
                   "%s:%s: status=%d, function=%d, value=%d",
                   driverName, functionName, status, function, *value );
  else        
    asynPrint( pasynUser, ASYN_TRACEIO_DEVICE, 
               "%s:%s: function=%d, value=%d\n", 
               driverName, functionName, function, *value );
  return status;
}

//------------------------------------------------------------------------------
//! @brief   Called when asyn clients call pasynUInt32Digital->write().
//!
//! @param   [in]  pasynUser  pasynUser structure that encodes the reason and address
//! @param   [in]  value      Value to write
//! @param   [in]  mask       Mask value to use when reading the value.
//!
//! @return  in case of no error occured asynSuccess is returned. Otherwise
//!          asynError or asynTimeout is returned. A error message is stored
//!          in pasynUser->errorMessage.
//------------------------------------------------------------------------------
asynStatus drvAsynIsegVds::writeUInt32Digital( asynUser *pasynUser, epicsUInt32 value, epicsUInt32 mask ){
  static const char* functionName = "writeUInt32Digital";
  int function = pasynUser->reason;
  int addr = 0;
  epicsTimeStamp timeStamp; getTimeStamp(&timeStamp);
  asynStatus status = asynSuccess;
  epicsUInt32 vmeAddr = 0;

  // Return if function is a read-only parameter
  if ( function == P_ModStatus   || \
       function == P_ChanStatus  )
    return asynSuccess;
  
  status = getAddress( pasynUser, &addr ); if( status != asynSuccess ) return status;

  cmdIter it = _modcmds.find( function );
  if( _modcmds.end() == it ) {
    it = _chancmds.find( function );
    if( _chancmds.end() == it ) return asynError;
    vmeAddr += chanAddr[addr];
  }
  vmeAddr += it->second;

  try{
    _vme->writeRegisterA16D32( _base, vmeAddr, value );
  } catch( VmeException &e ){
    epicsSnprintf( pasynUser->errorMessage, pasynUser->errorMessageSize, 
                   "%s:%s: function=%d %s",
                   driverName, functionName, function, e.what() );
    return asynError;
  }

  // update value of parameter
  status = (asynStatus) setUIntDigitalParam( addr, function, value, mask );
  status = (asynStatus) callParamCallbacks( addr, addr );
    
  if( status ) 
    epicsSnprintf( pasynUser->errorMessage, pasynUser->errorMessageSize, 
                   "%s:%s:%s: status=%d, function=%d, value=%d", 
                   driverName, _deviceName, functionName, status, function, value );
  else        
    asynPrint( pasynUser, ASYN_TRACEIO_DEVICE, 
               "%s:%s: function=%d, value=%d\n", 
               driverName, functionName, function, value );

  return status;
}

//------------------------------------------------------------------------------
//! @brief   Called when asyn clients call pasynFloat64->read().
//!
//! @param   [in]  pasynUser  pasynUser structure that encodes the reason and address
//! @param   [in]  value      Address of the value to read
//!
//! @return  in case of no error occured asynSuccess is returned. Otherwise
//!          asynError or asynTimeout is returned. A error message is stored
//!          in pasynUser->errorMessage.
//------------------------------------------------------------------------------
asynStatus drvAsynIsegVds::readFloat64( asynUser *pasynUser, epicsFloat64 *value ) {
  static const char *functionName = "readFloat64";
  int function = pasynUser->reason;
  int addr = 0;
  asynStatus status = asynSuccess;
  epicsTimeStamp timeStamp; getTimeStamp(&timeStamp);
  epicsUInt32 vmeAddr = 0;
  float_t data;
  
  status = getAddress(pasynUser, &addr); if (status != asynSuccess) return(status);

  cmdIter it = _modcmds.find( function );
  if( _modcmds.end() == it ) {
    it = _chancmds.find( function );
    if( _chancmds.end() == it ) return asynError;
    vmeAddr += chanAddr[addr];
  }
  vmeAddr += it->second;

  try {
    epicsUInt32 vmeData = _vme->readRegisterA16D32( _base, vmeAddr );
    data.ival = vmeData;
  } catch( VmeException &e ){
    epicsSnprintf( pasynUser->errorMessage, pasynUser->errorMessageSize, 
                   "%s:%s: function=%d %s",
                   driverName, functionName, function, e.what() );
    return asynError;
  }

  // convert current from A to uA
  if ( function == P_ChanImom || function == P_ChanIset ) data.fval *= 1.e6;

  status = setDoubleParam( addr, function, data.fval );
  status = (asynStatus) getDoubleParam(addr, function, value);
  pasynUser->timestamp = timeStamp;
  if (status) 
    epicsSnprintf( pasynUser->errorMessage, pasynUser->errorMessageSize, 
                   "%s:%s: status=%d, function=%d, value=%f", 
                   driverName, functionName, status, function, *value );
  else        
    asynPrint( pasynUser, ASYN_TRACEIO_DRIVER, 
               "%s:%s: function=%d, value=%f\n", 
               driverName, functionName, function, *value );
  return(status);
}

//------------------------------------------------------------------------------
//! @brief   Called when asyn clients call pasynFloat64->write().
//!
//! @param   [in]  pasynUser  pasynUser structure that encodes the reason and address
//! @param   [in]  value      Value to write
//!
//! @return  in case of no error occured asynSuccess is returned. Otherwise
//!          asynError or asynTimeout is returned. A error message is stored
//!          in pasynUser->errorMessage.
//------------------------------------------------------------------------------
asynStatus drvAsynIsegVds::writeFloat64( asynUser *pasynUser, epicsFloat64 value ) {
  static const char *functionName = "writeFloat64";
  int function = pasynUser->reason;
  asynStatus status = asynSuccess;
  int addr = 0;
  epicsUInt32 vmeAddr = 0;
  float_t vmeData; vmeData.fval = (epicsFloat32)value;

  // Return if function is a read-only parameter
  if ( function == P_VMax        || \
       function == P_IMax        || \
       function == P_SupplyP5    || \
       function == P_SupplyP12   || \
       function == P_SupplyN12   || \
       function == P_Temperature || \
       function == P_ChanVmom    || \
       function == P_ChanImom )
    return asynSuccess;

  status = getAddress( pasynUser, &addr ); if ( status != asynSuccess ) return status;

  // convert current from A to uA
  if ( function == P_ChanIset ) vmeData.fval *= 1.e-6;

  cmdIter it = _modcmds.find( function );
  if( _modcmds.end() == it ) {
    it = _chancmds.find( function );
    if( _chancmds.end() == it ) return asynError;
    vmeAddr += chanAddr[addr];
  }
  vmeAddr += it->second;

  try{
    _vme->writeRegisterA16D32( _base, vmeAddr, vmeData.ival );
  } catch( VmeException &e ){
    epicsSnprintf( pasynUser->errorMessage, pasynUser->errorMessageSize, 
                   "%s:%s: function=%d %s",
                   driverName, functionName, function, e.what() );
    return asynError;
  }

  status = setDoubleParam( addr, function, value );
  status = (asynStatus)callParamCallbacks( addr, addr );
  if ( status ) 
    asynPrint( pasynUser, ASYN_TRACE_ERROR, 
               "%s:%s: error, status=%d function=%d, value=%f\n", 
               driverName, functionName, status, function, value );
  else        
    asynPrint( pasynUser, ASYN_TRACEIO_DRIVER, 
               "%s:%s: function=%d, value=%f\n", 
               driverName, functionName, function, value );
  return status;
}

//------------------------------------------------------------------------------
//! @brief   Constructor for the drvAsynIsegVds class.
//!          Calls constructor for the asynPortDriver base class.
//!
//! @param   [in]  portName    The name of the asynPortDriver to be created.
//! @param   [in]  BA          The base address
//------------------------------------------------------------------------------
drvAsynIsegVds::drvAsynIsegVds( const char *portName, const int BA ) 
  : asynPortDriver( portName, 
                    8, // maxAddr
                    NUM_ISEGVDS_PARAMS,
                    asynCommonMask | asynInt32Mask | asynUInt32DigitalMask | asynFloat64Mask | asynDrvUserMask, // Interface mask
                    asynCommonMask | asynInt32Mask | asynUInt32DigitalMask | asynFloat64Mask,  // Interrupt mask
                    ASYN_CANBLOCK | ASYN_MULTIDEVICE, // asynFlags.
                    1, // Autoconnect
                    0, // Default priority
                    0 ) // Default stack size
{
  static const char *functionName = "drvAsynIsegVds";
  
  _deviceName = epicsStrDup( portName );
  _base       = BA;
  _vme        = VmeMaster::getInstance(); 
  if( !_vme ) {
    fprintf( stderr, "\033[31;1m %s:%s: Could not find VmeMastet. \033[0m \n",
             driverName, functionName );
    return;
  }

  // Module data
  createParam( P_ISEGVDS_MODSTATUS_STRING,         asynParamUInt32Digital, &P_ModStatus );
  createParam( P_ISEGVDS_MODEVTSTATUS_STRING,      asynParamUInt32Digital, &P_ModEvtStatus );
  createParam( P_ISEGVDS_MODEVTMASK_STRING,        asynParamUInt32Digital, &P_ModEvtMask );
  createParam( P_ISEGVDS_MODCTRL_STRING,           asynParamUInt32Digital, &P_ModCtrl );
  createParam( P_ISEGVDS_MODEVTCHANSTATUS_STRING,  asynParamUInt32Digital, &P_ModEvtChanStatus );
  createParam( P_ISEGVDS_MODEVTCHANMASK_STRING,    asynParamUInt32Digital, &P_ModEvtChanMask );
  createParam( P_ISEGVDS_MODEVTGRPSTATUS_STRING,   asynParamUInt32Digital, &P_ModEvtGrpStatus );
  createParam( P_ISEGVDS_MODEVTGRPMASK_STRING,     asynParamUInt32Digital, &P_ModEvtGrpMask );
  createParam( P_ISEGVDS_VRAMP_STRING,             asynParamFloat64,       &P_VRamp );
  createParam( P_ISEGVDS_CRAMP_STRING,             asynParamFloat64,       &P_CRamp );
  createParam( P_ISEGVDS_VMAX_STRING,              asynParamFloat64,       &P_VMax );
  createParam( P_ISEGVDS_IMAX_STRING,              asynParamFloat64,       &P_IMax );
  createParam( P_ISEGVDS_SUPPLYP5_STRING,          asynParamFloat64,       &P_SupplyP5 );
  createParam( P_ISEGVDS_SUPPLYP12_STRING,         asynParamFloat64,       &P_SupplyP12 );
  createParam( P_ISEGVDS_SUPPLYN12_STRING,         asynParamFloat64,       &P_SupplyN12 );
  createParam( P_ISEGVDS_TEMPERATURE_STRING,       asynParamFloat64,       &P_Temperature );
  // channel data
  createParam( P_ISEGVDS_CHANSTATUS_STRING,        asynParamUInt32Digital, &P_ChanStatus );
  createParam( P_ISEGVDS_CHANEVTSTATUS_STRING,     asynParamUInt32Digital, &P_ChanEvtStatus );
  createParam( P_ISEGVDS_CHANEVTMASK_STRING,       asynParamUInt32Digital, &P_ChanEvtMask );
  createParam( P_ISEGVDS_CHANCTRL_STRING,          asynParamUInt32Digital, &P_ChanCtrl );
  createParam( P_ISEGVDS_CHANVSET_STRING,          asynParamFloat64,       &P_ChanVset );
  createParam( P_ISEGVDS_CHANISET_STRING,          asynParamFloat64,       &P_ChanIset );
  createParam( P_ISEGVDS_CHANVMOM_STRING,          asynParamFloat64,       &P_ChanVmom );
  createParam( P_ISEGVDS_CHANIMOM_STRING,          asynParamFloat64,       &P_ChanImom );
  createParam( P_ISEGVDS_CHANVBOUNDS_STRING,       asynParamFloat64,       &P_ChanVBounds );
  createParam( P_ISEGVDS_CHANIBOUNDS_STRING,       asynParamFloat64,       &P_ChanIBounds );

  _modcmds.insert( std::make_pair( P_ModStatus,         0x0000 ) );
  _modcmds.insert( std::make_pair( P_ModEvtStatus,      0x0004 ) );
  _modcmds.insert( std::make_pair( P_ModEvtMask,        0x0008 ) );
  _modcmds.insert( std::make_pair( P_ModCtrl,           0x000c ) );
  _modcmds.insert( std::make_pair( P_ModEvtChanStatus,  0x0010 ) );
  _modcmds.insert( std::make_pair( P_ModEvtChanMask,    0x0014 ) );
  _modcmds.insert( std::make_pair( P_ModEvtGrpStatus,   0x0018 ) );
  _modcmds.insert( std::make_pair( P_ModEvtGrpMask,     0x001c ) );
  _modcmds.insert( std::make_pair( P_VRamp,             0x0020 ) );
  _modcmds.insert( std::make_pair( P_CRamp,             0x0024 ) );
  _modcmds.insert( std::make_pair( P_VMax,              0x0028 ) );
  _modcmds.insert( std::make_pair( P_IMax,              0x002c ) );
  _modcmds.insert( std::make_pair( P_SupplyP5,          0x0040 ) );
  _modcmds.insert( std::make_pair( P_SupplyP12,         0x0044 ) );
  _modcmds.insert( std::make_pair( P_SupplyN12,         0x0048 ) );
  _modcmds.insert( std::make_pair( P_Temperature,       0x004c ) );

  _chancmds.insert( std::make_pair( P_ChanStatus,        0x0000 ) );
  _chancmds.insert( std::make_pair( P_ChanEvtStatus,     0x0004 ) );
  _chancmds.insert( std::make_pair( P_ChanEvtMask,       0x0008 ) );
  _chancmds.insert( std::make_pair( P_ChanCtrl,          0x000c ) );
  _chancmds.insert( std::make_pair( P_ChanVset,          0x0010 ) );
  _chancmds.insert( std::make_pair( P_ChanIset,          0x0014 ) );
  _chancmds.insert( std::make_pair( P_ChanVmom,          0x0018 ) );
  _chancmds.insert( std::make_pair( P_ChanImom,          0x001c ) );
  _chancmds.insert( std::make_pair( P_ChanVBounds,       0x0020 ) );
  _chancmds.insert( std::make_pair( P_ChanIBounds,       0x0024 ) );

}

// Configuration routines. Called directly, or from the iocsh function below
extern "C" {
  
  //----------------------------------------------------------------------------
  //! @brief   EPICS iocsh callable function to call constructor
  //!          for the drvAsynIsegVds class.
  //!
  //! @param  [in]  portName The name of the asyn port driver to be created.
  //! @param  [in]  BA       RAM Base address of the ISEG VDS module
  //----------------------------------------------------------------------------
  int drvAsynIsegVdsConfigure( const char *portName, const int BA ) {
    new drvAsynIsegVds( portName, BA );
    return( asynSuccess );
  }
  static const iocshArg initIsegVdsArg0 = { "portName", iocshArgString };
  static const iocshArg initIsegVdsArg1 = { "BA",       iocshArgInt };
  static const iocshArg * const initIsegVdsArgs[] = { &initIsegVdsArg0, &initIsegVdsArg1 };
  static const iocshFuncDef initIsegVdsFuncDef = { "drvAsynIsegVdsConfigure", 2, initIsegVdsArgs };
  static void initIsegVdsCallFunc( const iocshArgBuf *args ) {
    drvAsynIsegVdsConfigure( args[0].sval, args[1].ival );
  }
  
  //----------------------------------------------------------------------------
  //! @brief   Register functions to EPICS
  //----------------------------------------------------------------------------
  void drvAsynIsegVdsDrvRegister( void ) {
    static int firstTime = 1;
    if ( firstTime ) {
      iocshRegister( &initIsegVdsFuncDef, initIsegVdsCallFunc );
      firstTime = 0;
    }
  }
  
  epicsExportRegistrar( drvAsynIsegVdsDrvRegister );
}
