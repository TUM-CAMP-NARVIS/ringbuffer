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


#include "ringbuffer/ring.h"
#include "ringbuffer/sequence.h"
#include "ringbuffer/detail/memory.h"
#include "ringbuffer/detail/ring_realloc_lock.h"
#include "ringbuffer/detail/guarantee.h"
#include "ringbuffer/detail/cuda.h"
#include "ringbuffer/detail/util.h"
#include <mutex>

#ifdef RINGBUFFER_WITH_NUMA
#include <numa.h>
#endif


namespace ringbuffer {

    void setup_logger(std::shared_ptr<spdlog::logger> logger) {
        spdlog::set_default_logger(logger);
    }

    void Ring::_add_guarantee(std::size_t offset) {
        auto& state = get_state();
        std::scoped_lock<state::mutex_type> lk(state.guarantees_mutex);
        auto iter = state.guarantees.find(offset);
        if( iter == state.guarantees.end() ) {
            state.guarantees.insert(std::make_pair(offset, 1));
        }
        else {
            ++iter->second;
        }
    }

    void Ring::_remove_guarantee(std::size_t offset) {
        auto& state = get_state();
        std::scoped_lock<state::mutex_type> lk(state.guarantees_mutex);
        auto iter = state.guarantees.find(offset);
        if( iter == state.guarantees.end() ) {
            throw RBException(RBStatus::STATUS_INTERNAL_ERROR);
        }
        if( !--iter->second ) {
            state.guarantees.erase(iter);
            state.write_condition.notify_all();
        }
    }

    std::size_t Ring::_get_earliest_guarantee() {
        auto& state = get_state();
        return state.guarantees.begin()->first;
    }

    state::RingState& Ring::get_state() {
        RB_ASSERT_EXCEPTION(m_state, RBStatus::STATUS_INVALID_STATE);
        return *m_state;
    }

    const state::RingState& Ring::get_state() const {
        RB_ASSERT_EXCEPTION(m_state, RBStatus::STATUS_INVALID_STATE);
        return *m_state;
    }

    Ring::Ring(std::string name, RBSpace space)
            : m_state(new state::RingState()) {
        auto& state = get_state();
        // initialize state
        state.name = std::move(name);
        state.space = space;
        state.buf = nullptr;
        state.ghost_span = 0;
        state.span = 0;
        state.stride = 0;
        state.nringlet = 0;
        state.offset0 = 0;
        state.tail = 0;
        state.head = 0;
        state.reserve_head = 0;
        state.ghost_dirty_beg = state.ghost_span;
        state.writing_begun = false;
        state.writing_ended = false;
        state.eod = 0;
        state.nread_open = 0;
        state.nwrite_open = 0;
        state.nrealloc_pending = 0;
        state.core = -1;
        state.device = -1;

#ifdef RINGBUFFER_WITH_CUDA
        RB_ASSERT_EXCEPTION(space==RBSpace::SPACE_SYSTEM       ||
                                  space==RBSpace::SPACE_CUDA         ||
                                  space==RBSpace::SPACE_CUDA_HOST    ||
                                  space==RBSpace::SPACE_CUDA_MANAGED,
                            RBStatus::STATUS_INVALID_ARGUMENT);
#else
        RB_ASSERT_EXCEPTION(space==RBSpace::SPACE_SYSTEM,
	                    RBStatus::STATUS_INVALID_ARGUMENT);
#endif

        // @todo: add proclog entry here
    }

    Ring::~Ring() {
        auto& state = get_state();
        // @todo: Should check if anything is still open here?
        if( state.buf ) {
            memory::free_(state.buf, state.space);
        }
        m_sequence_event.disconnect_all();
    }
    
