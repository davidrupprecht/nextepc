#define TRACE_MODULE _nasmsg

#include "core_debug.h"
#include "nas_message.h"

c_int32_t nas_decode_attach_request(nas_message_t *message, pkbuf_t *pkbuf)
{
    nas_attach_request_t *attach_request = &message->emm.attach_request;
    c_uint16_t decoded = pkbuf->len;
    c_int32_t size = 0;
    status_t rv;

    size = nas_decode_attach_type(&attach_request->attach_type, pkbuf);
    d_assert(size >= 0, return -1, "decode failed");

    size = nas_decode_eps_mobile_identity(
            &attach_request->eps_mobile_identity, pkbuf);
    d_assert(size >= 0, return -1, "decode failed");

    size = nas_decode_ue_network_capability(
            &attach_request->ue_network_capability, pkbuf);
    d_assert(size >= 0, return -1, "decode failed");

    size = nas_decode_esm_message_container(
            &attach_request->esm_message_container, pkbuf);
    d_assert(size >= 0, return -1, "decode failed");

    while(pkbuf->len > 0) 
    {
        c_uint8_t *buffer = pkbuf->payload;
        c_uint8_t type = (*buffer) >= 0x80 ? ((*buffer) & 0xf0) : (*buffer);

        rv = pkbuf_header(pkbuf, -(c_int16_t)(sizeof(c_uint8_t)));
        d_assert(rv == CORE_OK, return -1, "pkbuf_header error");

        switch(type)
        {
            case NAS_ATTACH_REQUEST_OLD_P_TMSI_SIGNATURE_IEI:
                size = nas_decode_p_tmsi_signature(
                        &attach_request->old_p_tmsi_signature, pkbuf);
                d_assert(size >= 0, return -1, "decode failed");
                attach_request->presencemask |=
                    NAS_ATTACH_REQUEST_OLD_P_TMSI_SIGNATURE_PRESENT;
                break;
            case NAS_ATTACH_REQUEST_ADDITIONAL_GUTI_IEI:
                size = nas_decode_eps_mobile_identity(
                        &attach_request->additional_guti, pkbuf);
                d_assert(size >= 0, return -1, "decode failed");
                attach_request->presencemask |=
                    NAS_ATTACH_REQUEST_ADDITIONAL_GUTI_PRESENT;
                break;
            case NAS_ATTACH_REQUEST_LAST_VISITED_REGISTERED_TAI_IEI:
                size = nas_decode_tracking_area_identity(
                        &attach_request->last_visited_registered_tai, pkbuf);
                d_assert(size >= 0, return -1, "decode failed");
                attach_request->presencemask |=
                    NAS_ATTACH_REQUEST_LAST_VISITED_REGISTERED_TAI_IEI;
                break;
            case NAS_ATTACH_REQUEST_DRX_PARAMETER_IEI:
                size = nas_decode_drx_parameter(
                        &attach_request->drx_parameter, pkbuf);
                d_assert(size >= 0, return -1, "decode failed");
                attach_request->presencemask |=
                    NAS_ATTACH_REQUEST_DRX_PARAMETER_PRESENT;
                break;
            case NAS_ATTACH_REQUEST_MS_NETWORK_CAPABILITY_IEI:
                size = nas_decode_ms_network_capability(
                        &attach_request->ms_network_capability, pkbuf);
                d_assert(size >= 0, return -1, "decode failed");
                attach_request->presencemask |=
                    NAS_ATTACH_REQUEST_MS_NETWORK_FEATURE_SUPPORT_PRESENT;
                break;
            case NAS_ATTACH_REQUEST_OLD_LOCATION_AREA_IDENTIFICATION_IEI:
                size = nas_decode_location_area_identification(
                        &attach_request->old_location_area_identification, pkbuf);
                d_assert(size >= 0, return -1, "decode failed");
                attach_request->presencemask |=
                    NAS_ATTACH_REQUEST_OLD_LOCATION_AREA_IDENTIFICATION_PRESENT;
                break;
            case NAS_ATTACH_REQUEST_TMSI_STATUS_IEI:
                size = nas_decode_tmsi_status(
                        &attach_request->tmsi_status, pkbuf);
                d_assert(size >= 0, return -1, "decode failed");
                attach_request->presencemask |=
                    NAS_ATTACH_REQUEST_TMSI_STATUS_PRESENT;
                break;
            case NAS_ATTACH_REQUEST_MOBILE_STATION_CLASSMARK_2_IEI:
                size = nas_decode_mobile_station_classmark_2(
                        &attach_request->mobile_station_classmark_2, pkbuf);
                d_assert(size >= 0, return -1, "decode failed");
                attach_request->presencemask |=
                    NAS_ATTACH_REQUEST_MOBILE_STATION_CLASSMARK_2_PRESENT;
                break;
            case NAS_ATTACH_REQUEST_MOBILE_STATION_CLASSMARK_3_IEI:
                size = nas_decode_mobile_station_classmark_3(
                        &attach_request->mobile_station_classmark_3, pkbuf);
                d_assert(size >= 0, return -1, "decode failed");
                attach_request->presencemask |=
                    NAS_ATTACH_REQUEST_MOBILE_STATION_CLASSMARK_3_PRESENT;
                break;
            case NAS_ATTACH_REQUEST_SUPPORTED_CODECS_IEI:
                size = nas_decode_supported_codec_list(
                        &attach_request->supported_codecs, pkbuf);
                d_assert(size >= 0, return -1, "decode failed");
                attach_request->presencemask |=
                    NAS_ATTACH_REQUEST_SUPPORTED_CODECS_PRESENT;
                break;
            case NAS_ATTACH_REQUEST_ADDITIONAL_UPDATE_TYPE_IEI:
                size = nas_decode_additional_update_type(
                        &attach_request->additional_update_type, pkbuf);
                d_assert(size >= 0, return -1, "decode failed");
                attach_request->presencemask |=
                    NAS_ATTACH_REQUEST_ADDITIONAL_UPDATE_TYPE_PRESENT;
                break;
            case NAS_ATTACH_REQUEST_VOICE_DOMAIN_PREFERENCE_AND_UE_USAGE_SETTING_IEI:
                size = nas_decode_voice_domain_preference_and_ue_usage_setting(
                        &attach_request->voice_domain_preference_and_ue_usage_setting, pkbuf);
                d_assert(size >= 0, return -1, "decode failed");
                attach_request->presencemask |=
                    NAS_ATTACH_REQUEST_VOICE_DOMAIN_PREFERENCE_AND_UE_USAGE_SETTING_PRESENT;
                break;
            case NAS_ATTACH_REQUEST_DEVICE_PROPERTIES_IEI:
                size = nas_decode_device_properties(
                        &attach_request->device_properties, pkbuf);
                d_assert(size >= 0, return -1, "decode failed");
                attach_request->presencemask |=
                    NAS_ATTACH_REQUEST_DEVICE_PROPERTIES_PRESENT;
                break;
            case NAS_ATTACH_REQUEST_OLD_GUTI_TYPE_IEI:
                size = nas_decode_guti_type(
                        &attach_request->old_guti_type, pkbuf);
                d_assert(size >= 0, return -1, "decode failed");
                attach_request->presencemask |=
                    NAS_ATTACH_REQUEST_OLD_GUTI_TYPE_PRESENT;
                break;
            case NAS_ATTACH_REQUEST_MS_NETWORK_FEATURE_SUPPORT_IEI:
                size = nas_decode_ms_network_feature_support(
                        &attach_request->ms_network_feature_support, pkbuf);
                d_assert(size >= 0, return -1, "decode failed");
                attach_request->presencemask |=
                    NAS_ATTACH_REQUEST_MS_NETWORK_FEATURE_SUPPORT_PRESENT;
                break;
            case NAS_ATTACH_REQUEST_TMSI_BASED_NRI_CONTAINER_IEI:
                size = nas_decode_network_resource_identifier_container(
                        &attach_request->tmsi_based_nri_container, pkbuf);
                d_assert(size >= 0, return -1, "decode failed");
                attach_request->presencemask |=
                    NAS_ATTACH_REQUEST_TMSI_BASED_NRI_CONTAINER_PRESENT;
                break;
            case NAS_ATTACH_REQUEST_T3324_VALUE_IEI:
                size = nas_decode_gprs_timer_2(
                        &attach_request->t3324_value, pkbuf);
                d_assert(size >= 0, return -1, "decode failed");
                attach_request->presencemask |=
                    NAS_ATTACH_REQUEST_T3324_VALUE_PRESENT;
                break;
            case NAS_ATTACH_REQUEST_T3412_EXTENDED_VALUE_IEI:
                size = nas_decode_gprs_timer_3(
                        &attach_request->t3412_extended_value, pkbuf);
                d_assert(size >= 0, return -1, "decode failed");
                attach_request->presencemask |=
                    NAS_ATTACH_REQUEST_T3412_EXTENDED_VALUE_PRESENT;
                break;
            case NAS_ATTACH_REQUEST_EXTENDED_DRX_PARAMETERS_IEI:
                size = nas_decode_extended_drx_parameters(
                        &attach_request->extended_drx_parameters, pkbuf);
                d_assert(size >= 0, return -1, "decode failed");
                attach_request->presencemask |=
                    NAS_ATTACH_REQUEST_EXTENDED_DRX_PARAMETERS_PRESENT;
                break;
            default:
                d_error("Unknown type(0x%x) or not implemented\n", type);
                return -1;
        }

    }

    return decoded;
}

