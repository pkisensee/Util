///////////////////////////////////////////////////////////////////////////////
//
//  Util.h
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

// Only Standard C++ headers here
#include <algorithm>
#include <array>
#include <charconv>
#include <concepts>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace PKIsensee {
namespace Util {

///////////////////////////////////////////////////////////////////////////////
//
// Concepts

template <typename ValType>
concept IsNumeric = std::is_integral_v<ValType> || std::is_floating_point_v<ValType>;

template <typename Container>
concept IsContainer = requires( Container a, const Container c )
{
  requires std::same_as<typename Container::reference, typename Container::value_type&>;
  requires std::same_as<typename Container::const_reference, const typename Container::value_type&>;
  requires std::forward_iterator<typename Container::iterator>;
  requires std::forward_iterator<typename Container::const_iterator>;
  { a.begin() } -> std::convertible_to<typename Container::iterator>;
  { a.end()   } -> std::convertible_to<typename Container::iterator>;
  { c.begin() } -> std::convertible_to<typename Container::const_iterator>;
  { c.end()   } -> std::convertible_to<typename Container::const_iterator>;
  { a.size()  } -> std::convertible_to<typename Container::size_type>;
};

///////////////////////////////////////////////////////////////////////////////
//
// Definitions in WinUtil library

class Window;
using FileList = std::vector<std::filesystem::path>;

uint32_t GetLastError();
void DebugBreak();
std::string GetRegistryValue( const std::string& regPath, const std::string& regEntry );
void StartProcess( const std::string& commandLine ); // e.g. "notepad.exe foo.log"
char GetKeyReleased();
FileList GetFileDialog( const Window& parent );

///////////////////////////////////////////////////////////////////////////////
//
// Generic window object; definition in WinWindow.cpp etc.

class Window
{
public:
  Window() = delete;
  Window( const Window& ) = delete;
  Window& operator=( const Window& ) = delete;

  explicit Window( void* windowHandle = nullptr );
  template<class T> T GetHandle();
  template<class T> const T GetHandle() const;

private:
  class Impl;
  using ImplDeleter = void ( * )( Impl* );
  std::unique_ptr<Impl, ImplDeleter> mImpl;
};

///////////////////////////////////////////////////////////////////////////////
//
// Generic event signalling object; definition in WinEvent.cpp etc.

class Event
{
public:
  Event();
  Event( const Event& ) = delete;
  Event& operator=( const Event& ) = delete;