    void Ring::resize(std::size_t contiguous_span,
                      std::size_t total_span,
                      std::size_t nringlet) {

        auto& state = get_state();
        state::unique_lock_type lock(state.mutex);

        // Check if reallocation is actually necessary
        if( contiguous_span <= state.ghost_span &&
            total_span      <= state.span &&
            nringlet        <= state.nringlet) {
            return;
        }

        state::realloc_lock_type realloc_lock(lock, this);

        // Check if reallocation is still actually necessary
        if( contiguous_span <= state.ghost_span &&
            total_span      <= state.span &&
            nringlet        <= state.nringlet) {
            return;
        }

        // Perform the reallocation
        std::size_t  new_ghost_span = std::max(contiguous_span, state.ghost_span);
        std::size_t  new_span       = std::max(total_span,      state.span);
        std::size_t  new_nringlet   = std::max(nringlet,        state.nringlet);

        //new_ghost_span = round_up(new_ghost_span, memory::getAlignment());
        //new_span       = round_up(new_span,       memory::getAlignment());

        new_span = std::max(new_span, memory::getAlignment());

        // **@todo: See if can avoid doing this, so that ghost-region memcpys can
        //           be avoided if user chooses gulp sizes in whole multiplies
        //           regardless of whether they are powers of two or not.

        // Note: This is critical to enable safe overflowing/wrapping of offsets
        new_span = util::round_up_pow2(new_span);

        // This is just to ensure nice indexing
        // @todo: Not sure if this is a good idea or not
        //new_ghost_span = round_up_pow2(new_ghost_span);
        new_ghost_span = util::round_up(new_ghost_span, memory::getAlignment());
        std::size_t  new_stride = new_span + new_ghost_span;
        std::size_t  new_nbyte  = new_stride*new_nringlet;

        //pointer new_buf    = (pointer)rfMalloc(new_nbyte, _space);
        //std::cout << "new_buf = " << (void*)new_buf << std::endl; // HACK TESTING

#ifdef RINGBUFFER_WITH_CUDA
        if (state.device != -1) {
            RB_ASSERT_EXCEPTION(cuda::deviceSet(state.device) == RBStatus::STATUS_SUCCESS, RBStatus::STATUS_DEVICE_ERROR);
        }
#endif // RINGBUFFER_WITH_CUDA

        pointer new_buf = nullptr;
        //std::cout << "contig_span:    " << contiguous_span << std::endl;
        //std::cout << "total_span:     " << total_span << std::endl;
        //std::cout << "new_span:       " << new_span << std::endl;
        //std::cout << "new_ghost_span: " << new_ghost_span << std::endl;
        //std::cout << "new_nringlet:   " << new_nringlet << std::endl;
        //std::cout << "new_stride:     " << new_stride << std::endl;
        //std::cout << "Allocating " << new_nbyte << std::endl;
        RB_ASSERT_EXCEPTION(memory::malloc_((void**)&new_buf, new_nbyte, state.space) == RBStatus::STATUS_SUCCESS,
                            RBStatus::STATUS_MEM_ALLOC_FAILED);
#ifdef RINGBUFFER_WITH_NUMA
        if( state.core != -1 ) {
            RB_ASSERT_EXCEPTION(numa_available() != -1, RBStatus::STATUS_UNSUPPORTED);
            int node = numa_node_of_cpu(state.core);
            RB_ASSERT_EXCEPTION(node != -1, RBStatus::STATUS_INVALID_ARGUMENT);
            numa_tonode_memory(new_buf, new_nbyte, node);
        }
#endif
        if( state.buf ) {
            // Must move existing data and delete old buf
            if( _buf_offset(state.tail) < _buf_offset(state.head) ) {
                // Copy middle to beginning
                memory::memcpy2D(new_buf, new_stride, state.space,
                             state.buf + _buf_offset(state.tail), state.stride, state.space,
                           std::size_t(state.head - state.tail), state.nringlet);
                state.offset0 = state.tail;
            }
            else {
                // Copy beg to beg and end to end, with larger gap between
                memory::memcpy2D(new_buf, new_stride, state.space,
                                 state.buf, state.stride, state.space,
                           _buf_offset(state.head), state.nringlet);

                memory::memcpy2D(new_buf + (_buf_offset(state.tail)+(new_span-state.span)), new_stride, state.space,
                                 state.buf + _buf_offset(state.tail), state.stride, state.space,
                                 state.span - _buf_offset(state.tail), state.nringlet);
                state.offset0 = state.head - _buf_offset(state.head); // @todo: Check this for sign/overflow issues
            }

            // Copy old ghost region to new buffer
            memory::memcpy2D(new_buf + new_span, new_stride, state.space,
                             state.buf + state.span, state.stride, state.space,
                             state.ghost_span, state.nringlet);

            // Copy the part of the beg corresponding to the extra ghost space
            memory::memcpy2D(new_buf + new_span + state.ghost_span, new_stride, state.space,
                             state.buf + state.ghost_span, state.stride, state.space,
                           std::min(new_ghost_span, state.span) - state.ghost_span, state.nringlet);

            //_ghost_dirty = true; // @todo: Is this the right thing to do?
            //_ghost_dirty_beg = new_ghost_span; // @todo: Is this the right thing to do?
            state.ghost_dirty_beg = 0; // @todo: Is this the right thing to do?
            memory::free_(state.buf, state.space);
            cuda::streamSynchronize();
        }
        state.buf        = new_buf;
        state.ghost_span = new_ghost_span;
        state.span       = new_span;
        state.stride     = new_stride;
        state.nringlet   = new_nringlet;

        // @todo: Update the ProcLog entry for this ring
    }

