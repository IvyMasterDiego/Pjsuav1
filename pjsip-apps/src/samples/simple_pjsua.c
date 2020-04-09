
#pragma region Documentation
/*
* simple_pjsua.c
**
* Este es un agente de usuario, pero con todas las funciones, cuenta con las
* Siguientes capacidades :
*-Registro SIP
* -Realizar y recibir llamadas
* -Realizar multiples llamadas a un mismo numero
* -Transferencia de llamadas
* -Manejo de Hold y retreive
* -Audio / medios a dispositivo de sonido.
* -Creacion de puertos para enviar archivos.wav
* *
*Uso:
*-Para realizar llamadas salientes, inicie simple_pjsua con la URL del control remoto
* destino para contactar.
* P.ej. :
	*sorbo simpleua : usuario @ remoto
	**
	* -Las llamadas entrantes serán respondidas automáticamente con 200.
	* *
	*Este programa se cerrará una vez que haya completado una sola llamada.
	*/
#pragma endregion

#pragma region Include

#include <pjsua-lib/pjsua.h>
#include "pjsua_app_common.c"
#include "pjsua_app_common.h"
#include <pjsua-lib\pjsua_internal.h>

#pragma endregion

#pragma region Define

#define THIS_FILE	"APP"
#define SIP_DOMAIN	"192.168.1.25"
#define SIP_USER	"500"
#define SIP_PASSWD	"500Die"
#define WAV_FILE "auddemo.wav"

#pragma endregion

#pragma region Variables

static unsigned dev_count;
pjsua_dtmf_info info;
pjmedia_conf_port_info conf_info;

#pragma endregion

#pragma region Metodos 

int my_atoi2(const char* cs)
{
	pj_str_t s;

	pj_cstr(&s, cs);
	if (cs[0] == '-') {
		s.ptr++; s.slen--;
		return 0 - (int)pj_strtoul(&s);
	}
	else if (cs[0] == '+') {
		s.ptr++; s.slen--;
		return (int)pj_strtoul(&s);
	}
	else {
		return (int)pj_strtoul(&s);
	}
}

static void list_devices(void)
{
	unsigned i;
	pj_status_t status;

	dev_count = pjmedia_aud_dev_count();
	if (dev_count == 0) {
		PJ_LOG(3, (THIS_FILE, "No devices found"));
		return;
	}

	PJ_LOG(3, (THIS_FILE, "Found %d devices:", dev_count));

	for (i = 0; i < dev_count; ++i) {
		pjmedia_aud_dev_info info;

		status = pjmedia_aud_dev_get_info(i, &info);
		if (status != PJ_SUCCESS)
			continue;

		PJ_LOG(3, (THIS_FILE, " %2d: %s [%s] (%d/%d)",
			i, info.driver, info.name, info.input_count, info.output_count));
	}
}

