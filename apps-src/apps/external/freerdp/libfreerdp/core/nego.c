/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Protocol Security Negotiation
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 Norbert Federa <norbert.federa@thincast.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if defined(_WIN32) || defined(ANDROID) || defined(__APPLE__)
#include "freerdp_config.h"
#endif

#include <winpr/crt.h>

#include <freerdp/log.h>

#include "tpkt.h"

#include "nego.h"

#include "transport.h"

#define TAG FREERDP_TAG("core.nego")

struct rdp_nego
{
	UINT16 port;
	UINT32 flags;
	const char* hostname;
	char* cookie;
	BYTE* RoutingToken;
	DWORD RoutingTokenLength;
	BOOL SendPreconnectionPdu;
	UINT32 PreconnectionId;
	char* PreconnectionBlob;

	NEGO_STATE state;
	BOOL TcpConnected;
	BOOL SecurityConnected;
	UINT32 CookieMaxLength;

	BOOL sendNegoData;
	UINT32 SelectedProtocol;
	UINT32 RequestedProtocols;
	BOOL NegotiateSecurityLayer;
	BOOL EnabledProtocols[16];
	BOOL RestrictedAdminModeRequired;
	BOOL GatewayEnabled;
	BOOL GatewayBypassLocal;

	rdpTransport* transport;
};

static const char* nego_state_string(NEGO_STATE state)
{
	static const char* const NEGO_STATE_STRINGS[] = { "NEGO_STATE_INITIAL", "NEGO_STATE_EXT",
		                                              "NEGO_STATE_NLA",     "NEGO_STATE_TLS",
		                                              "NEGO_STATE_RDP",     "NEGO_STATE_FAIL",
		                                              "NEGO_STATE_FINAL",   "NEGO_STATE_INVALID" };
	if (state >= ARRAYSIZE(NEGO_STATE_STRINGS))
		return NEGO_STATE_STRINGS[ARRAYSIZE(NEGO_STATE_STRINGS) - 1];
	return NEGO_STATE_STRINGS[state];
}

static const char* protocol_security_string(UINT32 security)
{
	static const char* PROTOCOL_SECURITY_STRINGS[] = { "RDP", "TLS", "NLA", "UNK", "UNK",
		                                               "UNK", "UNK", "UNK", "EXT", "UNK" };
	if (security >= ARRAYSIZE(PROTOCOL_SECURITY_STRINGS))
		return PROTOCOL_SECURITY_STRINGS[ARRAYSIZE(PROTOCOL_SECURITY_STRINGS) - 1];
	return PROTOCOL_SECURITY_STRINGS[security];
}

static void nego_process_negotiation_request(rdpNego* nego, wStream* s);
static void nego_process_negotiation_response(rdpNego* nego, wStream* s);
static void nego_process_negotiation_failure(rdpNego* nego, wStream* s);

BOOL nego_recv_response(rdpNego* nego)
{
	int status;
	wStream* s;
	s = Stream_New(NULL, 1024);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	status = transport_read_pdu(nego->transport, s);

	if (status < 0)
	{
		Stream_Free(s, TRUE);
		return FALSE;
	}

	status = nego_recv(nego->transport, s, nego);
	Stream_Free(s, TRUE);

	if (status < 0)
		return FALSE;

	return TRUE;
}

BOOL nego_recv_cc(rdpNego* nego)
{
	int status;
	wStream* s;
	s = Stream_New(NULL, 1024);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	status = transport_read_pdu(nego->transport, s);

	if (status < 0)
	{
		Stream_Free(s, TRUE);
		return FALSE;
	}

	status = nego_recv(nego->transport, s, nego);
	Stream_Free(s, TRUE);

	if (status < 0)
		return FALSE;

	rdpSettings* settings = nego->transport->settings;
	/* update settings with negotiated protocol security */
	settings->RequestedProtocols = nego->RequestedProtocols;
	settings->SelectedProtocol = nego->SelectedProtocol;
	settings->NegotiationFlags = nego->flags;
	return TRUE;
}

BOOL nego_disconnect(rdpNego* nego)
{
	nego->state = NEGO_STATE_INITIAL;
	return TRUE;
	// return nego_transport_disconnect(nego);
}