    void Ring::begin_writing() {
        auto& state = get_state();
        state::lock_guard_type lock(state.mutex);
        RB_ASSERT_EXCEPTION(!state.writing_begun, RBStatus::STATUS_INVALID_STATE);
        RB_ASSERT_EXCEPTION(!state.writing_ended, RBStatus::STATUS_INVALID_STATE);
        state.writing_begun = true;
    }

    void Ring::end_writing() {
        auto& state = get_state();
        state::lock_guard_type lock(state.mutex);
        RB_ASSERT_EXCEPTION(state.writing_begun && !state.writing_ended, RBStatus::STATUS_INVALID_STATE);
        RB_ASSERT_EXCEPTION(!state.nwrite_open,RBStatus::STATUS_INVALID_STATE);
        // @todo: Assert that no sequences are open for writing
        state.writing_ended = true;
        state.eod = state.head;
        state.sequence_condition.notify_all();
    }

    std::size_t Ring::_buf_offset(std::size_t offset) const {
        const auto& state = get_state();
        return (offset - state.offset0) % state.span;
    }
    
    pointer Ring::_buf_pointer(std::size_t offset) const {
        const auto& state = get_state();
        return state.buf + _buf_offset(offset);
    }
    
    void Ring::_ghost_write(std::size_t offset, std::size_t span) {
        auto& state = get_state();
        std::size_t buf_offset_beg = _buf_offset(offset);
        std::size_t buf_offset_end = _buf_offset(offset + span);
        if( buf_offset_end < buf_offset_beg ) {
            // The write went into the ghost region, so copy to the ghosted part
            this->_copy_from_ghost(0, buf_offset_end);
        }
        if( buf_offset_beg < (std::size_t)state.ghost_span ) {
            // The write touched the ghosted front of the buffer
            state.ghost_dirty_beg = std::min(state.ghost_dirty_beg, buf_offset_beg);
        }
    }
    
    void Ring::_ghost_read(std::size_t offset, std::size_t span) {
        auto& state = get_state();
        std::size_t buf_offset_beg = _buf_offset(offset);
        std::size_t buf_offset_end = _buf_offset(offset + span);
        if( buf_offset_end < buf_offset_beg ) {
            // The read will enter the ghost region, so copy from the ghosted part
            buf_offset_end = std::min(buf_offset_end, (std::size_t)state.ghost_span);
            std::size_t dirty_span =
                    std::max((delta_type)buf_offset_end - (delta_type)state.ghost_dirty_beg,
                             delta_type(0));
            this->_copy_to_ghost(state.ghost_dirty_beg, dirty_span);
            // Note: This is actually _decreasing_ the amount that is marked dirty
            state.ghost_dirty_beg += dirty_span;
        }
    }
    
    void Ring::_copy_to_ghost(std::size_t buf_offset, std::size_t span) {
        auto& state = get_state();
        // Copy from the front of the buffer to the ghost region at the end
        memory::memcpy2D(state.buf + (state.span + buf_offset), state.stride, state.space,
                   state.buf + buf_offset, state.stride, state.space,
                   span, state.nringlet);
        // @todo: commented out as it could be a source of instability ..
        cuda::streamSynchronize();
    }
    
    void Ring::_copy_from_ghost(std::size_t buf_offset, std::size_t span) {
        auto& state = get_state();
        // Copy from the ghost region to the front of the buffer
        memory::memcpy2D(state.buf + buf_offset, state.stride, state.space,
                         state.buf + (state.span + buf_offset), state.stride, state.space,
                         span, state.nringlet);
        // @todo: commented out as it could be a source of instability ..
        cuda::streamSynchronize();
    }

