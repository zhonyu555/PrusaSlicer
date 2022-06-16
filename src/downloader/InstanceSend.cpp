#include "InstanceSend.hpp"
 
#include "DownloaderApp.hpp"

#ifdef _WIN32
#include <windows.h>
#include <strsafe.h>
#endif //WIN32

#include <boost/nowide/convert.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/log/trivial.hpp>
#include <wx/utils.h>
#include <iostream>

#include "libslic3r/Config.hpp"


namespace Downloader {

namespace {
// TODO: Taken from from Config.cpp
	std::string escape_strings_cstyle(const std::vector<std::string>& strs)
	{
		// 1) Estimate the output buffer size to avoid buffer reallocation.
		size_t outbuflen = 0;
		for (size_t i = 0; i < strs.size(); ++i)
			// Reserve space for every character escaped + quotes + semicolon.
			outbuflen += strs[i].size() * 2 + 3;
		// 2) Fill in the buffer.
		std::vector<char> out(outbuflen, 0);
		char* outptr = out.data();
		for (size_t j = 0; j < strs.size(); ++j) {
			if (j > 0)
				// Separate the strings.
				(*outptr++) = ';';
			const std::string& str = strs[j];
			// Is the string simple or complex? Complex string contains spaces, tabs, new lines and other
			// escapable characters. Empty string shall be quoted as well, if it is the only string in strs.
			bool should_quote = strs.size() == 1 && str.empty();
			for (size_t i = 0; i < str.size(); ++i) {
				char c = str[i];
				if (c == ' ' || c == ';' || c == '\t' || c == '\\' || c == '"' || c == '\r' || c == '\n') {
					should_quote = true;
					break;
				}
			}
			if (should_quote) {
				(*outptr++) = '"';
				for (size_t i = 0; i < str.size(); ++i) {
					char c = str[i];
					if (c == '\\' || c == '"') {
						(*outptr++) = '\\';
						(*outptr++) = c;
					}
					else if (c == '\r') {
						(*outptr++) = '\\';
						(*outptr++) = 'r';
					}
					else if (c == '\n') {
						(*outptr++) = '\\';
						(*outptr++) = 'n';
					}
					else
						(*outptr++) = c;
				}
				(*outptr++) = '"';
			}
			else {
				memcpy(outptr, str.data(), str.size());
				outptr += str.size();
			}
		}
		return std::string(out.data(), outptr - out.data());
	}


#ifdef _WIN32
static HWND l_prusa_slicer_hwnd;
static HWND l_downloader_hwnd;
BOOL CALLBACK EnumWindowsProcSlicer(_In_ HWND hwnd, _In_ LPARAM lParam)
{
	//checks for other instances of prusaslicer, if found brings it to front and return false to cancel enumeration and quit this instance
	//search is done by classname(wxWindowNR is wxwidgets thing, so probably not unique) and name in window upper panel
	//other option would be do a mutex and check for its existence
	//BOOST_LOG_TRIVIAL(error) << "ewp: version: " << l_version_wstring;
	TCHAR 		 wndText[1000];
	TCHAR 		 className[1000];
	int          err;
	err = GetClassName(hwnd, className, 1000);
	if (err == 0)
		return true;
	err = GetWindowText(hwnd, wndText, 1000);
	if (err == 0)
		return true;
	std::wstring classNameString(className);
	std::wstring wndTextString(wndText);
	if (wndTextString.find(L"PrusaSlicer") != std::wstring::npos && classNameString == L"wxWindowNR") {
		//check if other instances has same instance hash
		//if not it is not same version(binary) as this version 
		HANDLE   handle = GetProp(hwnd, L"Instance_Hash_Minor");
		uint64_t other_instance_hash = PtrToUint(handle);
		uint64_t other_instance_hash_major;
		//TODO
		//uint64_t my_instance_hash = GUI::wxGetApp().get_instance_hash_int();
		handle = GetProp(hwnd, L"Instance_Hash_Major");
		other_instance_hash_major = PtrToUint(handle);
		other_instance_hash_major = other_instance_hash_major << 32;
		other_instance_hash += other_instance_hash_major;
		//if (my_instance_hash == other_instance_hash)
		{
			//BOOST_LOG_TRIVIAL(debug) << "win enum - found correct instance";
			l_prusa_slicer_hwnd = hwnd;
			ShowWindow(hwnd, SW_SHOWMAXIMIZED);
			SetForegroundWindow(hwnd);
			return false;
		}
		//BOOST_LOG_TRIVIAL(debug) << "win enum - found wrong instance";
	}
	return true;
}

bool send_message_slicer(const wxString& message)
{
	if (!EnumWindows(EnumWindowsProcSlicer, 0)) {
		std::wstring wstr(message.c_str());//boost::nowide::widen(message);
		std::unique_ptr<LPWSTR> command_line_args = std::make_unique<LPWSTR>(const_cast<LPWSTR>(wstr.c_str()));
		/*LPWSTR command_line_args = new wchar_t[wstr.size() + 1];
		copy(wstr.begin(), wstr.end(), command_line_args);
		command_line_args[wstr.size()] = 0;*/

		//Create a COPYDATASTRUCT to send the information
		//cbData represents the size of the information we want to send.
		//lpData represents the information we want to send.
		//dwData is an ID defined by us(this is a type of ID different than WM_COPYDATA).
		COPYDATASTRUCT data_to_send = { 0 };
		data_to_send.dwData = 1;
		data_to_send.cbData = sizeof(TCHAR) * (wcslen(*command_line_args.get()) + 1);
		data_to_send.lpData = *command_line_args.get();
		SendMessage(l_prusa_slicer_hwnd, WM_COPYDATA, 0, (LPARAM)&data_to_send);
		return true;
	}
	return false;
}


BOOL CALLBACK EnumWindowsProcDownloader(_In_ HWND hwnd, _In_ LPARAM lParam)
{
	//checks for other instances of prusaslicer, if found brings it to front and return false to cancel enumeration and quit this instance
	//search is done by classname(wxWindowNR is wxwidgets thing, so probably not unique) and name in window upper panel
	//other option would be do a mutex and check for its existence
	//BOOST_LOG_TRIVIAL(error) << "ewp: version: " << l_version_wstring;
	TCHAR 		 wndText[1000];
	TCHAR 		 className[1000];
	int          err;
	err = GetClassName(hwnd, className, 1000);
	if (err == 0)
		return true;
	err = GetWindowText(hwnd, wndText, 1000);
	if (err == 0)
		return true;
	std::wstring classNameString(className);
	std::wstring wndTextString(wndText);
	if (wndTextString.find(L"Downloader") != std::wstring::npos && classNameString == L"wxWindowNR") {
		//check if other instances has same instance hash
		//if not it is not same version(binary) as this version 
		HANDLE   handle = GetProp(hwnd, L"Instance_Hash_Minor");
		uint64_t other_instance_hash = PtrToUint(handle);
		uint64_t other_instance_hash_major;
		//TODO
		//uint64_t my_instance_hash = GUI::wxGetApp().get_instance_hash_int();
		handle = GetProp(hwnd, L"Instance_Hash_Major");
		other_instance_hash_major = PtrToUint(handle);
		other_instance_hash_major = other_instance_hash_major << 32;
		other_instance_hash += other_instance_hash_major;
		//if (my_instance_hash == other_instance_hash)
		{
			//BOOST_LOG_TRIVIAL(debug) << "win enum - found correct instance";
			l_downloader_hwnd = hwnd;
			ShowWindow(hwnd, SW_NORMAL);
			SetForegroundWindow(hwnd);
			return false;
		}
		//BOOST_LOG_TRIVIAL(debug) << "win enum - found wrong instance";
	}
	return true;
}


bool send_message_downloader(const wxString& message)
{
	if (!EnumWindows(EnumWindowsProcDownloader, 0)) {
		std::wstring wstr(message.c_str());//boost::nowide::widen(message);
		std::unique_ptr<LPWSTR> command_line_args = std::make_unique<LPWSTR>(const_cast<LPWSTR>(wstr.c_str()));
		/*LPWSTR command_line_args = new wchar_t[wstr.size() + 1];
		copy(wstr.begin(), wstr.end(), command_line_args);
		command_line_args[wstr.size()] = 0;*/

		//Create a COPYDATASTRUCT to send the information
		//cbData represents the size of the information we want to send.
		//lpData represents the information we want to send.
		//dwData is an ID defined by us(this is a type of ID different than WM_COPYDATA).
		COPYDATASTRUCT data_to_send = { 0 };
		data_to_send.dwData = 1;
		data_to_send.cbData = sizeof(TCHAR) * (wcslen(*command_line_args.get()) + 1);
		data_to_send.lpData = *command_line_args.get();
		SendMessage(l_downloader_hwnd, WM_COPYDATA, 0, (LPARAM)&data_to_send);
		return true;
	}
	return false;
}


bool execute_command(const wxString& command)
{
	return wxExecute(command);
}	

#elif defined(__linux__)

bool send_message_slicer(const std::string &message_text, const std::string &version)
{
	DBusMessage* msg;
    // DBusMessageIter args;
	DBusConnection* conn;
	DBusError 		err;
	dbus_uint32_t 	serial = 0;
	const char* sigval = message_text.c_str();
	//std::string		interface_name = "com.prusa3d.prusaslicer.InstanceCheck";
	std::string		interface_name = "com.prusa3d.prusaslicer.InstanceCheck.Object" + version;
	std::string   	method_name = "AnotherInstance";
	//std::string		object_name = "/com/prusa3d/prusaslicer/InstanceCheck";
	std::string		object_name = "/com/prusa3d/prusaslicer/InstanceCheck/Object" + version;

	std::cout << "interface_name: " << interface_name << std::endl;
	std::cout << "method_name: " << method_name<< std::endl;
	std::cout << "object_name: " << object_name << std::endl;
	// initialise the error value
	dbus_error_init(&err);

	// connect to bus, and check for errors (use SESSION bus everywhere!)
	conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
	if (dbus_error_is_set(&err)) {
		//BOOST_LOG_TRIVIAL(error) << "DBus Connection Error. Message to another instance wont be send.";
		//BOOST_LOG_TRIVIAL(error) << "DBus Connection Error: " << err.message;
		std::cout << "DBus Connection Error. Message to another instance wont be send." << std::endl;
		std::cout << "DBus Connection Error: " << err.message << std::endl;
		dbus_error_free(&err);
		return true;
	}
	if (NULL == conn) {
		//BOOST_LOG_TRIVIAL(error) << "DBus Connection is NULL. Message to another instance wont be send.";
		std::cout << "DBus Connection is NULL. Message to another instance wont be send." << std::endl;
		return true;
	}
	std::cout<< "dbus_bus_get SUCCESS" << std::endl;
	//some sources do request interface ownership before constructing msg but i think its wrong.

	//create new method call message
	msg = dbus_message_new_method_call(interface_name.c_str(), object_name.c_str(), interface_name.c_str(), method_name.c_str());
	if (NULL == msg) {
		//BOOST_LOG_TRIVIAL(error) << "DBus Message is NULL. Message to another instance wont be send.";
		std::cout << "DBus Message is NULL. Message to another instance wont be send."<< std::endl;
		dbus_connection_unref(conn);
		return true;
	}
	std::cout<< "dbus_message_new_method_call SUCCESS" << std::endl;
	//the AnotherInstance method is not sending reply.
	dbus_message_set_no_reply(msg, TRUE);

	//append arguments to message
	if (!dbus_message_append_args(msg, DBUS_TYPE_STRING, &sigval, DBUS_TYPE_INVALID)) {
		//BOOST_LOG_TRIVIAL(error) << "Ran out of memory while constructing args for DBus message. Message to another instance wont be send.";
		std::cout << "Ran out of memory while constructing args for DBus message. Message to another instance wont be send." << std::endl;
		dbus_message_unref(msg);
		dbus_connection_unref(conn);
		return true;
	}

	// send the message and flush the connection
	if (!dbus_connection_send(conn, msg, &serial)) {
		//BOOST_LOG_TRIVIAL(error) << "Ran out of memory while sending DBus message.";
		std::cout << "Ran out of memory while sending DBus message." << std::endl;
		dbus_message_unref(msg);
		dbus_connection_unref(conn);
		return true;
	}
	dbus_connection_flush(conn);

	BOOST_LOG_TRIVIAL(trace) << "DBus message sent.";
	std::cout << "DBus message sent." << std::endl;

	// free the message and close the connection
	dbus_message_unref(msg);                                                                                                                                                                                    
	dbus_connection_unref(conn);
	return true;
}

bool send_message_downloader(const std::string &message_text, const std::string &version)
{
	DBusMessage* msg;
    // DBusMessageIter args;
	DBusConnection* conn;
	DBusError 		err;
	dbus_uint32_t 	serial = 0;
	const char* sigval = message_text.c_str();
	//std::string		interface_name = "com.prusa3d.prusaslicer.InstanceCheck";
	std::string		interface_name = "com.prusa3d.prusaslicer.Downloader.Object" + version;
	std::string   	method_name = "AnotherInstance";
	//std::string		object_name = "/com/prusa3d/prusaslicer/InstanceCheck";
	std::string		object_name = "/com/prusa3d/prusaslicer/Downloader/Object" + version;

	std::cout << "interface_name: " << interface_name << std::endl;
	std::cout << "method_name: " << method_name<< std::endl;
	std::cout << "object_name: " << object_name << std::endl;
	// initialise the error value
	dbus_error_init(&err);

	// connect to bus, and check for errors (use SESSION bus everywhere!)
	conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
	if (dbus_error_is_set(&err)) {
		//BOOST_LOG_TRIVIAL(error) << "DBus Connection Error. Message to another instance wont be send.";
		//BOOST_LOG_TRIVIAL(error) << "DBus Connection Error: " << err.message;
		std::cout << "DBus Connection Error. Message to another instance wont be send." << std::endl;
		std::cout << "DBus Connection Error: " << err.message << std::endl;
		dbus_error_free(&err);
		return true;
	}
	if (NULL == conn) {
		//BOOST_LOG_TRIVIAL(error) << "DBus Connection is NULL. Message to another instance wont be send.";
		std::cout << "DBus Connection is NULL. Message to another instance wont be send." << std::endl;
		return true;
	}
	std::cout<< "dbus_bus_get SUCCESS" << std::endl;
	//some sources do request interface ownership before constructing msg but i think its wrong.

	//create new method call message
	msg = dbus_message_new_method_call(interface_name.c_str(), object_name.c_str(), interface_name.c_str(), method_name.c_str());
	if (NULL == msg) {
		//BOOST_LOG_TRIVIAL(error) << "DBus Message is NULL. Message to another instance wont be send.";
		std::cout << "DBus Message is NULL. Message to another instance wont be send."<< std::endl;
		dbus_connection_unref(conn);
		return true;
	}
	std::cout<< "dbus_message_new_method_call SUCCESS" << std::endl;
	//the AnotherInstance method is not sending reply.
	dbus_message_set_no_reply(msg, TRUE);

	//append arguments to message
	if (!dbus_message_append_args(msg, DBUS_TYPE_STRING, &sigval, DBUS_TYPE_INVALID)) {
		//BOOST_LOG_TRIVIAL(error) << "Ran out of memory while constructing args for DBus message. Message to another instance wont be send.";
		std::cout << "Ran out of memory while constructing args for DBus message. Message to another instance wont be send." << std::endl;
		dbus_message_unref(msg);
		dbus_connection_unref(conn);
		return true;
	}

	// send the message and flush the connection
	if (!dbus_connection_send(conn, msg, &serial)) {
		//BOOST_LOG_TRIVIAL(error) << "Ran out of memory while sending DBus message.";
		std::cout << "Ran out of memory while sending DBus message." << std::endl;
		dbus_message_unref(msg);
		dbus_connection_unref(conn);
		return true;
	}
	dbus_connection_flush(conn);

	BOOST_LOG_TRIVIAL(trace) << "DBus message sent.";
	std::cout << "DBus message sent." << std::endl;

	// free the message and close the connection
	dbus_message_unref(msg);                                                                                                                                                                                    
	dbus_connection_unref(conn);
	return true;
}

bool get_other_downloader_exists(const std::string &version)
{
	
	DBusMessage* msg;
	DBusMessage* reply;
	const char * result = nullptr;
    // DBusMessageIter args;
	DBusConnection* conn;
	DBusError 		err;
	dbus_uint32_t 	serial = 0;
	//const char* sigval = message_text.c_str();
	std::string		interface_name = "com.prusa3d.prusaslicer.Downloader.Object" + version;
	std::string   	method_name = "Introspect";
	std::string		object_name = "/com/prusa3d/prusaslicer/Downloader/Object" + version;

	// initialise the error value
	dbus_error_init(&err);

	// connect to bus, and check for errors (use SESSION bus everywhere!)
	conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
	if (dbus_error_is_set(&err)) {
		//BOOST_LOG_TRIVIAL(error) << "DBus Connection Error. Message to another instance wont be send.";
		//BOOST_LOG_TRIVIAL(error) << "DBus Connection Error: " << err.message;
		std::cout << "DBus Connection Error. Message to another instance wont be send." << std::endl;
		std::cout << "DBus Connection Error: " << err.message << std::endl;
		dbus_error_free(&err);
		return true;
	}
	if (NULL == conn) {
		//BOOST_LOG_TRIVIAL(error) << "DBus Connection is NULL. Message to another instance wont be send.";
		std::cout << "DBus Connection is NULL. Message to another instance wont be send." << std::endl;
		return true;
	}
	//some sources do request interface ownership before constructing msg but i think its wrong.

	//create new method call message
	msg = dbus_message_new_method_call(interface_name.c_str(), object_name.c_str(), interface_name.c_str(), method_name.c_str());
	if (NULL == msg) {
		//BOOST_LOG_TRIVIAL(error) << "DBus Message is NULL. Message to another instance wont be send.";
		std::cout << "DBus Message is NULL. Message to another instance wont be send."<< std::endl;
		dbus_connection_unref(conn);
		return true;
	}
	//the AnotherInstance method is not sending reply.
	dbus_message_set_no_reply(msg, TRUE);

	if ( nullptr == (reply = dbus_connection_send_with_reply_and_block(conn, msg, DBUS_TIMEOUT_USE_DEFAULT, &err)) ) {
        dbus_message_unref(msg);
        dbus_connection_unref(conn);
        perror(err.name);
        perror(err.message);
        return true;
    }

	if ( !dbus_message_get_args(reply, &err, DBUS_TYPE_STRING, &result, DBUS_TYPE_INVALID) ) {
        dbus_message_unref(msg);
        dbus_message_unref(reply);
        dbus_connection_unref(conn);
        perror(err.name);
        perror(err.message);
        return true;
    }
    // Work with the results of the remote procedure call

    std::cout << "Connected to D-Bus as \"" << ::dbus_bus_get_unique_name(conn) << "\"." << std::endl;
    std::cout << "Introspection Result:" << std::endl;
    std::cout << std::endl << result << std::endl << std::endl;

	//dbus_connection_flush(conn);

	//BOOST_LOG_TRIVIAL(trace) << "DBus message sent.";
	std::cout << "DBus message sent." << std::endl;

	// free the message and close the connection
	dbus_message_unref(msg);                                                                                                                                                                                    
	dbus_connection_unref(conn);
	return false;
	
}

#endif //__APPLE__/__linux__

}


// ------ SlicerSend ----------------


bool SlicerSend::get_instance_exists() const
{
#ifdef _WIN32
    return !EnumWindows(EnumWindowsProcSlicer, 0);
#endif
    return false;
}
bool SlicerSend::send_path(const wxString& path) const
{
#ifdef _WIN32
	std::string escaped = escape_strings_cstyle({ "prusa-downloader", boost::nowide::narrow(path) });
    return send_message_slicer(boost::nowide::widen(escaped));
#else
    // todo: this path will be different 
    std::string slicer_path = (boost::dll::program_location().parent_path().parent_path() / "prusa-slicer").string();
    size_t hashed_path = std::hash<std::string>{}(boost::filesystem::canonical(boost::filesystem::system_complete(slicer_path)).string());
    std::string lock_name = std::to_string(hashed_path);
	std::cout << "hash: "<< lock_name;
    std::string escaped = escape_strings_cstyle({ "prusa-downloader", boost::nowide::narrow(path) });
    return send_message_slicer(escaped, lock_name);
#endif
}

bool SlicerSend::start_with_path(const wxString& path) const
{
#ifdef _WIN32
	// "C:\\Users\\User\\Downloads\\PrusaSlicer-2.4.2+win64-202204251110\\prusa-slicer.exe " 
	std::string escaped = escape_strings_cstyle({  boost::nowide::narrow(path) });
	//return execute_command(boost::nowide::widen(escaped));
	std::string binary = (boost::dll::program_location().parent_path() / "prusa-slicer.exe").string() + " ";
	//return execute_command("C:\\Users\\User\\Downloads\\PrusaSlicer-2.4.2+win64-202204251110\\prusa-slicer.exe " + boost::nowide::widen(escaped));
	return execute_command(boost::nowide::widen(binary) + boost::nowide::widen(escaped));
#endif
	return false;
}

bool SlicerSend::start_or_send(const wxString& path) const
{
	if (send_path(path))
		return true;
	return start_with_path(path);
}


// ------ DownloaderSend ----------------


bool DownloaderSend::get_instance_exists() const
{
#ifdef _WIN32
	return !EnumWindows(EnumWindowsProcDownloader, 0);
#else
	std::string slicer_path = (boost::dll::program_location()).string();
    size_t hashed_path = std::hash<std::string>{}(boost::filesystem::canonical(boost::filesystem::system_complete(slicer_path)).string());
    std::string lock_name = std::to_string(hashed_path);
    return !get_other_downloader_exists(lock_name);
#endif 
	return false;
}
bool DownloaderSend::send_url(const wxString& url) const
{
#ifdef _WIN32
	//std::string escaped = escape_strings_cstyle({ boost::nowide::narrow(url) });
	return send_message_downloader(url);
#else
	std::string slicer_path = boost::dll::program_location().string();
    size_t hashed_path = std::hash<std::string>{}(boost::filesystem::canonical(boost::filesystem::system_complete(slicer_path)).string());
    std::string lock_name = std::to_string(hashed_path);
    return send_message_downloader(boost::nowide::narrow(url), lock_name);
#endif
	return false;
}


// --------------OtherDownloaderMessageHandler---------------------

wxDEFINE_EVENT(EVT_URL_MSG, wxCommandEvent);
wxEvtHandler* evt_handler_global;

void OtherDownloaderMessageHandler::init(wxEvtHandler* callback_evt_handler)
{
	assert(!m_initialized);
	assert(m_callback_evt_handler == nullptr);
	if (m_initialized) 
		return;

	m_initialized = true;
	m_callback_evt_handler = callback_evt_handler;

	

#if defined(__APPLE__)
	//this->register_for_messages(wxGetApp().get_instance_hash_string());
#endif //__APPLE__

#ifdef BACKGROUND_DOWNLOADER_MESSAGE_LISTENER
	std::cout<< "init OtherDownloaderMessageHandler"<<std::endl;
	m_thread = boost::thread((boost::bind(&OtherDownloaderMessageHandler::listen, this)));
#endif //BACKGROUND_DOWNLOADER_MESSAGE_LISTENER
}

void OtherDownloaderMessageHandler::shutdown()
{
	BOOST_LOG_TRIVIAL(debug) << "message handler shutdown().";
#ifndef _WIN32
	//instance_check_internal::delete_lockfile();
#endif //!_WIN32
	assert(m_initialized);
	if (m_initialized) {
#ifdef _WIN32
		//HWND hwnd = main_frame->GetHandle();
		//RemoveProp(hwnd, L"Instance_Hash_Minor");
		//RemoveProp(hwnd, L"Instance_Hash_Major");
#endif //_WIN32
#if __APPLE__
		//delete macos implementation
		//this->unregister_for_messages();
#endif //__APPLE__
#ifdef BACKGROUND_DOWNLOADER_MESSAGE_LISTENER
		if (m_thread.joinable()) {
			// Stop the worker thread, if running.
			{
				// Notify the worker thread to cancel wait on detection polling.
				std::lock_guard<std::mutex> lck(m_thread_stop_mutex);
				m_stop = true;
			}
			m_thread_stop_condition.notify_all();
			// Wait for the worker thread to stop.
			m_thread.join();
			m_stop = false;
		}
#endif //BACKGROUND_DOWNLOADER_MESSAGE_LISTENER
		m_callback_evt_handler = nullptr;
		m_initialized = false;
	}
}

#ifdef _WIN32 
void OtherDownloaderMessageHandler::init_windows_properties(MainFrame* main_frame, size_t instance_hash)
{
	//size_t       minor_hash = instance_hash & 0xFFFFFFFF;
	//size_t       major_hash = (instance_hash & 0xFFFFFFFF00000000) >> 32;
	//HWND         hwnd = main_frame->GetHandle();
	//HANDLE       handle_minor = UIntToPtr(minor_hash);
	//HANDLE       handle_major = UIntToPtr(major_hash);
	//SetProp(hwnd, L"Instance_Hash_Minor", handle_minor);
	//SetProp(hwnd, L"Instance_Hash_Major", handle_major);
	//BOOST_LOG_TRIVIAL(debug) << "window properties initialized " << instance_hash << " (" << minor_hash << " & "<< major_hash;
}

#if 0

void OtherInstanceMessageHandler::print_window_info(HWND hwnd)
{
	std::wstring instance_hash = boost::nowide::widen(wxGetApp().get_instance_hash_string());
	TCHAR 		 wndText[1000];
	TCHAR 		 className[1000];
	GetClassName(hwnd, className, 1000);
	GetWindowText(hwnd, wndText, 1000);
	std::wstring classNameString(className);
	std::wstring wndTextString(wndText);
	HANDLE       handle = GetProp(hwnd, L"Instance_Hash_Minor");
	size_t       result = PtrToUint(handle);
	handle = GetProp(hwnd, L"Instance_Hash_Major");
	size_t       r2 = PtrToUint(handle);
	r2 = (r2 << 32);
	result += r2;
	BOOST_LOG_TRIVIAL(info) << "window info: " << result;
}
#endif //0
#endif  //WIN32
namespace {
   // returns ::path to possible model or empty ::path if input string is not existing path
	boost::filesystem::path get_path(const std::string& possible_path)
	{
		BOOST_LOG_TRIVIAL(debug) << "message part:" << possible_path;

		if (possible_path.empty() || possible_path.size() < 3) {
			BOOST_LOG_TRIVIAL(debug) << "empty";
			return boost::filesystem::path();
		}
		if (boost::filesystem::exists(possible_path)) {
			BOOST_LOG_TRIVIAL(debug) << "is path";
			return boost::filesystem::path(possible_path);
		} else if (possible_path[0] == '\"') {
			if(boost::filesystem::exists(possible_path.substr(1, possible_path.size() - 2))) {
				BOOST_LOG_TRIVIAL(debug) << "is path in quotes";
				return boost::filesystem::path(possible_path.substr(1, possible_path.size() - 2));
			}
		}
		BOOST_LOG_TRIVIAL(debug) << "is NOT path";
		return boost::filesystem::path();
	}
} //namespace 

void OtherDownloaderMessageHandler::handle_message(const std::string& message) 
{
	BOOST_LOG_TRIVIAL(info) << "message from other instance: " << message;

	std::vector<std::string> args;
	bool parsed = Slic3r::unescape_strings_cstyle(message, args);
	assert(parsed);
	if (! parsed) {
		BOOST_LOG_TRIVIAL(error) << "message from other instance is incorrectly formatted: " << message;
		return;
	}

	std::vector<boost::filesystem::path> paths;
	// Skip the first argument, it is the path to the slicer executable.
	auto it = args.begin();
	for (++ it; it != args.end(); ++ it) {
		//boost::filesystem::path p = MessageHandlerInternal::get_path(*it);
		//if (! p.string().empty())
		//	paths.emplace_back(p);
	}
	if (! paths.empty()) {
		//wxEvtHandler* evt_handler = wxGetApp().plater(); //assert here?
		//if (evt_handler) {
	//		wxPostEvent(m_callback_evt_handler, LoadFromOtherInstanceEvent(Slic3r::GUI::EVT_LOAD_MODEL_OTHER_INSTANCE, std::vector<boost::filesystem::path>(std::move(paths))));
		//}
	}
}

#ifdef __APPLE__
//void OtherDownloaderMessageHandler::handle_message_other_closed() 
//{
//	instance_check_internal::get_lock(wxGetApp().get_instance_hash_string() + ".lock", data_dir() + "/cache/");
//}
#endif //__APPLE__

#ifdef BACKGROUND_DOWNLOADER_MESSAGE_LISTENER

namespace 
{
	std::string get_instance_hash()
	{
		std::string slicer_path = (boost::dll::program_location()).string();
	    size_t hashed_path = std::hash<std::string>{}(boost::filesystem::canonical(boost::filesystem::system_complete(slicer_path)).string());
	    std::string lock_name = std::to_string(hashed_path);
		//std::cout << "hash: "<< lock_name;
		return lock_name;	
	}
	//reply to introspect makes our DBus object visible for other programs like D-Feet
	void respond_to_introspect(DBusConnection *connection, DBusMessage *request) 
	{
    	DBusMessage *reply;
	    const char  *introspection_data =
	        " <!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\" "
	        "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">"
	        " <!-- dbus-sharp 0.8.1 -->"
	        " <node>"
	        "   <interface name=\"org.freedesktop.DBus.Introspectable\">"
	        "     <method name=\"Introspect\">"
	        "       <arg name=\"data\" direction=\"out\" type=\"s\" />"
	        "     </method>"
	        "   </interface>"
	        "   <interface name=\"com.prusa3d.prusaslicer.Downloader\">"
	        "     <method name=\"AnotherInstance\">"
	        "       <arg name=\"data\" direction=\"in\" type=\"s\" />"
	        "     </method>"
	        "   </interface>"
	        " </node>";
	     
	    reply = dbus_message_new_method_return(request);
	    dbus_message_append_args(reply, DBUS_TYPE_STRING, &introspection_data, DBUS_TYPE_INVALID);
	    dbus_connection_send(connection, reply, NULL);
	    dbus_message_unref(reply);
	}
	//method AnotherInstance receives message from another PrusaSlicer instance 
	void handle_method_another_instance(DBusConnection *connection, DBusMessage *request)
	{
	    DBusError     err;
	    char*         text = nullptr;
		wxEvtHandler* evt_handler;

	    dbus_error_init(&err);
	    dbus_message_get_args(request, &err, DBUS_TYPE_STRING, &text, DBUS_TYPE_INVALID);
	    if (dbus_error_is_set(&err)) {
	    	BOOST_LOG_TRIVIAL(trace) << "Dbus method AnotherInstance received with wrong arguments.";
	    	dbus_error_free(&err);
	        return;
	    }

		if (evt_handler_global) {
			wxCommandEvent* evt = new wxCommandEvent(EVT_URL_MSG);
			evt->SetString(boost::nowide::widen(text));
			evt_handler_global->QueueEvent(evt);
		}
		
	}

