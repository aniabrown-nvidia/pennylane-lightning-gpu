#pragma once

#include "cuda.h"
#include <cuda_runtime_api.h> // cudaMalloc, cudaMemcpy, etc.
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#include "Error.hpp"
#include "StateVectorBase.hpp"
#include "StateVectorManaged.hpp"

#include "DataBuffer.hpp"
#include "cuda_helpers.hpp"

/// @cond DEV
namespace {
namespace cuUtil = Pennylane::CUDA::Util;
} // namespace
/// @endcond

namespace Pennylane {

/**
 * @brief CRTP-enabled base class for CUDA-capable state-vector simulators.
 *
 * @tparam Precision Floating point precision.
 * @tparam Derived Derived class to instantiate using CRTP.
 */
template <class Precision, class Derived>
class StateVectorCudaBase : public StateVectorBase<Precision, Derived> {
  private:
    using BaseType = StateVectorBase<Precision, Derived>;

  public:
    /// Alias for accessing the class templated precision type
    using scalar_type_t = Precision;

    /// Alias for complex floating point datatype supported by class
    using CFP_t = decltype(cuUtil::getCudaType(Precision{}));

    /**
     * @brief Return a pointer to the GPU data.
     *
     * @return const CFP_t* Complex device pointer.
     */
    [[nodiscard]] auto getData() const -> const CFP_t * {
        return data_buffer_.getData();
    }
    /**
     * @brief Return a pointer to the GPU data.
     *
     * @return CFP_t* Complex device pointer.
     */
    [[nodiscard]] auto getData() -> CFP_t * { return data_buffer_.getData(); }

    /**
     * @brief Get the CUDA stream for the given object.
     *
     * @return cudaStream_t&
     */
    inline auto getStream() -> cudaStream_t & {
        return data_buffer_.getStream();
    }
    /**
     * @brief Get the CUDA stream for the given object.
     *
     * @return const cudaStream_t&
     */
    inline auto getStream() const -> const cudaStream_t & {
        return data_buffer_.getStream();
    }
    void setStream(const cudaStream_t &s) { data_buffer_.setStream(s); }

    /**
     * @brief Explicitly copy data from host memory to GPU device.
     *
     * @param sv StateVector host data class.
     */
    inline void CopyHostDataToGpu(const StateVectorManaged<Precision> &sv,
                                  bool async = false) {
        PL_ABORT_IF_NOT(BaseType::getNumQubits() == sv.getNumQubits(),
                        "Sizes do not match for Host and GPU data");
        data_buffer_.CopyHostDataToGpu(sv.getData(), sv.getLength(), async);
    }

    /**
     * @brief Explicitly copy data from host memory to GPU device.
     *
     * @param sv StateVector host data class.
     */
    inline void
    CopyHostDataToGpu(const std::vector<std::complex<Precision>> &sv,
                      bool async = false) {
        PL_ABORT_IF_NOT(BaseType::getLength() == sv.size(),
                        "Sizes do not match for Host and GPU data");
        data_buffer_.CopyHostDataToGpu(sv.data(), sv.size(), async);
    }

    /**
     * @brief Explicitly copy data from host memory to GPU device.
     *
     * @param host_sv Complex data pointer to array.
     * @param length Number of complex elements.
     */
    inline void CopyGpuDataToGpuIn(const CFP_t *gpu_sv, std::size_t length,
                                   bool async = false) {
        PL_ABORT_IF_NOT(BaseType::getLength() == length,
                        "Sizes do not match for Host and GPU data");
        data_buffer_.CopyGpuDataToGpu(gpu_sv, length, async);
    }
    /**
     * @brief Explicitly copy data from antoher GPU device memory block to this
     * GPU device.
     *
     * @param sv LightningGPU object to send data.
     */
    inline void CopyGpuDataToGpuIn(const Derived &sv, bool async = false) {
        PL_ABORT_IF_NOT(BaseType::getNumQubits() == sv.getNumQubits(),
                        "Sizes do not match for Host and GPU data");
        PL_ABORT_IF_NOT(typeid(data_buffer_.getData()) == typeid(sv.getData()),
                        "Data types are incompatible for GPU-GPU transfer");
        data_buffer_.CopyGpuDataToGpu(sv.getData(), sv.getLength(), async);
    }

    /**
     * @brief Explicitly copy data from host memory to GPU device.
     *
     * @param host_sv Complex data pointer to array.
     * @param length Number of complex elements.
     */
    inline void CopyHostDataToGpu(const std::complex<Precision> *host_sv,
                                  std::size_t length, bool async = false) {
        PL_ABORT_IF_NOT(BaseType::getLength() == length,
                        "Sizes do not match for Host and GPU data");
        data_buffer_.CopyHostDataToGpu(reinterpret_cast<const CFP_t *>(host_sv),
                                       length, async);
    }

