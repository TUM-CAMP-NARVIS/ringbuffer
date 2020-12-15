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


#include "ringbuffer/common.h"
#include <sstream>

static thread_local bool g_debug_enabled = true;

namespace ringbuffer {

    std::string getStatusString(RBStatus status) {
#define STATUS_STRING_CASE(x) case x: return std::string(#x);
        switch( status ) {
            STATUS_STRING_CASE(RBStatus::STATUS_SUCCESS);
            STATUS_STRING_CASE(RBStatus::STATUS_END_OF_DATA);
            STATUS_STRING_CASE(RBStatus::STATUS_WOULD_BLOCK);
            STATUS_STRING_CASE(RBStatus::STATUS_WAIT_TIMEOUT);
            STATUS_STRING_CASE(RBStatus::STATUS_INVALID_POINTER);
            STATUS_STRING_CASE(RBStatus::STATUS_INVALID_SEQUENCE_HANDLE);
            STATUS_STRING_CASE(RBStatus::STATUS_INVALID_ARGUMENT);
            STATUS_STRING_CASE(RBStatus::STATUS_INVALID_STATE);
            STATUS_STRING_CASE(RBStatus::STATUS_INVALID_SPACE);
            STATUS_STRING_CASE(RBStatus::STATUS_INVALID_SHAPE);
            STATUS_STRING_CASE(RBStatus::STATUS_INVALID_STRIDE);
            STATUS_STRING_CASE(RBStatus::STATUS_INVALID_DTYPE);
            STATUS_STRING_CASE(RBStatus::STATUS_MEM_ALLOC_FAILED);
            STATUS_STRING_CASE(RBStatus::STATUS_MEM_OP_FAILED);
            STATUS_STRING_CASE(RBStatus::STATUS_UNSUPPORTED);
            STATUS_STRING_CASE(RBStatus::STATUS_UNSUPPORTED_SPACE);
            STATUS_STRING_CASE(RBStatus::STATUS_UNSUPPORTED_SHAPE);
            STATUS_STRING_CASE(RBStatus::STATUS_UNSUPPORTED_STRIDE);
            STATUS_STRING_CASE(RBStatus::STATUS_UNSUPPORTED_DTYPE);
            STATUS_STRING_CASE(RBStatus::STATUS_FAILED_TO_CONVERGE);
            STATUS_STRING_CASE(RBStatus::STATUS_INSUFFICIENT_STORAGE);
            STATUS_STRING_CASE(RBStatus::STATUS_DEVICE_ERROR);
            STATUS_STRING_CASE(RBStatus::STATUS_INTERNAL_ERROR);
            default: {
                std::stringstream ss;
                ss << "Invalid status code: " << static_cast<int>(status);
                return ss.str();
            }
        }
#undef STATUS_STRING_CASE
    }

    std::string getSpaceString(RBSpace space) {
        switch( space ) {
            case RBSpace::SPACE_AUTO:         return "auto";
            case RBSpace::SPACE_SYSTEM:       return "system";
            case RBSpace::SPACE_CUDA:         return "cuda";
            case RBSpace::SPACE_CUDA_HOST:    return "cuda_host";
            case RBSpace::SPACE_CUDA_MANAGED: return "cuda_managed";
            case RBSpace::SPACE_SHM:          return "shm";
            default: return "unknown";
        }
    }

    void requireSuccess(RBStatus status) {
        if( status != RBStatus ::STATUS_SUCCESS ) {
            throw RBException(status);
        }
    }

    bool failOnError(RBStatus status) {
//    RBStatus::DISABLE_DEBUG();
        switch( status ) {
            case RBStatus::STATUS_MEM_ALLOC_FAILED:
            case RBStatus::STATUS_MEM_OP_FAILED:
            case RBStatus::STATUS_DEVICE_ERROR:
            case RBStatus::STATUS_INTERNAL_ERROR:
                ringbuffer::requireSuccess(status);
                return false; // Unreachable
            default: return status == RBStatus::STATUS_SUCCESS;
        }
    }


    bool should_report_error(RBStatus err) {
        return (err != RBStatus::STATUS_END_OF_DATA &&
                err != RBStatus::STATUS_WOULD_BLOCK);
    }

    bool getDebugEnabled() {
#ifdef RINGBUFFER_DEBUG
        return g_debug_enabled;
#else
        return false;
#endif
    }

    RBStatus setDebugEnabled(bool b) {
#ifndef RINGBUFFER_DEBUG
        return RBStatus ::STATUS_INVALID_STATE;
#else
        g_debug_enabled = b;
        return RBStatus ::STATUS_SUCCESS;
#endif
    }

    bool getCudaEnabled() {
#ifdef RINGBUFFER_WITH_CUDA
        return true;
#else
        return false;
#endif
    }

    bool getShmEnabled() {
        return false; // for now ..
    }

} // namespace ringbuffer