/* Devolución de llamada llamada por la biblioteca al recibir la llamada entrante */
static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id, pjsip_rx_data* rdata)
{
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

static void ui_input_url(const char* title, char* buf, pj_size_t len, input_result* result)
{
	result->nb_result = PJSUA_APP_NO_NB;
	result->uri_result = NULL;



	printf("Choices:\n"
		"   0         For current dialog.\n"
		"  URL        An URL\n"
		"  <Enter>    Empty input (or 'q') to cancel\n"
		, pjsua_get_buddy_count(), pjsua_get_buddy_count());
	printf("%s: ", title);

	fflush(stdout);
	if (fgets(buf, (int)len, stdin) == NULL)
		return;
	len = strlen(buf);

	/* Left trim */
	while (pj_isspace(*buf)) {
		++buf;
		--len;
	}

	/* Remove trailing newlines */
	while (len && (buf[len - 1] == '\r' || buf[len - 1] == '\n'))
		buf[--len] = '\0';

	if (len == 0 || buf[0] == 'q')
		return;

	if (pj_isdigit(*buf) || *buf == '-') {

		unsigned i;

		if (*buf == '-')
			i = 1;
		else
			i = 0;

		for (; i < len; ++i) {
			if (!pj_isdigit(buf[i])) {
				puts("Invalid input");
				return;
			}
		}


		result->nb_result = my_atoi2(buf);

		if (result->nb_result >= 0 &&
			result->nb_result <= (int)pjsua_get_buddy_count())
		{
			return;
		}
		if (result->nb_result == -1)
			return;

		puts("Invalid input");
		result->nb_result = PJSUA_APP_NO_NB;
		return;

	}
	else {
		pj_status_t status;

		if ((status = pjsua_verify_url(buf)) != PJ_SUCCESS) {
			pjsua_perror(THIS_FILE, "Invalid URL", status);
			return;
		}

		result->uri_result = buf;
	}
}

static void ui_conf_list()
{
	unsigned i, count;
	pjsua_conf_port_id id[PJSUA_MAX_CALLS];

	printf("Conference ports:\n");

	count = PJ_ARRAY_SIZE(id);
	pjsua_enum_conf_ports(id, &count);

	for (i = 0; i < count; ++i) {
		char txlist[PJSUA_MAX_CALLS * 4 + 10];
		unsigned j;
		pjsua_conf_port_info info;

		pjsua_conf_get_port_info(id[i], &info);

		txlist[0] = '\0';
		for (j = 0; j < info.listener_cnt; ++j) {
			char s[10];
			pj_ansi_snprintf(s, sizeof(s), "#%d ", info.listeners[j]);
			pj_ansi_strcat(txlist, s);
		}
		printf("Port #%02d[%2dKHz/%dms/%d] %20.*s  transmitting to: %s\n",
			info.slot_id,
			info.clock_rate / 1000,
			info.samples_per_frame * 1000 / info.channel_count / info.clock_rate,
			info.channel_count,
			(int)info.name.slen,
			info.name.ptr,
			txlist);

	}
	puts("");
}

static pj_bool_t simple_input(const char* title, char* buf, pj_size_t len)
{
	char* p;

	printf("%s (empty to cancel): ", title); fflush(stdout);
	if (fgets(buf, (int)len, stdin) == NULL)
		return PJ_FALSE;

	/* Remove trailing newlines. */
	for (p = buf; ; ++p) {
		if (*p == '\r' || *p == '\n') *p = '\0';
		else if (!*p) break;
	}

	if (!*buf)
		return PJ_FALSE;

	return PJ_TRUE;
}

static void ui_conf_connect(char menuin[])
{
	char tmp[10], src_port[10], dst_port[10];
	pj_status_t status;
	int cnt;
	const char* src_title, * dst_title;

	cnt = sscanf(menuin, "%s %s %s", tmp, src_port, dst_port);

	if (cnt != 3) {
		ui_conf_list();

		src_title = (menuin[1] == 'c' ? "Connect src port #" :
			"Disconnect src port #");
		dst_title = (menuin[1] == 'c' ? "To dst port #" : "From dst port #");

		if (!simple_input(src_title, src_port, sizeof(src_port)))
			return;

		if (!simple_input(dst_title, dst_port, sizeof(dst_port)))
			return;
	}

	if (menuin[1] == 'c') {
		status = pjsua_conf_connect(my_atoi2(src_port), my_atoi2(dst_port));
	}
	else {
		status = pjsua_conf_disconnect(my_atoi2(src_port), my_atoi2(dst_port));
	}
	if (status == PJ_SUCCESS) {
		puts("Success");
	}
	else {
		puts("ERROR!!");
	}
}

static void ui_conf_disconnect(char menuin[])
{
	char tmp[10], src_port[10], dst_port[10];
	pj_status_t status;
	int cnt;
	const char* src_title, * dst_title;

	cnt = sscanf(menuin, "%s %s %s", tmp, src_port, dst_port);

	if (cnt != 3) {
		ui_conf_list();

		src_title = (menuin[1] == 'd' ? "Disconnect src port #" :
			"Disconnect src port #");
		dst_title = (menuin[1] == 'd' ? "To dst port #" : "From dst port #");

		if (!simple_input(src_title, src_port, sizeof(src_port)))
			return;

		if (!simple_input(dst_title, dst_port, sizeof(dst_port)))
			return;
	}

	if (menuin[1] == 'd') {
		status = pjsua_conf_disconnect(my_atoi2(src_port), my_atoi2(dst_port));
	}
	else {
		status = pjsua_conf_disconnect(my_atoi2(src_port), my_atoi2(dst_port));
	}
	if (status == PJ_SUCCESS) {
		puts("Success");
	}
	else {
		puts("ERROR!!");
	}
}

static void ui_call_hold()
{
	if (current_call != -1) {
		pjsua_call_set_hold(current_call, NULL);
	}
	else {
		PJ_LOG(3, (THIS_FILE, "No current call"));
	}
}

static void ui_call_reinvite()
{
	call_opt.flag |= PJSUA_CALL_UNHOLD;
	pjsua_call_reinvite(current_call, call_opt.flag, NULL);
}

/*Devolución de llamada llamada por la biblioteca cuando el estado de las llamadas ha cambiado */
static void on_call_state(pjsua_call_id call_id, pjsip_event* e)
{
	pjsua_call_info ci;

	PJ_UNUSED_ARG(e);

	pjsua_call_get_info(call_id, &ci);
	PJ_LOG(3, (THIS_FILE, "Call %d state=%.*s", call_id,
		(int)ci.state_text.slen,
		ci.state_text.ptr));
}

/* Devolución de llamada invocada por la biblioteca cuando el estado de las llamadas ha cambiado */
static void on_call_media_state(pjsua_call_id call_id)
{
	pjsua_call_info ci;

	pjsua_call_get_info(call_id, &ci);

	if (ci.media_status == PJSUA_CALL_MEDIA_ACTIVE) {
		// When media is active, connect call to sound device.
		pjsua_conf_connect(ci.conf_slot, 0);
		pjsua_conf_connect(0, ci.conf_slot);
	}
}

/* Mostrar error y salir de la aplicación */
static void error_exit(const char* title, pj_status_t status)
{
	pjsua_perror(THIS_FILE, title, status);
	pjsua_destroy();
	exit(1);
}

static void print_buddy_list()
{
	pjsua_buddy_id ids[64];
	int i;
	unsigned count = PJ_ARRAY_SIZE(ids);

	puts("Buddy list:");

	pjsua_enum_buddies(ids, &count);

	if (count == 0) {
		puts(" -none-");
	}
	else {
		for (i = 0; i < (int)count; ++i) {
			pjsua_buddy_info info;

			if (pjsua_buddy_get_info(ids[i], &info) != PJ_SUCCESS)
				continue;

			printf(" [%2d] <%.*s>  %.*s\n",
				ids[i] + 1,
				(int)info.status_text.slen,
				info.status_text.ptr,
				(int)info.uri.slen,
				info.uri.ptr);
		}
	}
	puts("");
}

static void ui_make_multi_call()
{
	char menuin[32];
	int count;
	char buf[128];
	input_result result;
	pj_str_t tmp;
	int i;

	printf("(You currently have %d calls)\n", pjsua_call_get_count());

	if (!simple_input("Number of calls", menuin, sizeof(menuin)))
		return;

	count = my_atoi2(menuin);
	if (count < 1)
		return;

	ui_input_url("Make call", buf, sizeof(buf), &result);
	if (result.nb_result != PJSUA_APP_NO_NB) {
		pjsua_buddy_info binfo;
		if (result.nb_result == -1 || result.nb_result == 0) {
			puts("You can't do that with make call!");
			return;
		}
		pjsua_buddy_get_info(result.nb_result - 1, &binfo);
		tmp.ptr = buf;
		pj_strncpy(&tmp, &binfo.uri, sizeof(buf));
	}
	else {
		tmp = pj_str(result.uri_result);
	}

	for (i = 0; i < my_atoi2(menuin); ++i) {
		pj_status_t status;

		status = pjsua_call_make_call(current_acc, &tmp, NULL, NULL,
			NULL, NULL);
		if (status != PJ_SUCCESS)
			break;
	}
}

static void ui_make_new_call()
{
	char buf[128];
	pjsua_msg_data msg_data_;
	input_result result;
	pj_str_t tmp;

	printf("(You currently have %d calls)\n", pjsua_call_get_count());

	ui_input_url("Make call", buf, sizeof(buf), &result);
	if (result.nb_result != PJSUA_APP_NO_NB) {

		if (result.nb_result == -1 || result.nb_result == 0) {
			puts("You can't do that with make call!");
			return;
		}
		else {
			pjsua_buddy_info binfo;
			pjsua_buddy_get_info(result.nb_result - 1, &binfo);
			tmp.ptr = buf;
			pj_strncpy(&tmp, &binfo.uri, sizeof(buf));
		}
	}
	else if (result.uri_result) {
		tmp = pj_str(result.uri_result);
	}
	else {
		tmp.slen = 0;
	}

	pjsua_msg_data_init(&msg_data_);
	TEST_MULTIPART(&msg_data_);
	pjsua_call_make_call(current_acc, &tmp, NULL, NULL,
		&msg_data_, &current_call);
}

static void ui_call_transfer_replaces(pj_bool_t no_refersub)
{
	if (current_call == -1) {
		PJ_LOG(3, (THIS_FILE, "No current call"));
	}
	else {
		int call = current_call;
		int dst_call;
		pjsip_generic_string_hdr refer_sub;
		pj_str_t STR_REFER_SUB = { "Refer-Sub", 9 };
		pj_str_t STR_FALSE = { "false", 5 };
		pjsua_call_id ids[PJSUA_MAX_CALLS];
		pjsua_call_info ci;
		pjsua_msg_data msg_data_;
		char buf[128];
		unsigned i, count;

		count = PJ_ARRAY_SIZE(ids);
		pjsua_enum_calls(ids, &count);

		if (count <= 1) {
			puts("There are no other calls");
			return;
		}

		pjsua_call_get_info(current_call, &ci);
		printf("Transfer call [%d] %.*s to one of the following:\n",
			current_call,
			(int)ci.remote_info.slen, ci.remote_info.ptr);

		for (i = 0; i < count; ++i) {
			pjsua_call_info call_info;

			if (ids[i] == call)
				continue;

			pjsua_call_get_info(ids[i], &call_info);
			printf("%d  %.*s [%.*s]\n",
				ids[i],
				(int)call_info.remote_info.slen,
				call_info.remote_info.ptr,
				(int)call_info.state_text.slen,
				call_info.state_text.ptr);
		}

		if (!simple_input("Enter call number to be replaced", buf, sizeof(buf)))
			return;

		dst_call = my_atoi2(buf);

		/* Check if call is still there. */

		if (call != current_call) {
			puts("Call has been disconnected");
			return;
		}

		/* Check that destination call is valid. */
		if (dst_call == call) {
			puts("Destination call number must not be the same "
				"as the call being transferred");
			return;
		}
		if (dst_call >= PJSUA_MAX_CALLS) {
			puts("Invalid destination call number");
			return;
		}
		if (!pjsua_call_is_active(dst_call)) {
			puts("Invalid destination call number");
			return;
		}

		pjsua_msg_data_init(&msg_data_);
		if (no_refersub) {
			/* Add Refer-Sub: false in outgoing REFER request */
			pjsip_generic_string_hdr_init2(&refer_sub, &STR_REFER_SUB,
				&STR_FALSE);
			pj_list_push_back(&msg_data_.hdr_list, &refer_sub);
		}

		pjsua_call_xfer_replaces(call, dst_call,
			PJSUA_XFER_NO_REQUIRE_REPLACES,
			&msg_data_);
	}
}

/* Devolución automática de llamada del temporizador de suspensión */
static void hangup_timeout_callback(pj_timer_heap_t* timer_heap, struct pj_timer_entry* entry)
{
	PJ_UNUSED_ARG(timer_heap);
	PJ_UNUSED_ARG(entry);

	app_config.auto_hangup_timer.id = 0;
	pjsua_call_hangup_all();
}

static void call_on_dtmf_callback2(pjsua_call_id call_id, const pjsua_dtmf_info* info)
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

static void ui_send_dtmf_2833()
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
		if (!simple_input("DTMF strings to send (0-9*R#A-B)", buf,
			sizeof(buf)))
#else
		if (!simple_input("DTMF strings to send (0-9*#A-B)", buf,
			sizeof(buf)))
#endif
		{
			return;
		}

		if (call != current_call) {
			puts("Call has been disconnected");
			return;
		}

		digits = pj_str(buf);
		status = pjsua_call_dial_dtmf(current_call, &digits);
		if (status != PJ_SUCCESS) {
			pjsua_perror(THIS_FILE, "Unable to send DTMF", status);
		}
		else {
			puts("DTMF digits enqueued for transmission");
		}
	}
}

