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

#ifndef RINGFLOW_MEMORY_H
#define RINGFLOW_MEMORY_H

#include "ringbuffer/common.h"
#include "ringbuffer/visibility.h"

#ifndef RINGBUFFER_ALIGNMENT
    // @todo: what is a good size here ? originally in bifrost it is set to 4096
    #define RINGBUFFER_ALIGNMENT 512
#endif


namespace ringbuffer {
    namespace memory {

        /*
         * allocate memory of size in given space
         */
        RBStatus RINGBUFFER_EXPORT malloc_(void** ptr, std::size_t size, RBSpace space);

        /*
         * free allocated memory from given space
         */
        RBStatus RINGBUFFER_EXPORT free_(void* ptr, RBSpace space);

        /*
         * get space associated with given address
         */
        RBStatus RINGBUFFER_EXPORT getSpace(const void* ptr, RBSpace* space);

        /*
         * copy memory from one space to another
         * Note: This is sync wrt host but async wrt device
         */
        RBStatus RINGBUFFER_EXPORT memcpy_(void*       dst,
                         RBSpace     dst_space,
                         const void* src,
                         RBSpace     src_space,
                         std::size_t count);

        /*
         * copy 2D array from one space to another
         */
        RBStatus RINGBUFFER_EXPORT memcpy2D(void*       dst,
                          std::size_t dst_stride,
                          RBSpace     dst_space,
                          const void* src,
                          std::size_t src_stride,
                          RBSpace     src_space,
                          std::size_t width,
                          std::size_t height);

        /*
         * set memory to given value
         * Note: only works for byte types
         */
        RBStatus RINGBUFFER_EXPORT memset_(void*        ptr,
                         RBSpace      space,
                         int          value,
                         std::size_t  count);

        /*
         * set memory of 2D array to given value
         * Note: only works for byte types
         */
        RBStatus RINGBUFFER_EXPORT memset2D(void*       ptr,
                          std::size_t stride,
                          RBSpace     space,
                          int         value,
                          std::size_t width,
                          std::size_t height);

        /*
         * get global alignment
         */
        std::size_t RINGBUFFER_EXPORT getAlignment();

    }
}

#endif //RINGFLOW_MEMORY_H
