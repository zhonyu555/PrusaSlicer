#ifndef slic3r_Utils_Diff_hpp_
#define slic3r_Utils_Diff_hpp_

#include <string>
#include <vector>

namespace slic3r {
	class Diff {
	public:
		struct EditScriptAction;
		typedef std::vector<EditScriptAction> EditScript;

		struct EditScriptAction {
			struct ActionType {
				static const int nil				= 0;
				static const int baseTypeMask		= 0x0FFF;
				static const int lineBreakType		= 0x1000;

				static const int keep				= 1; //from A
				static const int remove				= 2; //from A
				static const int insert				= 3; //from B

				//only when split_newline==true
				static const int keep_lineBreak		= keep + lineBreakType;
				static const int remove_lineBreak	= remove + lineBreakType;
				static const int insert_lineBreak	= insert + lineBreakType;
			};

			int actionType;
			size_t offset;
			size_t count;

			EditScriptAction(int _actionType, size_t _off, size_t _count) :
				actionType(_actionType),
				offset(_off),
				count(_count)
			{};

			const std::string& get_target_str(const std::string& str1, const std::string& str2);

			EditScript split_by_newline(const std::string& str1, const std::string& str2);
		};


		Diff(const std::string& _str1, const std::string& _str2, bool split_newline = false);

		const EditScript& getSolution();

#ifdef _DEBUG
		void selfTest(int count = 100);
#endif
	private:
		EditScript solution;

		void solve(const std::string& str1, const std::string& str2, bool split_newline);
	};
}

#endif