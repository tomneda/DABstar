/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 * $Id: pa_ringbuffer.c 1738 2011-08-18 11:47:28Z rossb $
 * Portable Audio I/O Library
 * Ring Buffer utility.
 *
 * Author: Phil Burk, http://www.softsynth.com
 * modified for SMP safety on Mac OS X by Bjorn Roche
 * modified for SMP safety on Linux by Leland Lucius
 * also, allowed for const where possible
 * modified for multiple-byte-sized data elements by Sven Fischer
 *
 * Note that this is safe only for a single-thread reader and a
 * single-thread writer.
 *
 * This program uses the PortAudio Portable Audio Library.
 * For more information see: http://www.portaudio.com
 * Copyright (c) 1999-2000 Ross Bencina and Phil Burk
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 *
 *    Copyright (C) 2008, 2009, 2010
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    The ringbuffer here is a rewrite of the ringbuffer used in the PA code
 *    All rights remain with their owners
 *    This file is part of the SDR-J.
 *    Many of the ideas as implemented in SDR-J are derived from
 *    other work, made available through the GNU general Public License.
 *    All copyrights of the original authors are recognized.
 *
 *    SDR-J is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    SDR-J is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with ESDR; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include "glob_defs.h"
#include <array>
#include <atomic>
#include <vector>
#include <cstring>
#include <cassert>
#include <map>
#include <memory>

#ifdef __APPLE__
#include <libkern/OSAtomic.h>
#ifdef OSATOMIC_DEPRECATED
#include <atomic>
#endif
#
/* Mac OS X only provides
   full memory barriers, so the three types of barriers are the same,
   however, these barriers are superior to compiler-based ones. */

inline void Apple_MemoryBarrier() {
#ifdef OSATOMIC_DEPRECATED
std::atomic_thread_fence(std::memory_order_seq_cst);
#else
OSMemoryBarrier();
#endif
}
#   define PaUtil_FullMemoryBarrier()  Apple_MemoryBarrier()
#   define PaUtil_ReadMemoryBarrier()  Apple_MemoryBarrier()
#   define PaUtil_WriteMemoryBarrier() Apple_MemoryBarrier()
#elif defined(__GNUC__)
/* GCC >= 4.1 has built-in intrinsics. We'll use those */
#   if (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)
# define PaUtil_FullMemoryBarrier()  __sync_synchronize()
# define PaUtil_ReadMemoryBarrier()  __sync_synchronize()
# define PaUtil_WriteMemoryBarrier() __sync_synchronize()
/* as a fallback, GCC understands volatile asm and "memory" to mean it
 * should not reorder memory read/writes */
#   elif defined( __PPC__ )
#      define PaUtil_FullMemoryBarrier()  asm volatile("sync":::"memory")
#      define PaUtil_ReadMemoryBarrier()  asm volatile("sync":::"memory")
#      define PaUtil_WriteMemoryBarrier() asm volatile("sync":::"memory")
#   elif defined( __i386__ ) || defined( __i486__ ) || defined( __i586__ ) || defined( __i686__ ) || defined( __x86_64__ )
#      define PaUtil_FullMemoryBarrier()  asm volatile("mfence":::"memory")
#      define PaUtil_ReadMemoryBarrier()  asm volatile("lfence":::"memory")
#      define PaUtil_WriteMemoryBarrier() asm volatile("sfence":::"memory")
#   else
#      ifdef ALLOW_SMP_DANGERS
#         warning Memory barriers not defined on this system or system unknown
#         warning For SMP safety, you should fix this.
#         define PaUtil_FullMemoryBarrier()
#         define PaUtil_ReadMemoryBarrier()
#         define PaUtil_WriteMemoryBarrier()
#      else
#         error Memory barriers are not defined on this system. You can still compile by defining ALLOW_SMP_DANGERS, but SMP safety will not be guaranteed.
#      endif
#   endif
#else
#   ifdef ALLOW_SMP_DANGERS
#      warning Memory barriers not defined on this system or system unknown
#      warning For SMP safety, you should fix this.
#      define PaUtil_FullMemoryBarrier()
#      define PaUtil_ReadMemoryBarrier()
#      define PaUtil_WriteMemoryBarrier()
#   else
#      error Memory barriers are not defined on this system. You can still compile by defining ALLOW_SMP_DANGERS, but SMP safety will not be guaranteed.
#   endif
#endif

class RingbufferBase
{
public:
  struct SFillState
  {
    f32 Percent;
    u32 Filled;
    u32 Free;
    u32 Total;
  };
};

template<class TElem>
class RingBuffer : public RingbufferBase
{
public:
  explicit RingBuffer(const u32 elementCount)
  {
    bufferSize = _round_up_to_next_power_of_2(elementCount);
    buffer.resize(bufferSize * sizeof(TElem));
    smallMask = bufferSize - 1;
    bigMask = (bufferSize << 1) - 1;
  }

  ~RingBuffer() = default;