static pj_status_t wav_play_cb(void* user_data, pjmedia_frame* frame)
{
	return pjmedia_port_get_frame((pjmedia_port*)user_data, frame);
}

static void app_perror(const char* title, pj_status_t status)
{
	char errmsg[PJ_ERR_MSG_SIZE];

	pj_strerror(status, errmsg, sizeof(errmsg));
	printf("%s: %s (err=%d)\n",
		title, errmsg, status);
}

/* Notificación de finalización del archivo de reproducción, ajuste el temporizador para colgar llamadas */
void on_playfile_done(pjmedia_port* port, void* usr_data)
{
	pj_time_val delay;

	PJ_UNUSED_ARG(port);
	PJ_UNUSED_ARG(usr_data);

	/* Just rewind WAV when it is played outside of call */
	if (pjsua_call_get_count() == 0) {
		pjsua_player_set_pos(app_config.wav_id, 0);
	}

	/* Timer is already active */
	if (app_config.auto_hangup_timer.id == 1)
		return;

	app_config.auto_hangup_timer.id = 1;
	delay.sec = 0;
	delay.msec = 200; /* Give 200 ms before hangup */
	pjsip_endpt_schedule_timer(pjsua_get_pjsip_endpt(),
		&app_config.auto_hangup_timer,
		&delay);
}