    RBStatus Ring::_advance_reserve_head(state::unique_lock_type& lock, std::size_t size, bool nonblocking, std::chrono::nanoseconds timeout) {
        auto& state = get_state();
        // This waits until all guarantees have caught up to the new valid
        //   buffer region defined by _reserve_head, and then pulls the tail
        //   along to ensure it is within a distance of _span from _reserve_head.

        // Note: By using _span, this correctly handles ring resizes that occur
        //         while waiting on the condition.
        // @todo: This enables guaranteed reads to "cover for" unguaranteed
        //         siblings that would be too slow on their own. Is this actually
        //         a problem, and if so is there any way around it?
        state.reserve_head += size;
        auto postcondition_predicate = [this]() {
            const auto& state = get_state();
            bool no_guarantees;
            {
                std::scoped_lock<state::mutex_type> lk(state.guarantees_mutex);
                no_guarantees = (state.guarantees.empty() || std::size_t(state.reserve_head - _get_earliest_guarantee()) <= state.span);
            }
            return (no_guarantees && state.nrealloc_pending == 0);
        };

        if( !nonblocking ) {
            // either wait infinitely (timeout=0) or raise exception when timeout occurs
            if (timeout.count() == 0) {
                state.write_condition.wait(lock, postcondition_predicate);
            } else {
                if(!state.write_condition.wait_for(lock, timeout, postcondition_predicate)) {
                    // Revert and return failure
                    state.reserve_head -= size;
                    return RBStatus::STATUS_WAIT_TIMEOUT;
                }
            }
        } else if( !postcondition_predicate() ) {
            // Revert and return failure
            state.reserve_head -= size;
            return RBStatus::STATUS_WOULD_BLOCK;
        }

        std::size_t cur_span = state.reserve_head - state.tail;
        if( cur_span > state.span ) {
            // Pull the tail
            state.tail += cur_span - state.span;
            // Delete old sequences
            while( !state.sequence_queue.empty() &&
                   //_sequence_queue.front()->_end != Sequence::RB_SEQUENCE_OPEN &&
                   state.sequence_queue.front()->is_finished() &&
                   //_sequence_queue.front()->_end <= _tail ) {
                   std::size_t(state.head - state.sequence_queue.front()->m_end) >= std::size_t(state.head - state.tail) ) {
                if( !state.sequence_queue.front()->m_name.empty() ) {
                    state.sequence_map.erase(state.sequence_queue.front()->m_name);
                }
                if( state.sequence_queue.front()->m_time_tag != std::size_t(-1) ) {
                    spdlog::trace("Ringbuffer[{0}] discard timetag: {1}", state.name, state.sequence_queue.front()->m_time_tag);
                    state.sequence_time_tag_map.erase(state.sequence_queue.front()->m_time_tag);
                }
                //delete _sequence_queue.front();
                state.sequence_queue.pop();
            }
        }
        return RBStatus::STATUS_SUCCESS;
    }

    std::vector<uint64_t> Ring::list_time_tags() {
        const auto& state = get_state();
        state::lock_guard_type lock(state.mutex);
        std::vector<uint64_t> time_tags;
        for (auto& it : state.sequence_time_tag_map) {
            if (it.second->is_finished()) {
                time_tags.push_back(it.first);
            }
        }
        return time_tags;
    }

    SequencePtr Ring::begin_sequence(std::string   name,
                                     time_tag_type time_tag,
                                     std::size_t   header_size,
                                     const void*   header,
                                     std::size_t   nringlet,
                                     std::size_t   offset_from_head) {
//        RB_ASSERT_EXCEPTION(name.c_str(),           RBStatus::STATUS_INVALID_ARGUMENT); // not nice .. maybe does not work as intended..
        RB_ASSERT_EXCEPTION(header || !header_size, RBStatus::STATUS_INVALID_ARGUMENT);
        auto& state = get_state();
        SequencePtr sequence;
        {
            state::lock_guard_type lock(state.mutex);
            //unique_lock_type lock(_mutex);
            RB_ASSERT_EXCEPTION(nringlet <= state.nringlet,  RBStatus::STATUS_INVALID_ARGUMENT); // this assumes that providing nringlet==0 results in 1 ringlet ??
            // Cannot have the previous sequence still open
            RB_ASSERT_EXCEPTION(state.sequence_queue.empty() ||
                                state.sequence_queue.back()->is_finished(),
                                RBStatus::STATUS_INVALID_STATE);
            std::size_t seq_begin = state.head + offset_from_head;
            // Cannot have existing sequence with same name
            RB_ASSERT_EXCEPTION(state.sequence_map.count(name)==0,              RBStatus::STATUS_INVALID_ARGUMENT);
            RB_ASSERT_EXCEPTION(state.sequence_time_tag_map.count(time_tag)==0, RBStatus::STATUS_INVALID_ARGUMENT);
            sequence.reset(new Sequence(shared_from_this(), name, time_tag, header_size,
                                        header, nringlet, seq_begin));
            if( state.sequence_queue.size() ) {
                state.sequence_queue.back()->set_next(sequence);
            }
            state.sequence_queue.push(sequence);
            state.sequence_condition.notify_all();
            if( !std::string(name).empty() ) {
                state.sequence_map.insert(std::make_pair(std::string(name),sequence));
            }
            if( time_tag != std::size_t(-1) ) {
                state.sequence_time_tag_map.insert(std::make_pair(time_tag,sequence));
            }
        }
//        m_sequence_event.dispatch(time_tag);
        return sequence;
    }