/**
 * Receive protocol security negotiation message.\n
 * @msdn{cc240501}
 * @param transport transport
 * @param s stream
 * @param extra nego pointer
 */

int nego_recv(rdpTransport* transport, wStream* s, void* extra)
{
	BYTE li;
	BYTE type;
	UINT16 length;
	rdpNego* nego = (rdpNego*)extra;

	if (!tpkt_read_header(s, &length))
		return -1;

	if (length == 0)
		return -1;

	if (!tpdu_read_connection_confirm(s, &li))
		return -1;

	if (li > 6)
	{
		/* rdpNegData (optional) */
		Stream_Read_UINT8(s, type); /* Type */

		switch (type)
		{
			case TYPE_RDP_NEG_RSP:
				nego_process_negotiation_response(nego, s);
				WLog_DBG(TAG, "selected_protocol: %" PRIu32 "", nego->SelectedProtocol);

				/* enhanced security selected ? */

				if (nego->SelectedProtocol)
				{
					if ((nego->SelectedProtocol == PROTOCOL_HYBRID) &&
					    (!nego->EnabledProtocols[PROTOCOL_HYBRID]))
					{
						nego->state = NEGO_STATE_FAIL;
					}

					if ((nego->SelectedProtocol == PROTOCOL_SSL) &&
					    (!nego->EnabledProtocols[PROTOCOL_SSL]))
					{
						nego->state = NEGO_STATE_FAIL;
					}
				}
				else if (!nego->EnabledProtocols[PROTOCOL_RDP])
				{
					nego->state = NEGO_STATE_FAIL;
				}

				break;

			case TYPE_RDP_NEG_FAILURE:
				nego_process_negotiation_failure(nego, s);
				break;
		}
	}
	else if (li == 6)
	{
		WLog_DBG(TAG, "no rdpNegData");

		if (!nego->EnabledProtocols[PROTOCOL_RDP])
			nego->state = NEGO_STATE_FAIL;
		else
			nego->state = NEGO_STATE_FINAL;
	}
	else
	{
		WLog_ERR(TAG, "invalid negotiation response");
		nego->state = NEGO_STATE_FAIL;
	}

	return 0;
}

/**
 * Read optional routing token or cookie of X.224 Connection Request PDU.
 * @msdn{cc240470}
 * @param nego
 * @param s stream
 */

static BOOL nego_read_request_token_or_cookie(rdpNego* nego, wStream* s)
{
	/* routingToken and cookie are optional and mutually exclusive!
	 *
	 * routingToken (variable): An optional and variable-length routing
	 * token (used for load balancing) terminated by a 0x0D0A two-byte
	 * sequence: (check [MSFT-SDLBTS] for details!)
	 * Cookie:[space]msts=[ip address].[port].[reserved][\x0D\x0A]
	 *
	 * cookie (variable): An optional and variable-length ANSI character
	 * string terminated by a 0x0D0A two-byte sequence:
	 * Cookie:[space]mstshash=[ANSISTRING][\x0D\x0A]
	 */
	BYTE* str = NULL;
	UINT16 crlf = 0;
	size_t pos, len;
	BOOL result = FALSE;
	BOOL isToken = FALSE;
	size_t remain = Stream_GetRemainingLength(s);
	str = Stream_Pointer(s);
	pos = Stream_GetPosition(s);

	/* minimum length for token is 15 */
	if (remain < 15)
		return TRUE;

	if (memcmp(Stream_Pointer(s), "Cookie: mstshash=", 17) != 0)
	{
		isToken = TRUE;
	}
	else
	{
		/* not a token, minimum length for cookie is 19 */
		if (remain < 19)
			return TRUE;

		Stream_Seek(s, 17);
	}

	while ((remain = Stream_GetRemainingLength(s)) >= 2)
	{
		Stream_Read_UINT16(s, crlf);

		if (crlf == 0x0A0D)
			break;

		Stream_Rewind(s, 1);
	}

	if (crlf == 0x0A0D)
	{
		Stream_Rewind(s, 2);
		len = Stream_GetPosition(s) - pos;
		remain = Stream_GetRemainingLength(s);
		Stream_Write_UINT16(s, 0);

		if (strnlen((char*)str, len) == len)
		{
			if (isToken)
				result = nego_set_routing_token(nego, str, len);
			else
				result = nego_set_cookie(nego, (char*)str);
		}
	}

	if (!result)
	{
		Stream_SetPosition(s, pos);
		WLog_ERR(TAG, "invalid %s received", isToken ? "routing token" : "cookie");
	}
	else
	{
		WLog_DBG(TAG, "received %s [%s]", isToken ? "routing token" : "cookie", str);
	}

	return result;
}

