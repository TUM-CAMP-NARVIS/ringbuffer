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

#ifndef RINGBUFFER_RING_STATE_H
#define RINGBUFFER_RING_STATE_H

#pragma warning( disable : 4251 ) // needs to have dll-interface to be used by clients of class

#include "ringbuffer/common.h"
#include "ringbuffer/visibility.h"
#include "ringbuffer/types.h"

#include <string>
#include <queue>
#include <set>
#include <memory>


namespace ringbuffer {


    namespace state {
        // RingState is factored out of the class to allow shm allocation later

        struct RINGBUFFER_EXPORT RingState {
            std::string name;
            RBSpace space{RBSpace ::SPACE_AUTO};

            pointer buf{nullptr};

            std::size_t ghost_span{0};
            std::size_t span{0};
            std::size_t stride{0};
            std::size_t nringlet{0};
            std::size_t offset0{0};

            std::size_t tail{0};
            std::size_t head{0};
            std::size_t reserve_head{0};

            std::size_t ghost_dirty_beg{0};

            bool writing_begun{false};
            bool writing_ended{false};
            std::size_t eod{0};

            mutable mutex_type mutex;
            condition_type read_condition;
            condition_type write_condition;
            condition_type write_close_condition;
            condition_type realloc_condition;
            mutable condition_type sequence_condition;

            std::size_t nread_open{0};
            std::size_t nwrite_open{0};
            std::size_t nrealloc_pending{0};

            int core{-1};
            int device{-1};

            std::queue<SequencePtr> sequence_queue;
            std::map<std::string,SequencePtr> sequence_map;
            std::map<std::size_t,SequencePtr> sequence_time_tag_map;

            guarantee_set guarantees;
            mutable mutex_type guarantees_mutex;

        };


    }
}


#endif //RINGBUFFER_RING_STATE_H
