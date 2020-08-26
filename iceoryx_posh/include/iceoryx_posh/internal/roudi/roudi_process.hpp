// Copyright (c) 2019 by Robert Bosch GmbH. All rights reserved.
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
#ifndef IOX_POSH_ROUDI_ROUDI_PROCESS_HPP
#define IOX_POSH_ROUDI_ROUDI_PROCESS_HPP

#include "iceoryx_posh/internal/mepoo/segment_manager.hpp"
#include "iceoryx_posh/internal/popo/receiver_port.hpp"
#include "iceoryx_posh/internal/popo/sender_port.hpp"
#include "iceoryx_posh/internal/roudi/introspection/process_introspection.hpp"
#include "iceoryx_posh/internal/roudi/port_manager.hpp"
#include "iceoryx_posh/internal/runtime/message_queue_interface.hpp"
#include "iceoryx_posh/mepoo/chunk_header.hpp"
#include "iceoryx_posh/version/compatibility_check_level.hpp"
#include "iceoryx_posh/version/version_info.hpp"
#include "iceoryx_utils/fixed_string/string100.hpp"
#include "iceoryx_utils/posix_wrapper/posix_access_rights.hpp"

#include <csignal>
#include <cstdint>
#include <ctime>
#include <list>

namespace iox
{
namespace roudi
{
class RouDiProcess
{
  public:
    /// @brief This class represents an application which has registered at RouDi and manages the communication to the
    /// application
    /// @param [in] name of the process; this is equal to the mqueue name, which is used for communication
    /// @param [in] pid is the host system process id
    /// @param [in] payloadMemoryManager is a pointer to the payload memory manager for this process
    /// @param [in] isMonitored indicates if the process should be monitored for being alive
    /// @param [in] payloadSegmentId is an identifier for the shm payload segment
    /// @param [in] sessionId is an ID generated by RouDi to prevent sending outdated mqueue transmission
    RouDiProcess(cxx::CString100 name,
                 int32_t pid,
                 mepoo::MemoryManager* payloadMemoryManager,
                 bool isMonitored,
                 const uint64_t payloadSegmentId,
                 const uint64_t sessionId) noexcept;

    RouDiProcess(const RouDiProcess& other) = delete;
    RouDiProcess& operator=(const RouDiProcess& other) = delete;
    /// @note the move cTor and assignment operator are already implicitly deleted because of the atomic
    RouDiProcess(RouDiProcess&& other) = delete;
    RouDiProcess& operator=(RouDiProcess&& other) = delete;
    ~RouDiProcess() = default;

    int32_t getPid() const noexcept;

    const cxx::CString100 getName() const noexcept;

    void sendToMQ(const runtime::MqMessage& data) noexcept;

    /// @brief The session ID which is used to check outdated mqueue transmissions for this process
    /// @return the session ID for this process
    uint64_t getSessionId() noexcept;

    void setTimestamp(const mepoo::TimePointNs timestamp) noexcept;

    mepoo::TimePointNs getTimestamp() noexcept;

    mepoo::MemoryManager* getPayloadMemoryManager() const noexcept;
    uint64_t getPayloadSegmentId() const noexcept;

    bool isMonitored() const noexcept;

  private:
    int m_pid;
    runtime::MqInterfaceUser m_mq;
    mepoo::TimePointNs m_timestamp;
    mepoo::MemoryManager* m_payloadMemoryManager{nullptr};
    bool m_isMonitored{true};
    uint64_t m_payloadSegmentId;
    std::atomic<uint64_t> m_sessionId;
};

class ProcessManagerInterface
{
  public:
    /// @brief This is an interface to send messages to processes handled by a ProcessManager
    /// @param [in] name of the process the message shall be delivered
    /// @param [in] message which shall be delivered
    /// @param [in] sessionId the sender expects to be currently valid
    /// @return true if the message was delivered, false otherwise
    [[gnu::deprecated]] virtual bool sendMessageToProcess(const cxx::CString100& name,
                                                          const iox::runtime::MqMessage& message,
                                                          const uint64_t sessionId) = 0;

    // port handling
    virtual ReceiverPortType addInternalReceiverPort(const capro::ServiceDescription& service,
                                                     const cxx::CString100& process_name) = 0;
    virtual SenderPortType addInternalSenderPort(const capro::ServiceDescription& service,
                                                 const cxx::CString100& process_name) = 0;
    virtual void removeInternalPorts(const cxx::CString100& process_name) = 0;
    virtual void sendServiceRegistryChangeCounterToProcess(const cxx::CString100& process_name) = 0;
    virtual bool areAllReceiverPortsSubscribed(const cxx::CString100& process_name) = 0;
    virtual void discoveryUpdate() = 0;

    // enable data-triggering -> based on receiver port
    virtual ~ProcessManagerInterface()
    {
    }
};

class ProcessManager : public ProcessManagerInterface
{
  public:
    /// @todo use a fixed, stack based list once available
    // using ProcessList_t = cxx::list<RouDiProcess, MAX_PROCESS_NUMBER>;
    using ProcessList_t = std::list<RouDiProcess>;
    using PortConfigInfo = iox::runtime::PortConfigInfo;

    ProcessManager(RouDiMemoryInterface& roudiMemoryInterface,
                   PortManager& portManager,
                   const version::CompatibilityCheckLevel compatibilityCheckLevel) noexcept;
    virtual ~ProcessManager() noexcept override = default;

    ProcessManager(const ProcessManager& other) = delete;
    ProcessManager& operator=(const ProcessManager& other) = delete;