status_t nas_decode_pdu(nas_message_t *message, pkbuf_t *pkbuf)
{
    status_t rv = CORE_ERROR;
    nas_security_header_t *security_header = NULL;
    c_uint16_t decoded = pkbuf->len;
    c_uint16_t size = 0;

    d_assert(pkbuf, return CORE_ERROR, "Null param");
    d_assert(pkbuf->payload, return CORE_ERROR, "Null param");

    security_header = pkbuf->payload;
    if (security_header->security_header_type != 
            NAS_SECURITY_HEADER_PLAIN_NAS_MESSAGE)
    {
        rv = pkbuf_header(pkbuf, -(c_int16_t)(sizeof(nas_security_header_t)));
        d_assert(rv == CORE_OK, return CORE_ERROR, "pkbuf_header error");
    }

    memset(message, 0, sizeof(nas_message_t));

    size = sizeof(nas_header_t);
    rv = pkbuf_header(pkbuf, -size);
    d_assert(rv == CORE_OK, return CORE_ERROR, "pkbuf_header error");
    memcpy(&message->h, pkbuf->payload - size, size);

    switch(message->h.message_type)
    {
        case NAS_ATTACH_REQUEST:
            size = nas_decode_attach_request(message, pkbuf);
            d_assert(size >= CORE_OK, return CORE_ERROR, "decode error");
            break;
        case NAS_ATTACH_ACCEPT:
        case NAS_ATTACH_COMPLETE:
        case NAS_ATTACH_REJECT:
        case NAS_DETACH_REQUEST:
        case NAS_DETACH_ACCEPT:
        case NAS_TRACKING_AREA_UPDATE_REQUEST:
        case NAS_TRACKING_AREA_UPDATE_ACCEPT:
        case NAS_TRACKING_AREA_UPDATE_COMPLETE:
        case NAS_TRACKING_AREA_UPDATE_REJECT:
        case NAS_EXTENDED_SERVICE_REQUEST:
        case NAS_SERVICE_REJECT:
        case NAS_GUTI_REALLOCATION_COMMAND:
        case NAS_GUTI_REALLOCATION_COMPLETE:
        case NAS_AUTHENTICATION_REQUEST:
        case NAS_AUTHENTICATION_RESPONSE:
        case NAS_AUTHENTICATION_REJECT:
        case NAS_AUTHENTICATION_FAILURE:
        case NAS_IDENTITY_REQUEST:
        case NAS_IDENTITY_RESPONSE:
        case NAS_SECURITY_MODE_COMMAND:
        case NAS_SECURITY_MODE_COMPLETE:
        case NAS_SECURITY_MODE_REJECT:
        case NAS_EMM_STATUS:
        case NAS_EMM_INFORMATION:
        case NAS_DOWNLINK_NAS_TRANSPORT:
        case NAS_UPLINK_NAS_TRANSPORT:
        case NAS_CS_SERVICE_NOTIFICATION:
        case NAS_DOWNLINK_GENERIC_NAS_TRANSPORT:
        case NAS_UPLINK_GENERIC_NAS_TRANSPORT:

        case NAS_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_REQUEST:
        case NAS_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_ACCEPT:
        case NAS_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_REJECT:
        case NAS_ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_REQUEST:
        case NAS_ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_ACCEPT:
        case NAS_ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_REJECT:
        case NAS_MODIFY_EPS_BEARER_CONTEXT_REQUEST:
        case NAS_MODIFY_EPS_BEARER_CONTEXT_ACCEPT:
        case NAS_MODIFY_EPS_BEARER_CONTEXT_REJECT:
        case NAS_DEACTIVATE_EPS_BEARER_CONTEXT_REQUEST:
        case NAS_DEACTIVATE_EPS_BEARER_CONTEXT_ACCEPT:
        case NAS_PDN_CONNECTIVITY_REQUEST:
        case NAS_PDN_CONNECTIVITY_REJECT:
        case NAS_PDN_DISCONNECT_REQUEST:
        case NAS_PDN_DISCONNECT_REJECT:
        case NAS_BEARER_RESOURCE_ALLOCATION_REQUEST:
        case NAS_BEARER_RESOURCE_ALLOCATION_REJECT:
        case NAS_BEARER_RESOURCE_MODIFICATION_REQUEST:
        case NAS_BEARER_RESOURCE_MODIFICATION_REJECT:
        case NAS_ESM_INFORMATION_REQUEST:
        case NAS_ESM_INFORMATION_RESPONSE:
        case NAS_ESM_STATUS:
            break;
        default:
            d_error("Unknown message type (%d) or not implemented", 
                    message->h.message_type);
            break;
    }

    rv = pkbuf_header(pkbuf, decoded);
    d_assert(rv == CORE_OK, return CORE_ERROR, "pkbuf_header error");

    return CORE_OK;
}

