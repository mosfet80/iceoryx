// Copyright (c) 2020 by Robert Bosch GmbH. All rights reserved.
// Copyright (c) 2020 - 2022 by Apex.AI Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef IOX_POSH_POPO_BUILDING_BLOCKS_VARIANT_QUEUE_HPP
#define IOX_POSH_POPO_BUILDING_BLOCKS_VARIANT_QUEUE_HPP

#include "iox/assertions.hpp"
#include "iox/detail/mpmc_resizeable_lockfree_queue.hpp"
#include "iox/detail/spsc_fifo.hpp"
#include "iox/detail/spsc_sofi.hpp"
#include "iox/optional.hpp"
#include "iox/variant.hpp"

#include <cstdint>

namespace iox
{
namespace popo
{
/// @brief list of the supported underlying queue types
/// @note  if a new queue type is added the following steps have to be
///         performed:
///         1. add queue type here
///         2. add queue type in m_fifo data member variant type
///         3. increase numberOfQueueTypes in test_popo_variant_queue test
enum class VariantQueueTypes : uint64_t
{
    FiFo_SingleProducerSingleConsumer = 0,
    SoFi_SingleProducerSingleConsumer = 1,
    FiFo_MultiProducerSingleConsumer = 2,
    SoFi_MultiProducerSingleConsumer = 3
};

// remark: we need to consider to support the non-resizable queue as well
//         since it should have performance benefits if resize is not actually needed
//         for now we just use the most general variant, which allows resizing

/// @brief wrapper of multiple fifo's
/// @param[in] ValueType type which should be stored
/// @param[in] Capacity capacity of the underlying fifo
/// @code
///     popo::VariantQueue<int, 5> nonOverflowingQueue(popo::VariantQueueTypes::FiFo_SingleProducerSingleConsumer);
///     popo::VariantQueue<int, 5> overflowingQueue(popo::VariantQueueTypes::SoFi_SingleProducerSingleConsumer);
///
///     // overflow case
///     auto status = nonOverflowingQueue.push(123);
///     if ( !status ) {
///         IOX_LOG(Info, "queue is full");
///     }
///
///     auto overriddenElement = overflowingQueue.push(123);
///     if ( overriddenElement->has_value() ) {
///         IOX_LOG(Info, "element " << overriddenElement->value() << " was overridden");
///     }
/// @endcode
template <typename ValueType, uint64_t Capacity>
class VariantQueue
{
  public:
    using fifo_t = variant<concurrent::SpscFifo<ValueType, Capacity>,
                           concurrent::SpscSofi<ValueType, Capacity>,
                           concurrent::MpmcResizeableLockFreeQueue<ValueType, Capacity>,
                           concurrent::MpmcResizeableLockFreeQueue<ValueType, Capacity>>;

    /// @brief Constructor of a VariantQueue
    /// @param[in] type type of the underlying queue
    explicit VariantQueue(const VariantQueueTypes type) noexcept;

    /// @brief pushs an element into the fifo
    /// @param[in] value value which should be added in the fifo
    /// @return if the underlying queue has an overflow the optional will contain
    ///         the value which was overridden (SOFI) or which was dropped (FIFO)
    ///         otherwise the optional contains nullopt_t
    optional<ValueType> push(const ValueType& value) noexcept;

    /// @brief pops an element from the fifo
    /// @return if the fifo did contain an element it is returned inside the optional
    ///         otherwise the optional contains nullopt_t
    optional<ValueType> pop() noexcept;

    /// @brief returns true if empty otherwise true
    bool empty() const noexcept;

    /// @brief get the current size of the queue. Caution, another thread can have changed the size just after reading
    /// it
    /// @return queue size
    uint64_t size() noexcept;

    /// @brief set the capacity of the queue
    /// @param[in] newCapacity valid values are 0 < newCapacity < MAX_SUBSCRIBER_QUEUE_CAPACITY
    /// @return true if setting the new capacity succeeded, false otherwise
    /// @pre it is important that no pop or push calls occur during
    ///         this call
    /// @note depending on the internal queue used, concurrent pushes and pops are possible
    ///       (for FiFo_MultiProducerSingleConsumer and SoFi_MultiProducerSingleConsumer)
    /// @concurrent not thread safe
    bool setCapacity(const uint64_t newCapacity) noexcept;

    /// @brief get the capacity of the queue.
    /// @return queue size
    uint64_t capacity() const noexcept;

  private:
    const VariantQueueTypes m_type;
    fifo_t m_fifo;
};
} // namespace popo
} // namespace iox

#include "iceoryx_posh/internal/popo/building_blocks/variant_queue.inl"

#endif // IOX_POSH_POPO_BUILDING_BLOCKS_VARIANT_QUEUE_HPP
