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

#ifndef RINGBUFFER_SPAN_H
#define RINGBUFFER_SPAN_H

#pragma warning( disable : 4251 ) // needs to have dll-interface to be used by clients of class

#include "ringbuffer/common.h"
#include "ringbuffer/visibility.h"
#include "ringbuffer/types.h"

#include <chrono>

namespace ringbuffer {

    class RINGBUFFER_EXPORT Span {
        std::shared_ptr<Ring> m_ring;
        std::size_t  m_size;

    protected:
        // WAR for awkwardness in subclass constructors
        void set_base_size(std::size_t size);

    public:
        // No copy or move
        Span(Span const& )            = delete;
        Span& operator=(Span const& ) = delete;
        Span(Span&& )                 = delete;
        Span& operator=(Span&& )      = delete;

        Span(const std::shared_ptr<Ring>& ring, std::size_t size);
        virtual ~Span();

        std::shared_ptr<Ring> ring() const;
        std::size_t     size() const;
        // Note: These two are only safe to read while a span is open (preventing resize)
        std::size_t     stride() const;
        std::size_t     nringlet() const;

        virtual void*          data()     const = 0;
        virtual std::size_t    offset()   const = 0;
    };
    
    
    class RINGBUFFER_EXPORT WriteSpan : public Span {
        std::size_t     m_begin;
        std::size_t     m_commit_size;
        void*           m_data;
    public:
        // No copy or move
        WriteSpan(WriteSpan const& )            = delete;
        WriteSpan& operator=(WriteSpan const& ) = delete;
        WriteSpan(WriteSpan&& )                 = delete;
        WriteSpan& operator=(WriteSpan&& )      = delete;

        WriteSpan(const std::shared_ptr<Ring>& ring, std::size_t size, bool nonblocking, std::chrono::nanoseconds timeout=std::chrono::nanoseconds(0));
        ~WriteSpan() override;

        WriteSpan* commit(std::size_t size);

        void*           data() const override;
        // Note: This is the offset relative to the beginning of the ring,
        //         as wspans aren't firmly associated with a sequence.
        // @todo: This is likely to be confusing compared to ReadSpan::offset
        //         Can't easily change the name though because it's a shared API
        std::size_t     offset() const override;
    };
    
    
    class RINGBUFFER_EXPORT ReadSpan : public Span {
        ReadSequence*   m_sequence;
        std::size_t     m_begin;
        void*           m_data;

    public:
        // No copy or move
        ReadSpan(ReadSpan const& )            = delete;
        ReadSpan& operator=(ReadSpan const& ) = delete;
        ReadSpan(ReadSpan&& )                 = delete;
        ReadSpan& operator=(ReadSpan&& )      = delete;

        ReadSpan(ReadSequence* sequence,
                 std::size_t  offset,
                 std::size_t  size);
        ~ReadSpan();

        std::size_t size_overwritten() const;

        void*       data()     const override;
        // Note: This is the offset relative to the beginning of the sequence
        std::size_t offset()   const override;
    };
    
    

}

#endif //RINGBUFFER_SPAN_H