    SequencePtr Ring::_get_sequence_by_name(const std::string& name) {
        auto& state = get_state();
        RB_ASSERT_EXCEPTION(state.sequence_map.count(name), RBStatus::STATUS_INVALID_ARGUMENT);
        return state.sequence_map.find(name)->second;
    }

    SequencePtr Ring::open_sequence_by_name(const std::string& name,
                                            bool with_guarantee,
                                            std::unique_ptr<state::Guarantee>& guarantee) {
        // Note: Guarantee uses locks, so must be kept outside the lock scope here
        std::unique_ptr<state::Guarantee> scoped_guarantee;
        if( with_guarantee ) {
            // Ensure a guarantee is held while waiting for sequence to exist
            scoped_guarantee = std::unique_ptr<state::Guarantee>(new state::Guarantee(shared_from_this()));
        }
        auto& state = get_state();
        state::unique_lock_type lock(state.mutex);
        SequencePtr sequence = this->_get_sequence_by_name(name);
        if( scoped_guarantee ) {
            // Move guarantee to start of sequence
            scoped_guarantee->move_nolock(
                    this->_get_start_of_sequence_within_ring(sequence));
        }
        // Transfer ownership to the caller
        guarantee = std::move(scoped_guarantee);
        return sequence;
    }

    SequencePtr Ring::_get_sequence_at(time_tag_type time_tag) {
        // Note: This function only works if time_tag resides within the buffer
        //         (or in its overwritten history) at the time of the call.
        //         There is no way for the function to know if a time_tag
        //           representing the future will actually fall within the current
        //           sequence or in a later one, and thus the returned sequence
        //           may turn out to be incorrect.
        //         If time_tag falls before the first sequence currently in the
        //           buffer, the function returns RBStatus::STATUS_INVALID_ARGUMENT.
        //         TLDR; only use time_tag values representing times that have
        //           already happened, and be careful not to call this function
        //           before the very first sequence has been created.
        auto& state = get_state();
        auto iter = state.sequence_time_tag_map.upper_bound(time_tag);
        RB_ASSERT_EXCEPTION(iter != state.sequence_time_tag_map.begin(),
                            RBStatus::STATUS_INVALID_ARGUMENT);
        return (--iter)->second;
    }

    SequencePtr Ring::open_sequence_at(time_tag_type time_tag,
                                       bool with_guarantee,
                                       std::unique_ptr<state::Guarantee>& guarantee) {
        // Note: Guarantee uses locks, so must be kept outside the lock scope here
        std::unique_ptr<state::Guarantee> scoped_guarantee;
        if( with_guarantee ) {
            // Ensure a guarantee is held while waiting for sequence to exist
            scoped_guarantee = std::unique_ptr<state::Guarantee>(new state::Guarantee(shared_from_this()));
        }
        auto& state = get_state();
        state::unique_lock_type lock(state.mutex);
        SequencePtr sequence = this->_get_sequence_at(time_tag);
        if(sequence->begin() < state.tail) {
            //This sequence was already overwritten
            return nullptr;
        }
        if( scoped_guarantee ) {
            // Move guarantee to start of sequence
            scoped_guarantee->move_nolock(
                    this->_get_start_of_sequence_within_ring(sequence));
        }
        // Transfer ownership to the caller
        guarantee = std::move(scoped_guarantee);
        return sequence;
    }