	//every dbus message received comes here
DBusHandlerResult handle_dbus_object_message(DBusConnection *connection, DBusMessage *message, void *user_data)
{
	const char* interface_name = dbus_message_get_interface(message);
	const char* member_name    = dbus_message_get_member(message);
	std::string our_interface  = "com.prusa3d.prusaslicer.Downloader.Object" + get_instance_hash();
    //BOOST_LOG_TRIVIAL(trace) << "DBus message received: interface: " << interface_name << ", member: " << member_name;
    std::cout << "DBus message received: interface: " << interface_name << ", member: " << member_name << std::endl;
    if (0 == strcmp("org.freedesktop.DBus.Introspectable", interface_name) && 0 == strcmp("Introspect", member_name)) {		
        respond_to_introspect(connection, message);
        return DBUS_HANDLER_RESULT_HANDLED;
    } else if (0 == strcmp(our_interface.c_str(), interface_name) && 0 == strcmp("AnotherInstance", member_name)) {
        handle_method_another_instance(connection, message);
        return DBUS_HANDLER_RESULT_HANDLED;
    } else if (0 == strcmp(our_interface.c_str(), interface_name) && 0 == strcmp("Introspect", member_name)) {
        respond_to_introspect(connection, message);
        return DBUS_HANDLER_RESULT_HANDLED;
    } 
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}
	
} //namespace 

void OtherDownloaderMessageHandler::listen()
{
    DBusConnection* 	 conn;
    DBusError 			 err;
    int 				 name_req_val;
    DBusObjectPathVTable vtable;
    std::string 		 instance_hash  = get_instance_hash();
	std::string			 interface_name = "com.prusa3d.prusaslicer.Downloader.Object" + instance_hash;
    std::string			 object_name 	= "/com/prusa3d/prusaslicer/Downloader/Object" + instance_hash;

    //BOOST_LOG_TRIVIAL(debug) << "init dbus listen " << interface_name << " " << object_name;
    std::cout<< "init dbus listen " << interface_name << " " << object_name << std::endl;

    dbus_error_init(&err);

    // connect to the bus and check for errors (use SESSION bus everywhere!)
    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) { 
	    BOOST_LOG_TRIVIAL(error) << "DBus Connection Error: "<< err.message;
	    BOOST_LOG_TRIVIAL(error) << "Dbus Messages listening terminating.";
        dbus_error_free(&err); 
        return;
    }
    if (NULL == conn) { 
		BOOST_LOG_TRIVIAL(error) << "DBus Connection is NULL. Dbus Messages listening terminating.";
        return;
    }