  void* GetHandle();
  void Reset();
  bool IsSignalled( uint32_t timeoutMs ) const;

private:
  class Impl;
  using ImplDeleter = void (*)( Impl* );
  std::unique_ptr<Impl, ImplDeleter> mImpl;
};

///////////////////////////////////////////////////////////////////////////////

namespace { // anonymous
template< typename Target, typename Source >
Target ToNumInternal( const Source* start, const Source* end, [[maybe_unused]] int base )
{
  auto t = Target{};
  if constexpr( std::is_floating_point_v< Target > )
    std::from_chars( start, end, t, std::chars_format::general );
  else
    std::from_chars( start, end, t, base );
  return t;
}
} // end namespace anonymous

///////////////////////////////////////////////////////////////////////////////
//
// Convert string to number. Source must be raw characters or a type that 
// supports data()/size(). Base unused for floating point.

template< typename Target, typename Source >
Target ToNum( const Source& source, int base = 10 )
requires IsNumeric<Target>
{
  const auto* start = source.data();
  const auto* end = start + source.size();
  return ToNumInternal<Target>( start, end, base );
}

template< typename Target, size_t N >
Target ToNum( const char (&source)[N], int base = 10 )
requires IsNumeric<Target>
{
  const auto* start = source;
  const auto* end = start + N - 1;
  return ToNumInternal<Target>( start, end, base );
}

///////////////////////////////////////////////////////////////////////////////
//
// Convert number to string. Source must be integer or floating point.

template< typename Target, typename Number >
Target ToStr( Number number, [[maybe_unused]] int base = 10 )
requires Util::IsNumeric<Number>
{
  // Avoid unnecessary allocation at the cost of a copy operation by putting initial 
  // result on the stack. Largest potential value is 64-bit base 2 = 64 characters, 
  // plus the sign digit
  constexpr size_t kMaxDigits = size_t(64) + 1;
  std::array<char, kMaxDigits> arr = {};

  auto* start = arr.data();
  auto* end = start + arr.size();
  std::to_chars_result result;

  if constexpr( std::is_floating_point_v< Number > )
    result = std::to_chars( start, end, number, std::chars_format::general );
  else
    result = std::to_chars( start, end, number, base );

  if( result.ec != std::errc() )
    return {};

  auto t = Target{ start, result.ptr };
  return t;
}

template< typename Number >
std::string ToString( Number number, int base = 10 )
{
  return ToStr<std::string>( number, base );
}

///////////////////////////////////////////////////////////////////////////////
//
// Endian check

constexpr bool IsBigEndian()
{
  // TODO replace with std::endian
#ifdef _WIN32
  return false;
#else
  return true;
#endif

  /*
  constexpr static int32_t one = 1;
  return ( *(const int8_t*)(&one) == 0);
  */

  /*
  union {
      uint32_t i32;
      uint8_t  i8[ sizeof( i32 ) ];
  } constexpr static kEndianCheck = { 0xAABBCCDD };

  return x.i8[ 0 ] == 0xAA;
  */
}

///////////////////////////////////////////////////////////////////////////////
//
// Flips bytes; useful for endian conversions

#pragma warning(push)
#pragma warning(disable: 6001) // disable "Using uninitialized memory 'dst'
template <typename T>
constexpr T ReverseBytes( T u )
{
  union U
  {
    T val;
    std::array<uint8_t, sizeof( T )> raw;
  };
  U src{ u };
  U dst;
  std::reverse_copy( src.raw.begin(), src.raw.end(), dst.raw.begin() );
  return dst.val;
}
#pragma warning(pop)

///////////////////////////////////////////////////////////////////////////////
//
// Convert incoming value to big endian

template <typename T>
constexpr T ToBigEndian( T u )
{
  if constexpr( IsBigEndian() )
    return u;
  return ReverseBytes( u );
}

///////////////////////////////////////////////////////////////////////////////
//
// Convert incoming value to little endian

template <typename T>
constexpr T ToLittleEndian( T u )
{
  if constexpr( IsBigEndian() )
    return ReverseBytes( u );
  return u;
}

///////////////////////////////////////////////////////////////////////////////
//
// Four Character Code

constexpr uint32_t FourCC( const char code[ 5 ] ) {
  return uint32_t( ( code[ 3 ] << 24 ) |
                   ( code[ 2 ] << 16 ) |
                   ( code[ 1 ] <<  8 ) |
                     code[ 0 ] );
}

///////////////////////////////////////////////////////////////////////////////
//
// Takes each kBitsPerByte from the source and packs them together into the
// output value. For instance, to convert an ID3 synchSafe integer from an MP3 
// file to a real value, invoke PackBits<7>( intFromFile )
//
// Requires that the "empty" high bits of each byte of the incoming value be zero
//
// For T == uint32_t and kBitsPerByte == 7, this function is equivalent to:
// return
//   ( ( sourceInt & 0x7F000000 ) >> 3 ) |
//   ( ( sourceInt & 0x007F0000 ) >> 2 ) |
//   ( ( sourceInt & 0x00007F00 ) >> 1 ) |
//   ( ( sourceInt & 0x0000007F ) >> 0 );
//
// For details on synchSafe ints, see id3 6.2, https://handwiki.org/wiki/Synchsafe
//
// The code is complicated, but a good compiler will optimize away the loops

template <uint8_t kBitsPerByte, typename T >
T PackBits( T sourceInt )
{
  static_assert( kBitsPerByte <= CHAR_BIT );
  static_assert( kBitsPerByte > 0 );
  if constexpr( sizeof( T ) == 1 )
    return sourceInt;
  else if constexpr( kBitsPerByte == CHAR_BIT )
    return sourceInt;
  else
  {
    constexpr auto kZero = T( 0 );
    constexpr auto kOne = T( 1 );
    constexpr auto kEight = T( CHAR_BIT );
    constexpr auto kHighBits = kEight - kBitsPerByte;

    // Check the empty bits by creating a simple mask
    // A 7-bit mask would like like 0b10000000 == 0x80
    auto maskHighBits = kZero;
    for( auto i = 0; i < kHighBits; ++i )
      maskHighBits |= ( kOne << (kEight-1-i) );

    T highBitsSet{ T( 0 ) };
    for( auto i = 0; i < sizeof( T ); ++i )
    {
      highBitsSet |= maskHighBits;
      maskHighBits <<= kEight;
    }

    // If the "empty" bits of the incoming value are not actually empty,
    // return the original value.
    if( ( sourceInt & highBitsSet ) != kZero )
      return sourceInt;

    // Pack each kBitsPerByte into the result
    // A 7-bit mask would look like 0b1111111 == 0x7F
    auto maskLowBits = kZero;
    for( auto i = 0; i < kBitsPerByte; ++i )
      maskLowBits |= ( kOne << T( i ) );

    // The real work happens here
    auto result = kZero;
    for( auto i = 0; i < sizeof( T ); ++i )
    {
      result |= ( sourceInt & maskLowBits ) >> ( i * kHighBits );
      maskLowBits <<= kEight;
    }
    return result;
  }
}

///////////////////////////////////////////////////////////////////////////////
//
// Takes each kBits from the source and unpacks them into individual bytes.
// For instance, to convert an integer to an ID3 synchSafe integer that can be
// saved to an MP3 file, invoke UnpackBits<7>( int )
//
// For T == uint32_t and kBitsPerByte == 7, this function is equivalent to:
//  return
//    ( ( sourceInt & 0b00001111111000000000000000000000 ) << 3 ) |
//    ( ( sourceInt & 0b00000000000111111100000000000000 ) << 2 ) |
//    ( ( sourceInt & 0b00000000000000000011111110000000 ) << 1 ) |
//    ( ( sourceInt & 0b00000000000000000000000001111111 ) << 0 );

template <uint8_t kBits, typename T >
T UnpackBits( T sourceInt )
{
  static_assert( kBits <= CHAR_BIT );
  if constexpr( kBits == CHAR_BIT )
    return sourceInt;
  else
  {
    constexpr auto kZero = T( 0 );
    constexpr auto kOne = T( 1 );
    constexpr auto kEight = T( CHAR_BIT );
    constexpr auto kHighBits = kEight - kBits;

    // Unpack each kBits into the result
    // A 7-bit mask would look like 0b1111111 == 0x7F
    auto mask = kZero;
    for( auto i = 0; i < kBits; ++i )
      mask |= ( kOne << i );

    // The real work happens here
    auto result = kZero;
    for( auto i = 0; i < sizeof( T ); ++i )
    {
      result |= ( sourceInt & mask ) << ( i * kHighBits );
      mask <<= kBits;
    }
    return result;
  }
}

} // end namespace Util

} // end namespace PKIsensee