  [[nodiscard]] i32 get_ring_buffer_read_available() const
  {
    return (writeIndex - readIndex) & bigMask;
  }

  [[nodiscard]] i32 get_ring_buffer_write_available() const
  {
    return bufferSize - get_ring_buffer_read_available();
  }

  [[nodiscard]] f32 get_fill_state_in_percent() const
  {
    return 100.0f * (f32)get_ring_buffer_read_available() / (f32)bufferSize;
  }

  [[nodiscard]] SFillState get_fill_state() const
  {
    SFillState fs;
    fs.Filled = get_ring_buffer_read_available();
    fs.Free = bufferSize - fs.Filled;
    fs.Total = bufferSize;
    fs.Percent = 100.0f * fs.Filled / (f32)bufferSize;
    return fs;
  }

  void flush_ring_buffer()
  {
    writeIndex = 0;
    readIndex = 0;
  }

  /* ensure that previous writes are seen before we update the write index
     (write after write)
   */
  i32 advance_ring_buffer_write_index(i32 elementCount)
  {
    PaUtil_WriteMemoryBarrier();
    return writeIndex = (writeIndex + elementCount) & bigMask;
  }

  /* ensure that previous reads (copies out of the ring buffer) are
   * always completed before updating (writing) the read index.
   * (write-after-read) => full barrier
   */
  i32 advance_ring_buffer_read_index(i32 elementCount)
  {
    PaUtil_FullMemoryBarrier();
    return readIndex = (readIndex + elementCount) & bigMask;
  }

  i32 put_data_into_ring_buffer(const void * data, i32 elementCount)
  {
    i32 size1;
    i32 size2;
    void * data1;
    void * data2;

    const i32 numWritten = _get_ring_buffer_write_regions(elementCount, &data1, &size1, &data2, &size2);

    if (size2 > 0)
    {
      memcpy(data1, data, size1 * sizeof(TElem));
      data = (const char *)data + size1 * sizeof(TElem);
      memcpy(data2, data, size2 * sizeof(TElem));
    }
    else
    {
      memcpy(data1, data, size1 * sizeof(TElem));
    }

    advance_ring_buffer_write_index(numWritten);
    return numWritten;
  }

  i32 get_data_from_ring_buffer(void * data, i32 elementCount)
  {
    i32 size1;
    i32 size2;
    void * data1;
    void * data2;

    const i32 numRead = _get_ring_buffer_read_regions(elementCount, &data1, &size1, &data2, &size2);

    if (size2 > 0)
    {
      memcpy(data, data1, size1 * sizeof(TElem));
      data = ((char *)data) + size1 * sizeof(TElem);
      memcpy(data, data2, size2 * sizeof(TElem));
    }
    else
    {
      memcpy(data, data1, size1 * sizeof(TElem));
    }

    advance_ring_buffer_read_index(numRead);
    return numRead;
  }

  u32 skip_data_in_ring_buffer(u32 n_values)
  {
    //	ensure that we have the correct read and write indices
    PaUtil_FullMemoryBarrier();
    if ((i32)n_values > get_ring_buffer_read_available())
    {
      n_values = get_ring_buffer_read_available();
    }
    advance_ring_buffer_read_index(n_values);
    return n_values;
  }

  i32 get_writable_ring_buffer_segment(u32 iElemCnt, void ** const opData, i32 * opSize)
  {
    i32 dummySize;
    void * dummyData;
    return _get_ring_buffer_write_regions(iElemCnt, opData, opSize, &dummyData, &dummySize);
  }

private:
  u32 bufferSize;
  std::atomic<u32> writeIndex{ 0 };
  std::atomic<u32> readIndex{ 0 };
  u32 bigMask;
  u32 smallMask;
  std::vector<char> buffer;

  /***************************************************************************
  ** Get address of region(s) to which we can write data.
  ** If the region is contiguous, size2 will be zero.
  ** If non-contiguous, size2 will be the size of second region.
  ** Returns room available to be written or elementCount, whichever is smaller.
  */
  i32 _get_ring_buffer_write_regions(u32 elementCount, void ** const dataPtr1, i32 * sizePtr1, void ** dataPtr2, i32 * sizePtr2)
  {
    const u32 available = get_ring_buffer_write_available();

    if (elementCount > available)
    {
      elementCount = available;
    }

    /* Check to see if write is not contiguous. */

    if (const u32 index = writeIndex & smallMask;
        index + elementCount > bufferSize)
    {
      /* Write data in two blocks that wrap the buffer. */
      i32 firstHalf = bufferSize - index;
      *dataPtr1 = &buffer[index * sizeof(TElem)];
      *sizePtr1 = firstHalf;
      *dataPtr2 = &buffer[0];
      *sizePtr2 = (i32)elementCount - firstHalf;
    }
    else
    {    // fits
      *dataPtr1 = &buffer[index * sizeof(TElem)];
      *sizePtr1 = (i32)elementCount;
      *dataPtr2 = nullptr;
      *sizePtr2 = 0;
    }

    if (available > 0)
    {
      PaUtil_FullMemoryBarrier(); /* (write-after-read) => full barrier */
    }

    return (i32)elementCount;
  }

