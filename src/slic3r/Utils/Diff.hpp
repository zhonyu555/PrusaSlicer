#include <string>

namespace slic3r {
	class Diff {
	public:
		Diff(std::string _str1, std::string _str2);

		void solve();

	private:
		std::string str1;
		std::string str2;
	};
}