    bool Ring::_sequence_still_within_ring(SequencePtr sequence) const {
        const auto& state = get_state();
        return (!sequence->is_finished() ||
                std::size_t(state.head - sequence->end()) <= std::size_t(state.head - state.tail));
    }

    SequencePtr Ring::_get_earliest_or_latest_sequence(state::unique_lock_type& lock, bool latest, std::chrono::nanoseconds timeout) const {
        const auto& state = get_state();
        // Wait until a sequence has been opened or writing has ended
        auto condition_predicate = [this]() {
            const auto& state = get_state();
            return !state.sequence_queue.empty() || state.writing_ended;
        };
        // either wait infinitely (timeout=0) or raise exception when timeout occurs
        if (timeout.count() == 0) {
            state.sequence_condition.wait(lock, condition_predicate);
        } else {
            if(!state.sequence_condition.wait_for(lock, timeout, condition_predicate)) {
                throw RBException(RBStatus::STATUS_WAIT_TIMEOUT);
            }
        }
        RB_ASSERT_EXCEPTION(!(state.sequence_queue.empty() && !state.writing_ended), RBStatus::STATUS_INVALID_STATE);
        RB_ASSERT_EXCEPTION(!(state.sequence_queue.empty() &&  state.writing_ended), RBStatus::STATUS_END_OF_DATA);
        SequencePtr sequence = (latest ?
                                state.sequence_queue.back() :
                                state.sequence_queue.front());
        // Check that the sequence is still within the ring
        RB_ASSERT_EXCEPTION(this->_sequence_still_within_ring(sequence),
                            RBStatus::STATUS_INVALID_ARGUMENT);
        return sequence;
    }

    std::size_t Ring::_get_start_of_sequence_within_ring(SequencePtr sequence) const {
        const auto& state = get_state();
        if( std::size_t(state.head - sequence->begin()) > std::size_t(state.head - state.tail) ) {
            // Sequence starts before tail
            return state.tail;
        } else {
            return sequence->begin();
        }
    }

    SequencePtr Ring::_get_next_sequence(SequencePtr sequence,
                                         state::unique_lock_type& lock, std::chrono::nanoseconds timeout) const {
        const auto& state = get_state();
        // Wait until the next sequence has been opened or writing has ended
        auto condition_predicate = [&]() {
            return ((bool)sequence->m_next) || state.writing_ended;
        };
        // either wait infinitely (timeout=0) or raise exception when timeout occurs
        if (timeout.count() == 0) {
            state.sequence_condition.wait(lock, condition_predicate);
        } else {
            if(!state.sequence_condition.wait_for(lock, timeout, condition_predicate)) {
                throw RBException(RBStatus::STATUS_WAIT_TIMEOUT);
            }
        }
        RB_ASSERT_EXCEPTION(sequence->m_next, RBStatus::STATUS_END_OF_DATA);
        return sequence->m_next;
    }

    SequencePtr Ring::open_earliest_or_latest_sequence(bool with_guarantee,
                                                       std::unique_ptr<state::Guarantee>& guarantee,
                                                       bool latest, std::chrono::nanoseconds timeout) {
        // Note: Guarantee uses locks, so must be kept outside the lock scope here
        std::unique_ptr<state::Guarantee> scoped_guarantee;
        if( with_guarantee ) {
            // Ensure a guarantee is held while waiting for sequence to exist
            scoped_guarantee = std::unique_ptr<state::Guarantee>(new state::Guarantee(shared_from_this()));
        }
        auto& state = get_state();
        state::unique_lock_type lock(state.mutex);
        SequencePtr sequence = this->_get_earliest_or_latest_sequence(lock, latest, timeout);
        if( scoped_guarantee ) {
            // Move guarantee to start of sequence
            scoped_guarantee->move_nolock(
                    this->_get_start_of_sequence_within_ring(sequence));
        }
        // Transfer ownership to the caller
        guarantee = std::move(scoped_guarantee);
        return sequence;
    }