  /***************************************************************************
   ** Get address of region(s) from which we can read data.
   ** If the region is contiguous, size2 will be zero.
   ** If non-contiguous, size2 will be the size of second region.
   ** Returns room available to be read or elementCount, whichever is smaller.
   */
  i32 _get_ring_buffer_read_regions(u32 elementCount, void ** dataPtr1, i32 * sizePtr1, void ** dataPtr2, i32 * sizePtr2)
  {
    const u32 available = get_ring_buffer_read_available(); /* doesn't use memory barrier */

    if (elementCount > available)
    {
      elementCount = available;
    }

    /* Check to see if read is not contiguous. */

    if (const u32 index = readIndex & smallMask;
        index + elementCount > bufferSize)
    {
      /* Write data in two blocks that wrap the buffer. */
      i32 firstHalf = bufferSize - index;
      *dataPtr1 = &buffer[index * sizeof(TElem)];
      *sizePtr1 = firstHalf;
      *dataPtr2 = &buffer[0];
      *sizePtr2 = (i32)elementCount - firstHalf;
    }
    else
    {
      *dataPtr1 = &buffer[index * sizeof(TElem)];
      *sizePtr1 = (i32)elementCount;
      *dataPtr2 = nullptr;
      *sizePtr2 = 0;
    }

    if (available > 0)
    {
      PaUtil_ReadMemoryBarrier(); /* (read-after-read) => read barrier */
    }

    return (i32)elementCount;
  }

  [[nodiscard]] u32 _round_up_to_next_power_of_2(u32 iVal) const
  {
    // iVal with exact power of 2 keeps the size
    if (iVal == 0)
    {
      return 1;
    }
    --iVal;

    for (u32 i = 1; i < sizeof(u32) * 8; i <<= 1)
    {
      iVal |= iVal >> i;
    }

    ++iVal;
    assert(((iVal - 1) & iVal) == 0);
    return iVal;
  }
};


class RingBufferFactoryBase
{
public:
  RingBufferFactoryBase() = default;
  virtual ~RingBufferFactoryBase() = default;

  enum class EId
  {
    SpectrumBuffer,
    IqBuffer,
    CarrBuffer,
    ResponseBuffer,
    FrameBuffer,
    DataBuffer,
    AudioFromDecoder,
    AudioToOutput,
    TechDataBuffer,
    DeviceSampleBuffer,
    CirBuffer
  };

  struct SListPar
  {
    u32 MinVal;
    u32 MaxVal;
    bool ShowStatistics;
  };

protected:
  static constexpr i32 cBarWidth = 50;
  mutable std::array<char, cBarWidth + 3> mProgressBarBuffer; // + [...] plus zero

  const char * _show_progress_bar(f32 iPercentStop, f32 iPercentStart = -100) const;
  void _print_line(bool iResetMinMax) const;
  void _calculate_ring_buffer_statistics(f32 & oMinPrc, f32 & oMaxPrc, f32 & ioGlobMinPrc, f32 & ioGlobMaxPrc, RingBufferFactoryBase::SListPar & ioPar, bool iResetMinMax, const RingbufferBase::SFillState & iFillState) const;
};


template <typename T>
class RingBufferFactory : public RingBufferFactoryBase
{
public:
  RingBufferFactory();
  ~RingBufferFactory() override = default;

  struct SList
  {
    EId Id;
    const char * pName;
    std::shared_ptr<RingBuffer<T>> pRingBuffer;
    mutable SListPar Par;
  };

  void create_ringbuffer(EId iId, const char * const iName, u32 iElementCount, bool iShowStatistics = false)
  {
    assert (mMap.count(iId) == 0);
    SList list;
    list.Id = iId;
    list.pName = iName;
    list.pRingBuffer = std::make_shared<RingBuffer<T>>(iElementCount);
    list.Par.MinVal = iElementCount;
    list.Par.MaxVal = 0;
    list.Par.ShowStatistics = iShowStatistics;
    mMap[iId] = list;
  }

  std::shared_ptr<RingBuffer<T>> get_ringbuffer(EId iId)
  {
    const auto it = mMap.find(iId);
    assert (it != mMap.end());
    return it->second.pRingBuffer;
  }

  void print_status(bool iResetMinMax = false) const;

private:
  std::map<EId, SList> mMap;
};

extern RingBufferFactory<u8> sRingBufferFactoryUInt8;
extern RingBufferFactory<i16> sRingBufferFactoryInt16;
extern RingBufferFactory<ci16> sRingBufferFactoryCmplx16;
extern RingBufferFactory<f32>   sRingBufferFactoryFloat;
extern RingBufferFactory<cf32>   sRingBufferFactoryCmplx;

#endif

