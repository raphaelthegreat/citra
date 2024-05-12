// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/file_sys/archive_artic.h"
#include "core/file_sys/secure_value_backend_artic.h"

SERIALIZE_EXPORT_IMPL(FileSys::ArticSecureValueBackend)

namespace FileSys {
Result ArticSecureValueBackend::ObsoletedSetSaveDataSecureValue(u32 unique_id, u8 title_variation,
                                                                u32 secure_value_slot,
                                                                u64 secure_value) {
    auto req = client->NewRequest("FSUSER_ObsSetSaveDataSecureVal");

    req.AddParameter(secure_value);
    req.AddParameter(secure_value_slot);
    req.AddParameter(unique_id);
    req.AddParameter(title_variation);

    return ArticArchive::RespResult(client->Send(req));
}

ResultVal<std::tuple<bool, u64>> ArticSecureValueBackend::ObsoletedGetSaveDataSecureValue(
    u32 unique_id, u8 title_variation, u32 secure_value_slot) {

    auto req = client->NewRequest("FSUSER_ObsGetSaveDataSecureVal");

    req.AddParameter(secure_value_slot);
    req.AddParameter(unique_id);
    req.AddParameter(title_variation);

    auto resp = client->Send(req);
    auto res = ArticArchive::RespResult(resp);
    if (res.IsError())
        return res;

    struct {
        bool exists;
        u64 secure_value;
    } secure_value_result;
    static_assert(sizeof(secure_value_result) == 0x10);

    auto output_buf = resp->GetResponseBuffer(0);
    if (!output_buf.has_value())
        return res;

    if (output_buf->second != sizeof(secure_value_result))
        return ResultUnknown;

    memcpy(&secure_value_result, output_buf->first, output_buf->second);
    return std::make_tuple(secure_value_result.exists, secure_value_result.secure_value);
}

Result ArticSecureValueBackend::ControlSecureSave(u32 action, u8* input, size_t input_size,
                                                  u8* output, size_t output_size) {
    auto req = client->NewRequest("FSUSER_ControlSecureSave");

    req.AddParameter(action);
    req.AddParameterBuffer(std::span{input, input_size});
    req.AddParameter(static_cast<u32>(output_size));

    auto resp = client->Send(req);
    auto res = ArticArchive::RespResult(resp);
    if (res.IsError())
        return res;

    auto output_buf = resp->GetResponseBuffer(0);
    if (!output_buf.has_value())
        return res;

    if (output_buf->second != output_size)
        return ResultUnknown;

    memcpy(output, output_buf->first, output_buf->second);
    return res;
}

Result ArticSecureValueBackend::SetThisSaveDataSecureValue(u32 secure_value_slot,
                                                           u64 secure_value) {
    auto req = client->NewRequest("FSUSER_SetThisSaveDataSecVal");

    req.AddParameter(secure_value_slot);
    req.AddParameter(secure_value);

    return ArticArchive::RespResult(client->Send(req));
}

ResultVal<std::tuple<bool, bool, u64>> ArticSecureValueBackend::GetThisSaveDataSecureValue(
    u32 secure_value_slot) {
    auto req = client->NewRequest("FSUSER_GetThisSaveDataSecVal");

    req.AddParameter(secure_value_slot);

    auto resp = client->Send(req);
    auto res = ArticArchive::RespResult(resp);
    if (res.IsError())
        return res;

    struct {
        bool exists;
        bool isGamecard;
        u64 secure_value;
    } secure_value_result;
    static_assert(sizeof(secure_value_result) == 0x10);

    auto output_buf = resp->GetResponseBuffer(0);
    if (!output_buf.has_value())
        return res;

    if (output_buf->second != sizeof(secure_value_result))
        return ResultUnknown;

    memcpy(&secure_value_result, output_buf->first, output_buf->second);
    return std::make_tuple(secure_value_result.exists, secure_value_result.isGamecard,
                           secure_value_result.secure_value);
}
} // namespace FileSys