/**
 * Read protocol security negotiation request message.\n
 * @param nego
 * @param s stream
 */

BOOL nego_read_request(rdpNego* nego, wStream* s)
{
	BYTE li;
	BYTE type;
	UINT16 length;

	if (!tpkt_read_header(s, &length))
		return FALSE;

	if (!tpdu_read_connection_request(s, &li))
		return FALSE;

	if (li != Stream_GetRemainingLength(s) + 6)
	{
		WLog_ERR(TAG, "Incorrect TPDU length indicator.");
		return FALSE;
	}

	if (!nego_read_request_token_or_cookie(nego, s))
	{
		WLog_ERR(TAG, "Failed to parse routing token or cookie.");
		return FALSE;
	}

	if (Stream_GetRemainingLength(s) >= 8)
	{
		/* rdpNegData (optional) */
		Stream_Read_UINT8(s, type); /* Type */

		if (type != TYPE_RDP_NEG_REQ)
		{
			WLog_ERR(TAG, "Incorrect negotiation request type %" PRIu8 "", type);
			return FALSE;
		}

		nego_process_negotiation_request(nego, s);
	}

	return TRUE;
}

/**
 * Send RDP Negotiation Request (RDP_NEG_REQ).\n
 * @msdn{cc240500}\n
 * @msdn{cc240470}
 * @param nego
 */

BOOL nego_send_negotiation_request(rdpNego* nego)
{
	BOOL rc = FALSE;
	wStream* s;
	size_t length;
	size_t bm, em;
	BYTE flags = 0;
	size_t cookie_length;
	s = Stream_New(NULL, 512);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	length = TPDU_CONNECTION_REQUEST_LENGTH;
	bm = Stream_GetPosition(s);
	Stream_Seek(s, length);

	if (nego->RoutingToken)
	{
		Stream_Write(s, nego->RoutingToken, nego->RoutingTokenLength);

		/* Ensure Routing Token is correctly terminated - may already be present in string */

		if ((nego->RoutingTokenLength > 2) &&
		    (nego->RoutingToken[nego->RoutingTokenLength - 2] == 0x0D) &&
		    (nego->RoutingToken[nego->RoutingTokenLength - 1] == 0x0A))
		{
			WLog_DBG(TAG, "Routing token looks correctly terminated - use verbatim");
			length += nego->RoutingTokenLength;
		}
		else
		{
			WLog_DBG(TAG, "Adding terminating CRLF to routing token");
			Stream_Write_UINT8(s, 0x0D); /* CR */
			Stream_Write_UINT8(s, 0x0A); /* LF */
			length += nego->RoutingTokenLength + 2;
		}
	}
	else if (nego->cookie)
	{
		cookie_length = strlen(nego->cookie);

		if (cookie_length > nego->CookieMaxLength)
			cookie_length = nego->CookieMaxLength;

		Stream_Write(s, "Cookie: mstshash=", 17);
		Stream_Write(s, (BYTE*)nego->cookie, cookie_length);
		Stream_Write_UINT8(s, 0x0D); /* CR */
		Stream_Write_UINT8(s, 0x0A); /* LF */
		length += cookie_length + 19;
	}

	{
		nego->RequestedProtocols = PROTOCOL_HYBRID | PROTOCOL_SSL;
		nego->RestrictedAdminModeRequired = FALSE;
	}
	WLog_DBG(TAG, "RequestedProtocols: %" PRIu32 "", nego->RequestedProtocols);

	if ((nego->RequestedProtocols > PROTOCOL_RDP) || (nego->sendNegoData))
	{
		/* RDP_NEG_DATA must be present for TLS and NLA */
		if (nego->RestrictedAdminModeRequired)
			flags |= RESTRICTED_ADMIN_MODE_REQUIRED;

		Stream_Write_UINT8(s, TYPE_RDP_NEG_REQ);
		Stream_Write_UINT8(s, flags);
		Stream_Write_UINT16(s, 8);                        /* RDP_NEG_DATA length (8) */
		Stream_Write_UINT32(s, nego->RequestedProtocols); /* requestedProtocols */
		length += 8;
	}

	if (length > UINT16_MAX)
		goto fail;

	em = Stream_GetPosition(s);
	Stream_SetPosition(s, bm);
	tpkt_write_header(s, (UINT16)length);
	tpdu_write_connection_request(s, (UINT16)length - 5);
	Stream_SetPosition(s, em);
	Stream_SealLength(s);
	rc = (transport_write(nego->transport, s) >= 0);
fail:
	Stream_Free(s, TRUE);
	return rc;
}

