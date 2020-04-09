#include "Call.h"
#include <pjsua-lib/pjsua.h>
#include "pjsua_app_common.h"
#include "pjsua_app_common.c"
#include "pjmedia/stream.h"
#include <pjsua-lib\pjsua_internal.h>
#include "Wrapper.h"

#define THIS_FILE "CallDll"

using namespace PJWrapper;

static Int32 wavTmp = 0;

struct wavplayerEof_Data
{
	pjsua_player_id playerId;
	pjsua_call_id callId;
};

static void IncomingCall(pjsua_acc_id acc_id, pjsua_call_id call_id, pjsip_rx_data* rdata)
{
	Wrapper::InvokeIncomingCallEnvet(call_id, "acc_id=" + acc_id);

	pjsua_call_info ci;

	PJ_UNUSED_ARG(acc_id);
	PJ_UNUSED_ARG(rdata);

	pjsua_call_get_info(call_id, &ci);

	PJ_LOG(3, (THIS_FILE, "Incoming call from %.*s!!",
		(int)ci.remote_info.slen,
		ci.remote_info.ptr));

	/* Automatically answer incoming calls with 200/OK */
	pjsua_call_answer(call_id, 200, NULL, NULL);
}

static void PlayFileDone(pjmedia_port* port, void* usr_data) {

	pj_status_t status;
	wavplayerEof_Data* WavePlayerData = ((wavplayerEof_Data*)usr_data);

	// Read info from args
	pjsua_call_id call_id = WavePlayerData->callId;
	pjsua_player_id player_id = WavePlayerData->playerId;

	//Destroy the Wav Player
	status = pjsua_player_destroy(player_id);   // ! Problem if Destroying Here : cash at the end of callback, for most of wavs files

	// Free the memory allocated for the args
	free(usr_data);

	//PJ_LOG(3, (THIS_FILE, "End of Wav File, media_port: %d", port));
	PJ_LOG(3, (THIS_FILE, "End of Wav File"));

	// Invoke the Callback for C# managed code
	/*if (cb_wavplayerEnded != 0)
		(*cb_wavplayerEnded)(call_id, player_id);*/

	wavTmp = 2;
	//wavTmp

	//if (status == PJ_SUCCESS)    // Player correctly Destroyed
	//return -1;                      // Don't return PJ_SUCCESS, to prevent crash when returning from callback after Player Destruction

	//return PJ_SUCCESS;             // Else, return PJ_SUCCESS
}

static void CallMediaState(pjsua_call_id call_id)
{
	Wrapper::InvokeOnCallMediaStateEvent(call_id);

	pjsua_call_info ci;

	pjsua_call_get_info(call_id, &ci);

	if (ci.media_status == PJSUA_CALL_MEDIA_ACTIVE) {
		// When media is active, connect call to sound device.
		// pjsua_conf_connect(ci.conf_slot, 0);
		// pjsua_conf_connect(0, ci.conf_slot);
	}
}

static void CallState(pjsua_call_id call_id, pjsip_event* e)
{
	pjsua_call_info ci;

	PJ_UNUSED_ARG(e);

	pjsua_call_get_info(call_id, &ci);
	PJ_LOG(3, (THIS_FILE, "Call %d state=%.*s", call_id,
		(int)ci.state_text.slen,
		ci.state_text.ptr));

	String^ test = gcnew String(ci.state_text.ptr);

	Wrapper::InvokeCallStateEvent(call_id, test);
}

static void CallDtmfCallBack(pjsua_call_id call_id, const pjsua_dtmf_info* info)
{
	char duration[16];
	char method[16];

	duration[0] = '\0';

	switch (info->method) {
	case PJSUA_DTMF_METHOD_RFC2833:
		pj_ansi_snprintf(method, sizeof(method), "RFC2833");
		break;
	case PJSUA_DTMF_METHOD_SIP_INFO:
		pj_ansi_snprintf(method, sizeof(method), "SIP INFO");
		pj_ansi_snprintf(duration, sizeof(duration), ":duration(%d)",
			info->duration);
		break;
	};
	PJ_LOG(3, (THIS_FILE, "Incoming DTMF on call %d: %c%s, using %s method",
		call_id, info->digit, duration, method));
}