static void play_file(unsigned play_index, const char* filename)
{
	pj_pool_t* pool = NULL;
	pjmedia_port* wav = NULL;
	pjmedia_aud_param param;
	pjsua_player_id wav_id;
	pjmedia_aud_stream* strm = NULL;
	char line[10], * dummy;
	pj_status_t status;
	pjsua_call_info call_info;

	if (filename == NULL)
		filename = WAV_FILE;

	pool = pj_pool_create(pjmedia_aud_subsys_get_pool_factory(), "wav",
		1000, 1000, NULL);

	status = pjmedia_wav_player_port_create(pool, filename, 20, 0, 0, &wav);
	if (status != PJ_SUCCESS) {
		app_perror("Error opening WAV file", status);
		goto on_return;
	}

	status = pjmedia_aud_dev_default_param(play_index, &param);
	if (status != PJ_SUCCESS) {
		app_perror("pjmedia_aud_dev_default_param()", status);
		goto on_return;
	}

	param.dir = PJMEDIA_DIR_PLAYBACK;
	param.clock_rate = PJMEDIA_PIA_SRATE(&wav->info);
	param.samples_per_frame = PJMEDIA_PIA_SPF(&wav->info);
	param.channel_count = PJMEDIA_PIA_CCNT(&wav->info);
	param.bits_per_sample = PJMEDIA_PIA_BITS(&wav->info);

	status = pjmedia_aud_stream_create(&param, NULL, &wav_play_cb, wav,
		&strm);
	if (status != PJ_SUCCESS) {
		app_perror("Error opening the sound device", status);
		goto on_return;
	}

	status = pjmedia_aud_stream_start(strm);
	if (status != PJ_SUCCESS) {
		app_perror("Error starting the sound device", status);
		goto on_return;
	}

	PJ_LOG(3, (THIS_FILE, "Playback started, press ENTER to stop"));
	dummy = fgets(line, sizeof(line), stdin);
	PJ_UNUSED_ARG(dummy);

on_return:
	if (strm) {
		pjmedia_aud_stream_stop(strm);
		pjmedia_aud_stream_destroy(strm);
	}
	if (wav)
		pjmedia_port_destroy(wav);
	if (pool)
		pj_pool_release(pool);
}