c_int32_t nas_encode_attach_accept(pkbuf_t *pkbuf, nas_message_t *message)
{
    nas_attach_accept_t *attach_accept = &message->emm.attach_accept;
    c_int32_t size = 0;
    c_int32_t encoded = 0;

    size = nas_encode_attach_result(pkbuf, &attach_accept->attach_result);
    d_assert(size >= 0, return encoded, "decode failed");
    encoded += size;

    size = nas_encode_gprs_timer(pkbuf, &attach_accept->t3412_value);
    d_assert(size >= 0, return encoded, "decode failed");
    encoded += size;

    size = nas_encode_tracking_area_identity_list(pkbuf, 
            &attach_accept->tai_list);
    d_assert(size >= 0, return encoded, "decode failed");
    encoded += size;

    size = nas_encode_esm_message_container(pkbuf, 
            &attach_accept->esm_message_container);
    d_assert(size >= 0, return encoded, "decode failed");
    encoded += size;

    if (attach_accept->presencemask & NAS_ATTACH_ACCEPT_GUTI_PRESENT)
    {
        size = nas_encode_iei(pkbuf, NAS_ATTACH_ACCEPT_GUTI_IEI);
        d_assert(size >= 0, return encoded, "decode failed");
        encoded += size;

        size = nas_encode_eps_mobile_identity(pkbuf, &attach_accept->guti);
        d_assert(size >= 0, return encoded, "decode failed");
        encoded += size;
    }
    if (attach_accept->presencemask & 
            NAS_ATTACH_ACCEPT_LOCATION_AREA_IDENTIFICATION_PRESENT)
    {
    }
    if (attach_accept->presencemask & NAS_ATTACH_ACCEPT_MS_IDENTITY_PRESENT)
    {
    }
    if (attach_accept->presencemask & NAS_ATTACH_ACCEPT_EMM_CAUSE_PRESENT)
    {
    }
    if (attach_accept->presencemask & NAS_ATTACH_ACCEPT_T3402_VALUE_PRESENT)
    {
    }
    if (attach_accept->presencemask & NAS_ATTACH_ACCEPT_T3423_VALUE_PRESENT)
    {
    }
    if (attach_accept->presencemask & 
            NAS_ATTACH_ACCEPT_EQUIVALENT_PLMNS_PRESENT)
    {
    }
    if (attach_accept->presencemask & 
            NAS_ATTACH_ACCEPT_EMERGENCY_NUMBER_LIST_PRESENT)
    {
    }
    if (attach_accept->presencemask & 
            NAS_ATTACH_ACCEPT_EPS_NETWORK_FEATURE_SUPPORT_PRESENT)
    {
    }
    if (attach_accept->presencemask & 
            NAS_ATTACH_ACCEPT_ADDITIONAL_UPDATE_RESULT_PRESENT)
    {
    }
    if (attach_accept->presencemask & 
            NAS_ATTACH_ACCEPT_T3412_EXTENDED_VALUE_PRESENT)
    {
    }
    if (attach_accept->presencemask & NAS_ATTACH_ACCEPT_T3324_VALUE_PRESENT)
    {
    }
    if (attach_accept->presencemask & 
            NAS_ATTACH_ACCEPT_EXTENDED_DRX_PARAMETERS_PRESENT)
    {
    }

    return encoded;
}