    void Ring::increment_sequence_to_next(SequencePtr& sequence,
                                          std::unique_ptr<state::Guarantee>& guarantee, std::chrono::nanoseconds timeout) {
        // Take ownership of the guarantee (if it exists)
        // Note: Guarantee uses locks, so must be kept outside the lock scope here
        std::unique_ptr<state::Guarantee> scoped_guarantee = std::move(guarantee);
        auto& state = get_state();
        state::unique_lock_type lock(state.mutex);
        //SequencePtr next_sequence = this->_get_next_sequence(sequence, lock);
        sequence = this->_get_next_sequence(sequence, lock, timeout);
        if( scoped_guarantee ) {
            // Move the guarantee to the start of the new sequence
            scoped_guarantee->move_nolock(
                    this->_get_start_of_sequence_within_ring(sequence));
        }
        // Return ownership of the guarantee
        guarantee = std::move(scoped_guarantee);
    }



    void Ring::finish_sequence(SequencePtr sequence,
                               std::size_t offset_from_head,
                               std::size_t footer_size, void* footer) {
        auto& state = get_state();
        {
            state::lock_guard_type lock(state.mutex);
            // Must have the sequence still open
            RB_ASSERT_EXCEPTION(!state.sequence_queue.empty() &&
                                !state.sequence_queue.back()->is_finished(),
                                RBStatus::STATUS_INVALID_STATE);
            if (footer_size > 0) {
                sequence->set_footer(footer_size, footer);
            }
            // This marks the sequence as finished
            sequence->m_end = state.head + offset_from_head;
            state.read_condition.notify_all();
        }
        m_sequence_event.dispatch(sequence->time_tag());
    }

    void Ring::acquire_span(ReadSequence* rsequence,
                            std::size_t   offset, // Relative to sequence beg
                            std::size_t*  size_,
                            std::size_t*  begin_,
                            void**        data_,
                            std::chrono::nanoseconds timeout) {
        RB_ASSERT_EXCEPTION(rsequence,             RBStatus::STATUS_INVALID_SEQUENCE_HANDLE);
        RB_ASSERT_EXCEPTION(size_,                 RBStatus::STATUS_INVALID_POINTER);
        RB_ASSERT_EXCEPTION(begin_,                RBStatus::STATUS_INVALID_POINTER);
        RB_ASSERT_EXCEPTION(data_,                 RBStatus::STATUS_INVALID_POINTER);
        // Cannot go back beyond the start of the sequence
        RB_ASSERT_EXCEPTION(offset >= 0,     RBStatus::STATUS_INVALID_ARGUMENT);
        SequencePtr sequence = rsequence->sequence();
        auto& state = get_state();
        state::unique_lock_type lock(state.mutex);
        RB_ASSERT_EXCEPTION(*size_ <= state.ghost_span, RBStatus::STATUS_INVALID_ARGUMENT);

        std::size_t requested_begin = sequence->begin() + offset;
        std::size_t requested_end   = requested_begin + *size_;

        // @todo: If this function fails, should the guarantee be left where it was?
        //         This would be straightforward to implement using a scoped
        //           guarantee.

        if( rsequence->guarantee() ) {
            std::size_t guarantee_begin = rsequence->guarantee()->offset();
            delta_type distance_from_guarantee = delta_type(requested_begin -
                                                      guarantee_begin);
            // Note: Triggered dumps may open a guaranteed sequence that has
            //         already been partially overwritten. In such cases, the
            //         user may reasonably request spans at the beginning of the
            //         sequence that actually lie outside of the guarantee
            //         (i.e., distance_from_guarantee < 0), and so an error should
            //         _not_ be returned in this scenario (just a zero-size span).
            if( distance_from_guarantee > 0 ) {
                // Move the guarantee forward to the beginning of this span to
                //   allow writers to make progress.
                rsequence->guarantee()->move_nolock(requested_begin);
            }
        }

        // This function returns whatever part of the requested span is available
        //   (meaning not overwritten and not past the end of the sequence).
        //   It will return a 0-length span if the requested span has been
        //     completely overwritten.
        // It throws RBStatus::STATUS_END_OF_DATA if the requested span begins
        //   after the end of the sequence.

        // Wait until requested span has been written or sequence has ended

        auto condition_predicate = [&]() {
            auto& state = get_state();
            return ((delta_type(state.head - std::max(requested_begin, state.tail)) >=
                     delta_type(requested_end - std::max(requested_begin, state.tail)) ||
                     sequence->is_finished()) &&
                    state.nrealloc_pending == 0);
        };
        // either wait infinitely (timeout=0) or raise exception when timeout occurs
        if (timeout.count() == 0) {
            state.read_condition.wait(lock, condition_predicate);
        } else {
            if(!state.read_condition.wait_for(lock, timeout, condition_predicate)) {
                throw RBException(RBStatus::STATUS_WAIT_TIMEOUT);
                // @todo any cleanup needed ??
            }
        }

        // Constrain to what is in the buffer (i.e., what hasn't been overwritten)
        std::size_t begin = std::max(requested_begin, state.tail);
        // Note: This results in size being 0 if the requested span has been
        //         completely overwritten.
        std::size_t   size  = std::max(delta_type(requested_end - begin), delta_type(0));

        if( sequence->is_finished() ) {
            RB_ASSERT_EXCEPTION(begin < sequence->end(),
                                RBStatus::STATUS_END_OF_DATA);
            size = std::min(size, std::size_t(sequence->end() - begin));
        }
        *begin_ = begin;
        *size_  = size;

        ++state.nread_open;
        _ghost_read(begin, size);
        *data_ = _buf_pointer(begin);
    }