static void create_wav_port(unsigned play_index, const char* filename)
{
	pj_status_t status;
	pjsua_player_id wav_id;
	unsigned play_options = 0;


	play_options = PJMEDIA_FILE_NO_LOOP;

	pj_str_t wav = pj_str(WAV_FILE);
	app_config.wav_files[0] = wav;


	status = pjsua_player_create(&app_config.wav_files[0], play_options,
		&wav_id);
	if (status != PJ_SUCCESS)
		app_perror("Error creating player", status);

	if (app_config.wav_id == PJSUA_INVALID_ID) {
		app_config.wav_id = wav_id;
		app_config.wav_port = pjsua_player_get_conf_port(app_config.wav_id);
		if (app_config.auto_play_hangup) {
			pjmedia_port* port;

			pjsua_player_get_port(app_config.wav_id, &port);
			status = pjmedia_wav_player_set_eof_cb2(port, NULL,
				&on_playfile_done);
			if (status != PJ_SUCCESS)
				app_perror("Error creating player port", status);

			pj_timer_entry_init(&app_config.auto_hangup_timer, 0, NULL,
				&hangup_timeout_callback);
		}
	}
}

#pragma endregion

#pragma region Main

/* Metodo principal */
int main(int argc, char* argv[])
{
	int index;
	int port_count;
	int clock_rate;
	char filename[80];
	char menuin[3] = { 'a','c','d' };

	pj_pool_t* pool;
	pjsua_acc_id acc_id;
	pjmedia_port* wav = NULL;
	pj_status_t status;
	pjsua_player_id wav_id = PJSUA_INVALID_ID;
	pj_str_t dtmf = pj_str("1234");
	pjmedia_conf* conf;
	pjmedia_port* rec_port = NULL;

	/* Create pjsua first! */
	status = pjsua_create();
	if (status != PJ_SUCCESS) error_exit("Error in pjsua_create()", status);

	/* If argument is specified, it's got to be a valid SIP URL */
	if (argc > 1) {
		status = pjsua_verify_url(argv[1]);
		if (status != PJ_SUCCESS) error_exit("Invalid URL in argv", status);
	}

	/* Init pjsua */
	{
		pjsua_config cfg;
		pjsua_logging_config log_cfg;

		pjsua_config_default(&cfg);
		cfg.cb.on_incoming_call = &on_incoming_call;
		cfg.cb.on_call_media_state = &on_call_media_state;
		cfg.cb.on_call_state = &on_call_state;
		cfg.cb.on_dtmf_digit2 = &call_on_dtmf_callback2;
		pjsua_logging_config_default(&log_cfg);
		log_cfg.console_level = 4;

		status = pjsua_init(&cfg, &log_cfg, NULL);

		if (status != PJ_SUCCESS) error_exit("Error in pjsua_init()", status);
		if (app_config.wav_id == PJSUA_INVALID_ID) {
			app_config.wav_id = wav_id;
			app_config.wav_port = pjsua_player_get_conf_port(app_config.wav_id);
			if (app_config.auto_play_hangup) {
				pjmedia_port* port;

				pjsua_player_get_port(app_config.wav_id, &port);
				status = pjmedia_wav_player_set_eof_cb2(port, NULL,
					&on_playfile_done);
				if (status != PJ_SUCCESS)
					error_exit("Create wav player eof", status);

				pj_timer_entry_init(&app_config.auto_hangup_timer, 0, NULL,
					&hangup_timeout_callback);
			}
		}
	}

	/* Add UDP transport. */
	{
		pjsua_transport_config cfg;

		pjsua_transport_config_default(&cfg);
		cfg.port = 5060;
		status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &cfg, NULL);
		if (status != PJ_SUCCESS) error_exit("Error creating transport", status);
	}

	/* Initialization is done, now start pjsua */
	status = pjsua_start();
	if (status != PJ_SUCCESS) error_exit("Error starting pjsua", status);

	/* Register to SIP server by creating SIP account. */
	{
		pjsua_acc_config cfg;

		pjsua_acc_config_default(&cfg);
		cfg.id = pj_str("sip:" SIP_USER "@" SIP_DOMAIN);
		cfg.reg_uri = pj_str("sip:" SIP_DOMAIN);
		cfg.cred_count = 1;
		cfg.cred_info[0].realm = pj_str("*");
		cfg.cred_info[0].scheme = pj_str("digest");
		cfg.cred_info[0].username = pj_str(SIP_USER);
		cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
		cfg.cred_info[0].data = pj_str(SIP_PASSWD);

		status = pjsua_acc_add(&cfg, PJ_TRUE, &acc_id);
		if (status != PJ_SUCCESS) error_exit("Error adding account", status);
	}

	/* If URL is specified, make call to the URL. */
	if (argc > 1) {
		pj_str_t uri = pj_str(argv[1]);
		status = pjsua_call_make_call(acc_id, &uri, 0, NULL, NULL, NULL);
		if (status != PJ_SUCCESS) error_exit("Error making call", status);
	}
	pj_str_t uri3 = pj_str("sip:502@192.168.1.25");

	/* Wait until user press "q" to quit. */
	for (;;) {
		char option[80];
		char line[80];
		puts("Press 'g' to hangup all calls,  'x' to quit, 'c' to make a call, 'm' to make multiple call, 'h' to put in hold the call, 'r' to retreive the call, 't' to transfer, 'd' to send dtmf, 'l' to list the devices, 'v' to list de conference ports, 'a' to connect the ports, 'j' to disconnect the ports, 'p' to create the wav port");
		if (fgets(option, sizeof(option), stdin) == NULL) {
			puts("EOF while reading stdin, will quit now..");
			break;
		}
		switch (option[0])
		{
		case 'x':
			pjsua_destroy();
			pjsua_player_destroy(wav_id);
			return 0;
			break;
		case 'g':
			pjsua_call_hangup_all();
			break;
		case 'c':
			ui_make_new_call();
			break;
		case 'm':
			ui_make_multi_call();
			break;
		case 'h':
			ui_call_hold();
			break;
		case 'd':
			ui_send_dtmf_2833();
			break;
		case 't':
			ui_call_transfer_replaces(app_config.no_refersub);
			break;
		case 'p':
			create_wav_port(wav_id, WAV_FILE);
			break;
		case 'l':
			list_devices();
			break;
		case 'r':
			if (current_call != -1) {
				/* re-INVITE */
				ui_call_reinvite();
			}
			else
			{
				PJ_LOG(3, (THIS_FILE, "No current call"));
			}
			break;
		case 'a':
			ui_conf_connect(menuin);
			break;
		case 'j':
			ui_conf_disconnect(menuin);
			break;
		case'v':
			ui_conf_list();
			break;
		case'P':
			play_file(wav_id, WAV_FILE);
			break;
		}
	}
}

#pragma endregion