	// request our name on the bus and check for errors
	name_req_val = dbus_bus_request_name(conn, interface_name.c_str(), DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
	if (dbus_error_is_set(&err)) {
	    BOOST_LOG_TRIVIAL(error) << "DBus Request name Error: "<< err.message; 
	    BOOST_LOG_TRIVIAL(error) << "Dbus Messages listening terminating.";
	    dbus_error_free(&err); 
	    dbus_connection_unref(conn);
	    return;
	}
	if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != name_req_val) {
		BOOST_LOG_TRIVIAL(error) << "Not primary owner of DBus name - probably another PrusaSlicer instance is running.";
	    BOOST_LOG_TRIVIAL(error) << "Dbus Messages listening terminating.";
	    dbus_connection_unref(conn);
	    return;
	}

	evt_handler_global = m_callback_evt_handler;

	// Set callbacks. Unregister function should not be nessary.
	vtable.message_function = handle_dbus_object_message;
    vtable.unregister_function = NULL;

    // register new object - this is our access to DBus
    dbus_connection_try_register_object_path(conn, object_name.c_str(), &vtable, NULL, &err);
   	if ( dbus_error_is_set(&err) ) {
   		BOOST_LOG_TRIVIAL(error) << "DBus Register object Error: "<< err.message; 
	    BOOST_LOG_TRIVIAL(error) << "Dbus Messages listening terminating.";
	    dbus_connection_unref(conn);
		dbus_error_free(&err);
		return;
	}

	//BOOST_LOG_TRIVIAL(trace) << "Dbus object "<< object_name <<" registered. Starting listening for messages.";
	std::cout << "Dbus object "<< object_name <<" registered. Starting listening for messages." << std::endl;

	for (;;) {
		// Wait for 1 second 
		// Cancellable.
		{
			std::unique_lock<std::mutex> lck(m_thread_stop_mutex);
			m_thread_stop_condition.wait_for(lck, std::chrono::seconds(1), [this] { return m_stop; });
		}
		if (m_stop)
			// Stop the worker thread.

			break;
		//dispatch should do all the work with incoming messages
		//second parameter is blocking time that funciton waits for new messages
		//that is handled here with our own event loop above
		dbus_connection_read_write_dispatch(conn, 0);
     }
     
   	 dbus_connection_unref(conn);
}
#endif //BACKGROUND_DOWNLOADER_MESSAGE_LISTENER






}