wStream* nego_get_negotiation_request(rdpNego* nego)
{
	BOOL rc = FALSE;
	wStream* s;
	size_t length;
	size_t bm, em;
	BYTE flags = 0;
	size_t cookie_length;
	s = Stream_New(NULL, 512);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return s;
	}

	length = TPDU_CONNECTION_REQUEST_LENGTH;
	bm = Stream_GetPosition(s);
	Stream_Seek(s, length);

	if (nego->RoutingToken)
	{
		Stream_Write(s, nego->RoutingToken, nego->RoutingTokenLength);

		/* Ensure Routing Token is correctly terminated - may already be present in string */

		if ((nego->RoutingTokenLength > 2) &&
		    (nego->RoutingToken[nego->RoutingTokenLength - 2] == 0x0D) &&
		    (nego->RoutingToken[nego->RoutingTokenLength - 1] == 0x0A))
		{
			WLog_DBG(TAG, "Routing token looks correctly terminated - use verbatim");
			length += nego->RoutingTokenLength;
		}
		else
		{
			WLog_DBG(TAG, "Adding terminating CRLF to routing token");
			Stream_Write_UINT8(s, 0x0D); /* CR */
			Stream_Write_UINT8(s, 0x0A); /* LF */
			length += nego->RoutingTokenLength + 2;
		}
	}
	else if (nego->cookie)
	{
		cookie_length = strlen(nego->cookie);

		if (cookie_length > nego->CookieMaxLength)
			cookie_length = nego->CookieMaxLength;

		Stream_Write(s, "Cookie: mstshash=", 17);
		Stream_Write(s, (BYTE*)nego->cookie, cookie_length);
		Stream_Write_UINT8(s, 0x0D); /* CR */
		Stream_Write_UINT8(s, 0x0A); /* LF */
		length += cookie_length + 19;
	}

	{
		nego->RequestedProtocols = PROTOCOL_HYBRID | PROTOCOL_SSL;
		nego->RestrictedAdminModeRequired = FALSE;
	}
	WLog_DBG(TAG, "RequestedProtocols: %" PRIu32 "", nego->RequestedProtocols);

	if ((nego->RequestedProtocols > PROTOCOL_RDP) || (nego->sendNegoData))
	{
		/* RDP_NEG_DATA must be present for TLS and NLA */
		if (nego->RestrictedAdminModeRequired)
			flags |= RESTRICTED_ADMIN_MODE_REQUIRED;

		Stream_Write_UINT8(s, TYPE_RDP_NEG_REQ);
		Stream_Write_UINT8(s, flags);
		Stream_Write_UINT16(s, 8);                        /* RDP_NEG_DATA length (8) */
		Stream_Write_UINT32(s, nego->RequestedProtocols); /* requestedProtocols */
		length += 8;
	}

	if (length > UINT16_MAX) {
		Stream_Free(s, TRUE);
		return NULL;
	}

	em = Stream_GetPosition(s);
	Stream_SetPosition(s, bm);
	tpkt_write_header(s, (UINT16)length);
	tpdu_write_connection_request(s, (UINT16)length - 5);
	Stream_SetPosition(s, em);
	Stream_SealLength(s);
	// rc = (transport_write(nego->transport, s) >= 0);
