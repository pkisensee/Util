///////////////////////////////////////////////////////////////////////////////
//
//  Log.h
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

#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdarg>
#include <ctime>
#include <filesystem>
#include <memory>
#include <string>

#include "File.h"
#include "Util.h"
#include "..\frozen\unordered_map.h"

#define PKLOG_ERR(fmt, ...)  Log::Get().Write( LogType::Error,   fmt, ##__VA_ARGS__ )
#define PKLOG_WARN(fmt, ...) Log::Get().Write( LogType::Warning, fmt, ##__VA_ARGS__ )
#define PKLOG_SCRN(fmt, ...) Log::Get().Write( LogType::Screen,  fmt, ##__VA_ARGS__ )
#define PKLOG_NOTE(fmt, ...) Log::Get().Write( LogType::Note,    fmt, ##__VA_ARGS__ )
#define PKLOG_FILE(fmt, ...) Log::Get().Write( LogType::File,    fmt, ##__VA_ARGS__ )

namespace PKIsensee
{

constexpr size_t kLogBufferSize = 2048;
constexpr size_t kMaxStatusSize = 1024;
constexpr size_t kTimeBuffer = 26;
constexpr const char* kDefaultLogFileNames = "Log";

enum class LogType
{
  First = 0,
  Error = 0,  // error file and stderr
  Warning,    // warning file and stderr
  Screen,     // stdout only, no status prefix
  Note,       // log file and stdout, no status prefix
  File,       // log file only
  Last
};

inline constexpr size_t toInt( LogType logType )
{
  return static_cast<size_t>( logType );
}

inline constexpr LogType& operator++( LogType& logType )
{
  return logType = static_cast<LogType>( toInt( logType ) + 1 );
}

enum class StdOutput
{
  Err,
  Out,
  Null
};

struct LogFileInfo
{
  const char* ext;      // log file extension; nullptr if no file
  const char* header;   // message header
  StdOutput stdOutput;  // one of: stderr, stdout, NULL
  bool addStatusPrefix; // true if there is a status prefix
};

constexpr size_t kMaxLogs = toInt( LogType::Last );
constexpr frozen::unordered_map< LogType, LogFileInfo, kMaxLogs >
kLogFileInfo =
{ // LogType             ext      header      stdOutput        addStatusPrefix
  { LogType::Error,   { "err",   "Error: ",   StdOutput::Err,  true   } },
  { LogType::Warning, { "warn",  "Warning: ", StdOutput::Err,  true   } },
  { LogType::Screen,  { nullptr, "",          StdOutput::Out,  false  } },
  { LogType::Note,    { "log",   "",          StdOutput::Out,  false  } },
  { LogType::File,    { "file",  "",          StdOutput::Null, true } }, // nullptr to avoid deep file writes
};

///////////////////////////////////////////////////////////////////////////////

class Log
{
public:

  Log( const Log& ) = delete;
  Log& operator=( const Log& ) = delete;
  Log( Log&& ) = delete;
  Log& operator=( Log&& ) = delete;

  ~Log()
  {
    Close();
    if( !HasContent( LogType::Error ) )
      return;

    // Display log file if any error occurred
    // TODO Make less Windows-specific
    auto constexpr i = toInt( LogType::Error );
    std::string sCommand = std::string( "notepad.exe \"" ) + logs_[ i ].log.GetPath().string() + '\"';
    Util::StartProcess( sCommand );
  }

  ///////////////////////////////////////////////////////////////////////////
  //
  // Set log file names. Input extension is ignored. Must be called prior to
  // first write. Called by default in ctor.

  void SetLogFileNames( const std::filesystem::path& path )
  {
    Close();
    assert( path.has_filename() );
    std::filesystem::path logFile = path;

    // Extract the current time
    // TODO std::chrono
    time_t now = std::time( NULL );
    struct tm tmBuffer;
    localtime_s( &tmBuffer, &now );
    char currTime[ kTimeBuffer ];
    asctime_s( currTime, sizeof( currTime ), &tmBuffer );

    for( LogType logType = LogType::First; logType != LogType::Last; ++logType )
    {
      auto i = toInt( logType );
      LogFileInfo li = kLogFileInfo.at( logType );
      if( li.ext )
      {
        logs_[ i ].log.SetFile( logFile.replace_extension( li.ext ) );
        logs_[ i ].log.Create( FileFlags::Write | FileFlags::SequentialScan );
        logs_[ i ].log.Write( "File created ", 13 );
        logs_[ i ].log.Write( currTime, kTimeBuffer-1 );
      }
      switch( li.stdOutput )
      {
      case StdOutput::Err: logs_[ i ].stdStream = stderr; break;
      case StdOutput::Out: logs_[ i ].stdStream = stdout; break;
      case StdOutput::Null: logs_[ i ].stdStream = NULL;  break;
      default: assert( false ); break;
      }
    }
  }