void Call::Register(char* username, char* password, char* uri, char* reuri, int loglevel)
{
	int index;
	int port_count;
	int clock_rate;
	char filename[80];

	pj_pool_t* pool;
	pjsua_acc_id acc_id;
	pjmedia_port* wav = NULL;
	pj_status_t status;
	pjsua_player_id wav_id = PJSUA_INVALID_ID;
	pjmedia_conf* conf;
	pjmedia_port* rec_port = NULL;

	/* Create pjsua first! */
	status = pjsua_create();

	/* Init pjsua */
	{
		pjsua_config cfg;
		pjsua_logging_config log_cfg;

		pjsua_config_default(&cfg);
		cfg.cb.on_incoming_call = &IncomingCall;
		cfg.cb.on_call_media_state = &CallMediaState;
		cfg.cb.on_call_state = &CallState;
		cfg.cb.on_dtmf_digit2 = &CallDtmfCallBack;
		pjsua_logging_config_default(&log_cfg);
		log_cfg.console_level = loglevel;

		status = pjsua_init(&cfg, &log_cfg, NULL);

		if (app_config.wav_id == PJSUA_INVALID_ID) {
			app_config.wav_id = wav_id;
			app_config.wav_port = pjsua_player_get_conf_port(app_config.wav_id);
			if (app_config.auto_play_hangup) {
				pjmedia_port* port;

				pjsua_player_get_port(app_config.wav_id, &port);
				status = pjmedia_wav_player_set_eof_cb2(port, NULL,
					&PlayFileDone);
			}
		}
	}

	/* Add UDP transport. */
	{
		pjsua_transport_config cfg;

		pjsua_transport_config_default(&cfg);
		cfg.port = 5060;
		status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &cfg, NULL);
	}

	/* Initialization is done, now start pjsua */
	status = pjsua_start();

	/* Register to SIP server by creating SIP account. */
	{
		pjsua_acc_config cfg;

		pjsua_acc_config_default(&cfg);
		cfg.id = pj_str(uri);
		cfg.reg_uri = pj_str(reuri);
		cfg.cred_count = 1;
		cfg.cred_info[0].realm = pj_str("*");
		cfg.cred_info[0].scheme = pj_str("digest");
		cfg.cred_info[0].username = pj_str(username);
		cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
		cfg.cred_info[0].data = pj_str(password);

		status = pjsua_acc_add(&cfg, PJ_TRUE, &acc_id);
	}
}

void Call::MakeCall(char* uri)
{
	char buf[128];
	pjsua_msg_data msg_data_;
	input_result result;
	pj_str_t _uri = pj_str(uri);


	pjsua_msg_data_init(&msg_data_);
	TEST_MULTIPART(&msg_data_);
	pjsua_call_make_call(current_acc, &_uri, NULL, NULL,
		&msg_data_, &current_call);

}

void Call::Hangup()
{
	if (current_call == -1) {
		puts("No current call");
		fflush(stdout);
		return;
	}
	else {
		/* Hangup current calls */
		pjsua_call_hangup(current_call, 0, NULL, NULL);
	}
}

void Call::Hold()
{
	if (current_call != -1)
	{
		pjsua_call_set_hold(current_call, NULL);
	}
	else {
		PJ_LOG(3, (THIS_FILE, "No current call"));
	}
}

void Call::ReInvite()
{
	call_opt.flag |= PJSUA_CALL_UNHOLD;
	pjsua_call_reinvite(current_call, call_opt.flag, NULL);
}

void Call::Transfer(char* uri)
{
	if (current_call == -1) {
		PJ_LOG(3, (THIS_FILE, "No current call"));
	}
	else {
		int call = current_call;
		char buf[128];
		const pj_str_t* desturi;
		pjsip_generic_string_hdr refer_sub;
		pj_str_t STR_REFER_SUB = { "Refer-Sub", 9 };
		pj_str_t STR_FALSE = { "false", 5 };
		pjsua_call_info ci;
		input_result result;
		pjsua_msg_data msg_data_;

		pjsua_call_get_info(current_call, &ci);
		printf("Transferring current call [%d] %.*s\n", current_call,
			(int)ci.remote_info.slen, ci.remote_info.ptr);

		//ui_input_url("Transfer to URL", buf, sizeof(buf), &result);

		/* Check if call is still there. */

		if (call != current_call) {
			puts("Call has been disconnected");
			return;
		}

		//pjsua_msg_data_init(&msg_data_);
		//if (no_refersub) {
		//	/* Add Refer-Sub: false in outgoing REFER request */
		//	pjsip_generic_string_hdr_init2(&refer_sub, &STR_REFER_SUB,
		//		&STR_FALSE);
		//	pj_list_push_back(&msg_data_.hdr_list, &refer_sub);
		//}
		//if (result.nb_result != PJSUA_APP_NO_NB) {
		//	if (result.nb_result == -1 || result.nb_result == 0) {
		//		puts("You can't do that with transfer call!");
		//	}
		//	else {
		//		pjsua_buddy_info binfo;
		//		pjsua_buddy_get_info(result.nb_result - 1, &binfo);
		//		pjsua_call_xfer(current_call, &tmp, &msg_data_);
		//	}

		//}
		//else if (result.uri_result) {
			//pj_str_t tmp;


		pj_str_t tmp = pj_str(uri);
		pjsua_call_xfer(current_call, &tmp, NULL);
		//}
	}
}

