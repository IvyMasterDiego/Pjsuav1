#pragma once
#include <iostream>
#include "pjsua-lib/pjsua.h"
#include "pjsua-lib/pjsua_internal.h"

class Call
{
public:

	static void Register(char* username, char* password, char* uri, char* reuri, int loglevel);

	void MakeCall(char* uri);

	void Hangup();

	void Hold();

	void ReInvite();

	void Transfer(char* uri);

	void SendDTMF(char* uri);

	static void Destroy();

	void PlayWavFile(char* wavFile);
};
