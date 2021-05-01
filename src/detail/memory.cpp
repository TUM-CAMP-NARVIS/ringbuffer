/* **********************************************************************************
#                                                                                   #
# Copyright (c) 2019,                                                               #
# Research group CAMP                                                               #
# Technical University of Munich                                                    #
#                                                                                   #
# All rights reserved.                                                              #
# Ulrich Eck - ulrich.eck@tum.de                                                    #
#                                                                                   #
# Redistribution and use in source and binary forms, with or without                #
# modification, are restricted to the following conditions:                         #
#                                                                                   #
#  * The software is permitted to be used internally only by the research group     #
#    CAMP and any associated/collaborating groups and/or individuals.               #
#  * The software is provided for your internal use only and you may                #
#    not sell, rent, lease or sublicense the software to any other entity           #
#    without specific prior written permission.                                     #
#    You acknowledge that the software in source form remains a confidential        #
#    trade secret of the research group CAMP and therefore you agree not to         #
#    attempt to reverse-engineer, decompile, disassemble, or otherwise develop      #
#    source code for the software or knowingly allow others to do so.               #
#  * Redistributions of source code must retain the above copyright notice,         #
#    this list of conditions and the following disclaimer.                          #
#  * Redistributions in binary form must reproduce the above copyright notice,      #
#    this list of conditions and the following disclaimer in the documentation      #
#    and/or other materials provided with the distribution.                         #
#  * Neither the name of the research group CAMP nor the names of its               #
#    contributors may be used to endorse or promote products derived from this      #
#    software without specific prior written permission.                            #
#                                                                                   #
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND   #
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED     #
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE            #
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR   #
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES    #
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;      #
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND       #
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT        #
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS     #
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                      #
#                                                                                   #
*************************************************************************************/

#include "ringbuffer/detail/memory.h"
#include "ringbuffer/detail/cuda.h"
#include "ringbuffer/detail/trace.h"

#include <cstdlib> // For posix_memalign
#include <cstring> // For memcpy

namespace ringbuffer {
    namespace memory {

        RBStatus getSpace(const void* ptr, RBSpace* space) {
            RB_ASSERT(ptr, RBStatus::STATUS_INVALID_POINTER);
#ifndef RINGBUFFER_WITH_CUDA
            *space = RBSpace::SPACE_SYSTEM;
#else
            cudaPointerAttributes ptr_attrs;
            cudaError_t ret = cudaPointerGetAttributes(&ptr_attrs, ptr);
            RB_ASSERT(ret == cudaSuccess || ret == cudaErrorInvalidValue,
                      RBStatus::STATUS_DEVICE_ERROR);
            if( ret == cudaErrorInvalidValue ) {
                // @todo: Is there a better way to find out how a pointer was allocated?
                //         Alternatively, is there a way to prevent this from showing
                //           up in cuda-memcheck?
                // Note: cudaPointerGetAttributes only works for memory allocated with
                //         CUDA API functions, so if it fails we just assume sysmem.
                //         which is not an appropriate method if we want to support shared memory as well...
                *space = RBSpace::SPACE_SYSTEM;
                // WAR to avoid the ignored failure showing up later
                cudaGetLastError();
            } else {
                switch( ptr_attrs.type ) {
                    case cudaMemoryTypeManaged:
                        *space = RBSpace::SPACE_CUDA_MANAGED;
                        break;
                    case cudaMemoryTypeHost:
                        *space = RBSpace::SPACE_CUDA_HOST;
                        break;
                    case cudaMemoryTypeDevice:
                        *space = RBSpace::SPACE_CUDA;
                        break;
#if CUDART_VERSION >= 1100
                    case cudaMemoryTypeUnregistered:
                        *space = RBSpace::SPACE_SYSTEM;
                        break;
#endif
                    default: {
                        // This should never be reached - unless someone uses SHM Space which is not yet implemented ...
                        RB_FAIL("Valid memoryType", RBStatus::STATUS_INTERNAL_ERROR);
                    }
                }
            }
#endif
            return RBStatus::STATUS_SUCCESS;
        }