// fail:
	// Stream_Free(s, TRUE);
	return s;
}

/**
 * Process Negotiation Request from Connection Request message.
 * @param nego
 * @param s
 */

void nego_process_negotiation_request(rdpNego* nego, wStream* s)
{
	BYTE flags;
	UINT16 length;
	Stream_Read_UINT8(s, flags);
	Stream_Read_UINT16(s, length);
	Stream_Read_UINT32(s, nego->RequestedProtocols);
	WLog_DBG(TAG, "RDP_NEG_REQ: RequestedProtocol: 0x%08" PRIX32 "", nego->RequestedProtocols);
	nego->state = NEGO_STATE_FINAL;
}

/**
 * Process Negotiation Response from Connection Confirm message.
 * @param nego
 * @param s
 */

void nego_process_negotiation_response(rdpNego* nego, wStream* s)
{
	UINT16 length;
	WLog_DBG(TAG, "RDP_NEG_RSP");

	if (Stream_GetRemainingLength(s) < 7)
	{
		WLog_ERR(TAG, "Invalid RDP_NEG_RSP");
		nego->state = NEGO_STATE_FAIL;
		return;
	}

	Stream_Read_UINT8(s, nego->flags);
	Stream_Read_UINT16(s, length);
	Stream_Read_UINT32(s, nego->SelectedProtocol);
	nego->state = NEGO_STATE_FINAL;
}

/**
 * Process Negotiation Failure from Connection Confirm message.
 * @param nego
 * @param s
 */

void nego_process_negotiation_failure(rdpNego* nego, wStream* s)
{
	BYTE flags;
	UINT16 length;
	UINT32 failureCode;
	WLog_DBG(TAG, "RDP_NEG_FAILURE");
	Stream_Read_UINT8(s, flags);
	Stream_Read_UINT16(s, length);
	Stream_Read_UINT32(s, failureCode);

	switch (failureCode)
	{
		case SSL_REQUIRED_BY_SERVER:
			WLog_WARN(TAG, "Error: SSL_REQUIRED_BY_SERVER");
			break;

		case SSL_NOT_ALLOWED_BY_SERVER:
			WLog_WARN(TAG, "Error: SSL_NOT_ALLOWED_BY_SERVER");
			nego->sendNegoData = TRUE;
			break;

		case SSL_CERT_NOT_ON_SERVER:
			WLog_ERR(TAG, "Error: SSL_CERT_NOT_ON_SERVER");
			nego->sendNegoData = TRUE;
			break;

		case INCONSISTENT_FLAGS:
			WLog_ERR(TAG, "Error: INCONSISTENT_FLAGS");
			break;

		case HYBRID_REQUIRED_BY_SERVER:
			WLog_WARN(TAG, "Error: HYBRID_REQUIRED_BY_SERVER");
			break;

		default:
			WLog_ERR(TAG, "Error: Unknown protocol security error %" PRIu32 "", failureCode);
			break;
	}

	nego->state = NEGO_STATE_FAIL;
}

/**
 * Send RDP Negotiation Response (RDP_NEG_RSP).\n
 * @param nego
 */