    void Ring::release_span(ReadSequence* sequence,
                            std::size_t  begin,
                            std::size_t  size) {
        auto& state = get_state();
        state::unique_lock_type lock(state.mutex);
        --state.nread_open;
        state.realloc_condition.notify_all();
    }

    void Ring::reserve_span(std::size_t size, std::size_t* begin, void** data, bool nonblocking, std::chrono::nanoseconds timeout) {
        auto& state = get_state();
        state::unique_lock_type lock(state.mutex);
        RB_ASSERT_EXCEPTION(size <= state.ghost_span, RBStatus::STATUS_INVALID_ARGUMENT);
        *begin = state.reserve_head;
        auto ret = this->_advance_reserve_head(lock, size, nonblocking, timeout);
        if (ret != RBStatus::STATUS_SUCCESS) {
            throw RBException(ret);
        }
        ++state.nwrite_open;
        *data = _buf_pointer(*begin);
    }

    void Ring::commit_span(std::size_t begin, std::size_t reserve_size, std::size_t commit_size) {
        auto& state = get_state();
        state::unique_lock_type lock(state.mutex);
        _ghost_write(begin, commit_size);

        // @todo: Refactor/tidy this function a bit

        // Note: This allows unused open blocks to be 'cancelled' if they
        //         are closed in reverse order.

        // ueck: this may be causing a problem when commiting 0 bytes (empty buffers)
        if( commit_size == 0 &&
            state.reserve_head == begin + reserve_size ) {
            // This is the last-opened block so we can 'cancel' it by pulling back
            //   the reserve head.
            state.reserve_head = begin;
            --state.nwrite_open;
            state.realloc_condition.notify_all();
            return;
        }

        // Wait until this block is at the head
        // Note: This allows write blocks to be closed out of order,
        //         in which case they will block here until they are
        //         in order (i.e., they will automatically synchronise).
        //         This is useful for multithreading with OpenMP
        //std::cout << "(1) begin, head, rhead: " << begin << ", " << _head << ", " << _reserve_head << std::endl;

        auto condition_predicate = [&]() {
            auto& state = get_state();
            return (begin == state.head);
        };

        // here we cannot add a timeout as we need to guarantee the writing of the data if started.
        state.write_close_condition.wait(lock, condition_predicate);

        state.write_close_condition.notify_all();

        if( state.reserve_head == state.head + reserve_size ) {
            // This is the front-most wspan, so we can pull back
            //   the reserve head if commit_size < size.
            state.reserve_head = state.head + commit_size;
        }
        else if( commit_size < reserve_size ) {
            // There are reservations in front of this one, so we
            //   are not allowed to commit less than size.
            // @todo: How to deal with error here?
            //std::cout << "BFRING ERROR: Must commit whole wspan when other spans are reserved" << std::endl;
            //return;
            RB_ASSERT_EXCEPTION(false, RBStatus::STATUS_INVALID_STATE);
        }
        state.head += commit_size;

        state.read_condition.notify_all();
        --state.nwrite_open;
        state.realloc_condition.notify_all();
    }


	int Ring::subscribe_sequence_event(void(*callback)(time_tag_type, void*), void* const userData) {
		return m_sequence_event.connect([callback, userData](time_tag_type ts) {
			callback(ts, const_cast<void*>(userData));
		});
	}

	void Ring::unsubscribe_sequence_event(int connection_id) {
		m_sequence_event.disconnect(connection_id);
	}


}