        RBStatus malloc_(void** ptr, std::size_t size, RBSpace space) {
            //spdlog::trace("malloc_({0}, {1}, {2}", ptr, size, space);
            void* data;
            switch( space ) {
                case RBSpace::SPACE_SYSTEM: {
#ifdef RINGBUFFER_TARGET_WINDOWS
					//data = std::aligned_alloc(std::max(RINGBUFFER_ALIGNMENT,8), size);
					data = _aligned_malloc(size, std::max(RINGBUFFER_ALIGNMENT, 8));
#else
                    int err = ::posix_memalign((void**)&data, std::max(RINGBUFFER_ALIGNMENT,8), size);
                    RB_ASSERT(!err, RBStatus::STATUS_MEM_ALLOC_FAILED);
#endif
					break;
                }
#ifdef RINGBUFFER_WITH_CUDA
                case RBSpace::SPACE_CUDA: {
                    RB_CHECK_CUDA(cudaMalloc((void**)&data, size),
                                  RBStatus::STATUS_MEM_ALLOC_FAILED);
                    break;
                }
                case RBSpace::SPACE_CUDA_HOST: {
                    unsigned flags = cudaHostAllocDefault;
                    RB_CHECK_CUDA(cudaHostAlloc((void**)&data, size, flags),
                                  RBStatus::STATUS_MEM_ALLOC_FAILED);
                    break;
                }
                case RBSpace::SPACE_CUDA_MANAGED: {
                    unsigned flags = cudaMemAttachGlobal;
                    RB_CHECK_CUDA(cudaMallocManaged((void**)&data, size, flags),
                                  RBStatus::STATUS_MEM_ALLOC_FAILED);
                    break;
                }
#endif
                default: RB_FAIL("Valid malloc_() space", RBStatus::STATUS_INVALID_SPACE);
            }
            //return data;
            *ptr = data;
            return RBStatus::STATUS_SUCCESS;
        }

        RBStatus free_(void* ptr, RBSpace space) {
            RB_ASSERT(ptr, RBStatus::STATUS_INVALID_POINTER);
            if( space == RBSpace::SPACE_AUTO ) {
                memory::getSpace(ptr, &space);
            }
            switch( space ) {
                case RBSpace::SPACE_SYSTEM:

					// Windows: _aligned_free
#ifdef RINGBUFFER_TARGET_WINDOWS
					_aligned_free(ptr);
#else
					::free(ptr);
#endif
                    break;
#ifdef RINGBUFFER_WITH_CUDA
                case RBSpace::SPACE_CUDA:
                    cudaFree(ptr);
                    break;
                case RBSpace::SPACE_CUDA_HOST:
                    cudaFreeHost(ptr);
                    break;
                case RBSpace::SPACE_CUDA_MANAGED:
                    cudaFree(ptr);
                    break;
#endif
                default: RB_FAIL("Valid free_() space", RBStatus::STATUS_INVALID_ARGUMENT);
            }
            return RBStatus::STATUS_SUCCESS;
        }