void Call::SendDTMF(char* uri)
{
	if (current_call == -1) {
		PJ_LOG(3, (THIS_FILE, "No current call"));
	}
	else if (!pjsua_call_has_media(current_call)) {
		PJ_LOG(3, (THIS_FILE, "Media is not established yet!"));
	}
	else {
		pj_str_t digits;
		int call = current_call;
		pj_status_t status;
		char buf[128];

#if defined(PJMEDIA_HAS_DTMF_FLASH) && PJMEDIA_HAS_DTMF_FLASH!= 0	    	
		//if (!simple_input("DTMF strings to send (0-9*R#A-B)", buf,
		//	sizeof(buf)))
#else
		if (!simple_input("DTMF strings to send (0-9*#A-B)", buf,
			sizeof(buf)))
#endif
			//{	
			//	return;
			//}

			//if (call != current_call) {
			//	puts("Call has been disconnected");
			//	return;
			//}

			digits = pj_str(uri);
		status = pjsua_call_dial_dtmf(current_call, &digits);
		//if (status != PJ_SUCCESS) {
		//	pjsua_perror(THIS_FILE, "Unable to send DTMF", status);
		//}
		//else {
		//	puts("DTMF digits enqueued for transmission");
		//}
	}
}

void Call::Destroy() {
	pjsua_destroy();
}

std::string getFileExtension(std::string filePath)
{
	/********************************************
	/ Get File extension from File path or File Name
	********************************************/

	// Find the last position of '.' in given string
	std::size_t pos = filePath.rfind('\\');

	// If last '.' is found
	if (pos != std::string::npos) {
		// return the substring
		return filePath.substr(pos + 1);
	}
	// In case of no extension return empty string
	return "";
}

void Call::PlayWavFile(char* wavFile)
{
	unsigned play_options = PJMEDIA_FILE_NO_LOOP;
	pj_status_t status;

	/* Infos Player */
	pjsua_player_id player_id;            // Ident. for the player
	pjmedia_port* media_port;           // Struct. media_port
	pjsua_conf_port_id conf_port;
	pjsua_call_id callId;

	/* Infos Call Session */
	pjsua_call_info call_info;

	/********************************************
	/ 1- Get call_info from callId
	********************************************/
	pjsua_call_get_info(callId, &call_info);

	/********************************************
	/ 2- Check if Call exists and is active
	********************************************/
	if (call_info.media_status != PJSUA_CALL_MEDIA_ACTIVE)
		PJ_LOG(3, (THIS_FILE, "Call is not active"));

	/********************************************
	/ 3- Load the WAV File - Create the player
	********************************************/
	status = pjsua_player_create(&pj_str(wavFile), play_options, &player_id);

	/********************************************
	/ 4- Get media_port from player_id
	********************************************/
	if (status == PJ_SUCCESS)
		status = pjsua_player_get_port(player_id, &media_port);

	/********************************************
	/ 5- Register the Callback C++ Function "pjmedia_wav_player_set_eof_cb2"
	********************************************/
	if (status == PJ_SUCCESS)
	{
		// Prepare argument for Callback
		wavplayerEof_Data* args = (wavplayerEof_Data*)malloc(sizeof(wavplayerEof_Data));
		args->playerId = player_id;

		// Register the Callback, launched when the End of the Wave File is reached
		status = pjmedia_wav_player_set_eof_cb2(media_port, args, &PlayFileDone);
	}

	/********************************************
	/ 6- Get conf_port from player_id
	********************************************/
	if (status == PJ_SUCCESS)
		conf_port = pjsua_player_get_conf_port(player_id);

	/********************************************
	/ 7- pjsua_conf_connect
	********************************************/
	// one way connect conf_port (wav player) to call_info.conf_slot (call)
	// test if conf_port valid, and if conf_slot != soundcard
	if ((status == PJ_SUCCESS) && (conf_port != PJSUA_INVALID_ID) && (call_info.conf_slot != 0))
	{
		wavTmp = 1;
		status = pjsua_conf_connect(conf_port, call_info.conf_slot);
		if (status == PJ_SUCCESS)
		{
			//Get filename
			std::string FileName = getFileExtension(wavFile);

			PJ_LOG(3, (THIS_FILE, FileName.c_str(), ""));
		}
	}

	do
	{

	} while (wavTmp != 2);
}