status_t nas_encode_pdu(pkbuf_t **pkbuf, nas_message_t *message)
{
    status_t rv = CORE_ERROR;
    c_int32_t size = 0;
    c_int32_t encoded = 0;

    d_assert(message, return CORE_ERROR, "Null param");

    *pkbuf = pkbuf_alloc(0, NAS_SDU_SIZE);
    d_assert(*pkbuf, return -1, "Null Param");

    size = sizeof(nas_header_t);
    rv = pkbuf_header(*pkbuf, -size);
    d_assert(rv == CORE_OK, return CORE_ERROR, "pkbuf_header error");
    memcpy((*pkbuf)->payload - size, &message->h, size);
    encoded += size;

    switch(message->h.message_type)
    {
        case NAS_ATTACH_REQUEST:
            break;
        case NAS_ATTACH_ACCEPT:
            size = nas_encode_attach_accept(*pkbuf, message);
            d_assert(size >= 0, return CORE_ERROR, "decode error");
            encoded += size;
            break;
        case NAS_ATTACH_COMPLETE:
        case NAS_ATTACH_REJECT:
        case NAS_DETACH_REQUEST:
        case NAS_DETACH_ACCEPT:
        case NAS_TRACKING_AREA_UPDATE_REQUEST:
        case NAS_TRACKING_AREA_UPDATE_ACCEPT:
        case NAS_TRACKING_AREA_UPDATE_COMPLETE:
        case NAS_TRACKING_AREA_UPDATE_REJECT:
        case NAS_EXTENDED_SERVICE_REQUEST:
        case NAS_SERVICE_REJECT:
        case NAS_GUTI_REALLOCATION_COMMAND:
        case NAS_GUTI_REALLOCATION_COMPLETE:
        case NAS_AUTHENTICATION_REQUEST:
        case NAS_AUTHENTICATION_RESPONSE:
        case NAS_AUTHENTICATION_REJECT:
        case NAS_AUTHENTICATION_FAILURE:
        case NAS_IDENTITY_REQUEST:
        case NAS_IDENTITY_RESPONSE:
        case NAS_SECURITY_MODE_COMMAND:
        case NAS_SECURITY_MODE_COMPLETE:
        case NAS_SECURITY_MODE_REJECT:
        case NAS_EMM_STATUS:
        case NAS_EMM_INFORMATION:
        case NAS_DOWNLINK_NAS_TRANSPORT:
        case NAS_UPLINK_NAS_TRANSPORT:
        case NAS_CS_SERVICE_NOTIFICATION:
        case NAS_DOWNLINK_GENERIC_NAS_TRANSPORT:
        case NAS_UPLINK_GENERIC_NAS_TRANSPORT:

        case NAS_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_REQUEST:
        case NAS_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_ACCEPT:
        case NAS_ACTIVATE_DEFAULT_EPS_BEARER_CONTEXT_REJECT:
        case NAS_ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_REQUEST:
        case NAS_ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_ACCEPT:
        case NAS_ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_REJECT:
        case NAS_MODIFY_EPS_BEARER_CONTEXT_REQUEST:
        case NAS_MODIFY_EPS_BEARER_CONTEXT_ACCEPT:
        case NAS_MODIFY_EPS_BEARER_CONTEXT_REJECT:
        case NAS_DEACTIVATE_EPS_BEARER_CONTEXT_REQUEST:
        case NAS_DEACTIVATE_EPS_BEARER_CONTEXT_ACCEPT:
        case NAS_PDN_CONNECTIVITY_REQUEST:
        case NAS_PDN_CONNECTIVITY_REJECT:
        case NAS_PDN_DISCONNECT_REQUEST:
        case NAS_PDN_DISCONNECT_REJECT:
        case NAS_BEARER_RESOURCE_ALLOCATION_REQUEST:
        case NAS_BEARER_RESOURCE_ALLOCATION_REJECT:
        case NAS_BEARER_RESOURCE_MODIFICATION_REQUEST:
        case NAS_BEARER_RESOURCE_MODIFICATION_REJECT:
        case NAS_ESM_INFORMATION_REQUEST:
        case NAS_ESM_INFORMATION_RESPONSE:
        case NAS_ESM_STATUS:
            break;
        default:
            d_error("Unknown message type (%d) or not implemented", 
                    message->h.message_type);
            break;
    }

    rv = pkbuf_header(*pkbuf, encoded);
    d_assert(rv == CORE_OK, return CORE_ERROR, "pkbuf_header error");

    (*pkbuf)->len = encoded;

    return CORE_OK;
}