        RBStatus memcpy_(void*       dst,
                         RBSpace     dst_space,
                         const void* src,
                         RBSpace     src_space,
                         std::size_t count) {
            if( count ) {
                RB_ASSERT(dst, RBStatus::STATUS_INVALID_POINTER);
                RB_ASSERT(src, RBStatus::STATUS_INVALID_POINTER);
#ifndef RINGBUFFER_WITH_CUDA
                ::memcpy(dst, src, count);
#else
                // Note: Explicitly dispatching to ::memcpy was found to be much faster
                //         than using cudaMemcpyDefault.
                if( src_space == RBSpace::SPACE_AUTO ) memory::getSpace(src, &src_space);
                if( dst_space == RBSpace::SPACE_AUTO ) memory::getSpace(dst, &dst_space);
                cudaMemcpyKind kind = cudaMemcpyDefault;
                switch( src_space ) {
                    case RBSpace::SPACE_CUDA_HOST: // fall-through
                    case RBSpace::SPACE_SYSTEM: {
                        switch( dst_space ) {
                            case RBSpace::SPACE_CUDA_HOST: // fall-through
                            case RBSpace::SPACE_SYSTEM:
                                ::memcpy(dst, src, count);
                                return RBStatus::STATUS_SUCCESS;
                            case RBSpace::SPACE_CUDA:
                                kind = cudaMemcpyHostToDevice;
                                break;

                                // @todo: RBSpace::SPACE_CUDA_MANAGED
                                // @todo: RBSpace::SPACE_SHM
                            default: RB_FAIL("Valid memcpy_ dst space", RBStatus::STATUS_INVALID_ARGUMENT);
                        }
                        break;
                    }
                    case RBSpace::SPACE_CUDA: {
                        switch( dst_space ) {
                            case RBSpace::SPACE_CUDA_HOST: // fall-through
                            case RBSpace::SPACE_SYSTEM:
                                kind = cudaMemcpyDeviceToHost;
                                break;
                            case RBSpace::SPACE_CUDA:
                                kind = cudaMemcpyDeviceToDevice;
                                break;
                                // @todo: RBSpace::SPACE_CUDA_MANAGED
                                // @todo: RBSpace::SPACE_SHM
                            default: RB_FAIL("Valid memcpy_ dst space", RBStatus::STATUS_INVALID_ARGUMENT);
                        }
                        break;
                    }
                        // @todo: RBSpace::SPACE_SHM
                    default: RB_FAIL("Valid memcpy_ src space", RBStatus::STATUS_INVALID_ARGUMENT);
                }
                RB_TRACE_STREAM(cuda::g_cuda_stream);
                RB_CHECK_CUDA(cudaMemcpyAsync(dst, src, count, kind, cuda::g_cuda_stream),
                              RBStatus::STATUS_MEM_OP_FAILED);
#endif
            }
            return RBStatus::STATUS_SUCCESS;
        }

        void memcpy2D(void*       dst,
                      std::size_t dst_stride,
                      const void* src,
                      std::size_t src_stride,
                      std::size_t width,
                      std::size_t height) {
//    spdlog::trace("memcpy2D dst: {0}, dst_stride: {1}, src: {2}, src_stride: {4}, width: {5}, height: {6}",
//            dst, dst_stride, src, src_stride, width, height);
            for( std::size_t row=0; row<height; ++row ) {
                ::memcpy((char*)dst + row*dst_stride,
                         (char*)src + row*src_stride,
                         width);
            }
        }


        RBStatus memcpy2D(void*       dst,
                          std::size_t dst_stride,
                          RBSpace     dst_space,
                          const void* src,
                          std::size_t src_stride,
                          RBSpace     src_space,
                          std::size_t width,    // bytes
                          std::size_t height) { // rows
            if( width*height ) {
                RB_ASSERT(dst, RBStatus::STATUS_INVALID_POINTER);
                RB_ASSERT(src, RBStatus::STATUS_INVALID_POINTER);
#ifndef RINGBUFFER_WITH_CUDA
                memcpy2D(dst, dst_stride, src, src_stride, width, height);
#else
                // Note: Explicitly dispatching to ::memcpy was found to be much faster
                //         than using cudaMemcpyDefault.
                if( src_space == RBSpace::SPACE_AUTO ) memory::getSpace(src, &src_space);
                if( dst_space == RBSpace::SPACE_AUTO ) memory::getSpace(dst, &dst_space);
                cudaMemcpyKind kind = cudaMemcpyDefault;
                switch( src_space ) {
                    case RBSpace::SPACE_CUDA_HOST: // fall-through
                    case RBSpace::SPACE_SYSTEM: {
                        switch( dst_space ) {
                            case RBSpace::SPACE_CUDA_HOST: // fall-through
                            case RBSpace::SPACE_SYSTEM:
                                memcpy2D(dst, dst_stride, src, src_stride, width, height);
                                return RBStatus::STATUS_SUCCESS;
                            case RBSpace::SPACE_CUDA:
                                kind = cudaMemcpyHostToDevice;
                                break;
                                // @todo: Is this the right thing to do?
                            case RBSpace::SPACE_CUDA_MANAGED:
                                kind = cudaMemcpyDefault;
                                break;
                            default:
                                RB_FAIL("Valid memcpy2D dst space", RBStatus::STATUS_INVALID_ARGUMENT);
                        }
                        break;
                    }
                    case RBSpace::SPACE_CUDA: {
                        switch( dst_space ) {
                            case RBSpace::SPACE_CUDA_HOST: // fall-through
                            case RBSpace::SPACE_SYSTEM:
                                kind = cudaMemcpyDeviceToHost;
                                break;
                            case RBSpace::SPACE_CUDA:
                                kind = cudaMemcpyDeviceToDevice;
                                break;
                                // @todo: Is this the right thing to do?
                            case RBSpace::SPACE_CUDA_MANAGED:
                                kind = cudaMemcpyDefault;
                                break;
                            default:
                                RB_FAIL("Valid memcpy2D dst space", RBStatus::STATUS_INVALID_ARGUMENT);
                        }
                        break;
                    }
                    default:
                        RB_FAIL("Valid memcpy2D src space", RBStatus::STATUS_INVALID_ARGUMENT);
                }
                RB_TRACE_STREAM(cuda::g_cuda_stream);
                RB_CHECK_CUDA(cudaMemcpy2DAsync(dst, dst_stride,
                                                src, src_stride,
                                                width, height,
                                                kind, cuda::g_cuda_stream),
                              RBStatus::STATUS_MEM_OP_FAILED);
#endif
            }
            return RBStatus::STATUS_SUCCESS;
        }