BOOL nego_send_negotiation_response(rdpNego* nego)
{
	UINT16 length;
	size_t bm, em;
	BOOL status;
	wStream* s;
	BYTE flags;
	rdpSettings* settings;
	status = TRUE;
	settings = nego->transport->settings;
	s = Stream_New(NULL, 512);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	length = TPDU_CONNECTION_CONFIRM_LENGTH;
	bm = Stream_GetPosition(s);
	Stream_Seek(s, length);

	if (nego->SelectedProtocol & PROTOCOL_FAILED_NEGO)
	{
		UINT32 errorCode = (nego->SelectedProtocol & ~PROTOCOL_FAILED_NEGO);
		flags = 0;
		Stream_Write_UINT8(s, TYPE_RDP_NEG_FAILURE);
		Stream_Write_UINT8(s, flags); /* flags */
		Stream_Write_UINT16(s, 8);    /* RDP_NEG_DATA length (8) */
		Stream_Write_UINT32(s, errorCode);
		length += 8;
		status = FALSE;
	}
	else
	{
		flags = EXTENDED_CLIENT_DATA_SUPPORTED;

		if (settings->SupportGraphicsPipeline)
			flags |= DYNVC_GFX_PROTOCOL_SUPPORTED;

		/* RDP_NEG_DATA must be present for TLS, NLA, and RDP */
		Stream_Write_UINT8(s, TYPE_RDP_NEG_RSP);
		Stream_Write_UINT8(s, flags);                   /* flags */
		Stream_Write_UINT16(s, 8);                      /* RDP_NEG_DATA length (8) */
		Stream_Write_UINT32(s, nego->SelectedProtocol); /* selectedProtocol */
		length += 8;
	}

	em = Stream_GetPosition(s);
	Stream_SetPosition(s, bm);
	tpkt_write_header(s, length);
	tpdu_write_connection_confirm(s, length - 5);
	Stream_SetPosition(s, em);
	Stream_SealLength(s);

	if (transport_write(nego->transport, s) < 0)
	{
		Stream_Free(s, TRUE);
		return FALSE;
	}

	Stream_Free(s, TRUE);

	if (status)
	{
		/* update settings with negotiated protocol security */
		settings->RequestedProtocols = nego->RequestedProtocols;
		settings->SelectedProtocol = nego->SelectedProtocol;

		if (settings->SelectedProtocol == PROTOCOL_RDP)
		{
			settings->TlsSecurity = FALSE;
			settings->NlaSecurity = FALSE;
			settings->RdpSecurity = TRUE;
			settings->UseRdpSecurityLayer = TRUE;

			if (settings->EncryptionLevel == ENCRYPTION_LEVEL_NONE)
			{
				/**
				 * If the server implementation did not explicitely set a
				 * encryption level we default to client compatible
				 */
				settings->EncryptionLevel = ENCRYPTION_LEVEL_CLIENT_COMPATIBLE;
			}

			if (settings->LocalConnection)
			{
				/**
				 * Note: This hack was firstly introduced in commit 95f5e115 to
				 * disable the unnecessary encryption with peers connecting to
				 * 127.0.0.1 or local unix sockets.
				 * This also affects connections via port tunnels! (e.g. ssh -L)
				 */
				WLog_INFO(TAG, "Turning off encryption for local peer with standard rdp security");
				settings->UseRdpSecurityLayer = FALSE;
				settings->EncryptionLevel = ENCRYPTION_LEVEL_NONE;
			}

			if (!settings->RdpServerRsaKey && !settings->RdpKeyFile && !settings->RdpKeyContent)
			{
				WLog_ERR(TAG, "Missing server certificate");
				return FALSE;
			}
		}
		else if (settings->SelectedProtocol == PROTOCOL_SSL)
		{
			settings->TlsSecurity = TRUE;
			settings->NlaSecurity = FALSE;
			settings->RdpSecurity = FALSE;
			settings->UseRdpSecurityLayer = FALSE;
			settings->EncryptionLevel = ENCRYPTION_LEVEL_NONE;
		}
		else if (settings->SelectedProtocol == PROTOCOL_HYBRID)
		{
			settings->TlsSecurity = TRUE;
			settings->NlaSecurity = TRUE;
			settings->RdpSecurity = FALSE;
			settings->UseRdpSecurityLayer = FALSE;
			settings->EncryptionLevel = ENCRYPTION_LEVEL_NONE;
		}
	}

	return status;
}

/**
 * Initialize NEGO state machine.
 * @param nego
 */

void nego_init(rdpNego* nego)
{
	nego->state = NEGO_STATE_INITIAL;
	nego->RequestedProtocols = PROTOCOL_RDP;
	nego->CookieMaxLength = DEFAULT_COOKIE_MAX_LENGTH;
	nego->sendNegoData = FALSE;
	nego->flags = 0;
}

/**
 * Create a new NEGO state machine instance.
 * @param transport
 * @return
 */

