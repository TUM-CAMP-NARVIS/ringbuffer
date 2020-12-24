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

#ifndef RINGBUFFER_RING_H
#define RINGBUFFER_RING_H

#pragma warning( disable : 4251 ) // needs to have dll-interface to be used by clients of class

#include "ringbuffer/common.h"
#include "ringbuffer/visibility.h"
#include "ringbuffer/types.h"
#include "ringbuffer/detail/ring_state.h"
#include "ringbuffer/detail/signal.h"

#include <spdlog/logger.h>
#include <spdlog/spdlog.h>
#include <memory>
#include <chrono>

namespace ringbuffer {

    void RINGBUFFER_EXPORT setup_logger(std::shared_ptr<spdlog::logger> logger);

    class RINGBUFFER_EXPORT Ring : public std::enable_shared_from_this<Ring> {
        friend class ReadSequence;
        friend class WriteSequence;
        friend class state::RingReallocLock;
        friend class state::Guarantee;

        std::unique_ptr<state::RingState> m_state;

        std::size_t _buf_offset( std::size_t offset) const;
        pointer  _buf_pointer(std::size_t offset) const;

        void _ghost_write(std::size_t offset, std::size_t size);
        void _ghost_read( std::size_t offset, std::size_t size);

        void _copy_to_ghost(  std::size_t buf_offset, std::size_t span);
        void _copy_from_ghost(std::size_t buf_offset, std::size_t span);

        RBStatus _advance_reserve_head(state::unique_lock_type& lock, std::size_t size, bool nonblocking, std::chrono::nanoseconds timeout=std::chrono::nanoseconds(0));

        void _add_guarantee(std::size_t offset);
        void _remove_guarantee(std::size_t offset);
        std::size_t _get_earliest_guarantee();

        bool _sequence_still_within_ring(SequencePtr sequence) const;
        std::size_t _get_start_of_sequence_within_ring(SequencePtr sequence) const;
        SequencePtr _get_earliest_or_latest_sequence(state::unique_lock_type& lock, bool latest, std::chrono::nanoseconds timeout) const;

        SequencePtr open_earliest_or_latest_sequence(bool with_guarantee,
                                                     std::unique_ptr<state::Guarantee>& guarantee,
                                                     bool latest, std::chrono::nanoseconds timeout=std::chrono::nanoseconds(0));
        SequencePtr _get_next_sequence(SequencePtr sequence,
                                       state::unique_lock_type& lock, std::chrono::nanoseconds timeout) const;

        void increment_sequence_to_next(SequencePtr& sequence,
                                        std::unique_ptr<state::Guarantee>& guarantee, std::chrono::nanoseconds timeout=std::chrono::nanoseconds(0));
        SequencePtr _get_sequence_by_name(const std::string& name);

        SequencePtr open_sequence_by_name(const std::string& name,
                                          bool with_guarantee,
                                          std::unique_ptr<state::Guarantee>& guarantee);
        SequencePtr _get_sequence_at(time_tag_type time_tag);

        SequencePtr open_sequence_at(time_tag_type time_tag,
                                         bool with_guarantee,
                                         std::unique_ptr<state::Guarantee>& guarantee);
        void finish_sequence(SequencePtr sequence, std::size_t offset_from_head, std::size_t footer_size=0, void* footer=nullptr);


        state::RingState& get_state();
        const state::RingState& get_state() const;


        // private constructor to avoid creation of non-shared_ptr instances
        Ring(std::string name, RBSpace space);

        detail::Signal<time_tag_type> m_sequence_event;

    public:

        // No copy or move
        Ring(Ring const& )            = delete;
        Ring& operator=(Ring const& ) = delete;
        Ring(Ring&& )                 = delete;
        Ring& operator=(Ring&& )      = delete;

        // constructor/destructor
        static std::shared_ptr<Ring> create(std::string name, RBSpace space) {
            return std::shared_ptr<Ring>( new Ring(name, space) );
        }
        ~Ring();

        // public interface
        void resize(std::size_t max_contiguous_span,
                    std::size_t max_total_size,
                    std::size_t max_ringlets);
        
        inline std::string name() const { return m_state->name; }
        inline RBSpace     space()    const { return m_state->space; }
        inline void        set_core(int core)  { m_state->core = core; }
        inline int         core()    const { return m_state->core; }
        inline void        set_device(int device)  { m_state->device = device; }
        inline int         device()    const { return m_state->device; }
        inline void        lock()   { m_state->mutex.lock(); }
        inline void        unlock() { m_state->mutex.unlock(); }
        inline void*       locked_data()            const { return m_state->buf; }
        inline std::size_t locked_contiguous_span() const { return m_state->ghost_span; }
        inline std::size_t locked_total_span()      const { return m_state->span; }
        inline std::size_t locked_nringlet()        const { return m_state->nringlet; }
        inline std::size_t locked_stride()          const { return m_state->stride; }

        void begin_writing();
        void end_writing();

        inline bool writing_ended() { return m_state->writing_ended; }

        inline std::size_t current_tail_offset() const {
            const auto& state = get_state();
            state::lock_guard_type lock(state.mutex);
            return state.tail;
        }
        
        inline std::size_t current_stride() const {
            const auto& state = get_state();
            state::lock_guard_type lock(state.mutex);
            return state.stride;
        }
        
        inline std::size_t current_nringlet() const {
            const auto& state = get_state();
            state::lock_guard_type lock(state.mutex);
            return state.nringlet;
        }

        std::vector<uint64_t> list_time_tags();

        SequencePtr begin_sequence(std::string   name,
                                   time_tag_type time_tag,
                                   std::size_t   header_size,
                                   const void*   header,
                                   std::size_t   nringlet,
                                   std::size_t   offset_from_head=0);

        void reserve_span(std::size_t size, std::size_t* begin, void** data, bool nonblocking, std::chrono::nanoseconds timeout=std::chrono::nanoseconds(0));
        void commit_span(std::size_t begin, std::size_t reserve_size, std::size_t commit_size);

        void acquire_span(ReadSequence* sequence,
                          std::size_t  offset,
                          std::size_t* size,
                          std::size_t* begin,
                          void**      data,
                          std::chrono::nanoseconds timeout=std::chrono::nanoseconds(0));
        void release_span(ReadSequence* sequence,
                          std::size_t  begin,
                          std::size_t  size);



		int subscribe_sequence_event(void(*callback)(time_tag_type, void*), void* const userData=nullptr);

		template<class Callback, class T> int subscribe_sequence_event(Callback callback, T& userData);

		void unsubscribe_sequence_event(int connection_id);
    };

	template<class Callback, class T> int Ring::subscribe_sequence_event(Callback callback, T& userData) {
		/* Don't try to wrap a null function pointer. Need to cast first because
		   MSVC (even 2017) can't apply ! to a lambda. Ugh. */
		const auto callbackPtr = static_cast<void(*)(time_tag_type, void*)>(callback);
		if (!callbackPtr) return -1;

		return subscribe_sequence_event(callbackPtr, static_cast<void* const>(&userData));
	}

}

#endif //RINGBUFFER_RING_H