        RBStatus memset_(void*   ptr,
                         RBSpace space,
                         int     value,
                         std::size_t  count) {
            RB_ASSERT(ptr, RBStatus::STATUS_INVALID_POINTER);
            if( count ) {
                if( space == RBSpace::SPACE_AUTO ) {
                    // @todo: Check status here
                    memory::getSpace(ptr, &space);
                }
                switch( space ) {
                    case RBSpace::SPACE_SYSTEM:
                        ::memset(ptr, value, count);
                        break;
#ifdef RINGBUFFER_WITH_CUDA
                    case RBSpace::SPACE_CUDA_HOST:
                        ::memset(ptr, value, count);
                        break;
                    case RBSpace::SPACE_CUDA: // Fall-through
                    case RBSpace::SPACE_CUDA_MANAGED: {
                        RB_TRACE_STREAM(cuda::g_cuda_stream);
                        RB_CHECK_CUDA(cudaMemsetAsync(ptr, value, count, cuda::g_cuda_stream),
                                      RBStatus::STATUS_MEM_OP_FAILED);
                        break;
                    }
#endif
                    default:
                        RB_FAIL("Valid rfMemset space", RBStatus::STATUS_INVALID_ARGUMENT);
                }
            }
            return RBStatus::STATUS_SUCCESS;
        }

        void memset2D(void*  ptr,
                      std::size_t stride,
                      int    value,
                      std::size_t width,
                      std::size_t height) {
            for( std::size_t row=0; row<height; ++row ) {
                ::memset((char*)ptr + row*stride, value, width);
            }
        }

        RBStatus memset2D(void*   ptr,
                          std::size_t  stride,
                          RBSpace space,
                          int     value,
                          std::size_t  width,    // bytes
                          std::size_t  height) { // rows
            RB_ASSERT(ptr, RBStatus::STATUS_INVALID_POINTER);
            if( width*height ) {
                if( space == RBSpace::SPACE_AUTO ) {
                    memory::getSpace(ptr, &space);
                }
                switch( space ) {
                    case RBSpace::SPACE_SYSTEM:
                        memset2D(ptr, stride, value, width, height);
                        break;
#ifdef RINGBUFFER_WITH_CUDA
                    case RBSpace::SPACE_CUDA_HOST:
                        memset2D(ptr, stride, value, width, height);
                        break;
                    case RBSpace::SPACE_CUDA: // Fall-through
                    case RBSpace::SPACE_CUDA_MANAGED: {
                        RB_TRACE_STREAM(cuda::g_cuda_stream);
                        RB_CHECK_CUDA(cudaMemset2DAsync(ptr, stride, value, width, height, cuda::g_cuda_stream),
                                      RBStatus::STATUS_MEM_OP_FAILED);
                        break;
                    }
#endif
                    default:
                        RB_FAIL("Valid rfMemset2D space", RBStatus::STATUS_INVALID_ARGUMENT);
                }
            }
            return RBStatus::STATUS_SUCCESS;
        }

        std::size_t getAlignment() {
            return RINGBUFFER_ALIGNMENT;
        }

    } // namespace memory
} // namespace ringbuffer

