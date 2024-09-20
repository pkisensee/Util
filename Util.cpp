///////////////////////////////////////////////////////////////////////////////
//
//  Util.cpp
//
//  Copyright © Pete Isensee (PKIsensee@msn.com).
//  All rights reserved worldwide.
//
//  Permission to copy, modify, reproduce or redistribute this source code is
//  granted provided the above copyright notice is retained in the resulting 
//  source code.
// 
//  This software is provided "as is" and without any express or implied
//  warranties.
//
///////////////////////////////////////////////////////////////////////////////

#include "Util.h"
#include "Log.h"

using namespace PKIsensee;

bool Util::FailureHandler( const char* expr, const char* fileName, int lineNum, bool doThrow )
{
  Util::DebugBreak(); // break into debugger if available

  // log the error
  static constexpr int kFailureMsgLength = 2048;
  char msg[ kFailureMsgLength ];
  sprintf_s( msg, kFailureMsgLength, "Failed check '%s' in %s line %d\n", expr, fileName, lineNum );
  PKLOG_ERR( msg );

  if( doThrow )
    throw std::exception( msg );
  return false;
}