rdpNego* nego_new(rdpTransport* transport)
{
	rdpNego* nego = (rdpNego*)calloc(1, sizeof(rdpNego));

	if (!nego)
		return NULL;

	nego->transport = transport;
	nego_init(nego);
	return nego;
}

/**
 * Free NEGO state machine.
 * @param nego
 */

void nego_free(rdpNego* nego)
{
	if (nego)
	{
		free(nego->RoutingToken);
		free(nego->cookie);
		free(nego);
	}
}

/**
 * Set target hostname and port.
 * @param nego
 * @param hostname
 * @param port
 */

BOOL nego_set_target(rdpNego* nego, const char* hostname, UINT16 port)
{
	if (!nego || !hostname)
		return FALSE;

	nego->hostname = hostname;
	nego->port = port;
	return TRUE;
}

/**
 * Enable security layer negotiation.
 * @param nego pointer to the negotiation structure
 * @param enable_rdp whether to enable security layer negotiation (TRUE for enabled, FALSE for
 * disabled)
 */

void nego_set_negotiation_enabled(rdpNego* nego, BOOL NegotiateSecurityLayer)
{
	WLog_DBG(TAG, "Enabling security layer negotiation: %s",
	         NegotiateSecurityLayer ? "TRUE" : "FALSE");
	nego->NegotiateSecurityLayer = NegotiateSecurityLayer;
}

/**
 * Enable restricted admin mode.
 * @param nego pointer to the negotiation structure
 * @param enable_restricted whether to enable security layer negotiation (TRUE for enabled, FALSE
 * for disabled)
 */

void nego_set_restricted_admin_mode_required(rdpNego* nego, BOOL RestrictedAdminModeRequired)
{
	WLog_DBG(TAG, "Enabling restricted admin mode: %s",
	         RestrictedAdminModeRequired ? "TRUE" : "FALSE");
	nego->RestrictedAdminModeRequired = RestrictedAdminModeRequired;
}

void nego_set_gateway_enabled(rdpNego* nego, BOOL GatewayEnabled)
{
	nego->GatewayEnabled = GatewayEnabled;
}

void nego_set_gateway_bypass_local(rdpNego* nego, BOOL GatewayBypassLocal)
{
	nego->GatewayBypassLocal = GatewayBypassLocal;
}

/**
 * Enable RDP security protocol.
 * @param nego pointer to the negotiation structure
 * @param enable_rdp whether to enable normal RDP protocol (TRUE for enabled, FALSE for disabled)
 */

void nego_enable_rdp(rdpNego* nego, BOOL enable_rdp)
{
	WLog_DBG(TAG, "Enabling RDP security: %s", enable_rdp ? "TRUE" : "FALSE");
	nego->EnabledProtocols[PROTOCOL_RDP] = enable_rdp;
}

/**
 * Enable TLS security protocol.
 * @param nego pointer to the negotiation structure
 * @param enable_tls whether to enable TLS + RDP protocol (TRUE for enabled, FALSE for disabled)
 */

void nego_enable_tls(rdpNego* nego, BOOL enable_tls)
{
	WLog_DBG(TAG, "Enabling TLS security: %s", enable_tls ? "TRUE" : "FALSE");
	nego->EnabledProtocols[PROTOCOL_SSL] = enable_tls;
}

/**
 * Enable NLA security protocol.
 * @param nego pointer to the negotiation structure
 * @param enable_nla whether to enable network level authentication protocol (TRUE for enabled,
 * FALSE for disabled)
 */

void nego_enable_nla(rdpNego* nego, BOOL enable_nla)
{
	WLog_DBG(TAG, "Enabling NLA security: %s", enable_nla ? "TRUE" : "FALSE");
	nego->EnabledProtocols[PROTOCOL_HYBRID] = enable_nla;
}

/**
 * Enable NLA extended security protocol.
 * @param nego pointer to the negotiation structure
 * @param enable_ext whether to enable network level authentication extended protocol (TRUE for
 * enabled, FALSE for disabled)
 */

