#include "Call.h"
#include "marshal.h"

using namespace System;
using namespace System::Runtime::InteropServices;
using namespace msclr::interop;
using namespace System::Threading;
using namespace System::Collections::Generic;
using namespace System::Globalization;

namespace PJWrapper {

	public delegate void IncomingCallHandler(Int32 callID, String^ testMessage);
	public delegate void CallMediaStateHandler(Int32 callID);
	public delegate void CallStateHandler(Int32 callID, String^ callState);
	public delegate void CallDtmfCallBackHandler(Int32 callID, String^ testMessage);

	public ref class Wrapper {

	public:
		Call* call;

		Wrapper() {
			this->call = new Call();
		}

		static event IncomingCallHandler^ OnIncoming;
		static void InvokeIncomingCallEnvet(Int32 callID, String^ testMessage) {
			Wrapper::OnIncoming(callID, testMessage);
		}

		static event CallMediaStateHandler^ OnCallMediaState;
		static void InvokeOnCallMediaStateEvent(Int32 callID) {
			Wrapper::OnCallMediaState(callID);
		}

		static event CallStateHandler^ OnCallState;
		static void InvokeCallStateEvent(Int32 callID, String^ callState) {
			Wrapper::OnCallState(callID, callState);
		}

		static event CallDtmfCallBackHandler^ OnCallDtmfCallBack;
		static void InvokeCallDtmfCallBackEvent(Int32 callID, String^ testMessage) {
			Wrapper::OnCallDtmfCallBack(callID, testMessage);
		}

		void Register(String^ username, String^ password, String^ uri, String^ reuri, Int32 loglevel) {
			char* _usr = (char*)(void*)Marshal::StringToHGlobalAnsi(username);
			char* _psw = (char*)(void*)Marshal::StringToHGlobalAnsi(password);
			char* _uri = (char*)(void*)Marshal::StringToHGlobalAnsi(uri);
			char* _reuri = (char*)(void*)Marshal::StringToHGlobalAnsi(reuri);
			this->call->Register(_usr, _psw, _uri, _reuri, loglevel);
		}

		void MakeCall(String^ uri) {
			char* _uri = (char*)(void*)Marshal::StringToHGlobalAnsi(uri);
			this->call->MakeCall(_uri);
		}

		void Hangup() {
			this->call->Hangup();
		}

		void Hold() {
			this->call->Hold();
		}

		void ReInvite() {
			this->call->ReInvite();
		}

		void TransferCall(String^ uri) {

			char* _uri = (char*)(void*)Marshal::StringToHGlobalAnsi(uri);
			this->call->Transfer(_uri);
		}

		void SendDtmf(String^ digits) {
			char* _digits = (char*)(void*)Marshal::StringToHGlobalAnsi(digits);
			this->call->SendDTMF(_digits);
		}

		static void Destroy() {
			Call::Destroy();
		}
		void PlayWavFile(String^ file) {
			char* _file = (char*)(void*)Marshal::StringToHGlobalAnsi(file);
			this->call->PlayWavFile(_file);
			//if (result == -1) {
			//	throw gcnew System::Exception("Error reproduciendo el archivo");
			//}
		}

		void PlayWavFile(array<String^>^ files) {
			for each (String ^ file in files)
			{
				PlayWavFile(file);
			}
		}
	};
}