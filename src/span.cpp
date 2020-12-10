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

#include "ringbuffer/span.h"
#include "ringbuffer/ring.h"
#include "ringbuffer/sequence.h"

namespace ringbuffer {

    void Span::set_base_size(std::size_t size) { m_size = size; }

    Span::Span(const std::shared_ptr<Ring>& ring, std::size_t size) : m_ring(ring), m_size(size) {}
    Span::~Span() {}

    std::shared_ptr<Ring> Span::ring() const { return m_ring; }
    std::size_t Span::size()     const { return m_size; }
    std::size_t Span::stride()   const { return m_ring->current_stride(); }
    std::size_t Span::nringlet() const { return m_ring->current_nringlet(); }


    WriteSpan::WriteSpan(const std::shared_ptr<Ring>& ring, std::size_t size, bool nonblocking, std::chrono::nanoseconds timeout)
            : Span(ring, size), m_begin(0), m_commit_size(size), m_data(nullptr) {
        this->ring()->reserve_span(size, &m_begin, &m_data, nonblocking, timeout);
    }

    WriteSpan* WriteSpan::commit(std::size_t size) {
        RB_ASSERT_EXCEPTION(size <= this->size(), RBStatus::STATUS_INVALID_ARGUMENT);
        m_commit_size = size;
        return this;
    }

    WriteSpan::~WriteSpan() {
        this->ring()->commit_span(m_begin, this->size(), m_commit_size);
    }

    void*       WriteSpan::data()     const { return m_data; }
    std::size_t WriteSpan::offset()   const { return m_begin; }


    ReadSpan::ReadSpan(ReadSequence*   sequence,
                       std::size_t    offset, // Relative to sequence beg
                       std::size_t    requested_size)
            : Span(sequence->ring(), requested_size),
              m_sequence(sequence), m_begin(0), m_data(nullptr) {
        std::size_t returned_size = requested_size;
        // @todo: this call potentially blocks until data can read
        this->ring()->acquire_span(sequence, offset, &returned_size, &m_begin, &m_data);
        this->set_base_size(returned_size);
    }

    ReadSpan::~ReadSpan() {
        this->ring()->release_span(m_sequence, m_begin, this->size());
    }

    std::size_t ReadSpan::size_overwritten() const {
        if( m_sequence->guarantee() ) {
            return 0;
        }
        const auto ring = this->ring();
        std::size_t tail = ring->current_tail_offset();
        return std::max(std::min(delta_type(tail - m_begin),
                                 delta_type(this->size())),
                        delta_type(0));
    }

    void*       ReadSpan::data()     const { return m_data; }
    std::size_t ReadSpan::offset()   const { return m_begin - m_sequence->begin(); }

}
