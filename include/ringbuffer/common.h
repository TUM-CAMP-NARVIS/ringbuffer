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

#ifndef RINGFLOW_COMMON_H
#define RINGFLOW_COMMON_H

#pragma warning( disable : 4275 ) // non dll-interface class 'std::runtime_error' used as base for dll-interface class 'ringbuffer::RBException'

#include <string>
#include <stdexcept>
#include "spdlog/spdlog.h"
#include "ringbuffer/config.h"
#include "ringbuffer/visibility.h"

namespace ringbuffer {

    /*
     * Ringbuffer Return Status Codes
     */
    enum class RBStatus {
        STATUS_SUCCESS                                  = 0,
        STATUS_END_OF_DATA                              = 1,
        STATUS_WOULD_BLOCK                              = 2,
        STATUS_WAIT_TIMEOUT                             = 3,
        STATUS_INVALID_POINTER                          = 8,
        STATUS_INVALID_SEQUENCE_HANDLE                  = 9,
        STATUS_INVALID_ARGUMENT                         = 10,
        STATUS_INVALID_STATE                            = 11,
        STATUS_INVALID_SPACE                            = 12,
        STATUS_INVALID_SHAPE                            = 13,
        STATUS_INVALID_STRIDE                           = 14,
        STATUS_INVALID_DTYPE                            = 15,
        STATUS_MEM_ALLOC_FAILED                         = 32,
        STATUS_MEM_OP_FAILED                            = 33,
        STATUS_UNSUPPORTED                              = 48,
        STATUS_UNSUPPORTED_SPACE                        = 49,
        STATUS_UNSUPPORTED_SHAPE                        = 50,
        STATUS_UNSUPPORTED_STRIDE                       = 51,
        STATUS_UNSUPPORTED_DTYPE                        = 52,
        STATUS_FAILED_TO_CONVERGE                       = 64,
        STATUS_INSUFFICIENT_STORAGE                     = 65,
        STATUS_DEVICE_ERROR                             = 66,
        STATUS_INTERNAL_ERROR                           = 99
    };


    std::string RINGBUFFER_EXPORT getStatusString(RBStatus status);
    void RINGBUFFER_EXPORT requireSuccess(RBStatus status);
    bool RINGBUFFER_EXPORT raiseOnFailure(RBStatus status);


    /*
     * Enum to reference the memory space for allocation and memcpy calls
     */
    enum class RBSpace {
        SPACE_AUTO         = 0,
        SPACE_SYSTEM       = 1, // aligned_alloc
        SPACE_CUDA         = 2, // cudaMalloc
        SPACE_CUDA_HOST    = 3, // cudaHostAlloc
        SPACE_CUDA_MANAGED = 4, // cudaMallocManaged
        SPACE_SHM          = 5  // shared memory allocation
    };

    std::string RINGBUFFER_EXPORT getSpaceString(RBSpace space);


    /*
     * Helpers for checking if features are enabled
     */
    bool RINGBUFFER_EXPORT getDebugEnabled();
    RBStatus RINGBUFFER_EXPORT setDebugEnabled(bool enabled);
    bool RINGBUFFER_EXPORT getCudaEnabled();
    bool RINGBUFFER_EXPORT getShmEnabled();

    /*
     * Ringbuffer Exception Class
     */
    class RINGBUFFER_EXPORT RBException : public std::runtime_error {
        RBStatus _status;
    public:
        RBException(RBStatus stat, std::string msg = "")
                : std::runtime_error(getStatusString(stat) + ": " + msg),
                  _status(stat) {}
        RBStatus status() const { return _status; }
    };

    
#ifdef RINGBUFFER_DEBUG
    /*
     * Filter out warnings that don't need reporting
     */
    bool should_report_error(RBStatus err);

