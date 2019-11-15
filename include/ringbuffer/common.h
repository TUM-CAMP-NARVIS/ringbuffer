//
// Created by netlabs on 11/14/19.
//

#ifndef RINGFLOW_COMMON_H
#define RINGFLOW_COMMON_H

#include <string>
#include <stdexcept>
#include "spdlog/spdlog.h"

namespace ringbuffer {

    /*
     * Ringbuffer Return Status Codes
     */
    enum class RBStatus {
        STATUS_SUCCESS                                  = 0,
        STATUS_END_OF_DATA                              = 1,
        STATUS_WOULD_BLOCK                              = 2,
        STATUS_INVALID_POINTER                          = 8,
        STATUS_INVALID_HANDLE                           = 9,
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


    std::string getStatusString(RBStatus status);
    void requireSuccess(RBStatus status);
    bool raiseOnFailure(RBStatus status);


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

    std::string getSpaceString(RBSpace space);

    /*
     * Helpers for checking if features are enabled
     */
    bool getDebugEnabled();
    RBStatus setDebugEnabled(bool enabled);
    bool getCudaEnabled();
    bool getShmEnabled();

    /*
     * Ringbuffer Exception Class
     */
    class RBException : public std::runtime_error {
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
