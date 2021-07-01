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


#ifndef RINGBUFFER_SEQUENCE_H
#define RINGBUFFER_SEQUENCE_H

#pragma warning( disable : 4251 ) // needs to have dll-interface to be used by clients of class

#include "ringbuffer/common.h"
#include "ringbuffer/visibility.h"
#include "ringbuffer/types.h"

namespace ringbuffer {



    class RINGBUFFER_EXPORT Sequence {
        friend class Ring;
        enum { RF_SEQUENCE_OPEN = (std::size_t)-1 };
        std::weak_ptr<Ring> m_ring;
        std::string    m_name;
        time_tag_type  m_time_tag;
        std::size_t    m_nringlet;
        std::size_t    m_begin;
        std::size_t    m_end;
        header_type    m_header;
        footer_type    m_footer;
        SequencePtr    m_next;
        std::size_t    m_readrefcount; // ever used ??

    public:
        Sequence(const std::weak_ptr<Ring>&  ring,
                 std::string   name,
                 time_tag_type time_tag,
                 std::size_t   header_size,
                 const void*   header,
                 std::size_t   nringlet,
                 std::size_t   begin);

		~Sequence();

		Sequence(Sequence const&) = delete;
		Sequence& operator=(Sequence const&) = delete;
		Sequence(Sequence&&);
		Sequence& operator=(Sequence&&);

        void set_footer(std::size_t footer_size, void* footer);

        void set_next(SequencePtr next);

        inline bool          is_finished() const { return m_end != RF_SEQUENCE_OPEN; }
        inline std::weak_ptr<Ring> ring() { return m_ring;}
        inline std::string   name()        const { return m_name; }
        inline time_tag_type time_tag()    const { return m_time_tag; }
        inline const void*   header()      const { return m_header.size() ? &m_header[0] : nullptr; }
        inline std::size_t   header_size() const { return m_header.size(); }
        inline const void*   footer()      const { return m_footer.size() ? &m_footer[0] : nullptr; }
        inline std::size_t   footer_size() const { return m_footer.size(); }
        inline std::size_t   nringlet()    const { return m_nringlet; }
        inline std::size_t   begin()       const { return m_begin; }
        inline std::size_t   end()         const { return m_end; }
    };
    

    class RINGBUFFER_EXPORT SequenceWrapper {
    protected:
        SequencePtr m_sequence;
    public:
        inline explicit SequenceWrapper(SequencePtr sequence) : m_sequence(sequence) {}
		~SequenceWrapper();

		SequenceWrapper(SequenceWrapper const&) = delete;
		SequenceWrapper& operator=(SequenceWrapper const&) = delete;
		SequenceWrapper(SequenceWrapper&&);
		SequenceWrapper& operator=(SequenceWrapper&&);

		inline SequencePtr   sequence()    const { return m_sequence; }
        inline bool          is_finished() const { return m_sequence->is_finished(); }
        inline std::weak_ptr<Ring> ring() { return m_sequence->ring(); }
        inline std::string   name()        const { return m_sequence->name(); }
        inline time_tag_type time_tag()    const { return m_sequence->time_tag(); }
        inline const void*   header()      const { return m_sequence->header(); }
        inline std::size_t   header_size() const { return m_sequence->header_size(); }
        inline const void*   footer()      const { return m_sequence->footer(); }
        inline std::size_t   footer_size() const { return m_sequence->footer_size(); }
        inline std::size_t   nringlet()    const { return m_sequence->nringlet(); }
        inline std::size_t   begin()       const { return m_sequence->begin(); }
    };


    class RINGBUFFER_EXPORT ReadSequence : public SequenceWrapper {
        std::unique_ptr<state::Guarantee> m_guarantee;
    public:
        // @todo: See if can make these function bodies a bit more concise
        static ReadSequence earliest_or_latest(const std::shared_ptr<Ring>& ring, bool with_guarantee, bool latest, std::chrono::nanoseconds timeout=std::chrono::nanoseconds(0));
        static ReadSequence by_name(const std::shared_ptr<Ring>& ring, const std::string& name, bool with_guarantee);
        static ReadSequence at(const std::shared_ptr<Ring>& ring, time_tag_type time_tag, bool with_guarantee);
        static std::unique_ptr<ReadSequence> earliest_or_latest_ptr(const std::shared_ptr<Ring>& ring, bool with_guarantee, bool latest, std::chrono::nanoseconds timeout=std::chrono::nanoseconds(0));
        static std::unique_ptr<ReadSequence> by_name_ptr(const std::shared_ptr<Ring>& ring, const std::string& name, bool with_guarantee);
        static std::unique_ptr<ReadSequence> at_ptr(const std::shared_ptr<Ring>& ring, time_tag_type time_tag, bool with_guarantee);
        ReadSequence(SequencePtr sequence, std::unique_ptr<state::Guarantee>& guarantee);
		~ReadSequence();

		ReadSequence(ReadSequence const&) = delete;
		ReadSequence& operator=(ReadSequence const&) = delete;
		ReadSequence(ReadSequence&&);
		ReadSequence& operator=(ReadSequence&&);

        void increment_to_next(std::chrono::nanoseconds timeout=std::chrono::nanoseconds(0));

        inline std::unique_ptr<state::Guarantee>&       guarantee()       { return m_guarantee; }
        inline std::unique_ptr<state::Guarantee> const& guarantee() const { return m_guarantee; }
    };


    class RINGBUFFER_EXPORT WriteSequence : public SequenceWrapper {
        std::size_t m_end_offset_from_head;
    public:
        WriteSequence(WriteSequence const& )            = delete;
        WriteSequence& operator=(WriteSequence const& ) = delete;
        WriteSequence(WriteSequence&& )                 = delete;
        WriteSequence& operator=(WriteSequence&& )      = delete;

        WriteSequence(const std::weak_ptr<Ring>& ring, const std::string& name,
                      time_tag_type time_tag, std::size_t header_size,
                      const void* header, std::size_t nringlet,
                      std::size_t offset_from_head=0);

        ~WriteSequence();

        void finish(std::size_t footer_size=0, void* footer=nullptr);

        void set_end_offset_from_head(std::size_t end_offset_from_head);
    };
    
}


#endif //RINGBUFFER_SEQUENCE_H