  ///////////////////////////////////////////////////////////////////////////
  //
  // Write information to the log. To avoid potential memory failures within
  // a critical function, all work is done on the stack.

  void Write( LogType logType, const char* format, ... )
  {
    // TODO use std::format

    // Generate the formatting string
    char buffer[ kLogBufferSize ];
    va_list pArgList;
    va_start( pArgList, format );
    auto charsWritten = _vsnprintf_s( buffer, kLogBufferSize, kLogBufferSize - 1, format, pArgList );
    va_end( pArgList );
    assert( charsWritten < kLogBufferSize );

    // Replace LF with CRLF
    char replace[ kLogBufferSize ] = { '\0' };
    auto b = buffer, be = buffer + charsWritten;
    auto r = replace, re = replace + kLogBufferSize;
    while( b != be && r != ( re - 1 ) )
    {
      if( ( *b == '\n' ) && ( b != buffer ) && ( *(b-1) != '\r' ) )
        *r++ = '\r';
      *r++ = *b++;
    }
    *r = '\0';

    auto i = toInt( logType );
    logs_[ i ].hasContent = true;
    LogFileInfo li = kLogFileInfo.at( logType );

    // Format the final output string with leading status if required
    size_t statusSize = 0;
    b = buffer;
    if( li.addStatusPrefix && !status_.empty() )
    {
      static_assert( kMaxStatusSize < kLogBufferSize );
      statusSize = std::min( status_.size(), kMaxStatusSize );
      std::memcpy( b, status_.data(), statusSize );
      b += statusSize;
      *b++ = ':';
      *b++ = ' ';
      statusSize += 2; // colon and space
    }
    auto replaceChars = static_cast<size_t>( r - replace );
    auto logTextSize = std::min( replaceChars, kLogBufferSize - statusSize );
    std::memcpy( b, replace, logTextSize );
    b += logTextSize;
    *b = '\0';
    auto totalLogChars = statusSize + logTextSize;
    assert( totalLogChars < kLogBufferSize );

    // Write to file and/or output
    if( li.ext && totalLogChars )
    {
      File& logFile = logs_[ i ].log;
      assert( logFile.IsOpen() );
      logFile.Write( buffer, totalLogChars );
    }
    FILE* pStdStream = logs_[ i ].stdStream;
#pragma warning(push)
#pragma warning(disable:4774)
    if( pStdStream )
      fprintf( pStdStream, buffer );
#pragma warning(pop)
  }

  void Close()
  {
    for( LogType logType = LogType::First; logType != LogType::Last; ++logType )
    {
      auto i = toInt( logType );
      LogFileInfo li = kLogFileInfo.at( logType );
      if( li.ext )
        logs_[ i ].log.Close();
    }
  }

  bool HasContent( LogType logType ) const
  {
    assert( logType < LogType::Last );
    return logs_[ toInt( logType ) ].hasContent;
  }

  void SetStatus( const std::string& status )
  {
    status_ = status;
  }

  static Log& Get() // access singleton
  {
    static Log log;
    return log;
  }

protected:

  Log()
  {
    SetLogFileNames( kDefaultLogFileNames );
  }

private:

  struct LogFile
  {
    File  log;                // *.err, *.warn, *.log
    FILE* stdStream = NULL;   // stderr, stdout
    bool  hasContent = false; // true if wrote to this log

    LogFile() = default;
    LogFile( const LogFile& ) = delete;
    LogFile& operator=( const LogFile& ) = delete;
    LogFile( LogFile&& ) = delete;
    LogFile& operator=( LogFile&& ) = delete;
  };

  std::array<LogFile, toInt( LogType::Last )> logs_;
  std::string status_;
  
}; // end class Log

} // end PKIsensee

///////////////////////////////////////////////////////////////////////////////