    /// @brief Registers a process at the ProcessManager
    /// @param [in] name of the process which wants to register
    /// @param [in] pid is the host system process id
    /// @param [in] user is the posix user id to which the process belongs
    /// @param [in] isMonitored indicates if the process should be monitored for being alive
    /// @param [in] transmissionTimestamp is an ID for the application to check for the expected response
    /// @param [in] sessionId is an ID generated by RouDi to prevent sending outdated mqueue transmission
    /// @param [in] versionInfo Version of iceoryx used
    /// @return Returns if the process could be added successfully.
    bool registerProcess(const cxx::CString100& name,
                         int32_t pid,
                         posix::PosixUser user,
                         bool isMonitored,
                         int64_t transmissionTimestamp,
                         const uint64_t sessionId,
                         const version::VersionInfo& versionInfo) noexcept;

    void killAllProcesses() noexcept;

    void updateLivelinessOfProcess(const cxx::CString100& name) noexcept;

    void findServiceForProcess(const cxx::CString100& name, const capro::ServiceDescription& service) noexcept;

    void addInterfaceForProcess(const cxx::CString100& name,
                                capro::Interfaces interface,
                                const cxx::CString100& runnable) noexcept;

    void addApplicationForProcess(const cxx::CString100& name) noexcept;

    void addRunnableForProcess(const cxx::CString100& process, const cxx::CString100& runnable) noexcept;

    void addReceiverForProcess(const cxx::CString100& name,
                               const capro::ServiceDescription& service,
                               const cxx::CString100& runnable,
                               const PortConfigInfo& portConfigInfo = PortConfigInfo()) noexcept;

    void addSenderForProcess(const cxx::CString100& name,
                             const capro::ServiceDescription& service,
                             const cxx::CString100& runnable,
                             const PortConfigInfo& portConfigInfo = PortConfigInfo()) noexcept;

    void addConditionVariableForProcess(const cxx::CString100& processName) noexcept;

    void initIntrospection(ProcessIntrospectionType* processIntrospection) noexcept;

    void run() noexcept;

    SenderPortType addIntrospectionSenderPort(const capro::ServiceDescription& service,
                                              const cxx::CString100& process_name) noexcept;

    /// @brief Notify the application that it sent an unsupported message
    void sendMessageNotSupportedToRuntime(const cxx::CString100& name) noexcept;

    // BEGIN ProcessActivationInterface
    /// @brief This is an interface to send messages to processes handled by a ProcessManager
    /// @param [in] name of the process the message shall be delivered
    /// @param [in] message which shall be delivered
    /// @param [in] sessionId the sender expects to be currently valid
    /// @return true if the message was delivered, false otherwise
    [[gnu::deprecated]] bool sendMessageToProcess(const cxx::CString100& name,
                                                  const iox::runtime::MqMessage& message,
                                                  const uint64_t sessionId) noexcept override;
    // END

    // BEGIN PortHandling interface
    ReceiverPortType addInternalReceiverPort(const capro::ServiceDescription& service,
                                             const cxx::CString100& process_name) noexcept override;
    SenderPortType addInternalSenderPort(const capro::ServiceDescription& service,
                                         const cxx::CString100& process_name) noexcept override;
    void removeInternalPorts(const cxx::CString100& process_name) noexcept override;
    void sendServiceRegistryChangeCounterToProcess(const cxx::CString100& process_name) noexcept override;

    bool areAllReceiverPortsSubscribed(const cxx::CString100& process_name) noexcept override;
    // END

  private:
    RouDiProcess* getProcessFromList(const cxx::CString100& name) noexcept;
    void monitorProcesses() noexcept;
    void discoveryUpdate() noexcept override;

    /// @param [in] name of the process; this is equal to the mqueue name, which is used for communication
    /// @param [in] pid is the host system process id
    /// @param [in] payloadMemoryManager is a pointer to the payload memory manager for this process
    /// @param [in] isMonitored indicates if the process should be monitored for being alive
    /// @param [in] transmissionTimestamp is an ID for the application to check for the expected response
    /// @param [in] payloadSegmentId is an identifier for the shm payload segment
    /// @param [in] sessionId is an ID generated by RouDi to prevent sending outdated mqueue transmission
    /// @param [in] versionInfo Version of iceoryx used
    /// @return Returns if the process could be added successfully.
    bool addProcess(const cxx::CString100& name,
                    int32_t pid,
                    mepoo::MemoryManager* payloadMemoryManager,
                    bool isMonitored,
                    int64_t transmissionTimestamp,
                    const uint64_t payloadSegmentId,
                    const uint64_t sessionId,
                    const version::VersionInfo& versionInfo) noexcept;

    bool removeProcess(const cxx::CString100& name) noexcept;
    RouDiMemoryInterface& m_roudiMemoryInterface;
    PortManager& m_portManager;
    mepoo::SegmentManager<>* m_segmentManager{nullptr};
    mepoo::MemoryManager* m_introspectionMemoryManager{nullptr};
    RelativePointer::id_t m_mgmtSegmentId{RelativePointer::NULL_POINTER_ID};
    mutable std::mutex m_mutex;

    ProcessList_t m_processList;

    ProcessIntrospectionType* m_processIntrospection{nullptr};

    // this is currently used for the internal sender/receiver ports
    mepoo::MemoryManager* m_memoryManagerOfCurrentProcess{nullptr};
    version::CompatibilityCheckLevel m_compatibilityCheckLevel;
};

} // namespace roudi
} // namespace iox

#endif // IOX_POSH_ROUDI_ROUDI_PROCESS_HPP
