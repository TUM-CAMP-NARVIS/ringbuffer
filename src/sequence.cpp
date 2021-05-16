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

#include "ringbuffer/sequence.h"
#include "ringbuffer/ring.h"
#include "ringbuffer/detail/guarantee.h"

namespace ringbuffer {

    Sequence::Sequence(const std::shared_ptr<Ring>& ring,
                      std::string   name,
                      time_tag_type time_tag,
                      std::size_t   header_size,
                      const void*   header,
                      std::size_t   nringlet,
                      std::size_t   begin)
            : m_ring(ring), m_name(std::move(name)), m_time_tag(time_tag), m_nringlet(nringlet),
              m_begin(begin),
              m_end(RF_SEQUENCE_OPEN),
              m_header((const char*)header,
                      (const char*)header+header_size),
              m_next(nullptr), m_readrefcount(0) {
    }

	Sequence::~Sequence() = default;
	Sequence::Sequence(Sequence&&) = default;
	Sequence& Sequence::operator=(Sequence&&) = default;

    void Sequence::set_next(SequencePtr next) {
        m_next = std::move(next);
    }

    void Sequence::set_footer(std::size_t footer_size, void* footer) {
        m_footer = footer_type((const char*)footer, (const char*)footer+footer_size);
    }

	SequenceWrapper::~SequenceWrapper() = default;
	SequenceWrapper::SequenceWrapper(SequenceWrapper&&) = default;
	SequenceWrapper& SequenceWrapper::operator=(SequenceWrapper&&) = default;


    // @todo: See if can make these function bodies a bit more concise
    ReadSequence ReadSequence::earliest_or_latest(const std::shared_ptr<Ring>& ring, bool with_guarantee, bool latest, std::chrono::nanoseconds timeout) {
        std::unique_ptr<state::Guarantee> guarantee;
        SequencePtr sequence = ring->open_earliest_or_latest_sequence(with_guarantee, guarantee, latest, timeout);
        return std::move(ReadSequence(sequence, guarantee));
    }

    ReadSequence ReadSequence::by_name(const std::shared_ptr<Ring>& ring, const std::string& name, bool with_guarantee) {
        std::unique_ptr<state::Guarantee> guarantee;
        SequencePtr sequence = ring->open_sequence_by_name(name, with_guarantee, guarantee);
        return std::move(ReadSequence(sequence, guarantee));
    }

    ReadSequence ReadSequence::at(const std::shared_ptr<Ring>& ring, time_tag_type time_tag, bool with_guarantee) {
        std::unique_ptr<state::Guarantee> guarantee;
        SequencePtr sequence = ring->open_sequence_at(time_tag, with_guarantee, guarantee);
        return std::move(ReadSequence(sequence, guarantee));
    }

    std::unique_ptr<ReadSequence> ReadSequence::earliest_or_latest_ptr(const std::shared_ptr<Ring>& ring, bool with_guarantee, bool latest, std::chrono::nanoseconds timeout) {
        std::unique_ptr<state::Guarantee> guarantee;
        SequencePtr sequence = ring->open_earliest_or_latest_sequence(with_guarantee, guarantee, latest, timeout);
        return std::unique_ptr<ReadSequence>(new ReadSequence(sequence, guarantee));
    }

    std::unique_ptr<ReadSequence> ReadSequence::by_name_ptr(const std::shared_ptr<Ring>& ring, const std::string& name, bool with_guarantee) {
        std::unique_ptr<state::Guarantee> guarantee;
        SequencePtr sequence = ring->open_sequence_by_name(name, with_guarantee, guarantee);
        return std::unique_ptr<ReadSequence>(new ReadSequence(sequence, guarantee));
    }

    std::unique_ptr<ReadSequence> ReadSequence::at_ptr(const std::shared_ptr<Ring>& ring, time_tag_type time_tag, bool with_guarantee) {
        std::unique_ptr<state::Guarantee> guarantee;
        SequencePtr sequence = ring->open_sequence_at(time_tag, with_guarantee, guarantee);
        if(!sequence){
            return nullptr;
        }
        return std::unique_ptr<ReadSequence>(new ReadSequence(sequence, guarantee));
    }

    ReadSequence::ReadSequence(SequencePtr sequence, std::unique_ptr<state::Guarantee>& guarantee)
            : SequenceWrapper(sequence), m_guarantee(std::move(guarantee)) {}

	ReadSequence::~ReadSequence() = default;
	ReadSequence::ReadSequence(ReadSequence&&) = default;
	ReadSequence& ReadSequence::operator=(ReadSequence&&) = default;

    void ReadSequence::increment_to_next(std::chrono::nanoseconds timeout) {
        m_sequence->ring()->increment_sequence_to_next(m_sequence, m_guarantee, timeout);
    }


    WriteSequence::WriteSequence(const std::shared_ptr<Ring>& ring,
                                 const std::string& name,
                                 time_tag_type      time_tag,
                                 std::size_t        header_size,
                                 const void*        header,
                                 std::size_t        nringlet,
                                 std::size_t        offset_from_head)
            : SequenceWrapper(ring->begin_sequence(name, time_tag,
                                                   header_size,
                                                   header, nringlet,
                                                   offset_from_head)),
              m_end_offset_from_head(0) {}


    void WriteSequence::finish(std::size_t footer_size, void* footer) {
        this->ring()->finish_sequence(m_sequence, m_end_offset_from_head, footer_size, footer);

    }

    WriteSequence::~WriteSequence() {
        if (!m_sequence->is_finished()) {
            finish();
        }
    }

	void WriteSequence::set_end_offset_from_head(std::size_t end_offset_from_head) {
        m_end_offset_from_head = end_offset_from_head;
    }

}