    /**
     * @brief Explicitly copy data from GPU device to host memory.
     *
     * @param sv StateVector to receive data from device.
     */
    inline void CopyGpuDataToHost(StateVectorManaged<Precision> &sv,
                                  bool async = false) const {
        PL_ABORT_IF_NOT(BaseType::getNumQubits() == sv.getNumQubits(),
                        "Sizes do not match for Host and GPU data");
        data_buffer_.CopyGpuDataToHost(sv.getData(), sv.getLength(), async);
    }
    /**
     * @brief Explicitly copy data from GPU device to host memory.
     *
     * @param sv Complex data pointer to receive data from device.
     */
    inline void CopyGpuDataToHost(std::complex<Precision> *host_sv,
                                  size_t length, bool async = false) const {
        PL_ABORT_IF_NOT(BaseType::getLength() == length,
                        "Sizes do not match for Host and GPU data");
        data_buffer_.CopyGpuDataToHost(host_sv, length, async);
    }

    /**
     * @brief Explicitly copy data from this GPU device to another GPU device
     * memory block.
     *
     * @param sv LightningGPU object to receive data.
     */
    inline void CopyGpuDataToGpuOut(Derived &sv, bool async = false) {
        PL_ABORT_IF_NOT(BaseType::getNumQubits() == sv.getNumQubits(),
                        "Sizes do not match for GPU data objects");
        sv.getDataBuffer().CopyGpuDataToGpu(getData(),
                                            data_buffer_.getLength());
    }

    const CUDA::DataBuffer<CFP_t> &getDataBuffer() const {
        return data_buffer_;
    }

    CUDA::DataBuffer<CFP_t> &getDataBuffer() { return data_buffer_; }

    /**
     * @brief Update GPU device data from given derived object.
     *
     * @param other Source data to copy from.
     * @param async Use asynchronous copies.
     */
    void updateData(const Derived &other, bool async = false) {
        CopyGpuDataToGpuIn(other, async);
    }

    /**
     * @brief Initialize the statevector data to the |0...0> state
     *
     */
    void initSV(bool async = false) {
        std::vector<std::complex<Precision>> data(BaseType::getLength(),
                                                  {0, 0});
        data[0] = {1, 0};
        CopyHostDataToGpu(data.data(), data.size(), async);
    }

  protected:
    using ParFunc = std::function<void(const std::vector<size_t> &, bool,
                                       const std::vector<Precision> &)>;
    using FMap = std::unordered_map<std::string, ParFunc>;

    StateVectorCudaBase(size_t num_qubits, cudaStream_t stream,
                        int device_id = 0, cudaStream_t stream_id = 0,
                        bool device_alloc = true)
        : StateVectorBase<Precision, Derived>(num_qubits), stream_{stream},
          data_buffer_{Util::exp2(num_qubits), device_id, stream_id,
                       device_alloc} {}
    StateVectorCudaBase(size_t num_qubits)
        : StateVectorCudaBase(num_qubits, 0, true) {}
    StateVectorCudaBase() = delete;

    virtual ~StateVectorCudaBase(){};

    /**
     * @brief Return the mapping of named gates to amount of control wires they
     * have.
     *
     * @return const std::unordered_map<std::string, std::size_t>&
     */
    auto getCtrlMap() -> const std::unordered_map<std::string, std::size_t> & {
        return ctrl_map_;
    }
    /**
     * @brief Utility method to get the mappings from gate to supported wires.
     *
     * @return const std::unordered_map<std::string, std::size_t>&
     */
    auto getParametricGatesMap()
        -> const std::unordered_map<std::string, std::size_t> & {
        return ctrl_map_;
    }

  private:
    cudaStream_t stream_;
    int device_id_;
    CUDA::DataBuffer<CFP_t> data_buffer_;
    const std::unordered_set<std::string> const_gates_{
        "Identity", "PauliX", "PauliY", "PauliZ", "Hadamard", "T",
        "S",        "CNOT",   "SWAP",   "CZ",     "CSWAP",    "Toffoli"};
    const std::unordered_map<std::string, std::size_t> ctrl_map_{
        // Add mapping from function name to required wires.
        {"Identity", 0},
        {"PauliX", 0},
        {"PauliY", 0},
        {"PauliZ", 0},
        {"Hadamard", 0},
        {"T", 0},
        {"S", 0},
        {"RX", 0},
        {"RY", 0},
        {"RZ", 0},
        {"Rot", 0},
        {"PhaseShift", 0},
        {"ControlledPhaseShift", 1},
        {"CNOT", 1},
        {"SWAP", 0},
        {"CZ", 1},
        {"CRX", 1},
        {"CRY", 1},
        {"CRZ", 1},
        {"CRot", 1},
        {"CSWAP", 1},
        {"Toffoli", 2}};
};

} // namespace Pennylane