void nego_enable_ext(rdpNego* nego, BOOL enable_ext)
{
	WLog_DBG(TAG, "Enabling NLA extended security: %s", enable_ext ? "TRUE" : "FALSE");
	nego->EnabledProtocols[PROTOCOL_HYBRID_EX] = enable_ext;
}

/**
 * Set routing token.
 * @param nego
 * @param RoutingToken
 * @param RoutingTokenLength
 */

BOOL nego_set_routing_token(rdpNego* nego, BYTE* RoutingToken, DWORD RoutingTokenLength)
{
	if (RoutingTokenLength == 0)
		return FALSE;

	free(nego->RoutingToken);
	nego->RoutingTokenLength = RoutingTokenLength;
	nego->RoutingToken = (BYTE*)malloc(nego->RoutingTokenLength);

	if (!nego->RoutingToken)
		return FALSE;

	CopyMemory(nego->RoutingToken, RoutingToken, nego->RoutingTokenLength);
	return TRUE;
}

/**
 * Set cookie.
 * @param nego
 * @param cookie
 */

BOOL nego_set_cookie(rdpNego* nego, char* cookie)
{
	if (nego->cookie)
	{
		free(nego->cookie);
		nego->cookie = NULL;
	}

	if (!cookie)
		return TRUE;

	nego->cookie = _strdup(cookie);

	if (!nego->cookie)
		return FALSE;

	return TRUE;
}

/**
 * Set cookie maximum length
 * @param nego
 * @param CookieMaxLength
 */

void nego_set_cookie_max_length(rdpNego* nego, UINT32 CookieMaxLength)
{
	nego->CookieMaxLength = CookieMaxLength;
}

/**
 * Enable / disable preconnection PDU.
 * @param nego
 * @param send_pcpdu
 */

void nego_set_send_preconnection_pdu(rdpNego* nego, BOOL SendPreconnectionPdu)
{
	nego->SendPreconnectionPdu = SendPreconnectionPdu;
}

/**
 * Set preconnection id.
 * @param nego
 * @param id
 */

void nego_set_preconnection_id(rdpNego* nego, UINT32 PreconnectionId)
{
	nego->PreconnectionId = PreconnectionId;
}

/**
 * Set preconnection blob.
 * @param nego
 * @param blob
 */

void nego_set_preconnection_blob(rdpNego* nego, char* PreconnectionBlob)
{
	nego->PreconnectionBlob = PreconnectionBlob;
}

UINT32 nego_get_selected_protocol(rdpNego* nego)
{
	if (!nego)
		return 0;

	return nego->SelectedProtocol;
}

BOOL nego_set_selected_protocol(rdpNego* nego, UINT32 SelectedProtocol)
{
	if (!nego)
		return FALSE;

	nego->SelectedProtocol = SelectedProtocol;
	return TRUE;
}

UINT32 nego_get_requested_protocols(rdpNego* nego)
{
	if (!nego)
		return 0;

	return nego->RequestedProtocols;
}

BOOL nego_set_requested_protocols(rdpNego* nego, UINT32 RequestedProtocols)
{
	if (!nego)
		return FALSE;

	nego->RequestedProtocols = RequestedProtocols;
	return TRUE;
}

NEGO_STATE nego_get_state(rdpNego* nego)
{
	if (!nego)
		return NEGO_STATE_FAIL;

	return nego->state;
}

BOOL nego_set_state(rdpNego* nego, NEGO_STATE state)
{
	if (!nego)
		return FALSE;

	nego->state = state;
	return TRUE;
}
/*
freerdp-rose-fix
SEC_WINNT_AUTH_IDENTITY* nego_get_identity(rdpNego* nego)
{
	if (!nego)
		return NULL;

	return nla_get_identity(nego->transport->nla);
}

void nego_free_nla(rdpNego* nego)
{
	if (!nego || !nego->transport)
		return;

	nla_free(nego->transport->nla);
	nego->transport->nla = NULL;
}
*/
const BYTE* nego_get_routing_token(rdpNego* nego, DWORD* RoutingTokenLength)
{
	if (!nego)
		return NULL;
	if (RoutingTokenLength)
		*RoutingTokenLength = nego->RoutingTokenLength;
	return nego->RoutingToken;
}
