#include "InstanceSend.hpp"

#ifdef _WIN32
#include <windows.h>
#include <strsafe.h>
#endif //WIN32

#include <boost/nowide/convert.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <wx/utils.h>

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
#endif
}


// ------ SlicerSend ----------------


bool SlicerSend::get_instance_exists() const
{
    return !EnumWindows(EnumWindowsProcSlicer, 0);
}
bool SlicerSend::send_path(const wxString& path) const
{
	std::string escaped = escape_strings_cstyle({ "prusa-downloader", boost::nowide::narrow(path) });
    return send_message_slicer(boost::nowide::widen(escaped));
}

bool SlicerSend::start_with_path(const wxString& path) const
{
	// "C:\\Users\\User\\Downloads\\PrusaSlicer-2.4.2+win64-202204251110\\prusa-slicer.exe " 
	std::string escaped = escape_strings_cstyle({  boost::nowide::narrow(path) });
	//return execute_command(boost::nowide::widen(escaped));
	std::string binary = (boost::dll::program_location().parent_path() / "prusa-slicer.exe").string() + " ";
	//return execute_command("C:\\Users\\User\\Downloads\\PrusaSlicer-2.4.2+win64-202204251110\\prusa-slicer.exe " + boost::nowide::widen(escaped));
	return execute_command(boost::nowide::widen(binary) + boost::nowide::widen(escaped));
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
	return !EnumWindows(EnumWindowsProcDownloader, 0);
}
bool DownloaderSend::send_url(const wxString& url) const
{
	//std::string escaped = escape_strings_cstyle({ boost::nowide::narrow(url) });
	return send_message_downloader(url);
}


}