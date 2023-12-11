// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <tuple>
#include "common/assert.h"
#include "core/core.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/result.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/sm/srv.h"

namespace Service::SM {

static ResultCode ValidateServiceName(const std::string& name) {
    R_UNLESS(name.size() > 0 && name.size() <= 8, ERR_INVALID_NAME_SIZE);
    R_UNLESS(name.find('\0') == std::string::npos, ERR_NAME_CONTAINS_NUL);
    return RESULT_SUCCESS;
}

ServiceManager::ServiceManager(Core::System& system) : system(system) {}

void ServiceManager::InstallInterfaces(Core::System& system) {
    ASSERT(system.ServiceManager().srv_interface.expired());

    auto srv = std::make_shared<SRV>(system);
    srv->InstallAsNamedPort(system.Kernel());
    system.ServiceManager().srv_interface = srv;
}

ResultCode ServiceManager::RegisterService(std::shared_ptr<Kernel::ServerPort>& server_port,
                                           std::string name, u32 max_sessions) {
    R_TRY(ValidateServiceName(name));
    R_UNLESS(registered_services.find(name) == registered_services.end(), ERR_ALREADY_REGISTERED);

    std::shared_ptr<Kernel::ClientPort> client_port;
    std::tie(server_port, client_port) = system.Kernel().CreatePortPair(max_sessions, name);
    registered_services_inverse.emplace(client_port->GetObjectId(), name);
    registered_services.emplace(std::move(name), std::move(client_port));

    return RESULT_SUCCESS;
}

ResultCode ServiceManager::GetServicePort(std::shared_ptr<Kernel::ClientPort>& out_port,
                                          const std::string& name) {
    R_TRY(ValidateServiceName(name));

    auto it = registered_services.find(name);
    R_UNLESS(it != registered_services.end(), ERR_SERVICE_NOT_REGISTERED);

    out_port = it->second;
    return RESULT_SUCCESS;
}

ResultCode ServiceManager::ConnectToService(std::shared_ptr<Kernel::ClientSession>& session,
                                            const std::string& name) {
    std::shared_ptr<Kernel::ClientPort> client_port;
    R_TRY(GetServicePort(client_port, name));
    return client_port->Connect(session);
}

std::string ServiceManager::GetServiceNameByPortId(u32 port) const {
    if (registered_services_inverse.count(port)) {
        return registered_services_inverse.at(port);
    }

    return "";
}

} // namespace Service::SM