    #define RB_REPORT_ERROR(err) do { \
		if( getDebugEnabled() && \
		    should_report_error(err) ) { \
		    spdlog::error("{0}:{1} error {2}: {3}", __FILE__, __LINE__, static_cast<int>(err), getStatusString(err));\
		} \
		} while(0)

	#define RB_DEBUG_PRINT(x) do { \
		if( getDebugEnabled() ) { \
		    spdlog::debug("{0}:{1} {2} = {3}", __FILE__, __LINE__, #x, (x));\
		} \
		} while(0)

	#define RB_REPORT_PREDFAIL(pred, err) do { \
		if( getDebugEnabled() && \
		    should_report_error(err) ) { \
		    spdlog::error("{0}:{1} condition failed {2}", __FILE__, __LINE__, #pred);\
		} \
		} while(0)
#else
    #define RB_REPORT_ERROR(err)
    #define RB_DEBUG_PRINT(x)
    #define RB_REPORT_PREDFAIL(pred, err)
#endif // RINGBUFFER_DEBUG

#define RB_REPORT_INTERNAL_ERROR(msg) do { \
        spdlog::error("{0}:{1} internal error: {2}", __FILE__, __LINE__, msg);\
	} while(0)

#define RB_FAIL(msg, err) do { \
		RB_REPORT_PREDFAIL(msg, err); \
		RB_REPORT_ERROR(err); \
		return (err); \
	} while(0)
	    
#define RB_FAIL_EXCEPTION(msg, err) do { \
		RB_REPORT_PREDFAIL(msg, err); \
		RB_REPORT_ERROR(err); \
		throw RBException(err); \
	} while(0)

#define RB_ASSERT(pred, err) do { \
		if( !(pred) ) { \
			RB_REPORT_PREDFAIL(pred, err); \
			RB_REPORT_ERROR(err); \
			return (err); \
		} \
	} while(0)
	    
#define RB_TRY_ELSE(code, onfail) do { \
		try { code; } \
		catch( RBException const& err ) { \
			onfail; \
			RB_REPORT_ERROR(err.status()); \
			return err.status(); \
		} \
		catch(std::bad_alloc const& err) { \
			onfail; \
			RB_REPORT_ERROR(RBStatus::STATUS_MEM_ALLOC_FAILED); \
			return RBStatus::STATUS_MEM_ALLOC_FAILED; \
		} \
		catch(std::exception const& err) { \
			onfail; \
			RB_REPORT_INTERNAL_ERROR(err.what()); \
			return RBStatus::STATUS_INTERNAL_ERROR; \
		} \
		catch(...) { \
			onfail; \
			RB_REPORT_INTERNAL_ERROR("FOREIGN EXCEPTION"); \
			return RBStatus::STATUS_INTERNAL_ERROR; \
		} \
	} while(0)
#define RB_NO_OP (void)0
#define RB_TRY(code) RB_TRY_ELSE(code, RB_NO_OP)
#define RB_TRY_RETURN(code) RB_TRY(code); return RBStatus::STATUS_SUCCESS
#define RB_TRY_RETURN_ELSE(code, onfail) RB_TRY_ELSE(code, onfail); return RBStatus::STATUS_SUCCESS

#define RB_ASSERT_EXCEPTION(pred, err) \
	do { \
		if( !(pred) ) { \
			RB_REPORT_PREDFAIL(pred, err); \
			RB_REPORT_ERROR(err); \
			throw RBException(err); \
		} \
	} while(0)

#define RB_CHECK(call) do { \
	RBStatus status = call; \
	if( status != RBStatus::STATUS_SUCCESS ) { \
		RB_REPORT_ERROR(status); \
		return status; \
	} \
} while(0)

#define RB_CHECK_EXCEPTION(call) do { \
	RBStatus status = call; \
	if( status != RBStatus::STATUS_SUCCESS ) { \
		RB_REPORT_ERROR(status); \
		throw RBException(status); \
	} \
} while(0)
    
}

#endif //RINGFLOW_COMMON_H
